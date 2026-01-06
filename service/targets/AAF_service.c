#include <inttypes.h>
#include <windows.h>
#include <stdio.h>

#include "AAF_block.h"
#include "dup_handle.h"
#include "qp_timer.h"
#include "log_disk.h"
#include "simple_service.h"
#include "arguments_proc.h"
#include "is_admin.h"

static AAF_t AAF;

static void finishRequest()
{
    LogAAFBufferStat(LogFileName, AAF.pBuffer);

    printf("Success: %"PRId64", code %"PRId64", FileLength: %"PRId64", Fragments: %"PRId64"\n", (intmax_t)AAF.pBuffer->result.success,
           AAF.pBuffer->result.statusCode, AAF.pBuffer->stats.fileLength, AAF.pBuffer->stats.fileFragments);
    printf("Offset: %"PRId64", Time: %"PRId64", SearchSkip: %"PRId64", SearchTotal: %"PRId64"\n",
           AAF.pBuffer->stats.allocOffset, AAF.pBuffer->stats.allocTime,
           AAF.pBuffer->stats.searchSkip, AAF.pBuffer->stats.searchTotal);
    printf("MoveAttempts: %"PRId64", MoveTime: %"PRId64", PrefetchTime: %"PRId64"\n",
           AAF.pBuffer->stats.moveAttempts, AAF.pBuffer->stats.moveTime, AAF.pBuffer->stats.prefetchTime);
    // обнуление буфера перед следующим реквестом
    // на случай если какая то из следующих программ не запишет свои данные
    ZeroMemory(&AAF.pBuffer->request, sizeof(AAF_request_t));

    SetEvent(AAF.hEventRequestFinished);
}

static int WMain(int argc, wchar_t **argv)
{
    (void)argc; (void)argv;
    int ret;

    switch (AAFRequestBufferInit(&AAF)) {
        case 0:
            ret = 0; break;
        case 1:
            printf("Already opened, exit\n");
            ret = 2; goto err_WMainLoop;
        default:
            printf("Error create shared buffer, exit\n");
            ret = 3; goto err_WMainLoop;
    }

    LogSimple(LogFileName, "Start");
    LogAAFBufferHeaders(LogFileName);

    if (!isAdmin()) {
        LogSimple(LogFileName, "No admin rights, simple alloc only");
        printf("Warning: no admin rights, simple alloc only\n");
    }

    wprintf(L"Started, shared objects prefix: '%ls'\n", SHARED_OBJECTS_PREFIX);

    while (1)
    {
        printf("Wait for request...\n");

        WaitForSingleObject(AAF.hEventNewRequest, INFINITE);

        // Это событие от службы для выхода, оно так же триггерит hEventNewRequest перед этим
        // нужно толкько для сервиса, в обычном случае никогда не вернет true
        if (SimpleServiceIsStopEventRequested()) break;

        printf(
            "New request. PID: %"PRId64", hFile: %"PRId64", blockSize: %"PRId64", alignSize: %"PRId64"\n",
            AAF.pBuffer->request.PID, AAF.pBuffer->request.hFile,
            AAF.pBuffer->request.blockSize, AAF.pBuffer->request.alignSize
        );

        LONGLONG blockSize = AAF.pBuffer->request.blockSize;
        LONGLONG alignSize = AAF.pBuffer->request.alignSize;

        DWORD remotePID = AAF.pBuffer->request.PID;
        HANDLE remoteHFile = (HANDLE) (uintptr_t) AAF.pBuffer->request.hFile;

        HANDLE localHFile = DuplicateRemoteHandle(remotePID, remoteHFile);

        if (
            blockSize < 8192 ||
            alignSize < 10 * 1024 * 1024 || alignSize > 320LL * 1024 * 1024 * 1024 || // ограничение на 10 мегабайт буфер журнала
            blockSize > alignSize ||
            localHFile == INVALID_HANDLE_VALUE || GetFileType(localHFile) != FILE_TYPE_DISK
        ) {
            if (localHFile != INVALID_HANDLE_VALUE) CloseHandle(localHFile);
            printf("Invalid request parameters\n");
            AAF.pBuffer->result.statusCode = -100;
            finishRequest();
            continue;
        }

        AAF.pBuffer->result.success = AAF_alloc_block(
            localHFile, blockSize, alignSize,
            &AAF.pBuffer->result.statusCode, &AAF.pBuffer->stats
        );

        CloseHandle(localHFile);
        finishRequest();
    }

err_WMainLoop:
    AAFRequestBufferFree(&AAF);
    LogSimple(LogFileName, "Exit");
    return ret;
}

static void StopServiceCb() {
    SetEvent(AAF.hEventNewRequest);
}

int wmain(int argc, wchar_t* argv[])
{
    qp_timer_init();

    int ret = ArgumentsProc(argc, argv);
    if (ret != -1) return ret;

    return SimpleServiceStartServiceCtrlDispatcher(WMain, StopServiceCb, argc, argv);
}

