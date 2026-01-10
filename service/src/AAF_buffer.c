#include "AAF_buffer.h"
#include <sddl.h>

static inline int FillSecurityAttributes(SECURITY_ATTRIBUTES *pSA)
{
    PSECURITY_DESCRIPTOR pSD;
    
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
        STRING_LOW_SECURITY_DESCRIPTOR_RW,
        SDDL_REVISION_1, &pSD, NULL
    )) {
        return 0;
    }

    pSA->nLength              = sizeof(SECURITY_ATTRIBUTES);
    pSA->lpSecurityDescriptor = pSD;
    pSA->bInheritHandle       = FALSE;

    return 1;
}

int AAFRequestBufferInit(AAF_t *pAAF)
{
    int ret = 0;
    
    SECURITY_ATTRIBUTES sa = {};
    ZeroMemory((void*)pAAF, sizeof(AAF_t));

    // нужно для того чтобы обычные процессы могли читать и писать
    if (!FillSecurityAttributes(&sa)) {
        ret = -1;
        goto err_AAFRequestBufferInit;
    }
    
    // мьютекс для воркеров
    pAAF->hMutexWorkers = CreateMutexW(&sa, FALSE, MUTEX_WORKERS_NAME);
    if (pAAF->hMutexWorkers == NULL) {
        ret = -2; goto err_AAFRequestBufferInit;
    }
    // тут проверка на еще один запущенный процесс аллокатор
    // дальше таких проверок нет потому что бессмысленно
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        ret = 1;  goto err_AAFRequestBufferInit;
    }
    
    // событие что есть новый реквест
    pAAF->hEventNewRequest = CreateEventW(&sa, FALSE, FALSE, EVENT_NEW_REQUEST_NAME);
    if (pAAF->hEventNewRequest == NULL) {
        ret = -3; goto err_AAFRequestBufferInit;
    }

    // событие что реквест завершен
    pAAF->hEventRequestFinished = CreateEventW(&sa, FALSE, FALSE, EVENT_REQUEST_FINISHED_NAME);
    if (pAAF->hEventRequestFinished == NULL) {
        ret = -4; goto err_AAFRequestBufferInit;
    }

    // общий буфер реквеста
    pAAF->hMap = CreateFileMappingW(
        INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0,
        sizeof(AAF_buffer_t), SHARED_MEMORY_BUFFER_NAME
    );
    if (pAAF->hMap == NULL) {
        ret = -5; goto err_AAFRequestBufferInit;
    }
    
    // сам буфер
    pAAF->pBuffer = (AAF_buffer_t*)MapViewOfFile(
        pAAF->hMap,
        FILE_MAP_ALL_ACCESS,  // NOLINT(hicpp-signed-bitwise)
        0, 0, sizeof(AAF_buffer_t)
    );
    if (pAAF->pBuffer == NULL) {
        ret = -6; goto err_AAFRequestBufferInit;
    }

err_AAFRequestBufferInit:
    if (sa.lpSecurityDescriptor != INVALID_HANDLE_VALUE) LocalFree(sa.lpSecurityDescriptor);
    if (ret != 0) AAFRequestBufferFree(pAAF);
    return ret;
}

void AAFRequestBufferFree(AAF_t *pAAF)
{
    if (pAAF->pBuffer != NULL) UnmapViewOfFile(pAAF->pBuffer);
    if (pAAF->hMap != NULL) CloseHandle(pAAF->hMap);
    if (pAAF->hMutexWorkers != NULL) CloseHandle(pAAF->hMutexWorkers);
    if (pAAF->hEventNewRequest != NULL) CloseHandle(pAAF->hEventNewRequest);
    if (pAAF->hEventRequestFinished != NULL) CloseHandle(pAAF->hEventRequestFinished);
    
    ZeroMemory((void*)pAAF, sizeof(AAF_t));
}













