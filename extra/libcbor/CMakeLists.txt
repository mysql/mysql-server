#cmake_minimum_required(VERSION 2.8)

DISABLE_MISSING_PROFILE_WARNING()

set(VERSION "0.7.0")

IF(POLICY CMP0048)
  CMAKE_POLICY(SET CMP0048 NEW)
  project(libcbor VERSION ${VERSION} LANGUAGES C)
ELSE()
  project(libcbor C)
ENDIF()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/CMakeModules/")
include(CTest)

MY_CHECK_CXX_COMPILER_WARNING("-Wdocumentation" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_C_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()
MY_CHECK_CXX_COMPILER_WARNING("-Wstrict-prototypes" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_C_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

MSVC_CPPCHECK_DISABLE()

SET(CBOR_VERSION_MAJOR "0")
SET(CBOR_VERSION_MINOR "7")
SET(CBOR_VERSION_PATCH "0")
SET(CBOR_VERSION ${CBOR_VERSION_MAJOR}.${CBOR_VERSION_MINOR}.${CBOR_VERSION_PATCH})

set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY true)
include(CheckIncludeFiles)

include(TestBigEndian)
test_big_endian(BIG_ENDIAN)
if(BIG_ENDIAN)
    add_definitions(-DIS_BIG_ENDIAN)
endif()

option(CBOR_CUSTOM_ALLOC "Custom, dynamically defined allocator support" OFF)
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

endif()

set(CMAKE_EXE_LINKER_FLAGS_DEBUG "-g")


include(CheckTypeSize)
check_type_size("size_t" SIZEOF_SIZE_T)
if(SIZEOF_SIZE_T LESS 8)
    message(WARNING "Your size_t is less than 8 bytes. Long items with 64b length specifiers might not work as expected. Make sure to run the tests!")
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
                  COMMAND echo "Coverage report ready: file://${CMAKE_CURRENT_BINARY_DIR}/coverage_html/index.html")
include_directories(src)


option(COVERAGE "Enable code coverage instrumentation" OFF)
if (COVERAGE)
    message("Configuring code coverage instrumentation")
    if(NOT CMAKE_C_COMPILER MATCHES "gcc")
        message(WARNING "Gcov instrumentation only works with GCC")
    endif()
    # https://gcc.gnu.org/onlinedocs/gcc/Debugging-Options.html
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -fprofile-arcs -ftest-coverage --coverage")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -g -fprofile-arcs -ftest-coverage --coverage")
endif (COVERAGE)

# We want to generate configuration.h from the template and make it so that it is accessible using the same
# path during both library build and installed header use, without littering the source dir.
# Using cbor/configuration.h in the build dir works b/c headers will be installed to <prefix>/cbor
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/cbor/configuration.h.in ${PROJECT_BINARY_DIR}/cbor/configuration.h)
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
    check_ipo_supported(RESULT use_lto)
endif(${CMAKE_VERSION} VERSION_GREATER "3.9.0" OR ${CMAKE_VERSION} VERSION_EQUAL "3.9.0")
if(use_lto)
    message(STATUS "LTO is enabled")
else()
    message(STATUS "LTO is not enabled")
endif(use_lto)

subdirs(src)
if(use_lto)
    set_property(DIRECTORY src PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif(use_lto)
