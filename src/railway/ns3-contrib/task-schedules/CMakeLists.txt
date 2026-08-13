check_include_file_cxx(stdint.h HAVE_STDINT_H)
if(HAVE_STDINT_H)
    add_definitions(-DHAVE_STDINT_H)
endif()

set(examples_as_tests_sources)
if(${ENABLE_EXAMPLES})
    set(examples_as_tests_sources
        #test/task-schedules-examples-test-suite.cc
        )
endif()

build_lib(
    LIBNAME task-schedules
    SOURCE_FILES "model/task-schedules.cc"
                 "model/tasks.cc"
                 "model/parse_sche.cc"
                 "helper/task-schedules-helper.cc"
    HEADER_FILES "model/task-schedules.h"
                 "model/tasks.h"
                 "model/parse_sche.h"
                 "helper/task-schedules-helper.h"
    LIBRARIES_TO_LINK ${libcore}
    TEST_SOURCES "test/task-schedules-test-suite.cc"
                 ${examples_as_tests_sources}
)

