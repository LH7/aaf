#include <stdio.h>
#include "simple_service.h"

static wchar_t *CurrentServiceName = NULL;
static wchar_t *CurrentServiceDisplayName = NULL;
static wchar_t *CurrentServiceDescription = NULL;

static SERVICE_STATUS CurrentServiceStatus;
static SERVICE_STATUS_HANDLE CurrentServiceStatusHandle;
static HANDLE CurrentServiceStopEvent = NULL;

static FUNC_PTR_WMAIN CurrentServiceMainLoop = NULL;
static FUNC_PRR_STOP_SERVICE_CB CurrentServiceStopServiceCb = NULL;

static void _disableStdio()
{
    freopen("NUL", "r", stdin);
    freopen("NUL", "w", stdout);
    freopen("NUL", "w", stderr);
}

int SimpleServiceIsService()
{
    DWORD sessionId;
    if (ProcessIdToSessionId(GetCurrentProcessId(), &sessionId)) {
        return sessionId == 0;
    }
    return 0;
}

static int _openScmAndServiceW(SC_HANDLE *pScm, SC_HANDLE *pService, DWORD dwDesiredAccess)
{
    *pScm = NULL; *pService = NULL;

    *pScm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);  // NOLINT(hicpp-signed-bitwise)
    if (*pScm == NULL) {
        return 0;
    }

    *pService = OpenServiceW(*pScm, CurrentServiceName, dwDesiredAccess);
    if (*pService == NULL) {
        CloseServiceHandle(*pScm);
        *pScm = NULL;
        return 0;
    }

    return 1;
}

static void _closeScmAndServiceW(SC_HANDLE scm, SC_HANDLE service)
{
    if (service != NULL) CloseServiceHandle(service);
    if (scm != NULL) CloseServiceHandle(scm);
}

int SimpleServiceIsStopEventRequested()
{
    if (CurrentServiceStopEvent == NULL) return 0;
    return WaitForSingleObject(CurrentServiceStopEvent, 0) == WAIT_OBJECT_0;
}

int SimpleServiceCmdCreate(wchar_t *pSrvCmdLine)
{
    int ret = 1;

    SERVICE_DESCRIPTIONW sd;
    sd.lpDescription = CurrentServiceDescription;
    SC_HANDLE scm = NULL, service = NULL;

    scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS); // NOLINT(hicpp-signed-bitwise)
    if (scm == NULL) {
        goto err_SimpleServiceCreate;
    }

    service = CreateServiceW(
        scm, CurrentServiceName, CurrentServiceDisplayName,
        SERVICE_ALL_ACCESS, // NOLINT(hicpp-signed-bitwise)
        SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
        pSrvCmdLine,
        NULL, NULL, NULL, NULL, NULL
    );
    if (service == NULL) {
        goto err_SimpleServiceCreate;
    }

    if (!ChangeServiceConfig2W(service,  SERVICE_CONFIG_DESCRIPTION, &sd)){
        goto err_SimpleServiceCreate;
    }

    ret = 0;

err_SimpleServiceCreate:
    _closeScmAndServiceW(scm, service);
    return ret;
}

int SimpleServiceCmdDelete()
{
    int ret = 1;

    SC_HANDLE scm, service;
    if (!_openScmAndServiceW(&scm, &service, DELETE)) {
        goto err_SimpleServiceDelete;
    }

    if (!DeleteService(service)) {
        goto err_SimpleServiceDelete;
    }

    ret = 0;

err_SimpleServiceDelete:
    _closeScmAndServiceW(scm, service);
    return ret;
}

int SimpleServiceCmdStart()
{
    int ret = 1;

    SC_HANDLE scm, service;
    if (!_openScmAndServiceW(&scm, &service, SERVICE_START)) {
        goto err_SimpleServiceStart;
    }

    if (!StartService(service, 0, NULL)) {
        goto err_SimpleServiceStart;
    }

    ret = 0;

err_SimpleServiceStart:
    _closeScmAndServiceW(scm, service);
    return ret;
}

int SimpleServiceCmdStop()
{
    int ret = 1;
    SC_HANDLE scm, service;

    if (!_openScmAndServiceW(&scm, &service, SERVICE_STOP)) {
        goto err_SimpleServiceStop;
    }

    SERVICE_STATUS status;
    if (!ControlService(service, SERVICE_CONTROL_STOP, &status)) {
        goto err_SimpleServiceStop;
    }

    ret = 0;

err_SimpleServiceStop:
    _closeScmAndServiceW(scm, service);
    return ret;
}

int SimpleServiceCmdStatus(int *pStarted, int *pInstalled)
{
    int ret = -1;
    *pStarted = 0;
    *pInstalled = 0;

    SC_HANDLE scm = NULL, service = NULL;

    scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);  // NOLINT(hicpp-signed-bitwise)
    if (scm == NULL) {
        goto err_SimpleServiceStatus;
    }

    service = OpenServiceW(scm, CurrentServiceName, SERVICE_QUERY_STATUS);
    if (service == NULL) {
        if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST) {
            ret = 0;
        }
        goto err_SimpleServiceStatus;
    }

    SERVICE_STATUS_PROCESS srvStatus;
    DWORD bytesNeeded;
    if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, (LPBYTE)&srvStatus, sizeof(srvStatus), &bytesNeeded)) {
        goto err_SimpleServiceStatus;
    }

    *pInstalled = 1;

    switch (srvStatus.dwCurrentState)
    {
        case SERVICE_STOPPED:
            ret = 1;
            break;
        case SERVICE_START_PENDING:
        case SERVICE_STOP_PENDING:
        case SERVICE_RUNNING:
        case SERVICE_CONTINUE_PENDING:
        case SERVICE_PAUSE_PENDING:
        case SERVICE_PAUSED:
            *pStarted = 1;
            ret = 2;
            break;

        default:
            ret = -1;
            break;
    }

err_SimpleServiceStatus:
    _closeScmAndServiceW(scm, service);
    return ret;
}

VOID WINAPI SimpleServiceCtrlHandler(DWORD dwControl)
{
    switch (dwControl)
    {
        case SERVICE_CONTROL_STOP:
            CurrentServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
            SetServiceStatus(CurrentServiceStatusHandle, &CurrentServiceStatus);
            SetEvent(CurrentServiceStopEvent);
            if (CurrentServiceStopServiceCb != NULL) CurrentServiceStopServiceCb();
            return;

        case SERVICE_CONTROL_INTERROGATE:
            break;

        default:
            break;
    }

    SetServiceStatus(CurrentServiceStatusHandle, &CurrentServiceStatus);
}

VOID WINAPI SimpleServiceWMain(DWORD argc, LPTSTR *argv)
{
    (void)argc; (void)argv;

    CurrentServiceStatusHandle = RegisterServiceCtrlHandlerW(CurrentServiceName, SimpleServiceCtrlHandler);
    if (!CurrentServiceStatusHandle)
        return;

    ZeroMemory(&CurrentServiceStatus, sizeof(CurrentServiceStatus));
    CurrentServiceStatus.dwServiceType      = SERVICE_WIN32_OWN_PROCESS;
    CurrentServiceStatus.dwCurrentState     = SERVICE_RUNNING;
    CurrentServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    SetServiceStatus(CurrentServiceStatusHandle, &CurrentServiceStatus);

    //CurrentServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    CurrentServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(CurrentServiceStatusHandle, &CurrentServiceStatus);

    CurrentServiceStatus.dwWin32ExitCode = CurrentServiceMainLoop(argc, argv);
    CurrentServiceStatus.dwCurrentState = SERVICE_STOPPED;

    // CloseHandle(CurrentServiceStopEvent); его закроет основной поток
    SetServiceStatus(CurrentServiceStatusHandle, &CurrentServiceStatus);
}

void SimpleServiceSetName(wchar_t *pSvcName, wchar_t *pSvcDisplayName, wchar_t *pSvcDescription)
{
    CurrentServiceName = pSvcName;
    CurrentServiceDisplayName = pSvcDisplayName;
    CurrentServiceDescription = pSvcDescription;
}

int SimpleServiceStartServiceCtrlDispatcher(
    FUNC_PTR_WMAIN pWMain, FUNC_PRR_STOP_SERVICE_CB pStopServiceCb, int argc, wchar_t *argv[]
)
{
    int ret = 0;

    if (SimpleServiceIsService())
    {
        _disableStdio();
        CurrentServiceMainLoop = pWMain;
        CurrentServiceStopServiceCb = pStopServiceCb;

        SERVICE_TABLE_ENTRY ServiceTable[] =
                {
                        { CurrentServiceName, SimpleServiceWMain },
                        { NULL, NULL }
                };

        CurrentServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!StartServiceCtrlDispatcher(ServiceTable)) {
            ret = -1;
            goto err_SimpleServiceStartServiceCtrlDispatcher;
        }

        ret = 0;
        goto err_SimpleServiceStartServiceCtrlDispatcher;
    }

    ret = pWMain(argc, argv);

err_SimpleServiceStartServiceCtrlDispatcher:
    if (CurrentServiceStopEvent) CloseHandle(CurrentServiceStopEvent);
    return ret;
}
