# Aligned allocation file for [streamlink](https://github.com/streamlink/streamlink)
Windows only. A service to reduce file fragmentation when recording multiple channels
> [!NOTE]
> [My way to this code](https://llh.su/doc/notes/fragmentation)  

## Install
**Client**
1. Copy **`streamlink/pkgs/streamlink_cli/aligned_alloc`** to **`streamlink_cli`** folder
2. Add **`import streamlink_cli.aligned_alloc.patch_functions`** to the top of **`streamlink/pkgs/streamlink_cli/main.py`**

**Service**
1. Download from [releases](../../releases) or CMake + MinGW64
2. Call **`AAF_service serviceStart`** to install service and start
> [!NOTE]
> Call **`AAF_service help`** to show command line help  
> Can be run without install, but requires admin rights

> [!IMPORTANT]
> Ensure your streamlink version (32/64-bit) matches the service architecture

## New strealink command line keys
- **`--aaf-prealloc-size`** - preallocation size in megabytes
- **`--aaf-align-size`** - align size in gigabytes
- **`--aaf-back-48h-on-complete`** - set time for completed streams to 2 days back, for TC colors
> [!IMPORTANT]
> Key **`--aaf-prealloc-size`** is required for other keys
 
## Simple 100MB prealloc example
**`> streamlink --aaf-prealloc-size 100 ...`**  
> [!NOTE]
> Required client only

## Physical 5GB align example with 100MB prealloc
**`> streamlink --aaf-align-size 5 --aaf-prealloc-size 100 ...`**
> [!NOTE]
> A dedicated disk or partition is recommended for this option.  
> Required client and service.

## License
Same with streamlink