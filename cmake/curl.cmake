# Copyright (c) 2017, 2023, Oracle and/or its affiliates.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

MACRO(MYSQL_CHECK_CURL)
  IF(NOT WIN32)
   IF(WITH_CURL STREQUAL "system")
     #  FindCURL.cmake will set
     #  CURL_INCLUDE_DIRS   - where to find curl/curl.h, etc.
     #  CURL_LIBRARIES      - List of libraries when using curl.
     #  CURL_FOUND          - True if curl found.
     #  CURL_VERSION_STRING - the version of curl found (since CMake 2.8.8)
     FIND_PACKAGE(CURL)
     IF(CURL_FOUND)
       SET(CURL_LIBRARY ${CURL_LIBRARIES} CACHE PATH "Curl library")
     ENDIF()
     MESSAGE(STATUS "CURL_LIBRARY = ${CURL_LIBRARY}")
   ELSEIF(WITH_CURL)
    # Explicit path given. Normalize path for the following regex replace.
    FILE(TO_CMAKE_PATH "${WITH_CURL}" WITH_CURL)
    # Pushbuild adds /lib to the CURL path
    STRING(REGEX REPLACE "/lib$" "" WITH_CURL "${WITH_CURL}")

    LIST(REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
    FIND_LIBRARY(CURL_LIBRARY
      NAMES curl
      PATHS ${WITH_CURL} ${WITH_CURL}/lib
      NO_DEFAULT_PATH
      NO_CMAKE_ENVIRONMENT_PATH
      NO_SYSTEM_ENVIRONMENT_PATH
    )
    LIST(REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
    IF(NOT CURL_LIBRARY)
      MESSAGE(FATAL_ERROR "CURL library not found under '${WITH_CURL}'")
    ENDIF()

    FIND_PATH(CURL_INCLUDE_DIR
      NAMES curl/curl.h
      PATHS ${WITH_CURL} ${WITH_CURL}/include
      NO_DEFAULT_PATH
      NO_CMAKE_ENVIRONMENT_PATH
      NO_SYSTEM_ENVIRONMENT_PATH
      )
    IF(NOT CURL_INCLUDE_DIR)
      MESSAGE(FATAL_ERROR "CURL include files not found under '${WITH_CURL}'")
    ENDIF()

    MESSAGE(STATUS "CURL_LIBRARY = ${CURL_LIBRARY}")
    MESSAGE(STATUS "CURL_INCLUDE_DIR = ${CURL_INCLUDE_DIR}")
   ELSE()
     MESSAGE(STATUS
       "You need to set WITH_CURL. This"
       " variable needs to point to curl library.")
   ENDIF()
  ENDIF()
ENDMACRO()
