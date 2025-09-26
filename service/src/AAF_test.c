#include <stdio.h>
#include "AAF_block.h"
#include "qp_timer.h"

int wmain() {
    HANDLE hFile = CreateFileW(
            L"r:\\test.bin",
            GENERIC_READ | GENERIC_WRITE, 0, // NOLINT(hicpp-signed-bitwise)
            NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
    );
    if (hFile == INVALID_HANDLE_VALUE) {
        printf("Error open file\n");
        return 0;
    }

    qp_timer_init();

    LONGLONG block_size = 100*1024*1024ULL;
//    LONGLONG block_size = 16281ULL*1024*1024*1024;
    LONGLONG align_size = 10*1024*1024*1024ULL;
//    LONGLONG align_size = 1024*1024ULL;

    AAF_stats_t AAF_Stat = {};
    AAF_result_t AAF_Result = {};
    int iterations = 1;
    LONGLONG total_time = 0;
    LARGE_INTEGER zero_size = {};
    for (int i = 0; i < iterations; i++)
    {
        AAF_set_file_size(hFile, zero_size);
        AAF_Result.success = AAF_alloc_block(
                hFile, block_size, align_size, &AAF_Result.statusCode, &AAF_Stat
        );

        printf("Success: %I64d, code %I64d, FileLength: %I64d, Fragments: %I64d\n", AAF_Result.success, AAF_Result.statusCode, AAF_Stat.fileLength, AAF_Stat.fileFragments);
        printf("Offset: %I64d, Time: %.4f, SearchSkip: %I64d, SearchTotal: %I64d, MoveAttempts: %I64d\n", AAF_Stat.allocOffset, (double)AAF_Stat.allocTime / 10000, AAF_Stat.searchSkip, AAF_Stat.searchTotal, AAF_Stat.moveAttempts);
        total_time += AAF_Stat.allocTime;

    }
    printf("TimeAVG: %.4f\n", (double)total_time / iterations / 10000);

    CloseHandle(hFile);
    return 0;
}

