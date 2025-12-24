
set(SOURCES_AAF_test
        targets/AAF_test.c
        )
add_executable(AAF_test
        ${SOURCES_common}
        ${SOURCES_AAF_test}
        )
target_include_directories(AAF_test PRIVATE include)

list(APPEND ALL_TARGETS AAF_test)
