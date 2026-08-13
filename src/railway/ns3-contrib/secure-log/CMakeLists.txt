check_include_file_cxx(stdint.h HAVE_STDINT_H)
if(HAVE_STDINT_H)
    add_definitions(-DHAVE_STDINT_H)
endif()

set(examples_as_tests_sources)
if(${ENABLE_EXAMPLES})
    set(examples_as_tests_sources
        #test/log-examples-test-suite.cc
        )
endif()


build_lib(
    LIBNAME secure-log
    SOURCE_FILES model/secure-log.cc
                 model/log_entry.cc
                 helper/log-helper.cc
    HEADER_FILES model/secure-log.h
                 model/log_entry.h
                 helper/log-helper.h
    LIBRARIES_TO_LINK ${libcore}
                      ${libtask-schedules}
    TEST_SOURCES test/log-test-suite.cc
                 ${examples_as_tests_sources}
)

