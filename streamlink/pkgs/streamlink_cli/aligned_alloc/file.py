import msvcrt
import os

from streamlink_cli.aligned_alloc.ctype_def import \
    OpenMutexW, OpenFileMappingW, OpenEventW, WaitForSingleObject, ReleaseMutex, SetEvent, \
    CloseHandle, MapViewOfFile, UnmapViewOfFile

from streamlink_cli.aligned_alloc.ctype_def import \
    SHARED_MEMORY_BUFFER_NAME, MUTEX_WORKERS_NAME, EVENT_NEW_REQUEST_NAME, EVENT_REQUEST_FINISHED_NAME

from streamlink_cli.aligned_alloc.ctype_def import \
    INFINITE, WAIT_OBJECT_0, WAIT_TIMEOUT, SYNCHRONIZE, \
    EVENT_MODIFY_STATE, FILE_MAP_ALL_ACCESS, INVALID_HANDLE_VALUE

from streamlink_cli.aligned_alloc.ctype_def import SIZEOF_AAF_BUFFER_T
from streamlink_cli.aligned_alloc.ctype_def import ctypes_cast_aaf_buffer


def check_handle(handle):
    return handle and handle != INVALID_HANDLE_VALUE


def close_handle_if_valid(handle):
    if check_handle(handle):
        CloseHandle(handle)


def wait_for_event(event_name, interval_ms):
    while True:
        # открываем событие, ждем какое то время до короткого таймаута
        # далее ждем дальше, либо до завершения либо до смерти сервера
        handle = OpenEventW(SYNCHRONIZE, False, event_name)
        if not check_handle(handle):
            return False
        wait_result = WaitForSingleObject(handle, interval_ms)
        CloseHandle(handle)
        if wait_result != WAIT_TIMEOUT:
            return wait_result == WAIT_OBJECT_0


class AlignedBlockRet:
    def __init__(self,
                 success: int, status_code: int, file_length: int = 0, file_fragments: int = 0,
                 alloc_offset: int = 0, alloc_time: int = 0, search_skip: int = 0, search_total: int = 0,
                 move_attempts: int = 0, move_time: int = 0, prefetch_time: int = 0
                 ):
        self.success = success
        self.statusCode = status_code
        self.fileLength = file_length
        self.fileFragments = file_fragments

        self.allocOffset = alloc_offset
        self.allocTime = alloc_time
        self.searchSkip = search_skip
        self.searchTotal = search_total
        self.moveAttempts = move_attempts
        self.moveTime = move_time
        self.prefetchTime = prefetch_time


def new_block(file_descriptor, block_size, align_size) -> AlignedBlockRet:
    mutex_workers_handle = None
    shared_memory_buffer_handle = None
    event_new_request_handle = None
    mapped_memory_pointer = None

    try:
        mutex_workers_handle = OpenMutexW(SYNCHRONIZE, False, MUTEX_WORKERS_NAME)
        if not check_handle(mutex_workers_handle):
            return AlignedBlockRet(0, -1000)

        shared_memory_buffer_handle = OpenFileMappingW(FILE_MAP_ALL_ACCESS, False, SHARED_MEMORY_BUFFER_NAME)
        if not check_handle(shared_memory_buffer_handle):
            return AlignedBlockRet(0, -1001)

        event_new_request_handle = OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE, False, EVENT_NEW_REQUEST_NAME)
        if not check_handle(event_new_request_handle):
            return AlignedBlockRet(0, -1002)

        wait_result = WaitForSingleObject(mutex_workers_handle, INFINITE)
        if wait_result != WAIT_OBJECT_0:
            return AlignedBlockRet(0, -1003)

        mapped_memory_pointer = MapViewOfFile(
            shared_memory_buffer_handle, FILE_MAP_ALL_ACCESS, 0, 0, SIZEOF_AAF_BUFFER_T
        )
        if not mapped_memory_pointer:
            return AlignedBlockRet(0, -1004)

        aaf_buffer = ctypes_cast_aaf_buffer(mapped_memory_pointer)
        aaf_buffer.request.PID = os.getpid()
        aaf_buffer.request.hFile = msvcrt.get_osfhandle(file_descriptor.fileno())
        aaf_buffer.request.blockSize = block_size
        aaf_buffer.request.alignSize = align_size

        if not SetEvent(event_new_request_handle):
            return AlignedBlockRet(0, -1005)

        if not wait_for_event(EVENT_REQUEST_FINISHED_NAME, 1000):
            return AlignedBlockRet(0, -1006)

        if not ReleaseMutex(mutex_workers_handle):
            return AlignedBlockRet(0, -1007)

        return AlignedBlockRet(
            aaf_buffer.result.success, aaf_buffer.result.statusCode,
            aaf_buffer.stats.fileLength, aaf_buffer.stats.fileFragments,
            aaf_buffer.stats.allocOffset, aaf_buffer.stats.allocTime,
            aaf_buffer.stats.searchSkip, aaf_buffer.stats.searchTotal,
            aaf_buffer.stats.moveAttempts, aaf_buffer.stats.moveTime,
            aaf_buffer.stats.prefetchTime
        )

    except KeyboardInterrupt:
        return AlignedBlockRet(0, -1008)

    finally:
        if mapped_memory_pointer:
            UnmapViewOfFile(mapped_memory_pointer)
        close_handle_if_valid(event_new_request_handle)
        close_handle_if_valid(shared_memory_buffer_handle)
        close_handle_if_valid(mutex_workers_handle)
