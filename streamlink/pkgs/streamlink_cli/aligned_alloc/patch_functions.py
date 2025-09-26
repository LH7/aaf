import sys

#
# Add 'import streamlink_cli.aligned_alloc.patch_functions' to main.py
# Must be before 'streamlink_cli.argparser' and 'streamlink_cli.output' (and after __future__)
#

for m_name in ('streamlink_cli.argparser', 'streamlink_cli.output.file', 'streamlink_cli.output'):
    if m_name in sys.modules:
        raise ImportError(f"AAF: import 'streamlink_cli.aligned_alloc' must be before '{m_name}'")

from streamlink_cli.aligned_alloc.decorators import aaf_decorator_build_parser, aaf_decorator_file_output

import streamlink_cli.argparser
streamlink_cli.argparser.build_parser = aaf_decorator_build_parser(streamlink_cli.argparser.build_parser)

import streamlink_cli.output
streamlink_cli.output.FileOutput = aaf_decorator_file_output(streamlink_cli.output.FileOutput)
