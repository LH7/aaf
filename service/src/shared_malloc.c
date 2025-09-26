#include <windows.h>
#include <stdlib.h>
#include "shared_malloc.h"

static void *pMem = NULL;
static size_t current_size = 0;
static size_t page_size = 0;

void* shared_malloc(size_t new_size)
{
    if (new_size > current_size || new_size <= 0)
    {
        if (pMem != NULL) free(pMem);
        pMem = new_size > 0 ? malloc(new_size) : NULL;
        current_size = pMem == NULL ? 0 : new_size;
    }
    return pMem;
}

void* shared_VirtualAlloc(const size_t new_size)
{
    if (new_size > current_size || new_size <= 0)
    {
        if (page_size == 0) {
            SYSTEM_INFO sysInfo;
            GetSystemInfo(&sysInfo);
            page_size = sysInfo.dwPageSize;
        }
        if (pMem != NULL) {
            VirtualFree(pMem, 0, MEM_RELEASE);
        }
        pMem = new_size > 0
            ? VirtualAlloc(NULL, new_size,
                           MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) // NOLINT(hicpp-signed-bitwise)
            : NULL;
        current_size = pMem != NULL
            ? ((new_size / page_size) + 1) * page_size
            : 0;
    }
    return pMem;
}