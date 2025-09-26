#include "func_dyn.h"

typedef DWORD(WINAPI* GetFinalPathNameByHandleW_t)(
    HANDLE hFile,
    LPWSTR lpFilePath,
    DWORD  cchFilePath,
    DWORD  dwFlags
);

static GetFinalPathNameByHandleW_t pGetFinalPathNameByHandleW = NULL;

DWORD dyn_GetFinalPathNameByHandleW(HANDLE hFile, wchar_t *lpFilePath, DWORD cchFilePath, DWORD dwFlags)
{
    if (pGetFinalPathNameByHandleW == NULL) {
        HMODULE hKernel32 = LoadLibraryA("kernel32.dll");
        if (hKernel32 == NULL) return 0;
        pGetFinalPathNameByHandleW = (GetFinalPathNameByHandleW_t)GetProcAddress(hKernel32, "GetFinalPathNameByHandleW");
        if (pGetFinalPathNameByHandleW == NULL) return 0;
    }
    return pGetFinalPathNameByHandleW(hFile, lpFilePath, cchFilePath, dwFlags);
}

