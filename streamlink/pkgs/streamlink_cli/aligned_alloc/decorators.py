import functools
import logging
import os
from streamlink_cli.compat import stdout

from streamlink_cli.aligned_alloc.file import new_block as new_aligned_block_in_file
from streamlink_cli.aligned_alloc.utils import set_file_size, FileAutoCleanup
from streamlink_cli.aligned_alloc.settings import AAFSettings


log = logging.getLogger("streamlink.cli")


class GroupNotFoundError(Exception):
    pass


#
# Decorator for class:
# output->file.py->FileOutput(Output)
#
def aaf_decorator_file_output(cls):
    orig_open = cls._open
    orig_close = cls._close
    orig_write = cls._write

    def _print_aaf_alloc_params():
        aaf_info = ""
        if AAFSettings.bufferSize > 0:
            aaf_info = f"AAF: bufferSize={AAFSettings.bufferSize}"
        if AAFSettings.alignSize > 0:
            aaf_info = aaf_info + f", alignSize={AAFSettings.alignSize}"
        if aaf_info:
            log.info(aaf_info)

    def _print_aaf_stats(stats):
        log.debug(f"Align alloc stats:")
        log.debug(f"        Success: {stats.success}")
        log.debug(f"         Status: {stats.statusCode}")
        log.debug(f"     FileLength: {stats.fileLength}")
        log.debug(f"  FileFragments: {stats.fileFragments}")
        log.debug(f"    AllocOffset: {stats.allocOffset}")
        log.debug(f"     SearchSkip: {stats.searchSkip}")
        log.debug(f"    SearchTotal: {stats.searchTotal}")
        log.debug(f"   MoveAttempts: {stats.moveAttempts}")
        log.debug(f"         c time: {stats.allocTime / 10000}ms")

    @functools.wraps(orig_open)
    def aaf_open(self):
        result = orig_open(self)
        if self.filename and AAFSettings.bufferSize > 0:
            FileAutoCleanup.add(str(self.filename), self.fd)
            _print_aaf_alloc_params()
        return result

    @functools.wraps(orig_close)
    def aaf_close(self):
        if self.fd is not stdout and AAFSettings.bufferSize > 0:
            log.info(f"AAF: truncate file to: {self.fd.tell()} bytes")
            FileAutoCleanup.normal_clean(str(self.filename), self.fd)
        return orig_close(self)

    @functools.wraps(orig_write)
    def aaf_write(self, data):
        if AAFSettings.bufferSize > 0:
            current_cursor = self.fd.tell()
            file_size = os.fstat(self.fd.fileno()).st_size
            if current_cursor + len(data) > file_size:
                file_size = file_size + AAFSettings.bufferSize
                self.fd.flush()
                if AAFSettings.alignSize > 0:
                    ret = new_aligned_block_in_file(self.fd, AAFSettings.bufferSize, AAFSettings.alignSize)
                    _print_aaf_stats(ret)
                    if not ret.success:
                        log.warning(f"Cannot connect to AAF server (Err: {ret.statusCode}), use fallback")
                        set_file_size(self.fd, file_size)
                    elif ret.statusCode != 0:
                        log.warning(f"Cannot find free aligned block (Err: {ret.statusCode})")
                else:
                    set_file_size(self.fd, file_size)
                self.fd.seek(current_cursor)
        return orig_write(self, data)

    cls._open = aaf_open
    cls._close = aaf_close
    cls._write = aaf_write

    return cls


#
# Decorator for function:
# argparser.py->build_parser():
#
def aaf_decorator_build_parser(func):
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        # noinspection PyShadowingNames
        def find_output_group_args(parser):
            group_name = "File output options"
            # noinspection PyProtectedMember
            for group in parser._action_groups:
                if group.title == group_name:
                    return group
            raise GroupNotFoundError(f"AAF config: group '{group_name}' not found")

        # noinspection PyShadowingNames
        def add_aaf_args(parser):
            output = find_output_group_args(parser)
            output.add_argument(
                "--aaf-prealloc-size",
                type=int,
                default=0,
                help="""
                        AAF: prealloc file size in megabytes to avoid fragmentation, Windows only
                     """,
            )
            output.add_argument(
                "--aaf-align-size",
                type=int,
                default=0,
                help="""
                        AAF: align size in gigabytes to avoid fragmentation, Windows only
                        Worked with --aaf-prealloc-size
                     """,
            )
            output.add_argument(
                "--aaf-back-48h-on-complete",
                action='store_true',
                help="""
                        AAF: set time modify on complete to 48 hours back, Windows only
                        Worked with --aaf-prealloc-size
                     """,
            )

        # noinspection PyShadowingNames
        def get_aaf_args(args):
            AAFSettings.bufferSize = getattr(args, 'aaf_prealloc_size', 0) * 1024 * 1024
            AAFSettings.alignSize = getattr(args, 'aaf_align_size', 0) * 1024 * 1024 * 1024
            AAFSettings.back48Hours = getattr(args, 'aaf_back_48h_on_complete', False)
            if AAFSettings.bufferSize < 1024 * 1024:
                AAFSettings.bufferSize = 0
            if AAFSettings.alignSize < AAFSettings.bufferSize:
                AAFSettings.alignSize = 0

        # noinspection PyShadowingNames
        def parse_known_args_wrapper(*args, **kwargs):
            result, unknown = orig_parse_known_args(*args, **kwargs)
            if not hasattr(parser, '__aaf_args_is_parsed'):
                get_aaf_args(result)
                parser.__aaf_args_is_parsed = True
            return result, unknown

        parser = func(*args, **kwargs)
        add_aaf_args(parser)
        orig_parse_known_args = parser.parse_known_args
        parser.parse_known_args = parse_known_args_wrapper
        return parser

    return wrapper

