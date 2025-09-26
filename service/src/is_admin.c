#include <windows.h>
#include "is_admin.h"

static int __is_admin_cached = -1; // неопределено до первого вызова

int isAdmin()
{
    if (__is_admin_cached != -1) {
        return __is_admin_cached;
    }

    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        return 0;
    }

    DWORD dwSize;
    char buffer[64];

    if (!GetTokenInformation(hToken, TokenIntegrityLevel, buffer, 64, &dwSize)) {
        CloseHandle(hToken);
        return 0;
    }

    DWORD integrityLevel = *GetSidSubAuthority(
        ((PTOKEN_MANDATORY_LABEL)buffer)->Label.Sid,
        (DWORD)(UCHAR)(*GetSidSubAuthorityCount(((PTOKEN_MANDATORY_LABEL)buffer)->Label.Sid) - 1)
    );

    CloseHandle(hToken);

    __is_admin_cached = integrityLevel >= SECURITY_MANDATORY_HIGH_RID;
    return __is_admin_cached;
}


