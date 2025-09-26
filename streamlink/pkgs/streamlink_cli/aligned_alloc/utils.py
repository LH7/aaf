import msvcrt
import os
import time

from streamlink_cli.aligned_alloc.ctype_def import SetFilePointerEx, SetEndOfFile, LARGE_INTEGER
from streamlink_cli.aligned_alloc.ctype_def import HandlerRoutine, SetConsoleCtrlHandler
from streamlink_cli.aligned_alloc.ctype_def import CTRL_CLOSE_EVENT, CTRL_SHUTDOWN_EVENT
from streamlink_cli.aligned_alloc.settings import AAFSettings


class FileAutoCleanup:
    opened_files = {}
    __handler_func = None

    @staticmethod
    def __console_close_cleanup_handler(event):
        if event == CTRL_CLOSE_EVENT or event == CTRL_SHUTDOWN_EVENT:  # close / shutdown
            for fn in FileAutoCleanup.opened_files:
                truncate_file_to_cursor(FileAutoCleanup.opened_files[fn])
                back_to_48_hours(FileAutoCleanup.opened_files[fn])
        return False

    @classmethod
    def add(cls, filename, fd):
        if len(cls.opened_files) == 0:
            cls.__handler_func = HandlerRoutine(cls.__console_close_cleanup_handler)
            SetConsoleCtrlHandler(cls.__handler_func, True)
        cls.opened_files[filename] = fd

    @classmethod
    def normal_clean(cls, filename, fd):
        truncate_file_to_cursor(fd)
        back_to_48_hours(fd)
        del cls.opened_files[filename]


def set_file_size(fd, file_size):
    win32handle = msvcrt.get_osfhandle(fd.fileno())
    SetFilePointerEx(win32handle, LARGE_INTEGER(file_size), None, 0)
    SetEndOfFile(win32handle)


def truncate_file_to_cursor(fd):
    current_cursor = fd.tell()
    fd.flush()
    set_file_size(fd, current_cursor)


def set_file_modify_time_to_x_hours_ago(h_file, hours_back=48):
    new_time = time.time() - hours_back * 3600
    os.utime(h_file.name, (new_time, new_time))


def back_to_48_hours(fd):
    if AAFSettings.back48Hours:
        set_file_modify_time_to_x_hours_ago(fd, 48)