
set(SOURCES_AAF_lib
        targets/AAF_lib.c
        )
add_library(AAF_lib SHARED
        ${SOURCES_common}
        ${SOURCES_AAF_lib}
        res/resources.rc
        )
target_include_directories(AAF_lib PRIVATE include)

list(APPEND ALL_TARGETS AAF_lib)
