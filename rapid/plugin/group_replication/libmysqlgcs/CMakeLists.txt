# Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

PROJECT("mysql-gcs")

# Set up SSL definitions if requested.
ADD_DEFINITIONS(-DXCOM_HAVE_OPENSSL)

IF(WIN32)
  # CMake 2.8.12 is needed for Visual Studio 2013
  CMAKE_MINIMUM_REQUIRED(VERSION 2.8.12)
ELSE()
  # There's a bug in unzipping prior to 2.8.2
  CMAKE_MINIMUM_REQUIRED(VERSION 2.8.2)
ENDIF()

#
# CMake warnings suppressions
#

# We use the LOCATION target property (CMP0026)
# and get_target_property() for non-existent targets (CMP0045)
# and INSTALL_NAME_DIR (CMP0042)
# and PROJECT for subprojects such as xcom
IF(CMAKE_VERSION VERSION_EQUAL "3.0.0" OR
   CMAKE_VERSION VERSION_GREATER "3.0.0")
 CMAKE_POLICY(SET CMP0026 OLD)
 CMAKE_POLICY(SET CMP0045 OLD)
 CMAKE_POLICY(SET CMP0048 NEW)
ENDIF()


SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# Custom macros and functions
INCLUDE(MysqlGcsUtils)
INCLUDE(libutils)

#
# Configure platform specifics
#
INCLUDE(${PROJECT_SOURCE_DIR}/cmake/platform.cmake)

#
# VARIABLES
#
IF(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR
   CMAKE_CXX_COMPILER_ID MATCHES "Clang")

  SET(SHARED_FLAGS "-fPIC -Wall -Wextra")
  # Disable -Werror derived from parent source temporarily.
  # It will be removed after all warnings are fixed.
  SET(SHARED_FLAGS "${SHARED_FLAGS} -Wno-error")
  SET(SHARED_FLAGS "${SHARED_FLAGS} -Wformat-security -Wwrite-strings")
  SET(SHARED_FLAGS "${SHARED_FLAGS} -Wmissing-format-attribute")

  # Avoid the warnings that implicit conversion between longlong and size_t
  IF(SIZEOF_VOIDP EQUAL 8)
    SET(SHARED_FLAGS "${SHARED_FLAGS} -Wconversion")
  ENDIF()

  # Due to a bug in GCC, the pragma is ignored by the pre-processor
  # thus making the compilation fail when the xcom_vp.h is imported
  # by gcs_xcom_communication_interface.cc. For that reason, we use
  # -Wno-undef here.
  SET(SHARED_FLAGS "${SHARED_FLAGS} -Wno-undef")

  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Weffc++ -Woverloaded-virtual")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wsign-promo")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SHARED_FLAGS}")

  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SHARED_FLAGS}")

  IF(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wlogical-op")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wlogical-op")
  ENDIF()
ELSEIF(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  # Disable the warning that conversion from uint64_t to size_t on 32bit OS
  IF(SIZEOF_VOIDP EQUAL 4)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4244")
  ENDIF()
ENDIF()

IF(WITH_TRACE_OUTPUT)
  ADD_DEFINITIONS(-DWITH_LOG_DEBUG -DWITH_LOG_TRACE)
ELSEIF(WITH_DEBUG_OUTPUT)
  ADD_DEFINITIONS(-DWITH_LOG_DEBUG)
ENDIF()

MySQLGetVersionValue("MYSQL_GCS_VERSION_MAJOR" VERSION_MAJOR)
MySQLGetVersionValue("MYSQL_GCS_VERSION_MINOR" VERSION_MINOR)
MySQLGetVersionValue("MYSQL_GCS_VERSION_PATCH" VERSION_PATCH)
MySQLGetVersionValue("MYSQL_GCS_VERSION_EXTRA" VERSION_EXTRA)

SET(PROJECT_VERSION
    "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}-${VERSION_EXTRA}")

# Install global vars
SET(INSTALL_HEADERS "include")
SET(INSTALL_BIN     "bin")
SET(INSTALL_LIB     "lib")
SET(INSTALL_TESTS   "tests")
SET(INSTALL_DOCS    "docs")
SET(INSTALL_ROOT    ".")

SET(COMP_BIN        "bin-${CMAKE_SYSTEM}")
SET(COMP_TESTS      "tests-${CMAKE_SYSTEM}")
SET(COMP_SRC        "src")
SET(COMP_DOCS       "docs")
SET(COMP_SRC_GR     "libmysqlgcs")

#
# Options
#

# opt-in flags
OPTION(WITH_TESTS
       "Configure MySQL GCS with all tests. This sets WITH_UNIT_TESTS and WITH_SMOKE_TESTS implicitly." OFF)
OPTION(WITH_UNIT_TESTS "Configure MySQL GCS with unit tests" OFF)
OPTION(WITH_SMOKE_TESTS "Configure MySQL GCS with smoke tests" OFF)
OPTION(WITH_FUZZER_TESTS "Configure MySQL GCS with protocol fuzzing regression tests" OFF)
OPTION(WITH_GCS_EXAMPLES "Configure MySQL with all examples." OFF)
OPTION(WITH_DOCS "Configure MySQL GCS documentation" OFF)
OPTION(WITH_LANGUAGE_WRAPPERS
       "Compile MySQL GCS with language wrappers included" OFF)
OPTION(WITH_PYTHON_WRAPPER
       "Compile MySQL GCS with the Python wrapper included" OFF)
OPTION(WITH_JAVA_WRAPPER
       "Compile MySQL GCS with the Java wrapper included" OFF)
OPTION(GCS_ENABLE_GCOV "Enable gcov (debug, Linux builds only)" OFF)
OPTION(SKIP_INSTALL_MYSQL_GCS "Disables installation targets" OFF)
OPTION(SKIP_GCS_SHARED_LIB "Skips building the shared library." OFF)

# opt-out flags
OPTION(WITH_DOCS_ONLY
       "Configure documentation only. This sets WITH_DOCS implicitly." OFF)

# Turn on sub tests if the general tests flag is set
IF(WITH_TESTS)
  SET(WITH_UNIT_TESTS ON)
  SET(WITH_SMOKE_TESTS ON)
  SET(WITH_FUZZER_TESTS ON)
ENDIF()

IF(WITH_UNIT_TESTS)
  ADD_DEFINITIONS(-DWITH_UNIT_TESTS)
ENDIF()

# Turn on WITH_DOCS if WITH_DOCS_ONLY was supplied
IF(WITH_DOCS_ONLY)
  SET(WITH_DOCS ON)
ENDIF()

# ##############################################
# Configure components
# ##############################################

# Include the documentation configuration
IF (WITH_DOCS)
  INCLUDE(GcsDocs)
  IF(WITH_DOCS_ONLY)
    INCLUDE(GcsPackage)
    RETURN()
  ENDIF()
ENDIF()

#
#  This part contains build specific configuration
#

#
# Includes
#
INCLUDE(CheckIncludeFiles)
INCLUDE(CheckLibraryExists)
INCLUDE(CheckCXXSourceRuns)

#
# Configure threading
#
FIND_PACKAGE(Threads REQUIRED)

#
# setup the unit tests framework
#
IF (WITH_TESTS OR WITH_UNIT_TESTS OR WITH_SMOKE_TESTS OR WITH_FUZZER_TESTS)
  # Check to see if we are using LLVM's libc++ rather than e.g. libstd++
  # Can then check HAVE_LLVM_LIBCPP later without including e.g. ciso646.
  CHECK_CXX_SOURCE_RUNS("
  #include <ciso646>
  int main()
  {
  #ifdef _LIBCPP_VERSION
    return 0;
  #else
    return 1;
  #endif
  }" HAVE_LLVM_LIBCPP)

  # Visual Studio 11 needs this extra flag in order to compile gmock.
  IF(WIN32)
    ADD_DEFINITIONS(/D _VARIADIC_MAX=10)
  ENDIF()
  # libc++ doesn't have tuple in tr1
  IF(HAVE_LLVM_LIBCPP)
    ADD_DEFINITIONS(-DGTEST_USE_OWN_TR1_TUPLE=1)
  ENDIF()

  ENABLE_TESTING()
  INCLUDE(${PROJECT_SOURCE_DIR}/cmake/gmock.cmake)

  IF(NOT GMOCK_FOUND)
    MESSAGE(FATAL_ERROR "Unable to locate GMock framework!")
        RETURN()
  ENDIF()
ENDIF()

#
# DCov Configuration
#
# If this is built embedded into the MySQL server, we should
# override the gcov settings here, since MySQL build will
# do it for us
#
IF(GCS_ENABLE_GCOV AND NOT WIN32 AND NOT APPLE)
  FIND_PROGRAM(LCOV_EXECUTABLE NAMES lcov)
  FIND_PROGRAM(GENHTML_EXECUTABLE NAMES genhtml)

  IF(NOT LCOV_EXECUTABLE)
    MESSAGE(FATAL_ERROR "lcov not found. Please install lcov in order to have coverage values")
    RETURN()
  ENDIF(NOT LCOV_EXECUTABLE)

  IF(NOT GENHTML_EXECUTABLE)
    MESSAGE(FATAL_ERROR "genhtml not found. Please install genhtml in order to have coverage values")
    RETURN()
  ENDIF(NOT GENHTML_EXECUTABLE)

  SET(GCC_DEBUG_FLAGS "-g -Wall")
  SET(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} ${GCC_DEBUG_FLAGS} -fprofile-arcs -ftest-coverage"
      CACHE STRING "Flags used by the C++ compiler during coverage builds."
      FORCE)
  SET(CMAKE_C_FLAGS
      "${CMAKE_C_FLAGS} ${GCC_DEBUG_FLAGS} -fprofile-arcs -ftest-coverage"
      CACHE STRING "Flags used by the C compiler during coverage builds."
      FORCE)
  SET(CMAKE_EXE_LINKER_FLAGS
      "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage -lgcov"
      CACHE STRING "Flags used for linking binaries during coverage builds."
      FORCE)
  SET(CMAKE_SHARED_LINKER_FLAGS
      "${CMAKE_SHARED_LINKER_FLAGS} -fprofile-arcs -ftest-coverage -lgcov"
      CACHE STRING "Flags used by the shared libraries linker during coverage builds."
      FORCE)
  MARK_AS_ADVANCED(CMAKE_CXX_FLAGS
      CMAKE_C_FLAGS
      CMAKE_EXE_LINKER_FLAGS
      CMAKE_SHARED_LINKER_FLAGS)

  ADD_CUSTOM_TARGET(coverage
    COMMAND ${LCOV_EXECUTABLE} --capture --directory ${PROJECT_BINARY_DIR}
    --output-file ${PROJECT_BINARY_DIR}/coverage.info
    COMMAND ${LCOV_EXECUTABLE} --remove ${PROJECT_BINARY_DIR}/coverage.info
    '/usr/include/*' 'src/tests/*' '*/googletest-release-1.8.0/*'
    --output-file ${PROJECT_BINARY_DIR}/coverage.info
    COMMAND ${GENHTML_EXECUTABLE} ${PROJECT_BINARY_DIR}/coverage.info
    --output-directory ${PROJECT_BINARY_DIR}/Coverage_Report
    COMMENT "Generated Coverage Report")
ENDIF()

#
# Set include directories
#

# Include header files
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include
                    ${PROJECT_SOURCE_DIR}/include/mysql
                    ${PROJECT_SOURCE_DIR}/include/mysql/gcs
                    ${PROJECT_SOURCE_DIR}/include/mysql/gcs/xplatform
                    ${PROJECT_BINARY_DIR}/include)

###                                        ########################
### Configure subdirectories  (ESSENTIAL)  ########################
###                                        ########################

#
# First configure 3rd party libraries
#
ADD_SUBDIRECTORY(src/extra)

# configure the interface and bindings
ADD_SUBDIRECTORY(src)

###                                  ##############################
### Configure subdirectories (EXTRA) ##############################
###                                  ##############################

#
# configure the examples
#

IF (WITH_GCS_EXAMPLES)
  IF (EXISTS ${PROJECT_SOURCE_DIR}/src/examples)
    ADD_SUBDIRECTORY(src/examples)
  ELSE()
    MESSAGE(WARNING "Unable to find examples sources! Skipping examples.")
  ENDIF()
ENDIF()

#
# configure the tests
#
IF (WITH_TESTS OR WITH_UNIT_TESTS OR WITH_SMOKE_TESTS OR WITH_FUZZER_TESTS)
  IF (EXISTS ${PROJECT_SOURCE_DIR}/src/tests)
    ADD_SUBDIRECTORY(src/tests)
  ELSE()
    MESSAGE(WARNING "Unable to find tests sources! Skipping tests.")
  ENDIF()
ENDIF()

#
# configure the language wrappers
#
IF (WITH_LANGUAGE_WRAPPERS OR WITH_PYTHON_WRAPPER OR WITH_JAVA_WRAPPER)
  IF (EXISTS ${PROJECT_SOURCE_DIR}/src/wrappers)
    ADD_SUBDIRECTORY(src/wrappers)
  ELSE()
    MESSAGE(WARNING "Unable to find language wrappers sources! Skipping language wrappers.")
  ENDIF()
ENDIF()

CONFIGURE_FILE(${PROJECT_SOURCE_DIR}/mysql_gcs.h.cmake
               ${PROJECT_BINARY_DIR}/include/mysql/gcs/mysql_gcs.h)

# Configure packaging
IF(NOT SKIP_INSTALL_MYSQL_GCS)
  INCLUDE(GcsPackage)
ENDIF()

# Copy security information
IF(WITH_TESTS OR WITH_SMOKE_TESTS OR WITH_UNIT_TESTS)
  file(COPY ${PROJECT_SOURCE_DIR}/security
       DESTINATION ${PROJECT_BINARY_DIR})
ENDIF()
