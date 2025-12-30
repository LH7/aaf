#include <windows.h>
#include "shared_malloc.h"

static void *pMem[_SHARED_MALLOC_BUFFERS] = {NULL};
static size_t current_size[_SHARED_MALLOC_BUFFERS] = {0};
static size_t page_size = 0;

void* shared_malloc(size_t new_size, sm_buffer_idx_t buffer_idx)
{
    if (new_size > current_size[buffer_idx] || new_size <= 0)
    {
        if (page_size == 0) {
            SYSTEM_INFO sysInfo;
            GetSystemInfo(&sysInfo);
            page_size = sysInfo.dwPageSize;
        }
        if (pMem[buffer_idx] != NULL) {
            VirtualFree(pMem[buffer_idx], 0, MEM_RELEASE);
        }
        pMem[buffer_idx] = new_size > 0
            ? VirtualAlloc(NULL, new_size,
                           MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) // NOLINT(hicpp-signed-bitwise)
            : NULL;
        current_size[buffer_idx] = pMem[buffer_idx] != NULL
            ? ((new_size / page_size) + 1) * page_size
            : 0;
    }
    return pMem[buffer_idx];
}