#ifndef _ARG_PROC_H
#define _ARG_PROC_H

#include "simple_service.h"

#define AAF_SERVICE_NAME          L"AAF"
#define AAF_SERVICE_DISPLAY_NAME  L"Alloc aligned file service for streamlink"
#define AAF_SERVICE_DESCRIPTION   L"Сервис для Streamlink для аллоцирования файлов по выровненным большим сегментам"

int ArgumentsProc(int argc, wchar_t **argv);
extern wchar_t *LogFileName;

#endif
