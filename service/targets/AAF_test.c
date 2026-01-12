#include <stdio.h>
#include "AAF_block.h"
#include "qp_timer.h"
#include "shared_malloc.h"
//#include "zeros_array.h"
#include <inttypes.h>

#define VOLUME_BITMAP_HEADER_SIZE 16

static HANDLE AAF_get_volume_handle_by_file_handle(HANDLE hFile)
{
    wchar_t filePath[MAX_PATH];

    DWORD res = GetFinalPathNameByHandleW(hFile, filePath, MAX_PATH, VOLUME_NAME_GUID);
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

static int AAF_check_free_block_in_bitmap(
        HANDLE hVolume,
        LONGLONG StartingLcn_c8, LONGLONG BlockLength_c8,
        LONGLONG *pCheckedStartingLcn_c8, LONGLONG *pCheckedBlockLength_c8,
        LONGLONG *pBitmapSize_c8, int *pIsFreeBlock
)
{
    int ret = 0;

    size_t bufferSize;
    DWORD bytesReturned;

    LARGE_INTEGER         StartingLcn;
    VOLUME_BITMAP_BUFFER *pvb_buffer;

    StartingLcn.QuadPart = StartingLcn_c8 * 8;
    bufferSize = VOLUME_BITMAP_HEADER_SIZE + BlockLength_c8;

    // в данном случае буфер нужен только тут  поэтому используем грязный буфер без реаллокации
    pvb_buffer = (VOLUME_BITMAP_BUFFER*)shared_malloc(bufferSize, SM_BUFFER_COMMON, SM_DATA_DISCARD) ;

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
    //*pIsFreeBlock = is_all_zeros_array(pvb_buffer->Buffer, *pCheckedBlockLength_c8);

    err_aaf_check_free_block_in_bitmap:
    //if (pvb_buffer != NULL) free(pvb_buffer);
    return ret;
}

//void test_end_find(HANDLE hVolume)
//{
//    LONGLONG StartingLcn_c8;
//    LONGLONG BlockLength_c8;
//    size_t BlocksToFind;
//    LONGLONG CheckedStartingLcn_c8 = 0;
//    LONGLONG searchSkip = 0;
//    LONGLONG searchTotal = 0;
//    size_t FreeBlocksFound = 0;
//    int IsLastReadNonCached = 0;
//    int IsEndOfVolume = 0;
//
//    int ret;
//
//    StartingLcn_c8 = 0;
//    BlockLength_c8 = 5ULL*1024*1024*1024 / 4096 / 8;
//    BlocksToFind = 1000;
//    ret = find_free_block(
//            hVolume, StartingLcn_c8, BlockLength_c8, BlocksToFind,
//            &CheckedStartingLcn_c8, &searchSkip, &searchTotal,
//            &FreeBlocksFound, &IsLastReadNonCached, &IsEndOfVolume
//    );
//    printf("\nret: %d, StartingLcn_c8: %"PRId64", BlockLength_c8: %"PRId64", BlocksToFind: %"PRId64"\nCheckedStartingLcn_c8: %"PRId64", searchSkip: %"PRId64", searchTotal: %"PRId64",\nFreeBlocksFound: %d, IsLastReadNonCached: %d, IsEndOfVolume: %d\n",
//           ret, StartingLcn_c8, BlockLength_c8, BlocksToFind, CheckedStartingLcn_c8, searchSkip, searchTotal, (int)FreeBlocksFound, IsLastReadNonCached, IsEndOfVolume);
//
//}

void bench_bitmap_scan(HANDLE hVolume)
{
    LONGLONG StartingLcn_c8, BlockLength_c8;
    LONGLONG CheckedStartingLcn_c8, CheckedBlockLength_c8;
    LONGLONG BitmapSize_c8;
    int IsFreeBlock;
    int ret;
    size_t i;

    //void *dummy_buf = shared_VirtualAlloc(100*1024*1024); // чтобы не инициализировать каждый раз

    LONGLONG buffers_size[] = {
            4096, 4096*2, 4096*3, 4096*4, 4096*5, 4096*6, 4096*7,
            32768, 32768*2, 32768*3, 32768*4, 32768*5,
            327680,
            3276800, 32768 * 4096
    };

    size_t total_calls;

    printf("== skip test ==\n");
    for (i = 0; i < sizeof(buffers_size) / sizeof(buffers_size[0]); i++){
        BlockLength_c8 = buffers_size[i];
        StartingLcn_c8 = 0;
        total_calls = 0;
        LONGLONG init_time = qp_time_get();
        while (1)
        {
            total_calls++;
            ret = AAF_check_free_block_in_bitmap(
                    hVolume,
                    StartingLcn_c8, 8,
                    &CheckedStartingLcn_c8, &CheckedBlockLength_c8,
                    &BitmapSize_c8, &IsFreeBlock
            );
            //printf("StartingLcn_c8: %"PRId64", CheckedBlockLength_c8: %"PRId64"\n", StartingLcn_c8, CheckedBlockLength_c8);
            if (ret != 0) {
                printf("Error check block: %d\n", (int)ret);
                break;
            }

            // в данном случае скипается весь блок хоть запрашивали и 8 кластеров
            StartingLcn_c8 = CheckedStartingLcn_c8 + BlockLength_c8;
            if (BitmapSize_c8 <= BlockLength_c8) break;

            //StartingLcn_c8 = CheckedStartingLcn_c8 + CheckedBlockLength_c8;
            //if (BitmapSize_c8 == CheckedBlockLength_c8) break;
        }
        LONGLONG end_time = qp_time_get();

        LONGLONG time_to_proces = qp_timer_diff_100ns(init_time, end_time);
        printf("Calls: %d, Buffer_size: %"PRId64", Total time: %f\n", (int)total_calls, BlockLength_c8, (double)time_to_proces / 10000);
    }

    printf("\n== normal test ==\n");
    for (i = 0; i < sizeof(buffers_size) / sizeof(buffers_size[0]); i++){
        BlockLength_c8 = buffers_size[i];
        StartingLcn_c8 = 0;
        total_calls = 0;
        LONGLONG init_time = qp_time_get();
        while (1)
        {
            total_calls++;
            ret = AAF_check_free_block_in_bitmap(
                    hVolume,
                    StartingLcn_c8, BlockLength_c8,
                    &CheckedStartingLcn_c8, &CheckedBlockLength_c8,
                    &BitmapSize_c8, &IsFreeBlock
            );
            //printf("StartingLcn_c8: %"PRId64", CheckedBlockLength_c8: %"PRId64"\n", StartingLcn_c8, CheckedBlockLength_c8);
            if (ret != 0) {
                printf("Error check block: %d\n", (int)ret);
                break;
            }

            StartingLcn_c8 = CheckedStartingLcn_c8 + CheckedBlockLength_c8;
            if (BitmapSize_c8 == CheckedBlockLength_c8) break;
        }
        LONGLONG end_time = qp_time_get();

        LONGLONG time_to_proces = qp_timer_diff_100ns(init_time, end_time);
        printf("Calls: %d, Buffer_size: %"PRId64", Total time: %f\n", (int)total_calls, BlockLength_c8, (double)time_to_proces / 10000);
    }
}

void alloc_test(HANDLE hFile)
{
    int ret;
    AAF_stats_t Stats;
    LONGLONG blockSize, alignSize, StatusCode;

    blockSize = 2ULL*1024*1024*1024;
    alignSize = 3ULL*1024*1024*1024;

    ret = AAF_alloc_block(hFile, blockSize, alignSize, &StatusCode, &Stats);

    printf("Success: %"PRId64", code %"PRId64", FileLength: %"PRId64", Fragments: %"PRId64"\n",
           (intmax_t)ret, StatusCode, Stats.fileLength, Stats.fileFragments);
    printf("Offset: %"PRId64", Time: %"PRId64", SearchSkip: %"PRId64", SearchTotal: %"PRId64"\n",
           Stats.allocOffset, Stats.allocTime,
           Stats.searchSkip, Stats.searchTotal);
    printf("MoveAttempts: %"PRId64", MoveTime: %"PRId64", PrefetchTime: %"PRId64"\n",
           Stats.moveAttempts, Stats.moveTime, Stats.prefetchTime);

}

int wmain() {

    qp_timer_init();

    HANDLE hFile = CreateFileW(
            L"t:\\test.bin",
            GENERIC_READ | GENERIC_WRITE, 0, // NOLINT(hicpp-signed-bitwise)
            NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
    );
    if (hFile == INVALID_HANDLE_VALUE) {
        printf("Error open file\n");
        return 0;
    }

    HANDLE hVolume = AAF_get_volume_handle_by_file_handle(hFile);
    if (hVolume == INVALID_HANDLE_VALUE) {
        printf("Error open volume\n");
        return 0;
    }

    printf("hFile: %p, hVolume: %p\n", hFile, hVolume);

//    test_end_find(hVolume);
//    return 0;

//    bench_bitmap_scan(hVolume);
//    return 0;

//    LARGE_INTEGER new_size;
//    new_size.QuadPart = 15ULL*1024*1024*1024*1024;
//    AAF_set_file_size(hFile, new_size);

    alloc_test(hFile);

    //dummy_buf = shared_VirtualAlloc(0);

    CloseHandle(hVolume);
    CloseHandle(hFile);

    return 0;
}

