#ifndef _SIMPLE_SERVICE_H
#define _SIMPLE_SERVICE_H

#include <windows.h>

typedef int (*FUNC_PTR_WMAIN)(int, wchar_t **);
typedef void (*FUNC_PRR_STOP_SERVICE_CB)(void);

int SimpleServiceCmdCreate(wchar_t *pSrvCmdLine);
int SimpleServiceCmdDelete();
int SimpleServiceCmdStart();
int SimpleServiceCmdStop();
int SimpleServiceCmdStatus(int *pStarted, int *pInstalled);
void SimpleServiceSetName(wchar_t *pSvcName, wchar_t *pSvcDisplayName, wchar_t *pSvcDescription);
int SimpleServiceIsStopEventRequested();
int SimpleServiceIsService();

int SimpleServiceStartServiceCtrlDispatcher(
    FUNC_PTR_WMAIN pWMain, FUNC_PRR_STOP_SERVICE_CB pStopServiceCb, int argc, wchar_t *argv[]
);

#endif
