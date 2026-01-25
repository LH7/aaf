cmake_minimum_required(VERSION 3.15)

project(AAF_service C)
set(CMAKE_C_STANDARD 99)

if(POLICY CMP0115)
        cmake_policy(SET CMP0115 NEW)
endif()
add_compile_definitions(__USE_MINGW_ANSI_STDIO=0)

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -Wall -flto -fdata-sections -ffunction-sections -municode -Wl,-gc-sections -s -static")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/bin)

set(SOURCES_common
        src/AAF_buffer.c    include/AAF_buffer.h
        src/dup_handle.c    include/dup_handle.h
        src/AAF_block.c     include/AAF_block.h
        src/is_admin.c      include/is_admin.h
        src/log_disk.c      include/log_disk.h
        src/qp_timer.c      include/qp_timer.h
        src/zeros_array.c   include/zeros_array.h
        src/shared_malloc.c include/shared_malloc.h
        )

set(SOURCES_common_AVX2
        src/zeros_array_avx2.c
        )
set(SOURCES_common_AVX512
        src/zeros_array_avx512.c
        )

list(APPEND SOURCES_common
        ${SOURCES_common_AVX2} ${SOURCES_common_AVX512}
        )

set_source_files_properties(${SOURCES_common_AVX2}   PROPERTIES COMPILE_OPTIONS "-mavx2")
set_source_files_properties(${SOURCES_common_AVX512} PROPERTIES COMPILE_OPTIONS "-mavx512f")

set(SOURCES_all ${SOURCES_common})

set(ALL_TARGETS)
file(GLOB TARGETS_CMAKE "cmake/build_*.cmake")
foreach(target_file ${TARGETS_CMAKE})
        include(${target_file})
endforeach()
add_custom_target(_buid_all_ ALL DEPENDS ${ALL_TARGETS})

list(APPEND SOURCES_all
        res/manifest.xml
        res/resources.rc.in
        )

add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/res/resources.rc
        COMMAND ${CMAKE_COMMAND}
            -DPATH_FILE_VERSION_INFO="${CMAKE_CURRENT_SOURCE_DIR}/res/resources.rc"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/generate_version.cmake"
        DEPENDS ${SOURCES_all}
        )