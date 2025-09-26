# Aligned allocation file for [streamlink](https://github.com/streamlink/streamlink)
Windows only. A service to reduce file fragmentation when recording multiple channels. [My way to this code (in russian)](https://llh.su/doc/notes/fragmentation)  

### Client
- Copy `streamlink/pkgs/streamlink_cli/aligned_alloc` to `streamlink_cli` folder
- Add `import streamlink_cli.aligned_alloc.patch_functions` to the top of `streamlink/pkgs/streamlink_cli/main.py`

### Service
- Download from releases or Cmake + MinGW64
- Call `AAF_service.exe start` to install service and start
- Call `AAF_service.exe uninstall` to stop and remove service

## New strealink command line keys
- `--aaf-prealloc-size` - preallocation size in megabytes
- `--aaf-align-size` - align size in gigabytes
- `--aaf-back-48h-on-complete` - set time for completed files to 2 days back, for TC colors

Second two keys works with `--aaf-prealloc-size`
## Examples
#### `--aaf-prealloc-size 100`
- Streamlink simply increases the file size in 100MB increments instead of 8KB, resulting in 100MB fragments.
- Required client only.

#### `--aaf-prealloc-size 100 --aaf-align-size 10`
- File allocation uses 10GB physical alignment, resulting in 10GB fragments.
- A dedicated disk or partition is recommended for this option.
- Required client and service.
## License
Same with streamlink