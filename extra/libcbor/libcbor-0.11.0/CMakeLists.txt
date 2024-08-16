#cmake_minimum_required(VERSION 3.0)

project(libcbor VERSION ${CBOR_VERSION} LANGUAGES C)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules/")
include(CTest)
include(GNUInstallDirs) # Provides CMAKE_INSTALL_ variables

SET(CBOR_VERSION_MAJOR "0")
SET(CBOR_VERSION_MINOR "11")
SET(CBOR_VERSION_PATCH "0")
SET(CBOR_VERSION ${CBOR_VERSION_MAJOR}.${CBOR_VERSION_MINOR}.${CBOR_VERSION_PATCH})

option(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY "cmake --build --target install does not depend on cmake --build" true)
option(BUILD_SHARED_LIBS "Build as a shared library" false)

include(CheckIncludeFiles)

include(TestBigEndian)
test_big_endian(BIG_ENDIAN)
if(BIG_ENDIAN)
    add_definitions(-DIS_BIG_ENDIAN)
endif()

option(CBOR_CUSTOM_ALLOC "Custom, dynamically defined allocator support" OFF)
if(CBOR_CUSTOM_ALLOC)
    message(WARNING
        "CBOR_CUSTOM_ALLOC has been deprecated. Custom allocators are now enabled by default."
        "The flag is a no-op and will be removed in the next version. "
        "Please remove CBOR_CUSTOM_ALLOC from your build configuration.")
endif(CBOR_CUSTOM_ALLOC)

option(CBOR_PRETTY_PRINTER "Include a pretty-printing routine" ON)
set(CBOR_BUFFER_GROWTH "2" CACHE STRING "Factor for buffer growth & shrinking")
set(CBOR_MAX_STACK_SIZE "2048" CACHE STRING "maximum size for decoding context stack")

option(WITH_TESTS "[TEST] Build unit tests (requires CMocka)" OFF)
if(WITH_TESTS)
    add_definitions(-DWITH_TESTS)
endif(WITH_TESTS)

option(WITH_EXAMPLES "Build examples" ON)

option(HUGE_FUZZ "[TEST] Fuzz through 8GB of data in the test. Do not use with memory instrumentation!" OFF)
if(HUGE_FUZZ)
    add_definitions(-DHUGE_FUZZ)
endif(HUGE_FUZZ)

option(SANE_MALLOC "[TEST] Assume that malloc will not allocate multi-GB blocks. Tests only, platform specific" OFF)
if(SANE_MALLOC)
    add_definitions(-DSANE_MALLOC)
endif(SANE_MALLOC)

option(PRINT_FUZZ "[TEST] Print the fuzzer input" OFF)
if(PRINT_FUZZ)
    add_definitions(-DPRINT_FUZZ)
endif(PRINT_FUZZ)

option(SANITIZE "Enable ASan & a few compatible sanitizers in Debug mode" ON)

set(CPACK_GENERATOR "DEB" "TGZ" "RPM")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Pavel Kalvoda")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6")
set(CPACK_PACKAGE_VERSION_MAJOR ${CBOR_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${CBOR_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${CBOR_VERSION_PATCH})

include(CPack)

if(MINGW)
    # https://github.com/PJK/libcbor/issues/13
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
elseif(NOT MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -pedantic")
endif()

if(MSVC)
    # This just doesn't work right -- https://msdn.microsoft.com/en-us/library/5ft82fed.aspx
    set(CBOR_RESTRICT_SPECIFIER "")
else()
    set(CBOR_RESTRICT_SPECIFIER "restrict")

    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -Wall -g -ggdb -DDEBUG=true")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -Wall -DNDEBUG")

    if(SANITIZE)
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} \
            -fsanitize=undefined -fsanitize=address \
            -fsanitize=bounds -fsanitize=alignment")
    endif()
endif()

set(CMAKE_EXE_LINKER_FLAGS_DEBUG "-g")


include(CheckTypeSize)
check_type_size("size_t" SIZEOF_SIZE_T)
if(SIZEOF_SIZE_T LESS 8)
    message(WARNING "Your size_t is less than 8 bytes. Decoding of huge items that would exceed the memory address space will always fail. Consider implementing a custom streaming decoder if you need to deal with huge items.")
else()
    add_definitions(-DEIGHT_BYTE_SIZE_T)
endif()

enable_testing()

set(CTEST_MEMORYCHECK_COMMAND "/usr/bin/valgrind")
set(MEMORYCHECK_COMMAND_OPTIONS "--tool=memcheck --track-origins=yes --leak-check=full --error-exitcode=1")

add_custom_target(coverage
                  COMMAND ctest
                  COMMAND lcov --capture --directory . --output-file coverage.info
                  COMMAND genhtml coverage.info --highlight --legend --output-directory coverage_html
                  COMMAND echo "Coverage report ready: ${CMAKE_CURRENT_BINARY_DIR}/coverage_html/index.html")

add_custom_target(llvm-coverage
                    COMMAND make -j 16
                    COMMAND rm -rf coverage_profiles
                    COMMAND mkdir coverage_profiles
                    COMMAND bash -c [[ for TEST in $(ls test/*_test); do LLVM_PROFILE_FILE="coverage_profiles/$(basename -- ${TEST}).profraw" ./${TEST}; done ]]
                    # VERBATIM makes escaping working, but breaks shell expansions, so we need to explicitly use bash
                    COMMAND bash -c [[ llvm-profdata merge -sparse $(ls coverage_profiles/*.profraw) -o coverage_profiles/combined.profdata ]]
                    COMMAND bash -c [[ llvm-cov show -instr-profile=coverage_profiles/combined.profdata test/*_test -format=html > coverage_profiles/report.html ]]
                    COMMAND bash -c [[ llvm-cov report -instr-profile=coverage_profiles/combined.profdata test/*_test ]]
                    COMMAND echo "Coverage report ready: ${CMAKE_CURRENT_BINARY_DIR}/coverage_profiles/report.html"
                    VERBATIM)

include_directories(src)


option(COVERAGE "Enable code coverage instrumentation" OFF)
if (COVERAGE)
    message("Configuring code coverage instrumentation")
    if(CMAKE_C_COMPILER_ID MATCHES "GNU")
        # https://gcc.gnu.org/onlinedocs/gcc/Debugging-Options.html
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -fprofile-arcs -ftest-coverage --coverage")
        set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -g -fprofile-arcs -ftest-coverage --coverage")
    elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-instr-generate -fcoverage-mapping")
        set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fprofile-instr-generate")
    else()
        message(WARNING "Code coverage build not implemented for compiler ${CMAKE_C_COMPILER_ID}")
    endif()
endif (COVERAGE)

# We want to generate configuration.h from the template and make it so that it is accessible using the same
# path during both library build and installed header use, without littering the source dir.
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/cbor/configuration.h.in ${PROJECT_BINARY_DIR}/cbor/configuration.h)
# install(FILES ${PROJECT_BINARY_DIR}/cbor/configuration.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cbor)
# Make the header visible at compile time
include_directories(${PROJECT_BINARY_DIR})

# CMake >= 3.9.0 enables LTO for GCC and Clang with INTERPROCEDURAL_OPTIMIZATION
# Policy CMP0069 enables this behavior when we set the minimum CMake version < 3.9.0
# Checking for LTO support before setting INTERPROCEDURAL_OPTIMIZATION is mandatory with CMP0069 set to NEW.
set(use_lto FALSE)
if(${CMAKE_VERSION} VERSION_GREATER "3.9.0" OR ${CMAKE_VERSION} VERSION_EQUAL "3.9.0")
    cmake_policy(SET CMP0069 NEW)
    # Require LTO support to build libcbor with newer CMake versions
    include(CheckIPOSupported)
#    check_ipo_supported(RESULT use_lto)
endif(${CMAKE_VERSION} VERSION_GREATER "3.9.0" OR ${CMAKE_VERSION} VERSION_EQUAL "3.9.0")
if(use_lto)
    message(STATUS "LTO is enabled")
else()
    message(STATUS "LTO is not enabled")
endif(use_lto)

add_subdirectory(src)
if(use_lto)
    set_property(DIRECTORY src PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif(use_lto)

if (WITH_TESTS)
    add_subdirectory(test)
    if(use_lto)
        set_property(DIRECTORY test PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif(use_lto)
endif (WITH_TESTS)

if (WITH_EXAMPLES)
    add_subdirectory(examples)
    if(use_lto)
        set_property(DIRECTORY examples PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif(use_lto)
endif (WITH_EXAMPLES)
