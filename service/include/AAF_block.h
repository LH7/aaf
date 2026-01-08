#ifndef _ALIGNED_ALLOC_FILE_H
#define _ALIGNED_ALLOC_FILE_H

#include <inttypes.h>
#include <windows.h>
#include "AAF_buffer.h"

   int AAF_alloc_block(HANDLE hFile, LONGLONG blockSize, LONGLONG alignSize, LONGLONG *pStatusCode, AAF_stats_t *pStats);

#endif
