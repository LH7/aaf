#ifndef _FUNC_DYN_H
#define _FUNC_DYN_H

#include <windows.h>

DWORD dyn_GetFinalPathNameByHandleW(HANDLE hFile, wchar_t *lpFilePath, DWORD cchFilePath, DWORD dwFlags);

#endif
