#include "arguments_proc.h"
#include <stdio.h>

wchar_t *LogFileName = NULL;

static int makeServiceCmdLine(wchar_t *logPath, wchar_t *pSvcProcessCommandLine)
{
    wchar_t currentProcessPath[MAX_PATH];
    if (!GetModuleFileNameW(NULL, currentProcessPath, MAX_PATH)) {
        return 1;
    }
    if (logPath != NULL) {
        swprintf(pSvcProcessCommandLine, MAX_PATH, L"\"%s\" %s \"%s\"", currentProcessPath, L"log", logPath);
    } else {
        swprintf(pSvcProcessCommandLine, MAX_PATH, L"\"%s\"", currentProcessPath);
    }
    return 0;
}

static int PrintHelp(int argc, wchar_t* argv[])
{
    if (argc <= 1) return 0;
    if (wcscmp(argv[1], L"help") != 0) return 0;

    printf("Usage:\n");
    printf(" serviceInstall [log file_name] - Install service\n");
    printf(" serviceUninstall               - Uninstall service\n");
    printf(" serviceStart [log file_name]   - Start service\n");
    printf(" serviceStop                    - Stop service\n");
    printf(" help                           - Show this help\n");

    return 1;
}

int ArgumentsProc(int argc, wchar_t **argv)
{
    SimpleServiceSetName(AAF_SERVICE_NAME, AAF_SERVICE_DISPLAY_NAME, AAF_SERVICE_DESCRIPTION);

    if (argc > 1) {
        if (PrintHelp(argc, argv)) return 0;

        int svcStarted, svcInstalled;
        int svcStatus = SimpleServiceCmdStatus(&svcStarted, &svcInstalled);
        if (svcStatus == -1) return 1;

        int toInstall = wcscmp(argv[1], L"serviceInstall") == 0;
        int toUninstall = wcscmp(argv[1], L"serviceUninstall") == 0;
        int toStart = wcscmp(argv[1], L"serviceStart") == 0;
        int toStop = wcscmp(argv[1], L"serviceStop") == 0;

        int logArg1 = argc > 2 && wcscmp(argv[1], L"log") == 0;
        int logArg2 = argc > 3 && wcscmp(argv[2], L"log") == 0;

        if (logArg1) LogFileName = argv[2];
        if (logArg2) LogFileName = argv[3];

        if ((toInstall || toStart) && !svcInstalled) {
            wchar_t SvcProcessCommandLine[MAX_PATH] = {};
            makeServiceCmdLine(LogFileName, SvcProcessCommandLine);
            if (SimpleServiceCmdCreate(SvcProcessCommandLine)) {
                printf("Install service failed\n");
                return 1;
            }
            printf("Install service success\n");
        }
        if ((toUninstall || toStop) && svcStarted) {
            if (SimpleServiceCmdStop()) {
                printf("Stop service failed\n");
                return 1;
            }
            printf("Stop service success\n");
        }
        if (toUninstall && svcInstalled) {
            if (SimpleServiceCmdDelete()) {
                printf("Uninstall service failed\n");
                return 1;
            }
            printf("Uninstall service success\n");
        }
        if (toStart && !svcStarted) {
            if (SimpleServiceCmdStart()) {
                printf("Start service failed\n");
                return 1;
            }
            printf("Start service success\n");
        }

        if ((toStart && svcStarted) || (toStop && !svcStarted) || (toInstall && svcInstalled) || (toUninstall && !svcInstalled)) {
            printf("Already\n");
        }

        if (toInstall || toUninstall || toStart || toStop) return 0;
    }

    return -1;
}