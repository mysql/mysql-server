# Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

IF(WIN32)
  # CMake 3.8.0 is needed for Visual Studio 2017 and x64 toolset.
  CMAKE_MINIMUM_REQUIRED(VERSION 3.8.0)
ELSEIF(APPLE)
  # FindOpenSSL needs at least:
  CMAKE_MINIMUM_REQUIRED(VERSION 3.1.2)
ELSEIF(UNIX)
  # CMake 3.4.0 is needed for Solaris due to CMake bug 15673.
  # Cannot use CMAKE_SYSTEM_NAME until after PROJECT(), use uname directly.
  FIND_PROGRAM(MY_UNAME uname /bin /usr/bin /usr/local/bin /sbin)
  IF(MY_UNAME)
    EXEC_PROGRAM(uname ARGS -s OUTPUT_VARIABLE MY_HOST_SYSTEM_NAME)
    IF(MY_HOST_SYSTEM_NAME MATCHES "SunOS")
      CMAKE_MINIMUM_REQUIRED(VERSION 3.4.0)
    ENDIF()
  ENDIF()
ENDIF()
# 2.8.12 as minimum unless higher is checked above.
CMAKE_MINIMUM_REQUIRED(VERSION 2.8.12)

# Explicitly set CMP0018 and CMP0022 = NEW since newer versions of
# CMake has OLD as default even if OLD is deprecated.
# See cmake --help-policy CMP0018 / CMP0022
CMAKE_POLICY(SET CMP0018 NEW)
CMAKE_POLICY(SET CMP0022 NEW)

# We use get_target_property() for non-existent targets (CMP0045)
IF(CMAKE_VERSION VERSION_EQUAL "3.0.0" OR
   CMAKE_VERSION VERSION_GREATER "3.0.0")
 CMAKE_POLICY(SET CMP0045 OLD)
ENDIF()

MESSAGE(STATUS "Running cmake version ${CMAKE_VERSION}")

# Will set GIT_EXECUTABLE and GIT_FOUND
FIND_PACKAGE(Git)

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

# First, decide about build type (debug or release)
# If cmake is invoked with -DCMAKE_BUILD_TYPE, 
# respect user wishes and do not (re)define CMAKE_BUILD_TYPE. If WITH_DEBUG
# is given, set CMAKE_BUILD_TYPE = Debug. Otherwise, use Relwithdebinfo.

IF(DEFINED CMAKE_BUILD_TYPE)
  SET(HAVE_CMAKE_BUILD_TYPE TRUE)
ENDIF()

OPTION(WITH_DEBUG "Use dbug/safemutex" OFF)

# Distinguish between community and non-community builds, with the
# default being a community build. This does not impact the feature
# set that will be compiled in; it's merely provided as a hint to
# custom packaging steps.
OPTION(COMMUNITY_BUILD "Set to true if this is a community build" ON) 

# Use a default manufacturer if no manufacturer was identified.
SET(MANUFACTURER_DOCSTRING
  "Set the entity that appears as the manufacturer of packages that support a manufacturer field.")
IF(NOT DEFINED MANUFACTURER) 
  SET(MANUFACTURER "Built from Source" CACHE  STRING ${MANUFACTURER_DOCSTRING})
  MARK_AS_ADVANCED(MANUFACTURER)
ENDIF()


# MAX_INDEXES - Set the maximum number of indexes per table, default 64U
IF (NOT MAX_INDEXES)
  SET(MAX_INDEXES 64U)
ELSEIF(MAX_INDEXES MATCHES "^[0-9]+[Uu]?$")
  # MAX_INDEXES should be unsigned, so add the U suffix if it's missing.
  STRING(REGEX REPLACE "^([0-9]+).*$" "\\1U" MAX_INDEXES "${MAX_INDEXES}")
ELSE()
  MESSAGE(FATAL_ERROR "MAX_INDEXES should be an unsigned integer.")
ENDIF(NOT MAX_INDEXES)

IF(MAX_INDEXES GREATER 255)
  MESSAGE(FATAL_ERROR "MAX_INDEXES values greater than 255 is not supported!")
ELSEIF(MAX_INDEXES LESS 64)
  # Per documentation, ignore values less than 64 and use the default instead.
  MESSAGE(WARNING "MAX_INDEXES option ignored because it is less than 64.")
  SET(MAX_INDEXES 64U)
ENDIF()
MESSAGE(STATUS "Configuring with MAX_INDEXES = ${MAX_INDEXES}")

# We choose to provide WITH_DEBUG as alias to standard CMAKE_BUILD_TYPE=Debug
# which turns out to be not trivial, as this involves synchronization 
# between CMAKE_BUILD_TYPE and WITH_DEBUG. Besides, we have to deal with cases
# where WITH_DEBUG is  reset from ON to OFF  and here we need to reset 
# CMAKE_BUILD_TYPE to either none or default RelWithDebInfo

SET(BUILDTYPE_DOCSTRING
 "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or
 CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel")

IF(WITH_DEBUG)
  SET(CMAKE_BUILD_TYPE "Debug" CACHE STRING ${BUILDTYPE_DOCSTRING} FORCE)
  SET(OLD_WITH_DEBUG 1 CACHE INTERNAL "" FORCE)
ELSEIF(NOT HAVE_CMAKE_BUILD_TYPE OR OLD_WITH_DEBUG)
  IF(CMAKE_BUILD_TYPE MATCHES "Debug" OR NOT HAVE_CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING 
       ${BUILDTYPE_DOCSTRING} FORCE)
  ENDIF()
  SET(OLD_WITH_DEBUG 0 CACHE INTERNAL "" FORCE)
ENDIF()

IF(CMAKE_GENERATOR MATCHES "Visual Studio [1-9][0-9].*" AND
   CMAKE_GENERATOR_TOOLSET STREQUAL "")
  # Switch to 64 bit toolset on Windows (32 bit is default).
  # This is recommended as the 32 bit linker will run into address space issues
  # and not exit for long time.
  SET(CMAKE_GENERATOR_TOOLSET "host=x64")
ENDIF()

# On Linux el6/el7 the default gcc is too old, see if devtoolset is installed.
# We need to look for gcc before calling PROJECT below.
OPTION(FORCE_UNSUPPORTED_COMPILER "Disable compiler version checks" OFF)
MARK_AS_ADVANCED(WITHOUT_SERVER DISABLE_SHARED FORCE_UNSUPPORTED_COMPILER)
IF(CMAKE_HOST_UNIX AND NOT FORCE_UNSUPPORTED_COMPILER
    AND NOT CMAKE_C_COMPILER AND NOT CMAKE_CXX_COMPILER)
  # Cannot INCLUDE(CMakeDetermineSystem) prior to PROJECT initialization below.
  SET (ENV_CC "$ENV{CC}")
  SET (ENV_CXX "$ENV{CXX}")
  IF (ENV_CC STREQUAL "" AND ENV_CXX STREQUAL "")
    IF(MY_UNAME)
      EXEC_PROGRAM(uname ARGS -r OUTPUT_VARIABLE MY_HOST_SYSTEM_VERSION)
      IF(MY_HOST_SYSTEM_NAME MATCHES "Linux" AND
          MY_HOST_SYSTEM_VERSION MATCHES "\\.el[67](uek)?\\.")
        MESSAGE(STATUS
          "This is ${MY_HOST_SYSTEM_NAME} version ${MY_HOST_SYSTEM_VERSION}")
        FIND_PROGRAM(MY_GCC_COMPILER gcc)
        IF (MY_GCC_COMPILER)
          MESSAGE(STATUS "gcc is ${MY_GCC_COMPILER}")
          MESSAGE(STATUS
            "We probably need /opt/rh/devtoolset-7/root/usr/bin/gcc")
          EXECUTE_PROCESS(COMMAND ${MY_GCC_COMPILER} -dumpversion
            OUTPUT_VARIABLE MY_GCC_VERSION)
          IF(MY_GCC_VERSION VERSION_LESS 5.0)
            MESSAGE(STATUS "${MY_GCC_COMPILER} is version ${MY_GCC_VERSION}")
            FIND_PROGRAM(ALTERNATIVE_GCC gcc
              NO_DEFAULT_PATH
              PATHS "/opt/rh/devtoolset-7/root/usr/bin")
            FIND_PROGRAM(ALTERNATIVE_GPP g++
              NO_DEFAULT_PATH
              PATHS "/opt/rh/devtoolset-7/root/usr/bin")
            # On ARM there is no /opt/rh/devtoolset-7/ look for /usr/bin/gcc7
            FIND_PROGRAM(ALTERNATIVE_GCC gcc7
              NO_DEFAULT_PATH
              PATHS "/usr/bin")
            FIND_PROGRAM(ALTERNATIVE_GPP g++7
              NO_DEFAULT_PATH
              PATHS "/usr/bin")
            IF(ALTERNATIVE_GCC AND ALTERNATIVE_GPP)
              SET(CMAKE_C_COMPILER ${ALTERNATIVE_GCC})
              SET(CMAKE_CXX_COMPILER ${ALTERNATIVE_GPP})
              MESSAGE(STATUS "Using ${ALTERNATIVE_GCC}")
              MESSAGE(STATUS "Using ${ALTERNATIVE_GPP}")
            ELSE()
              MESSAGE(WARNING "Could not find devtoolset gcc")
            ENDIF()
          ENDIF()
        ENDIF()
      ENDIF()
    ENDIF()
  ENDIF()
ENDIF()

# Optionally set project name, e.g.
# foo.xcodeproj (mac) or foo.sln (windows)
SET(MYSQL_PROJECT_NAME_DOCSTRING "MySQL project name")
IF(DEFINED MYSQL_PROJECT_NAME)
  SET(MYSQL_PROJECT_NAME ${MYSQL_PROJECT_NAME} CACHE STRING
      ${MYSQL_PROJECT_NAME_DOCSTRING} FORCE)
ELSE()
  SET(MYSQL_PROJECT_NAME "MySQL" CACHE STRING
      ${MYSQL_PROJECT_NAME_DOCSTRING} FORCE)
  MARK_AS_ADVANCED(MYSQL_PROJECT_NAME)
ENDIF()

# Remember if WITH_NDBCLUSTER was specified by user
IF(DEFINED WITH_NDBCLUSTER)
  SET(HAVE_WITH_NDBCLUSTER 1)
ENDIF()
OPTION(WITH_NDBCLUSTER "Build MySQL Cluster" OFF)
OPTION(WITH_NDBCLUSTER_STORAGE_ENGINE
  "Legacy and deprecated alias for WITH_NDBCLUSTER" OFF)
OPTION(WITH_PLUGIN_NDBCLUSTER
  "Legacy and deprecated alias for WITH_NDBCLUSTER" OFF)
MARK_AS_ADVANCED(WITH_NDBCLUSTER_STORAGE_ENGINE WITH_PLUGIN_NDBCLUSTER)
IF(NOT HAVE_WITH_NDBCLUSTER)
  # Unless user has used WITH_NDBCLUSTER also look at
  # some of the legacy variables
  IF(WITH_PLUGIN_NDBCLUSTER)
    MESSAGE(WARNING "WITH_PLUGIN_NDBCLUSTER is deprecated and will be "
      "removed in a future release. Please use WITH_NDBCLUSTER instead.")
    SET(WITH_NDBCLUSTER ON)
  ENDIF()
  IF(WITH_NDBCLUSTER_STORAGE_ENGINE)
    MESSAGE(WARNING "WITH_NDBCLUSTER_STORAGE_ENGINE is deprecated and "
      "will be removed in a future release. Please use WITH_NDBCLUSTER "
      "instead.")
    SET(WITH_NDBCLUSTER ON)
  ENDIF()
ENDIF()
IF(WITH_NDBCLUSTER)
  MESSAGE(STATUS "Building MySQL Cluster")
ENDIF()

PROJECT(${MYSQL_PROJECT_NAME})
SET(BUILD_IS_SINGLE_CONFIG TRUE)
MESSAGE(STATUS "CMAKE_GENERATOR: ${CMAKE_GENERATOR}")
IF(CMAKE_GENERATOR MATCHES "Visual Studio" OR CMAKE_GENERATOR STREQUAL "Xcode")
  SET(BUILD_IS_SINGLE_CONFIG FALSE)
ENDIF()

# Maintainer mode is default on only for debug builds using GCC/G++
IF(CMAKE_BUILD_TYPE MATCHES "Debug" OR WITH_DEBUG)
  IF(CMAKE_COMPILER_IS_GNUCC AND CMAKE_COMPILER_IS_GNUCXX)
    SET(MYSQL_MAINTAINER_MODE ON CACHE BOOL
        "MySQL maintainer-specific development environment")
  ENDIF()
ENDIF()

OPTION(WITH_DEFAULT_COMPILER_OPTIONS
  "Use flags from cmake/build_configurations/compiler_options.cmake"
  ON)
OPTION(WITH_DEFAULT_FEATURE_SET
  "Use feature set in cmake/build_configurations/feature_set.cmake"
  ON)
IF(BUILD_CONFIG)
  INCLUDE(
  ${CMAKE_SOURCE_DIR}/cmake/build_configurations/${BUILD_CONFIG}.cmake)
ENDIF()

OPTION(INSTALL_STATIC_LIBRARIES "Install static libraries" ON)

#cmake on 64bit windows/mac/solaris doesn't set CMAKE_SYSTEM_PROCESSOR correctly
SET(MYSQL_MACHINE_TYPE ${CMAKE_SYSTEM_PROCESSOR})


# Include the platform-specific file. To allow exceptions, this code
# looks for files in order of how specific they are. If there is, for
# example, a generic Linux.cmake and a version-specific
# Linux-2.6.28-11-generic, it will pick Linux-2.6.28-11-generic and
# include it. It is then up to the file writer to include the generic
# version if necessary.
FOREACH(_base
    ${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_VERSION}-${CMAKE_SYSTEM_PROCESSOR}
    ${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_VERSION}
    ${CMAKE_SYSTEM_NAME})
  SET(_file ${CMAKE_SOURCE_DIR}/cmake/os/${_base}.cmake)
  IF(EXISTS ${_file})
    INCLUDE(${_file})
    BREAK()
  ENDIF()
ENDFOREACH()


IF(UNIX)
  OPTION(WITH_INNODB_MEMCACHED "" OFF)
  OPTION(ENABLE_MEMCACHED_SASL "Enable SASL on InnoDB Memcached" OFF)
  OPTION(ENABLE_MEMCACHED_SASL_PWDB "Enable SASL on InnoDB Memcached" OFF)
ELSE()
  OPTION(WITH_INNODB_MEMCACHED "" OFF)
ENDIF()
 
# Following autotools tradition, add preprocessor definitions
# specified in environment variable CPPFLAGS
IF(DEFINED ENV{CPPFLAGS})
  ADD_DEFINITIONS($ENV{CPPFLAGS})
ENDIF()

INCLUDE(CheckTypeSize)
CHECK_TYPE_SIZE("void *" SIZEOF_VOIDP)
MESSAGE(STATUS "SIZEOF_VOIDP ${SIZEOF_VOIDP}")
IF(WITH_DEFAULT_COMPILER_OPTIONS)
  INCLUDE(${CMAKE_SOURCE_DIR}/cmake/build_configurations/compiler_options.cmake)
ENDIF()
IF(WITH_DEFAULT_FEATURE_SET)
  INCLUDE(${CMAKE_SOURCE_DIR}/cmake/build_configurations/feature_set.cmake)
ENDIF()

# Add macros
INCLUDE(character_sets)
INCLUDE(cpu_info)
INCLUDE(zlib)
INCLUDE(lz4)
INCLUDE(lzma)
INCLUDE(re2)
INCLUDE(icu)
INCLUDE(libevent)
INCLUDE(ssl)
INCLUDE(readline)
INCLUDE(protobuf)
INCLUDE(mysql_version)
INCLUDE(libutils)
INCLUDE(plugin)
INCLUDE(component)
INCLUDE(install_macros)
INCLUDE(install_layout)
INCLUDE(mysql_add_executable)
INCLUDE(mysqlgcs)
INCLUDE(curl)

OPTION(DISABLE_SHARED 
 "Don't build shared libraries, compile code as position-dependent" OFF)
IF(DISABLE_SHARED)
  MESSAGE("Dynamic plugins are disabled.")
ENDIF()
OPTION(ENABLED_PROFILING "Enable profiling" ON)
OPTION(WITHOUT_SERVER OFF)
IF(UNIX)
  OPTION(WITH_VALGRIND "Valgrind instrumentation" OFF)
ENDIF()
IF(WIN32)
  OPTION(WITH_MSCRT_DEBUG "MS Visual Studio Debug CRT instrumentation" OFF)
ENDIF()
IF(NOT WITHOUT_SERVER)
  OPTION (WITH_UNIT_TESTS "Compile MySQL with unit tests" ON)
ENDIF()
OPTION(WITH_AUTHENTICATION_LDAP
       "Report error if the LDAP authentication plugin cannot be built." OFF)

include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)
# We need some extra FAIL_REGEX patterns
# Note that CHECK_C_SOURCE_COMPILES is a misnomer, it will also link.
MACRO (MY_CHECK_C_COMPILER_FLAG FLAG RESULT)
  SET(SAVE_CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}")
  SET(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${FLAG}")
  CHECK_C_SOURCE_COMPILES("int main(void) { return 0; }" ${RESULT}
    FAIL_REGEX "unknown argument ignored"
    FAIL_REGEX "argument unused during compilation"
    FAIL_REGEX "unsupported .*option"
    FAIL_REGEX "unknown .*option"
    FAIL_REGEX "unrecognized .*option"
    FAIL_REGEX "ignoring unknown option"
    FAIL_REGEX "[Ww]arning: [Oo]ption"
    FAIL_REGEX "error: visibility"
    FAIL_REGEX "warning: visibility"
    )
  SET(CMAKE_REQUIRED_FLAGS "${SAVE_CMAKE_REQUIRED_FLAGS}")
ENDMACRO()

MACRO (MY_CHECK_CXX_COMPILER_FLAG FLAG RESULT)
  SET(SAVE_CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}")
  SET(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${FLAG}")
  CHECK_CXX_SOURCE_COMPILES("int main(void) { return 0; }" ${RESULT}
    FAIL_REGEX "unknown argument ignored"
    FAIL_REGEX "argument unused during compilation"
    FAIL_REGEX "unsupported .*option"
    FAIL_REGEX "unknown .*option"
    FAIL_REGEX "unrecognized .*option"
    FAIL_REGEX "ignoring unknown option"
    FAIL_REGEX "[Ww]arning: [Oo]ption"
    FAIL_REGEX "error: visibility"
    FAIL_REGEX "warning: visibility"
    )
  SET(CMAKE_REQUIRED_FLAGS "${SAVE_CMAKE_REQUIRED_FLAGS}")
ENDMACRO()

MACRO(MY_SANITIZER_CHECK SAN_OPT ADD_OPTIMIZATION RESULT)
  MY_CHECK_C_COMPILER_FLAG("${SAN_OPT}" C_RESULT)
  MY_CHECK_CXX_COMPILER_FLAG("${SAN_OPT}" CXX_RESULT)
  IF(C_RESULT AND CXX_RESULT AND ${ADD_OPTIMIZATION})
    # We switch on basic optimization also for debug builds.
    # With optimization we may get some warnings, so we switch off -Werror
    SET(OPT_FLAG " -O1 ")
  ENDIF()
  IF(C_RESULT AND CXX_RESULT)
    SET(CMAKE_C_FLAGS_DEBUG
      "${CMAKE_C_FLAGS_DEBUG} ${SAN_OPT} ${OPT_FLAG} -Wno-error")
    SET(CMAKE_C_FLAGS_RELWITHDEBINFO
      "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${SAN_OPT}")
    SET(CMAKE_CXX_FLAGS_DEBUG
      "${CMAKE_CXX_FLAGS_DEBUG} ${SAN_OPT} ${OPT_FLAG} -Wno-error")
    SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO
      "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${SAN_OPT}")
    SET(${RESULT} 1)
  ELSE()
    SET(${RESULT} 0)
  ENDIF()
ENDMACRO()

OPTION(WITH_ASAN "Enable address sanitizer" OFF)
IF(WITH_ASAN)
  IF(WIN32_CLANG)
    # Feature detection of -faddress=sanitize is problematic with Clang on
    # Windows, since Clang (at least as of 5.0.0) requires /MT, but CMake
    # default is /MD, and this code is run before the one in
    # cmake/os/Windows.cmake, which sets /MT for ASAN on Win32 Clang.
    # Just enable it unconditionally; we know it's working.
    SET(CMAKE_C_FLAGS_DEBUG
      "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address -fsanitize-address-use-after-scope -Wno-error")
    SET(CMAKE_C_FLAGS_RELWITHDEBINFO
      "${CMAKE_C_FLAGS_RELWITHDEBINFO} -fsanitize=address -fsanitize-address-use-after-scope")
    SET(CMAKE_CXX_FLAGS_DEBUG
      "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fsanitize-address-use-after-scope -Wno-error")
    SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO
      "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -fsanitize=address -fsanitize-address-use-after-scope")
    SET(HAVE_ASAN 1)

    # Get the path to the ASAN support libraries.
    GET_FILENAME_COMPONENT(ASAN_LIB_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)
    SET(ASAN_LIB_DIR "${ASAN_LIB_DIR}/../lib/clang/${CMAKE_CXX_COMPILER_VERSION}/lib/windows")
    GET_FILENAME_COMPONENT(ASAN_LIB_DIR ${ASAN_LIB_DIR} REALPATH)
  ELSE()
    MY_SANITIZER_CHECK("-fsanitize=address" TRUE WITH_ASAN_OK)
    IF(WITH_ASAN_OK)
      OPTION(WITH_ASAN_SCOPE "Enable -fsanitize-address-use-after-scope" ON)
      SET(HAVE_ASAN 1)
      # This works with clang, but not gcc it seems.
      MY_CHECK_CXX_COMPILER_FLAG(
        "-fsanitize=address -fsanitize-address-use-after-scope"
        HAVE_SANITIZE_SCOPE)
      IF(WITH_ASAN_SCOPE AND HAVE_SANITIZE_SCOPE)
        SET(CMAKE_C_FLAGS_DEBUG
          "${CMAKE_C_FLAGS_DEBUG} -fsanitize-address-use-after-scope")
        SET(CMAKE_C_FLAGS_RELWITHDEBINFO
          "${CMAKE_C_FLAGS_RELWITHDEBINFO} -fsanitize-address-use-after-scope")
        SET(CMAKE_CXX_FLAGS_DEBUG
          "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize-address-use-after-scope")
        SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO
          "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -fsanitize-address-use-after-scope")
      ENDIF()
    ELSE()
      MESSAGE(FATAL_ERROR "Do not know how to enable address sanitizer")
    ENDIF()
  ENDIF()
ENDIF()

OPTION(WITH_MSAN "Enable memory sanitizer" OFF)
IF(WITH_MSAN)
  MY_SANITIZER_CHECK("-fsanitize=memory -fsanitize-memory-track-origins"
    TRUE WITH_MSAN_OK)
  IF(NOT WITH_MSAN_OK)
    MESSAGE(FATAL_ERROR "Do not know how to enable memory sanitizer")
  ELSE()
    MESSAGE(WARNING "Memory sanitizer support is currently experimental.")
  ENDIF()
ENDIF()

OPTION(WITH_UBSAN "Enable undefined behavior sanitizer" OFF)
IF(WITH_UBSAN)
  MY_SANITIZER_CHECK("-fsanitize=undefined" FALSE WITH_UBSAN_OK)
  IF(NOT WITH_UBSAN_OK)
    MESSAGE(FATAL_ERROR
            "Do not know how to enable undefined behavior sanitizer")
  ELSE()
    SET(HAVE_UBSAN 1)
  ENDIF()
ENDIF()

OPTION(WITH_TSAN "Enable thread sanitizer" OFF)
IF(WITH_TSAN)
  MY_SANITIZER_CHECK("-fsanitize=thread" TRUE WITH_TSAN_OK)
  IF(NOT WITH_TSAN_OK)
    MESSAGE(FATAL_ERROR "Do not know how to enable thread sanitizer")
  ELSE()
    MESSAGE(WARNING "Thread sanitizer support is currently experimental.")
  ENDIF()
ENDIF()

IF(WITH_ASAN AND WITH_MSAN)
  MESSAGE(FATAL_ERROR
          "Cannot use AddressSanitizer and MemorySanitizer together")
ENDIF()


# Always enable debug sync for debug builds.
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DENABLED_DEBUG_SYNC")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DENABLED_DEBUG_SYNC")
 
# Older versions of ccache must be disabled: export CCACHE_DISABLE=1
# See http://www.cmake.org/Wiki/CTest/Coverage
OPTION(ENABLE_GCOV "Enable gcov (debug, Linux builds only)" OFF)
IF (ENABLE_GCOV AND NOT WIN32 AND NOT APPLE)
  SET(CMAKE_CXX_FLAGS_DEBUG
    "${CMAKE_CXX_FLAGS_DEBUG} -fprofile-arcs -ftest-coverage -DHAVE_GCOV")
  SET(CMAKE_C_FLAGS_DEBUG
    "${CMAKE_C_FLAGS_DEBUG} -fprofile-arcs -ftest-coverage -DHAVE_GCOV")
  SET(CMAKE_EXE_LINKER_FLAGS_DEBUG
    "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fprofile-arcs -ftest-coverage -lgcov")
ENDIF()

OPTION(ENABLE_GPROF "Enable gprof (optimized, Linux builds only)" OFF)
IF (ENABLE_GPROF AND NOT WIN32 AND NOT APPLE)
  SET(CMAKE_C_FLAGS_RELWITHDEBINFO
    "${CMAKE_C_FLAGS_RELWITHDEBINFO} -pg")
  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO
    "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -pg")
  SET(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO
    "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} -pg")
ENDIF()

# Use gold if available and not explicitly disabled.
IF(WIN32)
  # Feature tests are expensive on Windows, and we only support MSVC,
  # which does not use gold. Thus, default to off to save the test.
  OPTION(USE_LD_GOLD "Use GNU gold linker" OFF)
ELSE()
  OPTION(USE_LD_GOLD "Use GNU gold linker" ON)
ENDIF()

IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
  OPTION(LINK_RANDOMIZE "Randomize the order of all symbols in the binary" OFF)
  SET(LINK_RANDOMIZE_SEED "mysql"
    CACHE STRING "Seed to use for link randomization")
ELSE()
  SET(LINK_RANDOMIZE OFF)
ENDIF()

IF (USE_LD_GOLD)
  # These tests will give a false negative on some Clang versions,
  # since there is no easy way in CMake to check for a linker flag
  # without reimplementing CheckCXXSourceCompiles, and
  # CheckCXXSourceCompiles tends to compile and link in two steps
  # (which causes warning output about unused flags, which we interpret
  # as the flag not existing).
  MY_CHECK_C_COMPILER_FLAG("-fuse-ld=gold" C_LD_GOLD_RESULT)
  MY_CHECK_CXX_COMPILER_FLAG("-fuse-ld=gold" CXX_LD_GOLD_RESULT)
  IF (C_LD_GOLD_RESULT AND CXX_LD_GOLD_RESULT AND NOT SOLARIS)
    SET(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -fuse-ld=gold")
    SET(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -fuse-ld=gold")

    # Seemingly --gc-sections causes problems with randomization, so only
    # turn it on if we are not making a randomized build.
    IF (NOT LINK_RANDOMIZE)
      SET(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,--gc-sections")
      SET(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -Wl,--gc-sections")
    ENDIF()
  ENDIF()
ENDIF()

IF(LINK_RANDOMIZE)
  SET(CMAKE_C_FLAGS_DEBUG
    "${CMAKE_C_FLAGS_DEBUG} -ffunction-sections -fdata-sections")
  SET(CMAKE_C_FLAGS_RELWITHDEBINFO
    "${CMAKE_C_FLAGS_RELWITHDEBINFO} -ffunction-sections -fdata-sections")
  SET(CMAKE_CXX_FLAGS_DEBUG
    "${CMAKE_CXX_FLAGS_DEBUG} -ffunction-sections -fdata-sections")
  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO
    "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -ffunction-sections -fdata-sections")

  SET(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,--sort-section=name")
  SET(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -Wl,--sort-section=name")

  SET(CMAKE_CXX_ARCHIVE_CREATE "sh -c '${CMAKE_SOURCE_DIR}/scripts/randomize-order.pl ${LINK_RANDOMIZE_SEED} <OBJECTS> && <CMAKE_AR> qc <TARGET> <LINK_FLAGS> <OBJECTS>'")
  SET(CMAKE_CXX_ARCHIVE_CREATE_APPEND "sh -c '${CMAKE_SOURCE_DIR}/scripts/randomize-order.pl ${LINK_RANDOMIZE_SEED} <OBJECTS> && <CMAKE_AR> q <TARGET> <LINK_FLAGS> <OBJECTS>'")
  SET(CMAKE_C_ARCHIVE_CREATE ${CMAKE_CXX_ARCHIVE_CREATE})
  SET(CMAKE_C_ARCHIVE_CREATE_APPEND ${CMAKE_CXX_ARCHIVE_CREATE_APPEND})
ENDIF()

IF(LINUX)
  # May be set to ON in cmake/build_configurations/mysql_release.cmake
  IF(DEFINED REPRODUCIBLE_BUILD)
    SET(REPRODUCIBLE_BUILD_DEFAULT ${REPRODUCIBLE_BUILD})
  ELSE()
    SET(REPRODUCIBLE_BUILD_DEFAULT OFF)
  ENDIF()
  OPTION(REPRODUCIBLE_BUILD "Take extra pains to make build result independent of build location and time" ${REPRODUCIBLE_BUILD_DEFAULT})
ENDIF()
IF(REPRODUCIBLE_BUILD)
  SET(DEBUG_PREFIX_FLAGS
    "-fdebug-prefix-map=${CMAKE_SOURCE_DIR}/=./ -fdebug-prefix-map=${CMAKE_CURRENT_BINARY_DIR}=./obj")

  # See if -fdebug-prefix= commands are included in the debug output,
  # making the build unreproducible with switches recorded.
  # See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=69821.
  EXECUTE_PROCESS(COMMAND ${CMAKE_C_COMPILER} -g3 -x c -S -fdebug-prefix-map=foo=bar -o - -
    INPUT_FILE /dev/null OUTPUT_VARIABLE DEBUG_PREFIX_MAP_RESULT)
  IF(DEBUG_PREFIX_MAP_RESULT MATCHES "foo=bar")
    SET(DEBUG_PREFIX_FLAGS "${DEBUG_PREFIX_FLAGS} -gno-record-gcc-switches")
  ENDIF()

  SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${DEBUG_PREFIX_FLAGS}")
  SET(CMAKE_C_FLAGS_RELWITHDEBINFO
    "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${DEBUG_PREFIX_FLAGS}")
  SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${DEBUG_PREFIX_FLAGS}")
  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO
    "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${DEBUG_PREFIX_FLAGS}")

  SET(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,--build-id=none")
  SET(CMAKE_CXX_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,--build-id=none")

  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE
    "${CMAKE_SOURCE_DIR}/scripts/invoke-with-relative-paths.pl")
ENDIF()

OPTION(ENABLED_LOCAL_INFILE
 "If we should enable LOAD DATA LOCAL by default" OFF)
MARK_AS_ADVANCED(ENABLED_LOCAL_INFILE)

#
# Options related to client-side protocol tracing
#

OPTION(WITH_CLIENT_PROTOCOL_TRACING
  "Support for client-side protocol tracing plugins" ON)
OPTION(WITH_TEST_TRACE_PLUGIN
  "Have a built-in test protocol trace plugin in libmysql (requires WITH_CLIENT_PROTOCOL_TRACING option)" OFF)

# Sanity checks for protocol tracing options

IF(WITH_TEST_TRACE_PLUGIN AND NOT WITH_CLIENT_PROTOCOL_TRACING)
MESSAGE("WARNING: Test trace plugin was selected but client protocol tracing infrastructure is not enabled - ignoring")
SET(WITH_TEST_TRACE_PLUGIN 0)
ENDIF()

IF(WITH_TEST_TRACE_PLUGIN AND NOT CMAKE_BUILD_TYPE MATCHES "Debug")
  MESSAGE(SEND_ERROR 
          "Test trace plugin was selected but it can be included only in debug binaries.
          Set WITH_TEST_TRACE_PLUGIN to OFF or WITH_DEBUG to ON.")
ENDIF()

# Set DBUG_OFF and other optional release-only flags for non-debug project types
FOREACH(BUILD_TYPE RELEASE RELWITHDEBINFO MINSIZEREL)
  FOREACH(LANG C CXX)
    SET(CMAKE_${LANG}_FLAGS_${BUILD_TYPE} 
     "${CMAKE_${LANG}_FLAGS_${BUILD_TYPE}} -DDBUG_OFF")
  ENDFOREACH()
ENDFOREACH()

IF(NOT CMAKE_BUILD_TYPE
    AND NOT CMAKE_GENERATOR MATCHES "Visual Studio" 
    AND NOT CMAKE_GENERATOR MATCHES "Xcode") 
    # This is the case of no CMAKE_BUILD_TYPE choosen, typical for VS and Xcode
    # or if custom C flags are set. In VS and Xcode for non-Debug configurations 
    # DBUG_OFF is already correctly set.
    ADD_DEFINITIONS(-DDBUG_OFF)
ENDIF()

# Add safemutex for debug configurations
FOREACH(LANG C CXX)
  SET(CMAKE_${LANG}_FLAGS_DEBUG
    "${CMAKE_${LANG}_FLAGS_DEBUG} -DSAFE_MUTEX")
ENDFOREACH()


# Set commonly used variables
IF(WIN32)
  SET(DEFAULT_MYSQL_HOME "C:/Program Files/MySQL/MySQL Server ${MYSQL_BASE_VERSION}" )
  SET(SHAREDIR share)
ELSE()
  SET(DEFAULT_MYSQL_HOME ${CMAKE_INSTALL_PREFIX})
  SET(SHAREDIR ${DEFAULT_MYSQL_HOME}/${INSTALL_MYSQLSHAREDIR})
ENDIF()

SET(DEFAULT_BASEDIR "${DEFAULT_MYSQL_HOME}")
IF(INSTALL_MYSQLDATADIR MATCHES "^/.*")
  SET(MYSQL_DATADIR ${INSTALL_MYSQLDATADIR} CACHE PATH "default MySQL data directory")
ELSE()
  SET(MYSQL_DATADIR "${DEFAULT_MYSQL_HOME}/${INSTALL_MYSQLDATADIR}" CACHE PATH "default MySQL data directory")
ENDIF()
IF(INSTALL_MYSQLKEYRINGDIR MATCHES "^/.*")
  SET(MYSQL_KEYRINGDIR ${INSTALL_MYSQLKEYRINGDIR} CACHE PATH "default MySQL keyring directory")
ELSE()
  SET(MYSQL_KEYRINGDIR "${DEFAULT_MYSQL_HOME}/${INSTALL_MYSQLKEYRINGDIR}" CACHE PATH "default MySQL keyring directory")
ENDIF()
SET(DEFAULT_CHARSET_HOME "${DEFAULT_MYSQL_HOME}")
SET(PLUGINDIR "${DEFAULT_MYSQL_HOME}/${INSTALL_PLUGINDIR}")
IF(SYSCONFDIR)
  SET(DEFAULT_SYSCONFDIR "${SYSCONFDIR}")
ENDIF()

IF(WIN32) # P_tmpdir is not defined on Windows as of VS2015.
  SET(TMPDIR "" # So we use empty path as default. In practice TMP/TEMP is used
    CACHE PATH
    "PATH to MySQL TMP dir")
ELSE()
  SET(TMPDIR "P_tmpdir"
    CACHE PATH
    "PATH to MySQL TMP dir. Defaults to the P_tmpdir macro in <stdio.h>")
ENDIF()
IF(TMPDIR STREQUAL "P_tmpdir")
  # Do not quote it, to refer to the P_tmpdir macro.
  SET(DEFAULT_TMPDIR "P_tmpdir")
ELSE()
  # Quote it, to make it a const char string.
  SET(DEFAULT_TMPDIR "\"${TMPDIR}\"")
ENDIF()

INCLUDE(cmake/boost.cmake)

IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
  OPTION(WITH_SYSTEMD "Enable installation of systemd support files" OFF)
  IF (WITH_SYSTEMD)
    INCLUDE(cmake/systemd.cmake)
  ENDIF()
ELSE()
  IF (WITH_SYSTEMD)
    MESSAGE(FATAL_ERROR "Installation of systemd support files not supported")
  ENDIF()
ENDIF()

# Run platform tests
INCLUDE(configure.cmake)

# Common defines and includes
ADD_DEFINITIONS(-DHAVE_CONFIG_H)
ADD_DEFINITIONS(-DHAVE_OPENSSL)            # TODO: remove #ifdef from C++ code
ADD_DEFINITIONS(-DRAPIDJSON_NO_SIZETYPEDEFINE)
ADD_DEFINITIONS(-D__STDC_LIMIT_MACROS)     # Enable C99 limit macros
ADD_DEFINITIONS(-D__STDC_FORMAT_MACROS)    # Enable C99 printf format macros
ADD_DEFINITIONS(-D_USE_MATH_DEFINES)       # Get access to M_PI, M_E, etc. in math.h

OPTION(ENABLE_EXPERIMENT_SYSVARS "Expose ussually hidden system variables to allow experiments" OFF)
IF(ENABLE_EXPERIMENT_SYSVARS)
  ADD_DEFINITIONS(-DENABLE_EXPERIMENT_SYSVARS)
ENDIF()


INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/include
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/libbinlogevents/include
  ${CMAKE_SOURCE_DIR}/libbinlogevents/export)

# Explicit list of all -DWITH_XXX=system possibilities
# *except* boost, which requires one specific version to be downloaded
SET(SYSTEM_LIBRARIES
  CURL
  EDITLINE
  ICU
  LIBEVENT
  LZ4
# LZMA     use bundled by default here
  PROTOBUF
  RE2
  SSL
  ZLIB
  )

OPTION(WITH_SYSTEM_LIBS
  "Use -DWITH_XXX=system for ${SYSTEM_LIBRARIES}" OFF)

GET_CMAKE_PROPERTY(CACHE_VARS CACHE_VARIABLES)
UNSET(COMMAND_LINE_VARS)
FOREACH(CACHE_VAR ${CACHE_VARS})
  GET_PROPERTY(CACHE_VAR_HELPSTRING CACHE ${CACHE_VAR} PROPERTY HELPSTRING)
  IF(CACHE_VAR_HELPSTRING STREQUAL
      "No help, variable specified on the command line."
      )
    LIST(APPEND COMMAND_LINE_VARS ${CACHE_VAR})
  ENDIF()
ENDFOREACH()

# Default to "system" for all SYSTEM_LIBRARIES
IF(WITH_SYSTEM_LIBS)
  FOREACH(SYSTEM_LIB ${SYSTEM_LIBRARIES})
    LIST(FIND COMMAND_LINE_VARS WITH_${SYSTEM_LIB} FOUNDIT)
    IF(FOUNDIT LESS 0)
      SET(WITH_${SYSTEM_LIB} "system")
      SET(WITH_${SYSTEM_LIB} "system" CACHE STRING
        "Use system rather than bundled" FORCE)
      MESSAGE(STATUS "Adding -DWITH_${SYSTEM_LIB}=${WITH_${SYSTEM_LIB}}")
    ELSE()
      MESSAGE(STATUS
        "Found WITH_${SYSTEM_LIB}=${WITH_${SYSTEM_LIB}} on command line")
    ENDIF()
  ENDFOREACH()
  # Don't redo the system analysis on subsequent runs of cmake
  UNSET(WITH_SYSTEM_LIBS)
  UNSET(WITH_SYSTEM_LIBS CACHE)
ENDIF()

# Add bundled or system zlib.
MYSQL_CHECK_ZLIB_WITH_COMPRESS()
IF(BUILD_BUNDLED_ZLIB)
  INCLUDE_DIRECTORIES(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/extra/zlib ${CMAKE_CURRENT_BINARY_DIR}/extra/zlib)
ENDIF()
# Add bundled wolfssl/wolfcrypt or system openssl.
MYSQL_CHECK_SSL()
MYSQL_CHECK_SSL_DLLS()

# Add system/bundled editline.
MYSQL_CHECK_EDITLINE()
# Add libevent
MYSQL_CHECK_LIBEVENT()
# Add lz4 library
MYSQL_CHECK_LZ4()
# Add lzma library
MYSQL_CHECK_LZMA()
# Add re2 library
MYSQL_CHECK_RE2()
# Add icu library
MYSQL_CHECK_ICU()
# Add protoc and libprotobuf
IF(NOT WITHOUT_SERVER)
  MYSQL_CHECK_PROTOBUF()
ENDIF()
# Try and set CURL_LIBRARY
MYSQL_CHECK_CURL()
# Add RapidJSON library.
# TODO: Support -DWITH_RAPIDJSON=system.
INCLUDE_DIRECTORIES(SYSTEM ${CMAKE_SOURCE_DIR}/extra/rapidjson/include)

IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
  # Check for pthread_setname_np
  CHECK_C_SOURCE_COMPILES("
  #include <pthread.h>

  int main(int ac, char **av)
  {
    const char *thread_name= 0;
    int ret = pthread_setname_np(pthread_self(), thread_name);
    return ret;
  }"
  HAVE_PTHREAD_SETNAME_NP)
ENDIF()

IF (BUILD_BUNDLED_LZMA)
  ADD_SUBDIRECTORY(extra/lzma)
ENDIF()

IF (BUILD_BUNDLED_RE2)
  ADD_SUBDIRECTORY(extra/re2)
ENDIF()

#
# Setup maintainer mode options by the end. Platform checks are
# not run with the warning options as to not perturb fragile checks
# (i.e. do not make warnings into errors).
#
# Why don't these flags affect the entire build?
# Because things may already have been included with ADD_SUBDIRECTORY
#
OPTION(MYSQL_MAINTAINER_MODE
       "MySQL maintainer-specific development environment" OFF)

INCLUDE(maintainer)

IF(WITH_UNIT_TESTS)
 ENABLE_TESTING()
ENDIF()

ADD_SUBDIRECTORY(include)
ADD_SUBDIRECTORY(dbug)
ADD_SUBDIRECTORY(strings)
ADD_SUBDIRECTORY(vio)
ADD_SUBDIRECTORY(mysys)
ADD_SUBDIRECTORY(mysys_ssl)
ADD_SUBDIRECTORY(extra/regex)
ADD_SUBDIRECTORY(libmysql)
ADD_SUBDIRECTORY(libbinlogevents)
ADD_SUBDIRECTORY(libbinlogstandalone)

IF(NOT WITHOUT_SERVER)
  SET (MYSQLD_STATIC_PLUGIN_LIBS "" CACHE INTERNAL "")
  # Add storage engines and plugins.
  CONFIGURE_PLUGINS()
ELSE()
  IF(EXISTS "${CMAKE_SOURCE_DIR}/storage/ndb")
    # We may still want Cluster client libraries, use -DWITH_NDBCLUSTER=1
    ADD_SUBDIRECTORY(storage/ndb)
  ENDIF()
ENDIF()

IF(NOT WITHOUT_SERVER)
  SET (MYSQLD_STATIC_COMPONENT_LIBS "" CACHE INTERNAL "")
  CONFIGURE_COMPONENTS()
ENDIF()

IF(WITH_UNIT_TESTS)
  ADD_SUBDIRECTORY(unittest)
  ADD_SUBDIRECTORY(unittest/examples)
  ADD_SUBDIRECTORY(unittest/mytap)
  ADD_SUBDIRECTORY(unittest/mytap/t)
ENDIF()

ADD_SUBDIRECTORY(client)
ADD_SUBDIRECTORY(utilities)
ADD_SUBDIRECTORY(share)
ADD_SUBDIRECTORY(libservices)

IF(UNIX)
  ADD_SUBDIRECTORY(man)
ENDIF()

IF(NOT WITHOUT_SERVER)
  ADD_SUBDIRECTORY(testclients)
  ADD_SUBDIRECTORY(sql)
ENDIF()

# scripts/mysql_config depends on client and server targets loaded above.
# It is referenced by some of the directories below, so we insert it here.
ADD_SUBDIRECTORY(scripts)

IF(NOT WITHOUT_SERVER)
  ADD_SUBDIRECTORY(mysql-test)
  ADD_SUBDIRECTORY(mysql-test/lib/My/SafeProcess)
  ADD_SUBDIRECTORY(support-files)
  IF(EXISTS ${CMAKE_SOURCE_DIR}/internal/CMakeLists.txt)
    ADD_SUBDIRECTORY(internal)
  ENDIF()
  ADD_SUBDIRECTORY(packaging/rpm-common)
  ADD_SUBDIRECTORY(packaging/rpm-oel)
  ADD_SUBDIRECTORY(packaging/rpm-fedora)
  ADD_SUBDIRECTORY(packaging/rpm-sles)
  ADD_SUBDIRECTORY(packaging/rpm-docker)
  ADD_SUBDIRECTORY(packaging/deb-in)
ENDIF()

INCLUDE(cmake/abi_check.cmake)
INCLUDE(cmake/tags.cmake)

CONFIGURE_FILE(config.h.cmake   ${CMAKE_BINARY_DIR}/include/my_config.h)
CONFIGURE_FILE(config.h.cmake   ${CMAKE_BINARY_DIR}/include/config.h)
CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/include/mysql_version.h.in
               ${CMAKE_BINARY_DIR}/include/mysql_version.h )
CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/sql/sql_builtin.cc.in
    ${CMAKE_BINARY_DIR}/sql/sql_builtin.cc)

GET_PROPERTY(CWD_DEFINITIONS DIRECTORY PROPERTY COMPILE_DEFINITIONS)
CONFIGURE_FILE(
    ${CMAKE_SOURCE_DIR}/cmake/info_macros.cmake.in
    ${CMAKE_BINARY_DIR}/info_macros.cmake @ONLY)

SET(KNOWN_CONVENIENCE_LIBRARIES "" CACHE INTERNAL "")
# Add to the 'make clean' target.
# With this you need to do 'cmake .' before doing 'make' again.
SET_PROPERTY(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
  "${CMAKE_BINARY_DIR}/archive_output_directory"
  "${CMAKE_BINARY_DIR}/library_output_directory"
  )

# Handle the "INFO_*" files.
INCLUDE(${CMAKE_BINARY_DIR}/info_macros.cmake)
# Source: This can be done during the cmake phase, all information is
# available, but should be repeated on each "make" just in case someone
# does "cmake ; make ; git pull ; make".
CREATE_INFO_SRC(${CMAKE_BINARY_DIR}/Docs)
ADD_CUSTOM_TARGET(INFO_SRC ALL
  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/info_src.cmake
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
# Build flags: This must be postponed to the make phase.
ADD_CUSTOM_TARGET(INFO_BIN ALL
  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/info_bin.cmake
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Packaging
IF(WIN32)
  OPTION(BUNDLE_RUNTIME_LIBRARIES "INSTALL runtime libraries" OFF)
  IF(BUNDLE_RUNTIME_LIBRARIES)
    SET(RUNTIME_LIBRARIES
      msvcp140
      ucrtbase
      vcruntime140
      )
    FOREACH(LIB ${RUNTIME_LIBRARIES})
      SET(LIB_PATH "C:/Windows/System32/${LIB}.dll")
      SET(LIB_D_PATH "C:/Windows/System32/${LIB}d.dll")
      FOREACH(LIBRARY_PATH ${LIB_PATH} ${LIB_D_PATH})
        IF(EXISTS "${LIBRARY_PATH}")
          MESSAGE(STATUS "INSTALL ${LIBRARY_PATH} to ${INSTALL_BINDIR}")
          INSTALL(FILES
            "${LIBRARY_PATH}" DESTINATION "${INSTALL_BINDIR}"
            COMPONENT SharedLibraries)
        ELSE()
          MESSAGE(STATUS "Could not find ${LIBRARY_PATH}")
        ENDIF()
      ENDFOREACH()
    ENDFOREACH()
  ENDIF()
ENDIF()

IF(WIN32)
  SET(CPACK_GENERATOR "ZIP")
ELSE()
  SET(CPACK_GENERATOR "TGZ")
ENDIF() 
ADD_SUBDIRECTORY(packaging/WiX)
ADD_SUBDIRECTORY(packaging/solaris)

IF(UNIX)
  INSTALL(FILES Docs/mysql.info DESTINATION ${INSTALL_INFODIR} OPTIONAL COMPONENT Info)
ENDIF()
#
# RPM installs documentation directly from the source tree
#
IF(NOT INSTALL_LAYOUT MATCHES "RPM")
  INSTALL(FILES README LICENSE DESTINATION ${INSTALL_DOCREADMEDIR} COMPONENT Readme)
  INSTALL(FILES ${CMAKE_BINARY_DIR}/Docs/INFO_SRC ${CMAKE_BINARY_DIR}/Docs/INFO_BIN DESTINATION ${INSTALL_DOCDIR})

  # MYSQL_DOCS_LOCATON is used in "make dist", points to the documentation directory
  SET(MYSQL_DOCS_LOCATION "" CACHE PATH "Location from where documentation is copied")
  MARK_AS_ADVANCED(MYSQL_DOCS_LOCATION)
  INSTALL(DIRECTORY Docs/ DESTINATION ${INSTALL_DOCDIR}
    COMPONENT Documentation
    PATTERN "Makefile.*" EXCLUDE
    PATTERN "glibc*" EXCLUDE
    PATTERN "linuxthreads.txt" EXCLUDE
    PATTERN "myisam.txt" EXCLUDE
    PATTERN "mysql.info" EXCLUDE
    PATTERN "sp-imp-spec.txt" EXCLUDE
    PATTERN "README.build" EXCLUDE
  )
ENDIF()

IF(UNIX)
  EXECUTE_PROCESS(
    COMMAND ${CMAKE_COMMAND} -E make_directory library_output_directory
    COMMAND ${CMAKE_COMMAND} -E make_directory plugin_output_directory
    COMMAND ${CMAKE_COMMAND} -E make_directory runtime_output_directory
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )
ENDIF()

# Pre-create directories for library_output_directory and
# plugin_output_directory, so create_symlink does not fail in ssl.cmake
# Do not pre-create runtime_output_directory/Debug or
# runtime_output_directory/RelWithDebInfo, mtr gets confused, and must be
# run with --vs-config=Debug or --vs-config=RelWithDebInfo in order to work.
IF(APPLE AND NOT BUILD_IS_SINGLE_CONFIG)
  FOREACH(dir
      library_output_directory
      plugin_output_directory
      )
    EXECUTE_PROCESS(
      COMMAND ${CMAKE_COMMAND} -E make_directory ${dir}/Debug
      COMMAND ${CMAKE_COMMAND} -E make_directory ${dir}/RelWithDebInfo
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      )
  ENDFOREACH()
ENDIF()

IF(UNIX)
  EXECUTE_PROCESS(
    COMMAND ${CMAKE_COMMAND} -E create_symlink
     ./runtime_output_directory bin
    COMMAND ${CMAKE_COMMAND} -E create_symlink
     ./library_output_directory lib
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )
ENDIF()

SET(DOXYGEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/doxygen")
FIND_PACKAGE(Doxygen)
IF(DOXYGEN_FOUND)
  CONFIGURE_FILE(Doxyfile.in Doxyfile @ONLY)
  ADD_CUSTOM_TARGET(
    doxygen
    COMMAND ${CMAKE_COMMAND}
    -DDOXYGEN_EXECUTABLE=${DOXYGEN_EXECUTABLE}
    -DDOXYFILE=${CMAKE_BINARY_DIR}/Doxyfile
    -DERROR_FILE=${CMAKE_BINARY_DIR}/doxyerror.log
    -DOUTPUT_FILE=${CMAKE_BINARY_DIR}/doxyoutput.log
    -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
    -DTOFIX_FILE=${CMAKE_BINARY_DIR}/tofix-all.log
    -DREGRESSION_FILE=${CMAKE_BINARY_DIR}/tofix-regressions.log
    -DIGNORE_FILE=${CMAKE_SOURCE_DIR}/Doxyfile-ignored
    -P ${CMAKE_SOURCE_DIR}/run_doxygen.cmake
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating Doxygen documentation" VERBATIM
    )
  SET_PROPERTY(TARGET doxygen PROPERTY EXCLUDE_FROM_ALL TRUE)
  IF(WIN32)
    SET_PROPERTY(TARGET doxygen PROPERTY EXCLUDE_FROM_DEFAULT_BUILD TRUE)
  ENDIF()
ENDIF(DOXYGEN_FOUND)

INCLUDE(CPack)

# C compiler flags consist of:
# CPPFLAGS        Taken from environment, see above.
# ADD_DEFINITIONS In each individual CMakeLists.txt
# CMAKE_C_FLAGS   From command line.
#                 We extend these in maintainer.cmake
# ENV{CFLAGS}     From environment, but environment is ignored if
#                 CMAKE_C_FLAGS is also given on command line
# CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE}
#                 We extend these in compiler_options.cmake
#
# Note that CMakeCache.txt contains cmake builtins for these variables,
# *not* the values that will actually be used:


IF(BUILD_IS_SINGLE_CONFIG)
  MESSAGE(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
ENDIF()
GET_PROPERTY(cwd_definitions DIRECTORY PROPERTY COMPILE_DEFINITIONS)
MESSAGE(STATUS "COMPILE_DEFINITIONS: ${cwd_definitions}")
MESSAGE(STATUS "CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")
MESSAGE(STATUS "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
MESSAGE(STATUS "CMAKE_C_LINK_FLAGS: ${CMAKE_C_LINK_FLAGS}")
MESSAGE(STATUS "CMAKE_CXX_LINK_FLAGS: ${CMAKE_CXX_LINK_FLAGS}")
IF(CMAKE_BUILD_TYPE AND BUILD_IS_SINGLE_CONFIG)
  STRING(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKEBT)
  MESSAGE(STATUS "CMAKE_C_FLAGS_${CMAKEBT}: ${CMAKE_C_FLAGS_${CMAKEBT}}")
  MESSAGE(STATUS "CMAKE_CXX_FLAGS_${CMAKEBT}: ${CMAKE_CXX_FLAGS_${CMAKEBT}}")
ENDIF()
IF(NOT BUILD_IS_SINGLE_CONFIG)
  MESSAGE(STATUS "CMAKE_C_FLAGS_DEBUG: ${CMAKE_C_FLAGS_DEBUG}")
  MESSAGE(STATUS "CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")
  MESSAGE(STATUS
    "CMAKE_C_FLAGS_RELWITHDEBINFO: ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
  MESSAGE(STATUS
    "CMAKE_CXX_FLAGS_RELWITHDEBINFO: ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
ENDIF()

