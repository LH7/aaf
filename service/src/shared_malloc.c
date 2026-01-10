#include <windows.h>
#include "shared_malloc.h"

static void *pMem[_SHARED_MALLOC_BUFFERS] = {NULL};
static size_t current_size[_SHARED_MALLOC_BUFFERS] = {0};
static size_t page_size = 0;

void* shared_malloc(size_t new_size, sm_buffer_idx_t buffer_idx, sm_data_keep_t data_keep)
{
    if (new_size > current_size[buffer_idx] || new_size <= SM_DATA_FREE)
    {
        if (page_size == 0) {
            SYSTEM_INFO sysInfo;
            GetSystemInfo(&sysInfo);
            page_size = sysInfo.dwPageSize;
        }

        void *pMemNew = new_size > SM_DATA_FREE
            ? VirtualAlloc(NULL, new_size,
                            MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) // NOLINT(hicpp-signed-bitwise)
        : NULL;

        if (pMem[buffer_idx] != NULL) {
            if (pMemNew != NULL && data_keep == SM_DATA_KEEP) {
                // может потом перепишу на предварительное резервирование, пока копирование
                memcpy(pMemNew, pMem[buffer_idx], current_size[buffer_idx]);
            }
            VirtualFree(pMem[buffer_idx], 0, MEM_RELEASE);
        }

        pMem[buffer_idx] = pMemNew;

        current_size[buffer_idx] = pMem[buffer_idx] != NULL
            ? ((new_size / page_size) + 1) * page_size
            : 0;
    }
    return pMem[buffer_idx];
}