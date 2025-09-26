#include "dup_handle.h"

HANDLE DuplicateRemoteHandle(DWORD remotePID, HANDLE remoteHandle)
{
    HANDLE hSourceProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, remotePID);
    if (hSourceProcess == NULL) return INVALID_HANDLE_VALUE;

    HANDLE duplicatedHandle = INVALID_HANDLE_VALUE;

    BOOL success = DuplicateHandle(
        hSourceProcess, remoteHandle, GetCurrentProcess(),
        &duplicatedHandle, 0, FALSE, DUPLICATE_SAME_ACCESS
    );
    
    CloseHandle(hSourceProcess);

    return success
        ? duplicatedHandle
        : INVALID_HANDLE_VALUE;
}

