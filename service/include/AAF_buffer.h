#ifndef _AAF_REQUEST_BUFFER_H
#define _AAF_REQUEST_BUFFER_H

#include <windows.h>
#include <stdint.h>

#define STRING_LOW_SECURITY_DESCRIPTOR_RW L"D:(A;;GA;;;AU)"

#define LSTR(x) L##x
#define MAKE_SHARE_NAMESPACE(name) L"Global\\AAFv1_" LSTR(#name)

#define SHARED_OBJECTS_PREFIX        MAKE_SHARE_NAMESPACE()
#define SHARED_MEMORY_BUFFER_NAME    MAKE_SHARE_NAMESPACE(Buf)
#define MUTEX_WORKERS_NAME           MAKE_SHARE_NAMESPACE(Mtx_Workers)
#define EVENT_NEW_REQUEST_NAME       MAKE_SHARE_NAMESPACE(Evt_NewRequest)
#define EVENT_REQUEST_FINISHED_NAME  MAKE_SHARE_NAMESPACE(Evt_RequestFinished)

typedef struct {
    int64_t  PID;
    int64_t  hFile;
    int64_t  blockSize;
    int64_t  alignSize;
} AAF_request_t;

typedef struct {
    int64_t  success;
    int64_t  statusCode;
} AAF_result_t;

typedef struct {
    int64_t  fileLength;
    int64_t  fileFragments;
    int64_t  allocOffset;
    int64_t  allocTime;
    int64_t  searchSkip;
    int64_t  searchTotal;
    int64_t  moveAttempts;
    int64_t  moveTime;
} AAF_stats_t;

typedef struct {
    AAF_request_t  request;
    AAF_result_t   result;
    AAF_stats_t    stats;
} AAF_buffer_t;

typedef struct {
    HANDLE         hMap;
    HANDLE         hMutexWorkers;
    HANDLE         hEventNewRequest;
    HANDLE         hEventRequestFinished;
    AAF_buffer_t  *pBuffer;
} AAF_t;

int AAFRequestBufferInit(AAF_t *pAAF);
void AAFRequestBufferFree(AAF_t *pAAF);

#endif
