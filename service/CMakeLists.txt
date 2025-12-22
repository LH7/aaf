cmake_minimum_required(VERSION 3.15)

project(AAF_service C)
set(CMAKE_C_STANDARD 99)

if(POLICY CMP0115)
        cmake_policy(SET CMP0115 NEW)
endif()
add_compile_definitions(__USE_MINGW_ANSI_STDIO=0)

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -Wall -flto -fdata-sections -ffunction-sections -municode -Wl,-gc-sections -s -static")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(CMAKE_SHARED_LIBRARY_PREFIX "")

set(SOURCES_common
        src/AAF_buffer.c    include/AAF_buffer.h
        src/dup_handle.c    include/dup_handle.h
        src/AAF_block.c     include/AAF_block.h
        src/func_dyn.c      include/func_dyn.h
        src/is_admin.c      include/is_admin.h
        src/log_disk.c      include/log_disk.h
        src/qp_timer.c      include/qp_timer.h
        src/zeros_array.c   include/zeros_array.h
        src/shared_malloc.c include/shared_malloc.h
        )

set(SOURCES_AAF_service
        src/main.c
        src/simple_service.c  include/simple_service.h
        src/arguments_proc.c  include/arguments_proc.h
        )
set(SOURCES_AAF_test
        src/AAF_test.c
        )
set(SOURCES_AAF_block_lib
        src/AAF_lib.c
        )

add_executable(AAF_service
        ${SOURCES_common}
        ${SOURCES_AAF_service}
        res/resources.rc
        )
add_executable(AAF_test
        ${SOURCES_common}
        ${SOURCES_AAF_test}
        )
add_library(AAF_block SHARED
        ${SOURCES_common}
        ${SOURCES_AAF_block_lib}
        res/resources.rc
        )

target_include_directories(AAF_test PRIVATE include)
target_include_directories(AAF_service PRIVATE include)
target_include_directories(AAF_block PRIVATE include)

add_custom_target(_buid_all_ ALL DEPENDS
        AAF_service AAF_test AAF_block
        )

add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/res/resources.rc
        COMMAND ${CMAKE_COMMAND}
            -DPATH_FILE_VERSION_INFO="${CMAKE_CURRENT_SOURCE_DIR}/res/resources.rc"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/generate_version.cmake"
        DEPENDS
            ${SOURCES_AAF_service} ${SOURCES_AAF_block_lib} ${SOURCES_AAF_test} ${SOURCES_common}
            ${CMAKE_CURRENT_SOURCE_DIR}/res/resources.rc.in
        )