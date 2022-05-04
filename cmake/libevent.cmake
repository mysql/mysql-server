# Copyright (c) 2011, 2022, Oracle and/or its affiliates.
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
#
# Usage:
#  cmake -DWITH_LIBEVENT="bundled"|"system"
#
# Default is "bundled"

# Router needs at least:
SET(MIN_LIBEVENT_VERSION_REQUIRED "2.1")

FUNCTION(WARN_MISSING_SYSTEM_LIBEVENT OUTPUT_WARNING)
  IF(NOT LIBEVENT_FOUND AND WITH_LIBEVENT STREQUAL "system")
    MESSAGE(WARNING "Cannot find LIBEVENT development libraries. "
      "You need to install the required packages:\n"
      "  Debian/Ubuntu:              apt install libevent-dev\n"
      "  RedHat/Fedora/Oracle Linux: yum install libevent-devel\n"
      "  SuSE:                       zypper install libevent-devel\n"
      )
    SET(${OUTPUT_WARNING} 1 PARENT_SCOPE)
  ENDIF()
ENDFUNCTION()

MACRO(RESET_LIBEVENT_VARIABLES)
  UNSET(LIBEVENT_CORE)
  UNSET(LIBEVENT_CORE CACHE)
  UNSET(LIBEVENT_EXTRA)
  UNSET(LIBEVENT_EXTRA CACHE)
  UNSET(LIBEVENT_INCLUDE_DIRECTORY)
  UNSET(LIBEVENT_INCLUDE_DIRECTORY CACHE)
  UNSET(LIBEVENT_OPENSSL)
  UNSET(LIBEVENT_OPENSSL CACHE)
  UNSET(LIBEVENT_PTHREADS)
  UNSET(LIBEVENT_PTHREADS CACHE)
ENDMACRO()

MACRO(FIND_LIBEVENT_VERSION)
  SET(TEST_SRC
    "#include <event.h>
     #include <stdio.h>
    int main()
    {
      fprintf(stdout, \"%s\", LIBEVENT_VERSION);
    }
    "
    )
  FILE(WRITE
    "${CMAKE_BINARY_DIR}/find_libevent_version.c"
    "${TEST_SRC}"
    )
  TRY_RUN(TEST_RUN_RESULT COMPILE_TEST_RESULT
    ${CMAKE_BINARY_DIR}
    "${CMAKE_BINARY_DIR}/find_libevent_version.c"
    CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${LIBEVENT_INCLUDE_DIRS}"
    COMPILE_OUTPUT_VARIABLE OUTPUT
    RUN_OUTPUT_VARIABLE RUN_OUTPUT
    )
  # MESSAGE(STATUS "TRY_EVENT TEST_RUN_RESULT is ${TEST_RUN_RESULT}")
  # MESSAGE(STATUS "TRY_EVENT COMPILE_TEST_RESULT is ${COMPILE_TEST_RESULT}")
  # MESSAGE(STATUS "TRY_EVENT COMPILE_OUTPUT_VARIABLE is ${OUTPUT}")
  # MESSAGE(STATUS "TRY_EVENT RUN_OUTPUT_VARIABLE is ${RUN_OUTPUT}")

  IF(COMPILE_TEST_RESULT)
    SET(LIBEVENT_VERSION_STRING "${RUN_OUTPUT}")
    STRING(REGEX REPLACE
      "([.-0-9]+).*" "\\1" LIBEVENT_VERSION "${LIBEVENT_VERSION_STRING}")
  ELSE()
    MESSAGE(WARNING "Could not determine LIBEVENT_VERSION")
  ENDIF()

  MESSAGE(STATUS "LIBEVENT_VERSION (${WITH_LIBEVENT}) ${LIBEVENT_VERSION}")
  MESSAGE(STATUS "LIBEVENT_INCLUDE_DIRS ${LIBEVENT_INCLUDE_DIRS}")
  MESSAGE(STATUS "LIBEVENT_LIBRARIES ${LIBEVENT_LIBRARIES}")
ENDMACRO()

MACRO (FIND_SYSTEM_LIBEVENT)
  IF (NOT LIBEVENT_INCLUDE_PATH)
    SET(LIBEVENT_INCLUDE_PATH /usr/local/include /opt/local/include)
  ENDIF()

  FIND_PATH(LIBEVENT_INCLUDE_DIRECTORY event.h PATHS ${LIBEVENT_INCLUDE_PATH})

  IF (NOT LIBEVENT_LIB_PATHS)
    SET(LIBEVENT_LIB_PATHS /usr/local/lib /opt/local/lib)
  ENDIF()

  FIND_LIBRARY(LIBEVENT_CORE event_core PATHS ${LIBEVENT_LIB_PATHS})
  FIND_LIBRARY(LIBEVENT_EXTRA event_extra PATHS ${LIBEVENT_LIB_PATHS})
  FIND_LIBRARY(LIBEVENT_PTHREADS event_pthreads PATHS ${LIBEVENT_LIB_PATHS})

  ## libevent_openssl.so is split out on Linux distros
  FIND_LIBRARY(LIBEVENT_OPENSSL event_openssl PATHS ${LIBEVENT_LIB_PATHS})

  IF (LIBEVENT_CORE AND LIBEVENT_INCLUDE_DIRECTORY)
    SET(LIBEVENT_FOUND TRUE)
    SET(LIBEVENT_LIBRARIES
      ${LIBEVENT_CORE}
      ${LIBEVENT_EXTRA}
      ${LIBEVENT_OPENSSL}
      ${LIBEVENT_PTHREADS}
      )
    SET(LIBEVENT_INCLUDE_DIRS ${LIBEVENT_INCLUDE_DIRECTORY})
    IF(NOT LIBEVENT_INCLUDE_DIRECTORY STREQUAL "/usr/include")
      # On FreeBSD this may interfere with BOOST_INCLUDE_DIR, so we rely on
      # users of libevent doing MY_INCLUDE_SYSTEM_DIRECTORIES(LIBEVENT)
      # INCLUDE_DIRECTORIES(AFTER SYSTEM ${LIBEVENT_INCLUDE_DIRS})
    ENDIF()
  ENDIF()
ENDMACRO()

MACRO (MYSQL_USE_BUNDLED_LIBEVENT)
  SET(LIBEVENT_LIBRARIES event_core event_extra event_openssl)
  IF(CMAKE_USE_PTHREADS_INIT)
    LIST(APPEND LIBEVENT_LIBRARIES event_pthreads)
  ENDIF(CMAKE_USE_PTHREADS_INIT)

  SET(LIBEVENT_BUNDLE_PATH "extra/libevent/libevent-2.1.11-stable")
  SET(LIBEVENT_INCLUDE_DIRS
    "${CMAKE_SOURCE_DIR}/${LIBEVENT_BUNDLE_PATH}/include"
    "${CMAKE_BINARY_DIR}/${LIBEVENT_BUNDLE_PATH}/include")
  SET(LIBEVENT_FOUND TRUE)
  ADD_SUBDIRECTORY(${LIBEVENT_BUNDLE_PATH})
ENDMACRO()

MACRO (MYSQL_CHECK_LIBEVENT)

  IF (NOT WITH_LIBEVENT)
    SET(WITH_LIBEVENT "bundled"
      CACHE STRING "By default use bundled libevent.")
  ENDIF()
  
  IF(WITH_LIBEVENT STREQUAL "bundled")
    MYSQL_USE_BUNDLED_LIBEVENT()
  ELSEIF(WITH_LIBEVENT STREQUAL "system")
    FIND_SYSTEM_LIBEVENT()
    IF(NOT LIBEVENT_FOUND)
      RESET_LIBEVENT_VARIABLES()
    ENDIF()
  ELSE()
    MESSAGE(FATAL_ERROR "WITH_LIBEVENT must be bundled or system")
  ENDIF()
  IF(LIBEVENT_FOUND)
    FIND_LIBEVENT_VERSION()
    IF(LIBEVENT_VERSION VERSION_LESS MIN_LIBEVENT_VERSION_REQUIRED)
      MESSAGE(FATAL_ERROR
        "LIBEVENT version must be at least ${MIN_LIBEVENT_VERSION_REQUIRED}, "
        "found ${LIBEVENT_VERSION}.\nPlease use -DWITH_LIBEVENT=bundled")
    ENDIF()
    SET(HAVE_LIBEVENT2 1)
    SET(HAVE_LIBEVENT2 1 CACHE BOOL "")
  ENDIF()
ENDMACRO()
