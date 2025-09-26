cmake_minimum_required(VERSION 3.8)

project(AAF_service)

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -s -ffunction-sections -Wl,-gc-sections -Wall -municode -static-libgcc -static-libstdc++")
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
        src/arguments_proc.c include/arguments_proc.h)
list(APPEND SOURCES_AAF_service
        ${SOURCES_common})
list(APPEND SOURCES_AAF_service
        res/resources.rc)

set(SOURCES_AAF_test
        src/AAF_test.c)
list(APPEND SOURCES_AAF_test
        ${SOURCES_common})

set(SOURCES_AAF_block_lib
        src/AAF_lib.c)
list(APPEND SOURCES_AAF_block_lib
        ${SOURCES_common})
list(APPEND SOURCES_AAF_block_lib
        res/resources.rc)

add_executable(AAF_service ${SOURCES_AAF_service})
set(INCLUDE_AAF_service include)
target_include_directories(AAF_service PRIVATE ${INCLUDE_AAF_service})

add_executable(AAF_test ${SOURCES_AAF_test})
set(INCLUDE_AAF_test include)
target_include_directories(AAF_test PRIVATE ${INCLUDE_AAF_test})

add_library(AAF_block SHARED ${SOURCES_AAF_block_lib})
set(INCLUDE_AAF_block_lib include)
target_include_directories(AAF_block PRIVATE ${INCLUDE_AAF_block_lib})

add_custom_target(_buid_all_ ALL DEPENDS AAF_service AAF_test AAF_block)

add_custom_command(TARGET AAF_service
        PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -DPATH_FILE_VERSION_INFO="${CMAKE_CURRENT_SOURCE_DIR}/res/resources.rc"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/generate_version.cmake"
        )