#include "AAF_block.h"
#include "func_dyn.h"
#include "shared_malloc.h"
#include "is_admin.h"
#include "zeros_array.h"
#include "qp_timer.h"

#define MOVE_FILE_MAX_RETRIES 3

// Идет замер времени операций чтения битмапа
// Если _самое_последнее_ чтение больше в X раз чем самое длинное из прошлых то это не кеш
#define THRESHOLD_X_TIMES_FOR_NON_CACHED_OP 2

// Сколько блоков нужно закешировать вперед
#define BLOCKS_TO_PREFETCH 10

// Два 8 байтовых числа без заглушки буфера
// LARGE_INTEGER StartingLcn;
// LARGE_INTEGER BitmapSize;
#define VOLUME_BITMAP_HEADER_SIZE 16

// 8*8 кластеров это минимальный блок без ошибки
#define FAST_CHECK_BITMAP_BYTES 8

static inline HANDLE AAF_get_volume_handle_by_file_handle(HANDLE hFile)
{
    wchar_t filePath[MAX_PATH];

    DWORD res = dyn_GetFinalPathNameByHandleW(hFile, filePath, MAX_PATH, VOLUME_NAME_GUID);
    if (res == 0) {
        return INVALID_HANDLE_VALUE;
    }
    if (wcsncmp(filePath, L"\\\\?\\Volume{", 11) != 0) {
        return INVALID_HANDLE_VALUE;
    }
    filePath[48] = 0; // обрезка пути к файлу
    
    return CreateFileW(
        filePath,
        GENERIC_READ,  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, // NOLINT(hicpp-signed-bitwise)
        NULL, OPEN_EXISTING, 0, NULL
    );
}

// Пока не знаю хочу ли оптимизировать с функцией выше
static inline DWORD AAF_get_cluster_size_by_file_handle(HANDLE hFile)
{
    wchar_t filePath[MAX_PATH];

    DWORD res = dyn_GetFinalPathNameByHandleW(hFile, filePath, MAX_PATH, VOLUME_NAME_GUID);
    if (res == 0) {
        return 0;
    }
    if (wcsncmp(filePath, L"\\\\?\\Volume{", 11) != 0) {
        return 0;
    }
    filePath[49] = 0; // обрезка пути к файлу

    DWORD lpSectorsPerCluster;
    DWORD lpBytesPerSector;
    DWORD lpNumberOfFreeClusters;
    DWORD lpTotalNumberOfClusters;

    if (!GetDiskFreeSpaceW(
        filePath,
        &lpSectorsPerCluster,    &lpBytesPerSector,
        &lpNumberOfFreeClusters, &lpTotalNumberOfClusters
    )) {
        return 0;
    }
    return lpSectorsPerCluster * lpBytesPerSector;
}

static inline int AAF_check_free_block_in_bitmap(HANDLE hVolume, LONGLONG StartingLcn_c8, LONGLONG BlockLength_c8, LONGLONG *pCheckedStartingLcn_c8, LONGLONG *pCheckedBlockLength_c8, LONGLONG *pBitmapSize_c8, int *pIsFreeBlock)
{
    int ret = 0;
    
    size_t bufferSize;
    DWORD bytesReturned;
    
    LARGE_INTEGER         StartingLcn;
    VOLUME_BITMAP_BUFFER *pvb_buffer;

    StartingLcn.QuadPart = StartingLcn_c8 * 8;
    bufferSize = VOLUME_BITMAP_HEADER_SIZE + BlockLength_c8;

    // в данном случае буфер нужен только тут  поэтому используем грязный буфер без реаллокации
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

static inline int AAF_get_file_extents(HANDLE hFile, RETRIEVAL_POINTERS_BUFFER **ppBuffer)
{
    int ret = 0;

    DWORD bytesReturned = 0;
    STARTING_VCN_INPUT_BUFFER inputBuffer = {};

    DWORD bufferSize = 2048; // 126 фрагментов
    
    while (1) {
        // используем грязный буфер, нам нужен результат только одной функции
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
    
    ret = -3; // слишком много фрагментов

err_aaf_get_file_extents:
    return ret;
}

static inline int AAF_move_extent(HANDLE hVolume, MOVE_FILE_DATA *pMoveFileData)
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

static inline LONGLONG calc_extents_size(RETRIEVAL_POINTERS_BUFFER *pRPB, DWORD extents_num)
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

static inline LARGE_INTEGER get_file_last_extent_vcn(RETRIEVAL_POINTERS_BUFFER *pRPB)
{
    if (pRPB->ExtentCount < 2) {
        return pRPB->StartingVcn;
    }
    return pRPB->Extents[pRPB->ExtentCount - 2].NextVcn;
}

static int get_file_size_and_extents(HANDLE hFile, LARGE_INTEGER *file_size, RETRIEVAL_POINTERS_BUFFER **ppBuffer)
{
    if (!GetFileSizeEx(hFile, file_size)) return 0;
    if (AAF_get_file_extents(hFile, ppBuffer) != 0) return 0;
    return 1;
}

// сверяется текущее вычисленное время и прошлое максимальное
// threshold_x_times - во сколько раз максимальные прошлый замер должен быть меньше текущего
// 0 - сброс статистики
static int is_non_cache_operation(size_t threshold_x_times)
{
    static LONGLONG prev_time, prev_max_time;
    LONGLONG current_time, operation_time;
    int is_non_cache = 0;

    current_time = qp_time_get();
    if (threshold_x_times == 0) {
       prev_max_time = 0;
    } else {
        operation_time = qp_timer_diff_100ns(prev_time, current_time);
        //printf("OP time: %"PRId64"\n", operation_time);
        is_non_cache = operation_time > prev_max_time * threshold_x_times;
        if (prev_max_time < operation_time) prev_max_time = operation_time;
    }

    prev_time = current_time;
    return is_non_cache;
}

static int find_free_block(
        HANDLE hVolume, LONGLONG StartingLcn_c8, LONGLONG BlockLength_c8, size_t BlocksToFind,
        LONGLONG *CheckedStartingLcn_c8, LONGLONG *searchSkip, LONGLONG *searchTotal,
        size_t *FreeBlocksFound, size_t *IsLastReadNonCached
) {
    LONGLONG CheckedBlockLength_c8;
    LONGLONG BitmapSize_c8;
    int IsFreeBlock;

    is_non_cache_operation(0);
    while (1)
    {
        if (searchTotal != NULL) (*searchTotal)++;

        // суть в том чтобы не читать весь битмап при прыжках по заполненному диску
        // Больще оптимизация для самого алллокатора
        //printf("Search free block (cluster) at: %"PRId64"\n", StartingLcn_c8);
        if (AAF_check_free_block_in_bitmap(
                hVolume, StartingLcn_c8, FAST_CHECK_BITMAP_BYTES,
                CheckedStartingLcn_c8, &CheckedBlockLength_c8,
                &BitmapSize_c8, &IsFreeBlock
        )) {
            return 1;
        }
        *IsLastReadNonCached = is_non_cache_operation(THRESHOLD_X_TIMES_FOR_NON_CACHED_OP);

        //printf("Checked clusters: %"PRId64"\n", CheckedBlockLength_c8);
        if (!IsFreeBlock) {
            if (searchSkip != NULL) (*searchSkip)++;
            //printf("BitmapSize_c8: %"PRId64", block: %"PRId64"\n", BitmapSize_c8, BlockLength_c8);
            //printf("Non free, skip: %"PRId64"\n", BlockLength_c8);
            // в данном случае скипается весь блок хоть запрашивали и 8 кластеров
            StartingLcn_c8 = *CheckedStartingLcn_c8 + BlockLength_c8;
            if (BitmapSize_c8 <= BlockLength_c8) break;
            continue;
        }

        //printf("Search full free block at: %"PRId64"\n", StartingLcn_c8 * 8 * 4096);

        // Если начало пустое то проверяем уже весь блок
        if (AAF_check_free_block_in_bitmap(
                hVolume, StartingLcn_c8, BlockLength_c8,
                CheckedStartingLcn_c8, &CheckedBlockLength_c8,
                &BitmapSize_c8, &IsFreeBlock
        )) {
            return 1;
        }
        *IsLastReadNonCached = is_non_cache_operation(THRESHOLD_X_TIMES_FOR_NON_CACHED_OP);

        if (IsFreeBlock) {
            (*FreeBlocksFound)++;
            //printf("FreeBlocksFound: %d, BlocksToFind: %d\n", (int)(*FreeBlocksFound), (int)BlocksToFind);
            if (*FreeBlocksFound >= BlocksToFind) return 0;
        }

        StartingLcn_c8 = *CheckedStartingLcn_c8 + CheckedBlockLength_c8;
        if (BitmapSize_c8 == CheckedBlockLength_c8) break;
    }

    return 0;
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
    LARGE_INTEGER new_file_size; // планированный размер + 1 блок
    LARGE_INTEGER frg_file_size; // размер [без врагментов] + 1 кластер

    DWORD old_extent_count, new_extent_count;

    // текущий размер и фрагменты
    if (!get_file_size_and_extents(hFile, &old_file_size, &pRPB_shared)) {
        status_code = -1;
        goto err_aaf_alloc_block;
    }
    old_extent_count = pRPB_shared->ExtentCount;
    //printf("old size: %"PRId64"\n", old_file_size.QuadPart);
    //printf("old extent count: %d\n", (int)old_extent_count);

    new_file_size.QuadPart = old_file_size.QuadPart + blockSize;

    // расширение файла
    if (!AAF_set_file_size(hFile, new_file_size)){
        status_code = -2;
        goto err_aaf_alloc_block;
    }

    // новый размер и блоки (размер перевычисляется для актуальности)
    if (!get_file_size_and_extents(hFile, &new_file_size, &pRPB_shared)) {
        status_code = -3;
        goto err_aaf_alloc_block;
    }
    new_extent_count = pRPB_shared->ExtentCount;
    //printf("resize to: %"PRId64"\n", new_file_size.QuadPart);
    //printf("new extent count: %d\n", (int)new_extent_count);

    // нет дополнительной фрагментации
    if (old_extent_count == new_extent_count) {
        //printf("No new fragments\n");
        goto err_aaf_alloc_block;
    }

    // без административных прав мы не можем дальше продолжать
    // файл был просто аллоцирован с фрагментацией но не выровнен
    if (!isAdmin()) {
        status_code = 1;
        goto err_aaf_alloc_block;
    }

    // для кластерного перемещения нужен хендл диска
    hVolume = AAF_get_volume_handle_by_file_handle(hFile);
    if (hVolume == INVALID_HANDLE_VALUE) {
        status_code = -4;
        goto err_aaf_alloc_block;
    }

    LONGLONG cluster_size = AAF_get_cluster_size_by_file_handle(hFile);
    if (cluster_size == 0) {
        status_code = -5;
        goto err_aaf_alloc_block;
    }

    // подсчет длины файла без последнего фрагмента в кластерах
    LONGLONG frag_size = calc_extents_size(pRPB_shared, old_extent_count);
    // а это размер в байтах + 1 кластер
    frg_file_size.QuadPart = (frag_size + 1) * cluster_size;
    //printf("resize to: %"PRId64"\n", frg_file_size.QuadPart);

    // уменьшаем файл чтобы образовался 1 кластер от последнего фрагмента
    if (!AAF_set_file_size(hFile, frg_file_size)){
        status_code = -6;
        goto err_aaf_alloc_block;
    }

    LONGLONG StartingLcn_c8 = 0;
    // битмап содержит в каждом байте по 8 кластеров
    LONGLONG BlockLength_c8 = alignSize / cluster_size / 8LL;
    LONGLONG CheckedStartingLcn_c8;

    MOVE_FILE_DATA pMoveFileData;
    pMoveFileData.FileHandle   = hFile;
    pMoveFileData.StartingVcn  = get_file_last_extent_vcn(pRPB_shared);
    pMoveFileData.ClusterCount = 1; // тут всегда 1 кластер, ради этого и уменьшали

    size_t FreeBlocksFound = 0, IsLastReadNonCached = 0;

    while (1)
    {
        if (find_free_block(
                hVolume, StartingLcn_c8, BlockLength_c8, 1,
                &CheckedStartingLcn_c8, &pStats->searchSkip, &pStats->searchTotal,
                &FreeBlocksFound, &IsLastReadNonCached
        )) {
            status_code = -7;
            goto err_aaf_alloc_block;
        }
        StartingLcn_c8 = CheckedStartingLcn_c8; // для следующей итерации
        if (FreeBlocksFound == 0) break;

        // пробуем переместить кластер
        pStats->moveAttempts++;
        pMoveFileData.StartingLcn.QuadPart = StartingLcn_c8 * 8LL;
        qp_timer_move_start = qp_time_get();
        res = AAF_move_extent(hVolume, &pMoveFileData);
        qp_timer_move_end = qp_time_get();
        //printf("move last cluster to: %"PRId64"\n", pMoveFileData.StartingLcn.QuadPart * 4096);

        if (res == 0) {
            pStats->allocOffset = pMoveFileData.StartingLcn.QuadPart * cluster_size;
            break;
        }
        FreeBlocksFound = 0; // блок мы нашди но не переместился, повторяем

        if (pStats->moveAttempts == MOVE_FILE_MAX_RETRIES) {
            // По сути блок мы нашли, просто что-то не переместилсо...
            // вообще этого быть не должно, просто в логах будет 3 попытки и 0 в позиции
            // так же используем обычное расширение, поэтому найдено не пишем
            break;
        }
        //printf("Can't move, retry\n");
    }

    if (FreeBlocksFound == 0) {
        // если блок не найден то просто оставляем как есть
        // увеличиваем средствами ntfs до планированного

        //printf("can't find free block, use default allocator\n");

        // сжимаем вначале файл до оригинального размера
        // иначе ntfs без выравнивания оставит этот висящий кластер и выделит полный блок
        // ниже будет полная аллокация
        if (!AAF_set_file_size(hFile, old_file_size)) {
            status_code = -8;
            goto err_aaf_alloc_block;
        }
        status_code = 2;
    }

    // если последнее чтение было не из кеша битмапа то находим еще 10 новых
    // ну или сколько найдется, энивей ловим только ошибку
    if (IsLastReadNonCached) {
        // printf("Need prefetch\n");
        qp_timer_prefetch_start = qp_time_get();
        if (find_free_block(
                hVolume, StartingLcn_c8, BlockLength_c8, BLOCKS_TO_PREFETCH,
                &CheckedStartingLcn_c8, NULL, NULL,
                &FreeBlocksFound, &IsLastReadNonCached
        )) {
            status_code = -7;
            goto err_aaf_alloc_block;
        }
        qp_timer_prefetch_end = qp_time_get();
    }

    // Теперь увеличиваем размер до планируемого
    if (!AAF_set_file_size(hFile, new_file_size)) {
        status_code = -9;
        goto err_aaf_alloc_block;
    }
    //printf("resize file to: %"PRId64"\n", new_file_size.QuadPart);

    // в случае если не смогли переместить и юзали стандартное выделение
    // интересно будет ли вообще такое
    if (pStats->moveAttempts == MOVE_FILE_MAX_RETRIES) {
        status_code = 3;
        goto err_aaf_alloc_block;
    }

err_aaf_alloc_block:

    if (get_file_size_and_extents(hFile, &old_file_size, &pRPB_shared)) {
        pStats->fileLength = new_file_size.QuadPart;
        pStats->fileFragments = pRPB_shared->ExtentCount;
    } else {
        status_code = -10;
    }

    if (hVolume != INVALID_HANDLE_VALUE) CloseHandle(hVolume);
    if (pStatusCode != NULL) *pStatusCode = status_code;

    qp_timer_end = qp_time_get();
    pStats->allocTime = qp_timer_diff_100ns(qp_timer_start, qp_timer_end);
    pStats->moveTime = qp_timer_diff_100ns(qp_timer_move_start, qp_timer_move_end);
    if (qp_timer_prefetch_end != 0) {
        pStats->prefetchTime = qp_timer_diff_100ns(qp_timer_prefetch_start, qp_timer_prefetch_end);
    }

    // аналог free
    shared_malloc(0, SM_BUFFER_COMMON, SM_DATA_DISCARD);

    return status_code >= 0;
}

int AAF_set_file_size(HANDLE hFile, LARGE_INTEGER new_size) {
    if (!SetFilePointerEx(hFile, new_size, NULL, FILE_BEGIN)) {
        //printf("->set pointer error %d\n", GetLastError());
        return 0;
    }
    if (!SetEndOfFile(hFile)) {
        //printf("->set size error %d\n", GetLastError());
        return 0;
    }
    return 1;
}


