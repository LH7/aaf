#ifndef _LOG_DISK_H
#define _LOG_DISK_H

#include <inttypes.h>
#include "AAF_buffer.h"

void LogSimple(const wchar_t *filename, const char *text);
void LogAAFBufferStat(const wchar_t *filename, const AAF_buffer_t *AAFb);
void LogAAFBufferHeaders(const wchar_t *filename);

#endif
