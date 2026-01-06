import ctypes
from ctypes import wintypes

INFINITE = 0xFFFFFFFF
WAIT_OBJECT_0 = 0
WAIT_ABANDONED = 0x80
WAIT_TIMEOUT = 0x102
WAIT_FAILED = 0xFFFFFFFF

INVALID_HANDLE_VALUE = wintypes.HANDLE(-1).value
LARGE_INTEGER = ctypes.c_longlong

SYNCHRONIZE = 0x00100000
EVENT_MODIFY_STATE = 0x0002
FILE_MAP_ALL_ACCESS = 0xF001F

CTRL_C_EVENT = 0
CTRL_BREAK_EVENT = 1
CTRL_CLOSE_EVENT = 2
CTRL_LOGOFF_EVENT = 5
CTRL_SHUTDOWN_EVENT = 6

kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)

HandlerRoutine = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.DWORD)

OpenMutexW = kernel32.OpenMutexW
OpenMutexW.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.LPCWSTR]
OpenMutexW.restype = wintypes.HANDLE

OpenFileMappingW = kernel32.OpenFileMappingW
OpenFileMappingW.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.LPCWSTR]
OpenFileMappingW.restype = wintypes.HANDLE

OpenEventW = kernel32.OpenEventW
OpenEventW.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.LPCWSTR]
OpenEventW.restype = wintypes.HANDLE

WaitForSingleObject = kernel32.WaitForSingleObject
WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
WaitForSingleObject.restype = wintypes.DWORD

ReleaseMutex = kernel32.ReleaseMutex
ReleaseMutex.argtypes = [wintypes.HANDLE]
ReleaseMutex.restype = wintypes.BOOL

SetEvent = kernel32.SetEvent
SetEvent.argtypes = [wintypes.HANDLE]
SetEvent.restype = wintypes.BOOL

CloseHandle = kernel32.CloseHandle
CloseHandle.argtypes = [wintypes.HANDLE]
CloseHandle.restype = wintypes.BOOL

MapViewOfFile = kernel32.MapViewOfFile
MapViewOfFile.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.DWORD,
                          wintypes.DWORD, ctypes.c_size_t]
MapViewOfFile.restype = wintypes.LPVOID

UnmapViewOfFile = kernel32.UnmapViewOfFile
UnmapViewOfFile.argtypes = [wintypes.LPCVOID]
UnmapViewOfFile.restype = wintypes.BOOL

GetLastError = kernel32.GetLastError
GetLastError.argtypes = []
GetLastError.restype = wintypes.DWORD

SetFilePointerEx = kernel32.SetFilePointerEx
SetFilePointerEx.argtypes = [
    wintypes.HANDLE, ctypes.c_longlong,
    ctypes.POINTER(ctypes.c_longlong), wintypes.DWORD]
SetFilePointerEx.restype = wintypes.BOOL

SetEndOfFile = kernel32.SetEndOfFile
SetEndOfFile.argtypes = [wintypes.HANDLE]
SetEndOfFile.restype = wintypes.BOOL

SetConsoleCtrlHandler = kernel32.SetConsoleCtrlHandler
SetConsoleCtrlHandler.argtypes = [HandlerRoutine, wintypes.BOOL]
SetConsoleCtrlHandler.restype = wintypes.BOOL


def add_ctype_fields_pack1(cls):
    if hasattr(cls, '__annotations__'):
        cls._fields_ = [(name, typ) for name, typ in cls.__annotations__.items()]
    cls._pack_ = 1
    return cls


@add_ctype_fields_pack1
class AAFRequestT(ctypes.Structure):
    PID:       ctypes.c_int64
    hFile:     ctypes.c_int64
    blockSize: ctypes.c_int64
    alignSize: ctypes.c_int64


@add_ctype_fields_pack1
class AAFResultT(ctypes.Structure):
    success:    ctypes.c_int64
    statusCode: ctypes.c_int64


@add_ctype_fields_pack1
class AAFStatsT(ctypes.Structure):
    fileLength:    ctypes.c_int64
    fileFragments: ctypes.c_int64
    allocOffset:    ctypes.c_int64
    allocTime:      ctypes.c_int64
    searchSkip:     ctypes.c_int64
    searchTotal:    ctypes.c_int64
    moveAttempts:   ctypes.c_int64
    moveTime:       ctypes.c_int64
    prefetchTime:   ctypes.c_int64


@add_ctype_fields_pack1
class AAFBufferT(ctypes.Structure):
    request: AAFRequestT
    result:  AAFResultT
    stats:   AAFStatsT


SIZEOF_AAF_BUFFER_T = ctypes.sizeof(AAFBufferT)

SHARE_NAMESPACE = "Global\\AAFv1_"
SHARED_MEMORY_BUFFER_NAME = SHARE_NAMESPACE + "Buf"
MUTEX_WORKERS_NAME = SHARE_NAMESPACE + "Mtx_Workers"
EVENT_NEW_REQUEST_NAME = SHARE_NAMESPACE + "Evt_NewRequest"
EVENT_REQUEST_FINISHED_NAME = SHARE_NAMESPACE + "Evt_RequestFinished"


def ctypes_cast_aaf_buffer(mapped_memory) -> AAFBufferT:
    return ctypes.cast(mapped_memory, ctypes.POINTER(AAFBufferT)).contents
