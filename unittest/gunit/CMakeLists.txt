# Copyright (C) 2009 Sun Microsystems,Inc
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

# In case we need to override the choice of compiler, see below.
INCLUDE(CMakeForceCompiler)

# Builtin GTest support is only available with CMake 2.8. For 2.6, we use 
# own copy of the FindGTest.cmake module, located in current source directory.
IF("${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}" LESS "2.8")
  SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}")  
ELSE()
  SET(HAVE_CMAKE_2_8 1)
ENDIF() 

IF(CMAKE_COMPILER_IS_GNUCXX)
  # Remove -fno-implicit-templates, gunit sources cannot be compiled with it.
  STRING(REPLACE "-fno-implicit-templates" "" CMAKE_CXX_FLAGS
    ${CMAKE_CXX_FLAGS})

  # MySQL is often compiled/linked with gcc rather than g++
  # (to avoid dependencies on libstdc++.so)
  # This does not work for googletest binaries.
  # If we are building with ccache, we need to change arg1.
  IF(CMAKE_CXX_COMPILER_ARG1)
    IF(${CMAKE_CXX_COMPILER_ARG1} MATCHES "gcc")
      SET(CMAKE_CXX_COMPILER_ARG1 "g++")
    ENDIF()
  # If we are building with gcc, substitute gcc with g++
  ELSEIF(${CMAKE_CXX_COMPILER} MATCHES ".*/gcc")
    STRING(REGEX REPLACE "/gcc$" "/g++" PATH_TO_GCC ${CMAKE_CXX_COMPILER})
    CMAKE_FORCE_CXX_COMPILER(${PATH_TO_GCC} "GNU g++")
  ENDIF()
ENDIF()

IF(MSVC)
  # Restore exception handling flags to avoid tons of warnings when using STL.
  ADD_DEFINITIONS(/EHcs)
ENDIF()

# Where to download and build gtest if not found.
IF(NOT DOWNLOAD_ROOT)
  SET(DOWNLOAD_ROOT ${CMAKE_SOURCE_DIR}/source_downloads)
ENDIF()
IF(NOT EXISTS DOWNLOAD_ROOT)
  MAKE_DIRECTORY(${DOWNLOAD_ROOT})
ENDIF()
SET(GTEST_PACKAGE_NAME "gtest-1.5.0")
SET(GTEST_SOURCE_DIR ${DOWNLOAD_ROOT}/${GTEST_PACKAGE_NAME})


# Hint for FindGTest: With MSVC, we compile with static CRT (compile flags
# /MT or /MTd) and need compatible gtest.lib
SET(GTEST_MSVC_SEARCH MT) 

# Hint for where to look: pushbuild uses GTEST_ROOT env.var.
IF (DEFINED ENV{GTEST_PREFIX} AND NOT DEFINED ENV{GTEST_ROOT})
  SET(ENV{GTEST_ROOT} $ENV{GTEST_PREFIX})
ENDIF()

# We may have downloaded gtest already, building in a different directory.
IF(NOT DEFINED ENV{GTEST_ROOT} AND EXISTS ${GTEST_SOURCE_DIR})
  MESSAGE(STATUS "GTEST_SOURCE_DIR:${GTEST_SOURCE_DIR}")
  SET(GTEST_DOWNLOADED 1 CACHE INTERNAL "")
  SET(GTEST_FOUND 1 CACHE INTERNAL "")
ENDIF()


IF(NOT GTEST_DOWNLOADED)
  # Hint for find_library(): reverse list ".so;.a" to prefer static linking.
  LIST(REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)

  # Now look for gtest.
  FIND_PACKAGE(GTest QUIET)

  # Revert suffix list back to what it was before.
  LIST(REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
ENDIF()


OPTION(ENABLE_DOWNLOADS
  "Download and build 3rd party source code components, e.g google test"
  OFF)

# While experimenting, use local URL rather than google.
SET(GTEST_TARBALL "${GTEST_PACKAGE_NAME}.tar.gz")
SET(GTEST_DOWNLOAD_URL 
  "http://googletest.googlecode.com/files/${GTEST_TARBALL}"
# "https://clustra.norway.sun.com/~td136807/${GTEST_TARBALL}"
  )
  
IF(NOT GTEST_FOUND)
  IF(NOT ENABLE_DOWNLOADS)
    # Give one-time warning
    IF(NOT ONETIME_GTEST_WARNING)
      MESSAGE(STATUS 
        "Googletest was not found. gtest-based unit tests will be disabled. "
        "You can run cmake . -DENABLE_DOWNLOADS=1 to automatically download and "
        "build required components from source.")
      SET(ONETIME_GTEST_WARNING 1 CACHE INTERNAL "")
    ENDIF()
    RETURN()
  ENDIF()
  
  # Download gtest source
  IF(NOT EXISTS ${GTEST_SOURCE_DIR})
    IF(NOT EXISTS ${DOWNLOAD_ROOT}/${GTEST_TARBALL})
      # Download the tarball
      IF(NOT HAVE_CMAKE_2_8)
        # In versions earlier than 2.8, try wget for downloading
        FIND_PROGRAM(WGET_EXECUTABLE wget)
        MARK_AS_ADVANCED(WGET_EXECUTABLE)
        IF(WGET_EXECUTABLE)
          EXECUTE_PROCESS(COMMAND  ${WGET_EXECUTABLE} -T 30 ${GTEST_DOWNLOAD_URL}
            WORKING_DIRECTORY ${DOWNLOAD_ROOT} RESULT_VARIABLE ERR)
          IF(ERR EQUAL 0)
            SET(DOWNLOAD_SUCCEEDED 1)
          ENDIF()
        ENDIF()
      ELSE()
        # Use CMake builtin download capabilities
        FILE(DOWNLOAD ${GTEST_DOWNLOAD_URL}
          ${DOWNLOAD_ROOT}/${GTEST_TARBALL} 30 STATUS ERR)
        IF(ERR EQUAL 0)
          SET(DOWNLOAD_SUCCEEDED 1)
        ENDIF()
      ENDIF()
      
      IF (DOWNLOAD_SUCCEEDED)
        MESSAGE(STATUS 
          "Successfully downloaded ${GTEST_DOWNLOAD_URL} to ${DOWNLOAD_ROOT}")
      ELSE()
        MESSAGE(STATUS 
          "To enable google test, please download ${GTEST_DOWNLOAD_URL} "
          "to the directory ${DOWNLOAD_ROOT}")
        RETURN()
      ENDIF()
    ENDIF()
    
    # Unpack tarball
    EXECUTE_PROCESS(
      COMMAND ${CMAKE_COMMAND} -E tar xfz  "${DOWNLOAD_ROOT}/${GTEST_TARBALL}"
        WORKING_DIRECTORY "${DOWNLOAD_ROOT}"
        OUTPUT_QUIET
        ERROR_QUIET
      )
  ENDIF()
  SET(GTEST_DOWNLOADED 1 CACHE INTERNAL "")
  SET(GTEST_FOUND 1 CACHE INTERNAL "")
ENDIF()

IF(NOT GTEST_FOUND)
  RETURN()
ENDIF()

IF(GTEST_DOWNLOADED)
  # Build gtest library
  INCLUDE_DIRECTORIES(
    ${GTEST_SOURCE_DIR} 
    ${GTEST_SOURCE_DIR}/include
  )
  ADD_LIBRARY(gtest STATIC ${GTEST_SOURCE_DIR}/src/gtest-all.cc)
  
  # Set CMake variables to make FindPackage(GTest) happy next time.
  SET(GTEST_FOUND 1 CACHE INTERNAL "")
  SET(GTEST_LIBRARY gtest CACHE INTERNAL "")
  SET(GTEST_LIBRARIES gtest CACHE INTERNAL "")
  SET(GTEST_MAIN_LIBRARY no_gtest_main_library CACHE INTERNAL "")
  SET(GTEST_INCLUDE_DIRS ${GTEST_SOURCE_DIR}/include CACHE INTERNAL "")
  SET(GTEST_INCLUDE_DIR "${GTEST_SOURCE_DIR}/include" CACHE INTERNAL "")
ENDIF()


INCLUDE_DIRECTORIES(
  ${GTEST_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/regex
  ${CMAKE_SOURCE_DIR}/sql
  ${CMAKE_SOURCE_DIR}/unittest/mytap
)

# main-wrapper library (with tap-compatible option).
ADD_LIBRARY(gunit STATIC
  gunit_test_main.cc tap_event_listener.cc thread_utils.cc)
TARGET_LINK_LIBRARIES(gunit mysys mytap dbug strings ${GTEST_LIBRARIES})
MESSAGE(STATUS "GTEST_LIBRARIES:${GTEST_LIBRARIES}")

# Add some defines.
ADD_DEFINITIONS(-DMYSQL_SERVER)
# We use -fno-rtti with gcc, so disable RTTI in gtest as well.
ADD_DEFINITIONS(-DGTEST_HAS_RTTI=0)
# /opt/studio12/SUNWspro/bin/CC complains about
#   iterator_traits is not a member of std
IF (CMAKE_CXX_COMPILER_ID STREQUAL "SunPro")
  ADD_DEFINITIONS("-library=stlport4")
ENDIF()

# Add tests (link them with gunit library) 
SET(TESTS
  bounded_queue
  dbug
  mdl
  mdl_mytap
  my_decimal
  my_regex
  sql_list
  thread_utils
  )

FOREACH(test ${TESTS})
  ADD_EXECUTABLE(${test}-t ${test}-t.cc)
  TARGET_LINK_LIBRARIES(${test}-t gunit sqlgunitlib strings dbug regex)
  IF (CMAKE_CXX_COMPILER_ID STREQUAL "SunPro")
    SET_TARGET_PROPERTIES(${test}-t  PROPERTIES LINK_FLAGS "-library=stlport4")
  ENDIF()
  ADD_TEST(${test} ${test}-t)
ENDFOREACH()
