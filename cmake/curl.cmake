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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# cmake -DWITH_CURL=system|<path/to/custom/installation>|no
# system is the default for unix builds.
# bundled is also supported on el7, for -DWITH_SSL=openssl11.
# no will disable build of binaries that use curl.

# We create an INTERFACE library called curl_interface,
# and an alias ext::curl
# It will have the necessary INTERFACE_INCLUDE_DIRECTORIES property,
# so just link with it, no need to do INCLUDE_DIRECTORIES.

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
  UNSET(CURL_FOUND)
  UNSET(CURL_FOUND CACHE)
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
  UNSET(CURL_VERSION)
  UNSET(CURL_VERSION CACHE)
  UNSET(INTERNAL_CURL_LIBRARY)
  UNSET(INTERNAL_CURL_LIBRARY CACHE)
  UNSET(INTERNAL_CURL_INCLUDE_DIR)
  UNSET(INTERNAL_CURL_INCLUDE_DIR CACHE)
  UNSET(HAVE_CURL_DLL)
  UNSET(HAVE_CURL_DLL CACHE)
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

FUNCTION(FIND_SYSTEM_CURL ARG_CURL_INCLUDE_DIR)
  #  FindCURL.cmake will set
  #  CURL_INCLUDE_DIRS   - where to find curl/curl.h, etc.
  #  CURL_LIBRARIES      - List of libraries when using curl.
  #  CURL_FOUND          - True if curl found.
  #  CURL_VERSION_STRING - the version of curl found (since CMake 2.8.8)
  FIND_PACKAGE(CURL)
  IF(CURL_FOUND AND CURL_LIBRARIES)
    SET(CURL_LIBRARY ${CURL_LIBRARIES} CACHE FILEPATH "Curl library")
    SET(ARG_CURL_INCLUDE_DIR ${CURL_INCLUDE_DIRS})
    SET(ARG_CURL_INCLUDE_DIR ${CURL_INCLUDE_DIRS} PARENT_SCOPE)
    SET(CURL_FOUND ON CACHE INTERNAL "")
  ELSE()
    RESET_CURL_VARIABLES()
    RETURN()
  ENDIF()

  ADD_LIBRARY(curl_interface INTERFACE)
  TARGET_LINK_LIBRARIES(curl_interface INTERFACE ${CURL_LIBRARY})
  IF(WITH_CURL STREQUAL "system")
    IF(NOT ARG_CURL_INCLUDE_DIR STREQUAL "/usr/include")
      TARGET_INCLUDE_DIRECTORIES(curl_interface SYSTEM INTERFACE
        ${ARG_CURL_INCLUDE_DIR})
    ENDIF()
  ENDIF()
ENDFUNCTION(FIND_SYSTEM_CURL)

SET(CURL_VERSION_DIR "curl-8.1.2")
FUNCTION(MYSQL_USE_BUNDLED_CURL CURL_INCLUDE_DIR)
  SET(WITH_CURL "bundled" CACHE STRING "Bundled curl library")
  ADD_SUBDIRECTORY(extra/curl)
  SET(CURL_FOUND ON CACHE INTERNAL "")
  SET(CURL_LIBRARY libcurl)
  SET(CURL_INCLUDE_DIR
    ${CMAKE_SOURCE_DIR}/extra/curl/${CURL_VERSION_DIR}/include)
  SET(CURL_INCLUDE_DIR ${CURL_INCLUDE_DIR} PARENT_SCOPE)

  ADD_LIBRARY(curl_interface INTERFACE)
  TARGET_LINK_LIBRARIES(curl_interface INTERFACE ${CURL_LIBRARY})
  TARGET_INCLUDE_DIRECTORIES(curl_interface SYSTEM BEFORE INTERFACE
    ${CURL_INCLUDE_DIR})
ENDFUNCTION(MYSQL_USE_BUNDLED_CURL)


FUNCTION(FIND_CUSTOM_UNIX_CURL_LIBRARY WITH_CURL CURL_LIBRARY)
  # Prefer the static library, if found.
  LIST(REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
  FIND_LIBRARY(INTERNAL_CURL_LIBRARY
    NAMES curl libcurl
    PATHS ${WITH_CURL} ${WITH_CURL}/lib
    NO_DEFAULT_PATH
    NO_CMAKE_ENVIRONMENT_PATH
    NO_SYSTEM_ENVIRONMENT_PATH
    )
  LIST(REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
  IF(NOT INTERNAL_CURL_LIBRARY)
    MESSAGE(FATAL_ERROR "CURL library not found under '${WITH_CURL}'")
  ENDIF()
  SET(CURL_LIBRARY ${INTERNAL_CURL_LIBRARY} PARENT_SCOPE)
ENDFUNCTION(FIND_CUSTOM_UNIX_CURL_LIBRARY)


FUNCTION(FIND_CUSTOM_CURL_INCLUDE WITH_CURL CURL_INCLUDE_DIR)
  FIND_PATH(INTERNAL_CURL_INCLUDE_DIR
    NAMES curl/curl.h
    PATHS ${WITH_CURL} ${WITH_CURL}/include
    NO_DEFAULT_PATH
    NO_CMAKE_ENVIRONMENT_PATH
    NO_SYSTEM_ENVIRONMENT_PATH
    )
  IF(NOT INTERNAL_CURL_INCLUDE_DIR)
    MESSAGE(FATAL_ERROR "CURL include files not found under '${WITH_CURL}'")
  ENDIF()
  SET(CURL_INCLUDE_DIR ${INTERNAL_CURL_INCLUDE_DIR} PARENT_SCOPE)
ENDFUNCTION(FIND_CUSTOM_CURL_INCLUDE)


FUNCTION(FIND_CUSTOM_CURL CURL_INCLUDE_DIR)
  # Explicit path given. Normalize path for the following regex replace.
  FILE(TO_CMAKE_PATH "${WITH_CURL}" WITH_CURL)
  # Pushbuild adds /lib to the CURL path
  STRING(REGEX REPLACE "/lib$" "" WITH_CURL "${WITH_CURL}")

  FIND_CUSTOM_CURL_INCLUDE(${WITH_CURL} CURL_INCLUDE_DIR)
  SET(CURL_INCLUDE_DIR ${CURL_INCLUDE_DIR} PARENT_SCOPE)

  IF(WIN32)
    FIND_CUSTOM_WIN_CURL_LIBRARY(${WITH_CURL} CURL_LIBRARY)
  ELSE()
    FIND_CUSTOM_UNIX_CURL_LIBRARY(${WITH_CURL} CURL_LIBRARY)
  ENDIF()

  SET(CURL_FOUND ON CACHE INTERNAL "")

  ADD_LIBRARY(curl_interface INTERFACE)
  TARGET_LINK_LIBRARIES(curl_interface INTERFACE ${CURL_LIBRARY})
  TARGET_INCLUDE_DIRECTORIES(curl_interface SYSTEM BEFORE INTERFACE
    ${CURL_INCLUDE_DIR})

  IF(WIN32)
    ADD_DEPENDENCIES(curl_interface copy_curl_dlls)
  ENDIF()
  IF(APPLE)
    TARGET_LINK_LIBRARIES(curl_interface
      INTERFACE "-framework CoreFoundation")
    TARGET_LINK_LIBRARIES(curl_interface
      INTERFACE "-framework SystemConfiguration")
  ENDIF()

ENDFUNCTION(FIND_CUSTOM_CURL)


FUNCTION(MYSQL_CHECK_CURL)
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

  IF(WITH_CURL STREQUAL "none")
    MESSAGE(STATUS "WITH_CURL=none, not using any curl library.")
    RESET_CURL_VARIABLES()
    RETURN()
  ENDIF()

  UNSET(CURL_INCLUDE_DIR)
  IF(WITH_CURL STREQUAL "system")
    FIND_SYSTEM_CURL(CURL_INCLUDE_DIR)
  ELSEIF(WITH_CURL STREQUAL "bundled")
    IF(ALTERNATIVE_SYSTEM_SSL)
      MYSQL_USE_BUNDLED_CURL(CURL_INCLUDE_DIR)
    ELSE()
      MESSAGE(WARNING "WITH_CURL options: ${WITH_CURL_DOC}")
      MESSAGE(FATAL_ERROR "Bundled CURL library is not supported.")
    ENDIF()
  ELSEIF(WITH_CURL)
    FIND_CUSTOM_CURL(CURL_INCLUDE_DIR)
  ENDIF()

  MESSAGE(STATUS "CURL_INCLUDE_DIR = ${CURL_INCLUDE_DIR}")
  FIND_CURL_VERSION()
  ADD_LIBRARY(ext::curl ALIAS curl_interface)
  # Downgrade errors to warnings
  # We could instead do INTERFACE_COMPILE_DEFINITIONS CURL_DISABLE_DEPRECATION
  # That would silence curl warnings completely.
  IF(MY_COMPILER_IS_GNU AND CURL_VERSION VERSION_GREATER "7.86")
    SET_TARGET_PROPERTIES(curl_interface PROPERTIES INTERFACE_COMPILE_OPTIONS
      "-Wno-error=deprecated-declarations")
  ENDIF()

ENDFUNCTION(MYSQL_CHECK_CURL)


# Find libcurl.dll and libcurl_imp.lib
# We need the shared libraries, libcurl.lib cannot be used.
FUNCTION(FIND_CUSTOM_WIN_CURL_LIBRARY WITH_CURL_PATH CURL_LIBRARY)

  FIND_FILE(HAVE_CURL_DLL
    NAMES "libcurl.dll"
    PATHS "${WITH_CURL_PATH}/lib"
    NO_DEFAULT_PATH
    )
  IF(NOT HAVE_CURL_DLL)
    MESSAGE(FATAL_ERROR "libcurl.dll not found under ${WITH_CURL_PATH}")
  ENDIF()

  FIND_LIBRARY(INTERNAL_CURL_LIBRARY
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

  SET(CURL_LIBRARY ${INTERNAL_CURL_LIBRARY} PARENT_SCOPE)

  COPY_CUSTOM_DLL("${HAVE_CURL_DLL}" OUTPUT_CURL_TARGET)
  MY_ADD_CUSTOM_TARGET(copy_curl_dlls ALL)
  ADD_DEPENDENCIES(copy_curl_dlls ${OUTPUT_CURL_TARGET})

  SET(ZLIB_DLL_REQUIRED 1)
  FIND_OBJECT_DEPENDENCIES("${HAVE_CURL_DLL}" DEPENDENCY_LIST)
  LIST(FIND DEPENDENCY_LIST "zlib.dll" FOUNDIT1)
  LIST(FIND DEPENDENCY_LIST "zlib1.dll" FOUNDIT2)
  GET_FILENAME_COMPONENT(CURL_DLL_NAME "${HAVE_CURL_DLL}" NAME)
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
    COPY_CUSTOM_DLL("${HAVE_ZLIB_DLL}" OUTPUT_ZLIB_TARGET)
    ADD_DEPENDENCIES(copy_curl_dlls ${OUTPUT_ZLIB_TARGET})
  ENDIF()

ENDFUNCTION(FIND_CUSTOM_WIN_CURL_LIBRARY)
