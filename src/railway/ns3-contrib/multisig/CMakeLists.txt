check_include_file_cxx(stdint.h HAVE_STDINT_H)
if(HAVE_STDINT_H)
    add_definitions(-DHAVE_STDINT_H)
endif()

set(examples_as_tests_sources)
if(${ENABLE_EXAMPLES})
    set(examples_as_tests_sources
        #test/multisig-examples-test-suite.cc
        )
endif()

# find_external_library(DEPENDENCY_NAME OpenSSL
#                       HEADER_NAME sha.h
#                       LIBRARY_NAME )

link_libraries(ssl gmp pbc crypto)

build_lib(
    LIBNAME multisig
    SOURCE_FILES model/MultiSigObj.cc
                 model/multisig.cc
                 helper/multisig-helper.cc
    HEADER_FILES model/MultiSigObj.h
                 model/multisig.h
                 helper/multisig-helper.h
    LIBRARIES_TO_LINK ${libcore} 
    TEST_SOURCES test/multisig-test-suite.cc
                 ${examples_as_tests_sources}
)

