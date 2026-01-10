#include "AAF_block.h"
#include "is_admin.h"
#include "qp_timer.h"

#define EXPORT __declspec(dllexport)

EXPORT WINAPI int AAF_block(HANDLE hFile, LONGLONG blockSize, LONGLONG alignSize, LONGLONG *pStatusCode, AAF_stats_t *pStats)
{
	return AAF_alloc_block(hFile, blockSize, alignSize, pStatusCode, pStats);
}

EXPORT WINAPI int is_admin()
{
	return isAdmin();
}

EXPORT WINAPI BOOL DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH) {
        qp_timer_init();
    }
    return TRUE;
}
