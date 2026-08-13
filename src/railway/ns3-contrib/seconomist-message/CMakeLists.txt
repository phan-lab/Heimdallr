check_include_file_cxx(stdint.h HAVE_STDINT_H)
if(HAVE_STDINT_H)
    add_definitions(-DHAVE_STDINT_H)
endif()

set(examples_as_tests_sources)
if(${ENABLE_EXAMPLES})
    set(examples_as_tests_sources
        #test/seconomist-message-examples-test-suite.cc
        )
endif()

build_lib(
    LIBNAME seconomist-message
    SOURCE_FILES model/seconomist-message.cc
                 helper/seconomist-message-helper.cc
    HEADER_FILES model/seconomist-message.h
                 model/msg_def.h
                 model/move_auth.h
                 model/move_auth_fwd.h
                 model/ping-accept.h
                 model/ping-inter.h
                 model/ping-prepare.h
                 model/ping-propose.h
                 model/poc.h
                 model/location-speed-peer.h
                 helper/seconomist-message-helper.h
    LIBRARIES_TO_LINK ${libcore}
                      ${libtask-schedule}
                      ${libmultisig}
                      ${libnetwork}
    TEST_SOURCES test/seconomist-message-test-suite.cc
                 ${examples_as_tests_sources}
)

