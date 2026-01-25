#include "AAF_block.h"
#include "shared_malloc.h"
#include "is_admin.h"
#include "zeros_array.h"
#include "qp_timer.h"

#define MOVE_FILE_MAX_RETRIES 3

// Порог сравнения текущего времени операции (в X раз) и самого длинного из прошлых
// Используется для детекта некешированного чтения
#define THRESHOLD_X_TIMES_FOR_NON_CACHED_OP 2
#define THRESHOLD_X_TIMES_INIT 0
// Сколько блоков нужно закешировать вперед
#define BLOCKS_TO_PREFETCH 10

// Два 8 байтовых числа без заглушки буфера
// LARGE_INTEGER StartingLcn;
// LARGE_INTEGER BitmapSize;
#define VOLUME_BITMAP_HEADER_SIZE 16

// 8*8 кластеров это минимальный блок без ошибки
#define FAST_CHECK_BITMAP_BYTES 8

//
// _SM_ - shared_malloc функции
// префикс для функций использующих общий грязный буфер
// результаты этих функций должны сохраняться отдельно либо использоваться до вызова следующей
//

static inline wchar_t* _SM_get_file_path_by_file_handle(HANDLE hFile)
{
    DWORD symbols_needed, symbols_buf = 2048;
    wchar_t *filePath;

    while (1) {
        filePath = (wchar_t *)shared_malloc(symbols_buf * sizeof(wchar_t), SM_BUFFER_COMMON, SM_DATA_DISCARD);
        symbols_needed = GetFinalPathNameByHandleW(hFile, filePath, symbols_buf, VOLUME_NAME_GUID);
        if (symbols_needed == 0) return NULL;
        if (symbols_needed < symbols_buf) break;
        symbols_buf = symbols_needed;
    }
    if (wcsncmp(filePath, L"\\\\?\\Volume{", 11) != 0) return NULL;

    return filePath;
}

static inline HANDLE _SM_get_volume_handle_by_file_handle(HANDLE hFile)
{
    wchar_t *filePath = _SM_get_file_path_by_file_handle(hFile);
    if (filePath == NULL) return INVALID_HANDLE_VALUE;

    filePath[48] = 0; // обрезка до GUID последний слеш *не нужен*
    
    return CreateFileW(
        filePath,
        GENERIC_READ,  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, // NOLINT(hicpp-signed-bitwise)
        NULL, OPEN_EXISTING, 0, NULL
    );
}

static inline DWORD _SM_get_cluster_size_by_file_handle(HANDLE hFile)
{
    wchar_t *filePath = _SM_get_file_path_by_file_handle(hFile);
    if (filePath == NULL) return 0;

    filePath[49] = 0; // обрезка до GUID последний слеш *нужен*

    DWORD lpSectorsPerCluster, lpBytesPerSector,
          lpNumberOfFreeClusters, lpTotalNumberOfClusters;

    if (!GetDiskFreeSpaceW(
        filePath,
        &lpSectorsPerCluster,    &lpBytesPerSector,
        &lpNumberOfFreeClusters, &lpTotalNumberOfClusters
    )) {
        return 0;
    }
    return lpSectorsPerCluster * lpBytesPerSector;
}

static inline int _SM_check_free_block_in_bitmap(HANDLE hVolume, LONGLONG StartingLcn_c8, LONGLONG BlockLength_c8, LONGLONG *pCheckedStartingLcn_c8, LONGLONG *pCheckedBlockLength_c8, LONGLONG *pBitmapSize_c8, int *pIsFreeBlock)
{
    int ret = 0;
    
    size_t bufferSize;
    DWORD bytesReturned;
    
    LARGE_INTEGER         StartingLcn;
    VOLUME_BITMAP_BUFFER *pvb_buffer;

    StartingLcn.QuadPart = StartingLcn_c8 * 8;
    bufferSize = VOLUME_BITMAP_HEADER_SIZE + BlockLength_c8;

    pvb_buffer = (VOLUME_BITMAP_BUFFER*)shared_malloc(bufferSize, SM_BUFFER_COMMON, SM_DATA_DISCARD);

    if (pvb_buffer == NULL) {
        ret = -1;
        goto err_aaf_check_free_block_in_bitmap;
    }
    
    BOOL res = DeviceIoControl(
        hVolume, FSCTL_GET_VOLUME_BITMAP, // NOLINT(hicpp-signed-bitwise)
        &StartingLcn, sizeof(StartingLcn),
        pvb_buffer, bufferSize,
        &bytesReturned, NULL
    );
    if (!res && (GetLastError() != ERROR_MORE_DATA)){
        ret = -2;
        goto err_aaf_check_free_block_in_bitmap;
    }

    *pCheckedStartingLcn_c8 = pvb_buffer->StartingLcn.QuadPart / 8LL;
    *pCheckedBlockLength_c8 = bytesReturned - VOLUME_BITMAP_HEADER_SIZE;
    *pBitmapSize_c8 = ((pvb_buffer->BitmapSize.QuadPart + 7ULL) & ~7ULL) / 8LL;
    *pIsFreeBlock = is_all_zeros_array(pvb_buffer->Buffer, *pCheckedBlockLength_c8);

err_aaf_check_free_block_in_bitmap:
    return ret;
}

static inline int _SM_get_file_extents(HANDLE hFile, RETRIEVAL_POINTERS_BUFFER **ppBuffer)
{
    int ret = 0;

    DWORD bytesReturned = 0;
    STARTING_VCN_INPUT_BUFFER inputBuffer = {};

    DWORD bufferSize = 2048; // 126 фрагментов
    
    while (1) {
        *ppBuffer = shared_malloc(bufferSize, SM_BUFFER_COMMON, SM_DATA_DISCARD);
        if (!*ppBuffer) {
            ret = -1;
            goto err_aaf_get_file_extents;
        }

        inputBuffer.StartingVcn.QuadPart = 0;
        BOOL result = DeviceIoControl(
            hFile,
            FSCTL_GET_RETRIEVAL_POINTERS, // NOLINT(hicpp-signed-bitwise)
            &inputBuffer, sizeof(inputBuffer),
            *ppBuffer, bufferSize,
            &bytesReturned, NULL
        );
        if (result || GetLastError() == ERROR_HANDLE_EOF) {
            ret = 0;
            goto err_aaf_get_file_extents;
        } else {
            if (GetLastError() == ERROR_MORE_DATA) {
                bufferSize *= 2;
                if (bufferSize >= 1024 * 1024) break; // это больше 65K фрагментов, нереально в нормальных условиях
                continue;
            } else {
                ret = -2;
                goto err_aaf_get_file_extents;
            }
        }
    }
    
    ret = -3;

err_aaf_get_file_extents:
    return ret;
}

static inline int move_extent(HANDLE hVolume, MOVE_FILE_DATA *pMoveFileData)
{
    int ret = 0;

    DWORD bytesReturned;
    BOOL result;

    if (hVolume == INVALID_HANDLE_VALUE) {
        ret = -1;
        goto err_aaf_move_file_clusters;
    }

    result = DeviceIoControl(
        hVolume,
        FSCTL_MOVE_FILE,  // NOLINT(hicpp-signed-bitwise)
        pMoveFileData, sizeof(MOVE_FILE_DATA),
        NULL, 0, &bytesReturned, NULL
    );
    if (!result) {
        ret = -2;
        goto err_aaf_move_file_clusters;
    }

err_aaf_move_file_clusters:
    return ret;
}

static inline LONGLONG calc_extents_size_in_clusters(RETRIEVAL_POINTERS_BUFFER *pRPB, DWORD extents_num)
{
    LARGE_INTEGER currentVcn = pRPB->StartingVcn;
    LONGLONG ret = 0;

    for (DWORD i = 0; i < extents_num; i++) {
        LARGE_INTEGER nextVcn = pRPB->Extents[i].NextVcn;
        ret += nextVcn.QuadPart - currentVcn.QuadPart;
        currentVcn = nextVcn;
    }
    
    return ret;
}

static inline LARGE_INTEGER get_file_extent_vcn(RETRIEVAL_POINTERS_BUFFER *pRPB, size_t extent_num)
{
    if (extent_num < 2) {
        return pRPB->StartingVcn;
    }
    return pRPB->Extents[extent_num - 2].NextVcn;
}

static int _SM_get_file_size_and_extents(HANDLE hFile, LARGE_INTEGER *file_size, RETRIEVAL_POINTERS_BUFFER **ppBuffer)
{
    if (!GetFileSizeEx(hFile, file_size)) return 0;
    if (_SM_get_file_extents(hFile, ppBuffer) != 0) return 0;
    return 1;
}

// сверяется текущее вычисленное время и прошлое максимальное
// threshold_x_times - во сколько раз максимальные прошлый замер должен быть меньше текущего
// threshold_x_times = THRESHOLD_X_TIMES_INIT - сброс статистики
static int is_non_cache_operation(size_t threshold_x_times)
{
    static LONGLONG prev_time, prev_max_time;
    LONGLONG current_time, operation_time;
    int is_non_cache = 0;

    current_time = qp_time_get();
    if (threshold_x_times == THRESHOLD_X_TIMES_INIT) {
       prev_max_time = 0;
    } else {
        operation_time = qp_timer_diff_100ns(prev_time, current_time);
        is_non_cache = operation_time > prev_max_time * threshold_x_times;
        if (prev_max_time < operation_time) prev_max_time = operation_time;
    }

    prev_time = current_time;
    return is_non_cache;
}

static int _SM_find_free_block(
        HANDLE hVolume, LONGLONG StartingLcn_c8, LONGLONG BlockLength_c8, size_t BlocksToFind,
        LONGLONG *CheckedStartingLcn_c8, LONGLONG *searchSkip, LONGLONG *searchTotal,
        size_t *FreeBlocksFound, int *IsLastReadNonCached, int *IsEndOfVolume
) {
    LONGLONG CheckedBlockLength_c8;
    LONGLONG BitmapSize_c8;
    int IsFreeBlock;

    is_non_cache_operation(THRESHOLD_X_TIMES_INIT);
    while (1)
    {
        if (searchTotal != NULL) (*searchTotal)++;

        // суть в том чтобы не читать весь битмап при прыжках по заполненному диску
        // Больще оптимизация для самого алллокатора
        if (_SM_check_free_block_in_bitmap(
                hVolume, StartingLcn_c8, FAST_CHECK_BITMAP_BYTES,
                CheckedStartingLcn_c8, &CheckedBlockLength_c8,
                &BitmapSize_c8, &IsFreeBlock
        )) {
            return 1;
        }
        *IsLastReadNonCached = is_non_cache_operation(THRESHOLD_X_TIMES_FOR_NON_CACHED_OP);
        *IsEndOfVolume = BitmapSize_c8 <= BlockLength_c8;

        if (!IsFreeBlock) {
            if (searchSkip != NULL) (*searchSkip)++;
            if (*IsEndOfVolume) return 0;
            StartingLcn_c8 = *CheckedStartingLcn_c8 + BlockLength_c8;
            continue;
        }

        if (_SM_check_free_block_in_bitmap(
                hVolume, StartingLcn_c8, BlockLength_c8,
                CheckedStartingLcn_c8, &CheckedBlockLength_c8,
                &BitmapSize_c8, &IsFreeBlock
        )) {
            return 1;
        }
        *IsLastReadNonCached = is_non_cache_operation(THRESHOLD_X_TIMES_FOR_NON_CACHED_OP);
        *IsEndOfVolume = BitmapSize_c8 <= CheckedBlockLength_c8;

        if (IsFreeBlock) {
            (*FreeBlocksFound)++;
            if (*FreeBlocksFound >= BlocksToFind) return 0;
        }

        StartingLcn_c8 = *CheckedStartingLcn_c8 + CheckedBlockLength_c8;
        if (*IsEndOfVolume) return 0;
    }
}

static int AAF_set_file_size(HANDLE hFile, LARGE_INTEGER new_size)
{
    if (!SetFilePointerEx(hFile, new_size, NULL, FILE_BEGIN)) return 0;
    if (!SetEndOfFile(hFile)) return 0;
    return 1;
}

int AAF_alloc_block(HANDLE hFile, LONGLONG blockSize, LONGLONG alignSize, LONGLONG *pStatusCode, AAF_stats_t *pStats)
{
    LONGLONG qp_timer_start, qp_timer_end;
    LONGLONG qp_timer_move_start = 0, qp_timer_move_end = 0;
    LONGLONG qp_timer_prefetch_start = 0, qp_timer_prefetch_end = 0;
    qp_timer_start = qp_time_get();

    int res;
    int status_code = 0;

    ZeroMemory(pStats, sizeof(AAF_stats_t));

    HANDLE hVolume = INVALID_HANDLE_VALUE;
    RETRIEVAL_POINTERS_BUFFER *pRPB_shared;

    LARGE_INTEGER old_file_size; // текущий размер файла
    LARGE_INTEGER new_file_size; // текущий размер файла + 1 блок
    LARGE_INTEGER frg_file_size; // размер [без врагментов] + 1 кластер

    DWORD old_extent_count, new_extent_count, old_file_size_in_clusters;

    // текущий размер и фрагменты
    if (!_SM_get_file_size_and_extents(hFile, &old_file_size, &pRPB_shared)) {
        status_code = -1;
        goto err_aaf_alloc_block;
    }
    old_extent_count = pRPB_shared->ExtentCount;
    old_file_size_in_clusters = calc_extents_size_in_clusters(pRPB_shared, old_extent_count);

    // Попытка просто аллоцировать новый блок
    new_file_size.QuadPart = old_file_size.QuadPart + blockSize;
    if (!AAF_set_file_size(hFile, new_file_size)) {
        status_code = -2;
        goto err_aaf_alloc_block;
    }

    // новый размер и блоки
    if (!_SM_get_file_size_and_extents(hFile, &new_file_size, &pRPB_shared)) {
        status_code = -3;
        goto err_aaf_alloc_block;
    }
    new_extent_count = pRPB_shared->ExtentCount;

    // нет дополнительной фрагментации
    if (old_extent_count >= new_extent_count) {
        goto err_aaf_alloc_block;
    }

    // без административных прав мы не можем дальше продолжать
    // файл был просто аллоцирован с фрагментацией но не выровнен
    if (!isAdmin()) {
        status_code = 1;
        goto err_aaf_alloc_block;
    }

    // это vcn нового фрагмента, указывает +1 от старого потому что новых внезапно может быть больше 1
    LARGE_INTEGER new_fragment_vcn = get_file_extent_vcn(pRPB_shared, old_extent_count + 1);

    // для кластерного перемещения нужен хендл диска
    hVolume = _SM_get_volume_handle_by_file_handle(hFile);
    if (hVolume == INVALID_HANDLE_VALUE) {
        status_code = -4;
        goto err_aaf_alloc_block;
    }

    // размер кластера необходим так как все вычисления в экстентах и битмапе идут в кластерах
    LONGLONG cluster_size = _SM_get_cluster_size_by_file_handle(hFile);
    if (cluster_size == 0) {
        status_code = -5;
        goto err_aaf_alloc_block;
    }

    // нам нужно оставить 1 кластер во фрагменте который будем перемещать
    // поэтому берем старый размер в кластерах и прибавляем 1
    // размер нужен в байтах поэтому * cluster_size
    frg_file_size.QuadPart = (old_file_size_in_clusters + 1) * cluster_size;

    // уменьшаем файл чтобы образовался 1 кластер от последнего фрагмента
    if (!AAF_set_file_size(hFile, frg_file_size)) {
        status_code = -6;
        goto err_aaf_alloc_block;
    }

    // Откуда искать, можно скипнуть первый блок, он всегда будет занят, но энивей это быстрый скип по 8 байтам
    // *_c8 переменные относятся к битмапу, так как в нем каждый байт это 8 кластеров '
    LONGLONG StartingLcn_c8 = 0;
    LONGLONG CheckedStartingLcn_c8 = 0;
    LONGLONG BlockLength_c8 = alignSize / cluster_size / 8LL;

    // Структура заполняется таким образом чтобы переместился 1 последний экстент из одного кластера что готовли выше
    MOVE_FILE_DATA pMoveFileData;
    pMoveFileData.FileHandle   = hFile;
    pMoveFileData.StartingVcn  = new_fragment_vcn;
    pMoveFileData.ClusterCount = 1;

    size_t FreeBlocksFound = 0;
    int IsLastReadNonCached = 0, IsEndOfVolume = 0;

    while (1)
    {
        // Ищем первый свободный блок
        if (_SM_find_free_block(
                hVolume, StartingLcn_c8, BlockLength_c8, 1,
                &CheckedStartingLcn_c8, &pStats->searchSkip, &pStats->searchTotal,
                &FreeBlocksFound, &IsLastReadNonCached, &IsEndOfVolume
        )) {
            status_code = -8;
            goto err_aaf_alloc_block;
        }
        if (FreeBlocksFound == 0) break;

        // Смещаем точку поиска для следующего поиска
        StartingLcn_c8 = CheckedStartingLcn_c8 + BlockLength_c8;

        // Пробуем переместить кластер, CheckedStartingLcn_c8 это актуальный адрес блока
        pStats->moveAttempts++;
        pMoveFileData.StartingLcn.QuadPart = CheckedStartingLcn_c8 * 8LL;
        qp_timer_move_start = qp_time_get();
        res = move_extent(hVolume, &pMoveFileData);
        qp_timer_move_end = qp_time_get();
        if (res == 0) {
            pStats->allocOffset = pMoveFileData.StartingLcn.QuadPart * cluster_size;
            break;
        }

        // Сюда мы попасть не должны, но вдруг бывают ошибки перемещений
        FreeBlocksFound = 0;
        // Выходим если уже много ошибок или дошли до конца диска
        if (pStats->moveAttempts == MOVE_FILE_MAX_RETRIES || IsEndOfVolume) break;
    }

    if (FreeBlocksFound == 0) {
        // Если свободный блок не найден
        // То сжимаем вначале файл до оригинального размера
        // Иначе ntfs оставит этот висящий кластер и выделит полный блок
        if (!AAF_set_file_size(hFile, old_file_size)) {
            status_code = -9;
            goto err_aaf_alloc_block;
        }
        status_code = 2;
    }

    // Теперь увеличиваем размер до планируемого
    if (!AAF_set_file_size(hFile, new_file_size)) {
        status_code = -10;
        goto err_aaf_alloc_block;
    }

    // В случае если не смогли переместить и юзали стандартное выделение
    // Интересно будет ли вообще такое
    if (pStats->moveAttempts == MOVE_FILE_MAX_RETRIES) {
        status_code = 3;
        goto err_aaf_alloc_block;
    }

    // Если последнее чтение битмапа было не из кеша и есть что читать
    // То читаем битмап до BLOCKS_TO_PREFETCH новых свободных блоков
    // ну или сколько найдется, энивей ловим только ошибку
    if (IsLastReadNonCached && !IsEndOfVolume) {
        qp_timer_prefetch_start = qp_time_get();
        if (_SM_find_free_block(
                hVolume, StartingLcn_c8, BlockLength_c8, BLOCKS_TO_PREFETCH,
                &CheckedStartingLcn_c8, NULL, NULL,
                &FreeBlocksFound, &IsLastReadNonCached, &IsEndOfVolume
        )) {
            status_code = -11;
            goto err_aaf_alloc_block;
        }
        qp_timer_prefetch_end = qp_time_get();
    }

err_aaf_alloc_block:

    // Актуальные числа размера и екстентов для статистики
    if (_SM_get_file_size_and_extents(hFile, &new_file_size, &pRPB_shared)) {
        pStats->fileLength = new_file_size.QuadPart;
        pStats->fileFragments = pRPB_shared->ExtentCount;
    } else {
        status_code = -12;
    }

    if (hVolume != INVALID_HANDLE_VALUE) CloseHandle(hVolume);
    if (pStatusCode != NULL) *pStatusCode = status_code;

    qp_timer_end = qp_time_get();
    pStats->allocTime = qp_timer_diff_100ns(qp_timer_start, qp_timer_end);
    pStats->moveTime = qp_timer_diff_100ns(qp_timer_move_start, qp_timer_move_end);
    if (qp_timer_prefetch_end != 0) {
        pStats->prefetchTime = qp_timer_diff_100ns(qp_timer_prefetch_start, qp_timer_prefetch_end);
    }

    shared_malloc(SM_DATA_FREE, SM_BUFFER_COMMON, SM_DATA_DISCARD);

    return status_code >= 0;
}

