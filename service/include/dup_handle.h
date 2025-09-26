#ifndef _DUP_HANDLE_H
#define _DUP_HANDLE_H

#include <windows.h>

HANDLE DuplicateRemoteHandle(DWORD remotePID, HANDLE remoteHandle);

#endif
