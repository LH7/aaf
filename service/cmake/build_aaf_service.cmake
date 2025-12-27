
set(SOURCES_AAF_service
        targets/AAF_service.c
        src/simple_service.c  include/simple_service.h
        src/arguments_proc.c  include/arguments_proc.h
        )
add_executable(AAF_service
        ${SOURCES_common}
        ${SOURCES_AAF_service}
        res/resources.rc
        )
target_include_directories(AAF_service PRIVATE include)

list(APPEND ALL_TARGETS AAF_service)
list(APPEND SOURCES_all ${SOURCES_AAF_service})
