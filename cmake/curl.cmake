# Copyright (c) 2017, 2022, Oracle and/or its affiliates.
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

# cmake -DWITH_CURL=system|<path/to/custom/installation>|no
# system is the default for unix builds.
# bundled is also supported on el7, for -DWITH_SSL=openssl11.
# no will disable build of binaries that use curl.

SET(WITH_CURL_DOC "\nsystem (use the OS curl library)")
STRING_APPEND(WITH_CURL_DOC ", \n</path/to/custom/installation>")
STRING_APPEND(WITH_CURL_DOC ", \n 0 | no | off | none (skip curl)")
STRING_APPEND(WITH_CURL_DOC
  ", \n bundled (only supported for WITH_SSL=openssl11 on el7")
STRING_APPEND(WITH_CURL_DOC "\n")

STRING(REPLACE "\n" "| " WITH_CURL_DOC_STRING "${WITH_CURL_DOC}")

# FindCURL.cmake will set a zillion variables.
# We reset the ones we use, plus a couple more.
MACRO(RESET_CURL_VARIABLES)
  UNSET(CURL_INCLUDE_DIR)
  UNSET(CURL_INCLUDE_DIR CACHE)
  UNSET(CURL_LIBRARIES)
  UNSET(CURL_LIBRARIES CACHE)
  UNSET(CURL_LIBRARY)
  UNSET(CURL_LIBRARY CACHE)
  UNSET(CURL_LIBRARY_DEBUG)
  UNSET(CURL_LIBRARY_DEBUG CACHE)
  UNSET(CURL_LIBRARY_RELEASE)
  UNSET(CURL_LIBRARY_RELEASE CACHE)
  UNSET(WITH_CURL_PATH)
  UNSET(WITH_CURL_PATH CACHE)
ENDMACRO()

FUNCTION(WARN_MISSING_SYSTEM_CURL OUTPUT_WARNING)
  IF(NOT CURL_FOUND AND WITH_CURL STREQUAL "system")
    MESSAGE(WARNING "Cannot find CURL development libraries. "
      "You need to install the required packages:\n"
      "  Debian/Ubuntu:              apt install libcurl4-openssl-dev\n"
      "  RedHat/Fedora/Oracle Linux: yum install libcurl-devel\n"
      "  SuSE:                       zypper install libcurl-devel\n"
      )
    SET(${OUTPUT_WARNING} 1 PARENT_SCOPE)
  ENDIF()
ENDFUNCTION()

MACRO(FIND_CURL_VERSION)
  IF(CURL_INCLUDE_DIR AND EXISTS "${CURL_INCLUDE_DIR}/curl/curlver.h")
    FILE(STRINGS "${CURL_INCLUDE_DIR}/curl/curlver.h"
      CURL_VERSION_NUMBERS
      REGEX "^#[ ]*define[\t ]+LIBCURL_VERSION_[A-Z]+[\t ]+[0-9].*"
      )
    STRING(REGEX REPLACE
      "^.*LIBCURL_VERSION_MAJOR[\t ]+([0-9]+).*$" "\\1"
      CURL_VERSION_MAJOR "${CURL_VERSION_NUMBERS}"
      )
    STRING(REGEX REPLACE
      "^.*LIBCURL_VERSION_MINOR[\t ]+([0-9]+).*$" "\\1"
      CURL_VERSION_MINOR "${CURL_VERSION_NUMBERS}"
      )
    SET(CURL_VERSION "${CURL_VERSION_MAJOR}.${CURL_VERSION_MINOR}")
    SET(CURL_VERSION "${CURL_VERSION}" CACHE INTERNAL "CURL major.minor")
    MESSAGE(STATUS "CURL_VERSION (${WITH_CURL}) is ${CURL_VERSION}")
  ENDIF()
ENDMACRO()

MACRO(FIND_SYSTEM_CURL)
  #  FindCURL.cmake will set
  #  CURL_INCLUDE_DIRS   - where to find curl/curl.h, etc.
  #  CURL_LIBRARIES      - List of libraries when using curl.
  #  CURL_FOUND          - True if curl found.
  #  CURL_VERSION_STRING - the version of curl found (since CMake 2.8.8)
  FIND_PACKAGE(CURL)
  IF(CURL_FOUND AND CURL_LIBRARIES)
    SET(CURL_LIBRARY ${CURL_LIBRARIES} CACHE FILEPATH "Curl library")
    SET(CURL_INCLUDE_DIR ${CURL_INCLUDE_DIRS} CACHE PATH "Curl include")
  ELSE()
    SET(CURL_LIBRARY "")
    SET(CURL_INCLUDE_DIR "")
  ENDIF()
ENDMACRO()

SET(CURL_VERSION_DIR "curl-7.86.0")
MACRO(MYSQL_USE_BUNDLED_CURL)
  SET(WITH_CURL "bundled" CACHE STRING "Bundled curl library")
  ADD_SUBDIRECTORY(extra/curl)
  SET(CURL_FOUND ON)
  SET(CURL_LIBRARY libcurl)
  SET(CURL_INCLUDE_DIR
    ${CMAKE_SOURCE_DIR}/extra/curl/${CURL_VERSION_DIR}/include)
ENDMACRO(MYSQL_USE_BUNDLED_CURL)

MACRO(FIND_CUSTOM_CURL)
  # Explicit path given. Normalize path for the following regex replace.
  FILE(TO_CMAKE_PATH "${WITH_CURL}" WITH_CURL)
  # Pushbuild adds /lib to the CURL path
  STRING(REGEX REPLACE "/lib$" "" WITH_CURL "${WITH_CURL}")

  # Prefer the static library, if found.
  LIST(REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
  FIND_LIBRARY(CURL_LIBRARY
    NAMES curl libcurl
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
  # Save the path, for copy_curl_dlls on WIN32
  SET(WITH_CURL_PATH ${WITH_CURL} CACHE PATH "path to CURL installation")
  SET(WITH_CURL_PATH ${WITH_CURL})
ENDMACRO()


MACRO(MYSQL_CHECK_CURL)
  # Map 0 | no | off to "none".
  IF(DEFINED WITH_CURL AND NOT WITH_CURL)
    SET(WITH_CURL "none")
    SET(WITH_CURL "none" CACHE STRING "${WITH_CURL_DOC_STRING}" FORCE)
    RESET_CURL_VARIABLES()
  ENDIF()

  # Use "none" by default on win, "system" on other platforms.
  IF(NOT DEFINED WITH_CURL)
    SET(WITH_CURL ${WITH_CURL_DEFAULT} CACHE STRING "${WITH_CURL_DOC_STRING}")
  ENDIF()

  IF(WITH_CURL STREQUAL "system")
    FIND_SYSTEM_CURL()
  ELSEIF(WITH_CURL STREQUAL "bundled")
    IF(ALTERNATIVE_SYSTEM_SSL)
      MYSQL_USE_BUNDLED_CURL()
    ELSE()
      MESSAGE(WARNING "WITH_CURL options: ${WITH_CURL_DOC}")
      MESSAGE(FATAL_ERROR "Bundled CURL library is not supported.")
    ENDIF()
  ELSEIF(WITH_CURL STREQUAL "none")
    MESSAGE(STATUS "WITH_CURL=none, not using any curl library.")
    RESET_CURL_VARIABLES()
  ELSEIF(WITH_CURL)
    FIND_CUSTOM_CURL()
  ELSE()
    MESSAGE(WARNING "No WITH_CURL has been set.")
    SET(CURL_LIBRARY "")
    SET(CURL_INCLUDE_DIR "")
  ENDIF()

  FIND_CURL_VERSION()

  MESSAGE(STATUS "CURL_LIBRARY = ${CURL_LIBRARY}")
  MESSAGE(STATUS "CURL_INCLUDE_DIR = ${CURL_INCLUDE_DIR}")
ENDMACRO()


MACRO(MYSQL_CHECK_CURL_DLLS)

  IF (WITH_CURL_PATH AND WIN32)

    MESSAGE(STATUS "WITH_CURL_PATH ${WITH_CURL_PATH}")
    GET_FILENAME_COMPONENT(CURL_NAME "${CURL_LIBRARY}" NAME_WE)
    FIND_FILE(HAVE_CURL_DLL
      NAMES "${CURL_NAME}.dll"
      PATHS "${WITH_CURL_PATH}/lib"
      NO_DEFAULT_PATH
      )
    MESSAGE(STATUS "HAVE_CURL_DLL ${HAVE_CURL_DLL}")
    IF(HAVE_CURL_DLL)
      # Found the .dll, forget about libcurl.lib and lookup libcurl_imp.lib
      UNSET(CURL_LIBRARY)
      UNSET(CURL_LIBRARY CACHE)
      FIND_LIBRARY(CURL_LIBRARY
        NAMES libcurl_imp.lib
        PATHS ${WITH_CURL} ${WITH_CURL}/lib
        NO_DEFAULT_PATH
        NO_CMAKE_ENVIRONMENT_PATH
        NO_SYSTEM_ENVIRONMENT_PATH
        )
      IF(NOT CURL_LIBRARY)
        MESSAGE(FATAL_ERROR
          "CURL dll import library not found under '${WITH_CURL}'")
      ENDIF()
      GET_FILENAME_COMPONENT(CURL_DLL_NAME "${HAVE_CURL_DLL}" NAME)
      MY_ADD_CUSTOM_TARGET(copy_curl_dlls ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${HAVE_CURL_DLL}"
        "${CMAKE_BINARY_DIR}/runtime_output_directory/${CMAKE_CFG_INTDIR}/${CURL_DLL_NAME}"
        )

      MESSAGE(STATUS "INSTALL ${HAVE_CURL_DLL} to ${INSTALL_BINDIR}")
      INSTALL(FILES "${HAVE_CURL_DLL}"
        DESTINATION "${INSTALL_BINDIR}" COMPONENT SharedLibraries)

      SET(ZLIB_DLL_REQUIRED 1)
      FIND_OBJECT_DEPENDENCIES("${HAVE_CURL_DLL}" DEPENDENCY_LIST)
      LIST(FIND DEPENDENCY_LIST "zlib.dll" FOUNDIT1)
      LIST(FIND DEPENDENCY_LIST "zlib1.dll" FOUNDIT2)
      MESSAGE(STATUS "${CURL_DLL_NAME} DEPENDENCY_LIST ${DEPENDENCY_LIST}")
      IF(FOUNDIT1 LESS 0 AND FOUNDIT2 LESS 0)
        UNSET(ZLIB_DLL_REQUIRED)
      ENDIF()

      FIND_FILE(HAVE_ZLIB_DLL
        NAMES zlib.dll zlib1.dll
        PATHS "${WITH_CURL_PATH}/lib"
        NO_DEFAULT_PATH
        )
      MESSAGE(STATUS "HAVE_ZLIB_DLL ${HAVE_ZLIB_DLL}")

      IF(ZLIB_DLL_REQUIRED AND NOT HAVE_ZLIB_DLL)
        MESSAGE(FATAL_ERROR "libcurl.dll depends on zlib.dll or zlib1.dll")
      ENDIF()

      IF(ZLIB_DLL_REQUIRED AND HAVE_ZLIB_DLL)
        MESSAGE(STATUS "INSTALL ${HAVE_ZLIB_DLL} to ${INSTALL_BINDIR}")
        INSTALL(FILES "${HAVE_ZLIB_DLL}"
          DESTINATION "${INSTALL_BINDIR}" COMPONENT SharedLibraries)
        GET_FILENAME_COMPONENT(ZLIB_DLL_NAME "${HAVE_ZLIB_DLL}" NAME)
        MY_ADD_CUSTOM_TARGET(copy_zlib_dlls ALL
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${HAVE_ZLIB_DLL}"
          "${CMAKE_BINARY_DIR}/runtime_output_directory/${CMAKE_CFG_INTDIR}/${ZLIB_DLL_NAME}"
          )
        ADD_DEPENDENCIES(copy_curl_dlls copy_zlib_dlls)
      ENDIF()
    ELSE()
      MESSAGE(STATUS "Cannot find CURL dynamic libraries")
    ENDIF()

  ENDIF()
ENDMACRO()
