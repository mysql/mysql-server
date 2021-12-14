# Copyright (c) 2009, 2021, Oracle and/or its affiliates.
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

# We support different versions of SSL:
# - "system"  (typically) uses headers/libraries in /usr/lib and /usr/lib64
# - a custom installation of openssl can be used like this
#     - cmake -DCMAKE_PREFIX_PATH=</path/to/custom/openssl> -DWITH_SSL="system"
#   or
#     - cmake -DWITH_SSL=</path/to/custom/openssl>
#
# The default value for WITH_SSL is "system".
#
# WITH_SSL="system" means: use the SSL library that comes with the operating
# system. This typically means you have to do 'yum install openssl-devel'
# or something similar.
#
# For Windows or macOS, WITH_SSL="system" is handled a bit differently:
# We assume you have installed
#     https://slproweb.com/products/Win32OpenSSL.html
#     We look for "C:/Program Files/OpenSSL-Win64/"
# or
#     https://brew.sh
#     https://formulae.brew.sh/formula/openssl@1.1
#     We look for "/usr/local/opt/openssl@1.1"
#     We look for the static libraries, rather than the .dylib ones.
# When the package has been located, we treat it as if cmake had been
# invoked with  -DWITH_SSL=</path/to/custom/openssl>


SET(WITH_SSL_DOC "\nsystem (use the OS openssl library)")
SET(WITH_SSL_DOC
  "${WITH_SSL_DOC}, \nyes (synonym for system)")
SET(WITH_SSL_DOC
  "${WITH_SSL_DOC}, \n</path/to/custom/openssl/installation>")

STRING(REPLACE "\n" "| " WITH_SSL_DOC_STRING "${WITH_SSL_DOC}")

MACRO(FATAL_SSL_NOT_FOUND_ERROR string)
  MESSAGE(STATUS "\n${string}"
    "\nMake sure you have specified a supported SSL version. "
    "\nValid options are : ${WITH_SSL_DOC}\n"
    )
  IF(UNIX)
    MESSAGE(FATAL_ERROR
      "Please install the appropriate openssl developer package.\n")
  ENDIF()
  IF(WIN32)
    MESSAGE(FATAL_ERROR
      "Please see https://wiki.openssl.org/index.php/Binaries\n")
  ENDIF()
  IF(APPLE)
    MESSAGE(FATAL_ERROR
      "Please see http://brewformulas.org/Openssl\n")
  ENDIF()
ENDMACRO()

MACRO(RESET_SSL_VARIABLES)
  UNSET(WITH_SSL_PATH)
  UNSET(WITH_SSL_PATH CACHE)
  UNSET(OPENSSL_VERSION)
  UNSET(OPENSSL_VERSION CACHE)
  UNSET(OPENSSL_ROOT_DIR)
  UNSET(OPENSSL_ROOT_DIR CACHE)
  UNSET(OPENSSL_INCLUDE_DIR)
  UNSET(OPENSSL_INCLUDE_DIR CACHE)
  UNSET(OPENSSL_APPLINK_C)
  UNSET(OPENSSL_APPLINK_C CACHE)
  UNSET(OPENSSL_LIBRARY)
  UNSET(OPENSSL_LIBRARY CACHE)
  UNSET(CRYPTO_LIBRARY)
  UNSET(CRYPTO_LIBRARY CACHE)
  UNSET(HAVE_SHA512_DIGEST_LENGTH)
  UNSET(HAVE_SHA512_DIGEST_LENGTH CACHE)
ENDMACRO()

# MYSQL_CHECK_SSL
#
# Provides the following configure options:
# WITH_SSL=[yes|system|<path/to/custom/installation>]
MACRO (MYSQL_CHECK_SSL)
  IF(NOT WITH_SSL)
    SET(WITH_SSL "system" CACHE STRING ${WITH_SSL_DOC_STRING} FORCE)
  ENDIF()

  # See if WITH_SSL is of the form </path/to/custom/installation>
  FILE(GLOB WITH_SSL_HEADER ${WITH_SSL}/include/openssl/ssl.h)
  IF (WITH_SSL_HEADER)
    FILE(TO_CMAKE_PATH "${WITH_SSL}" WITH_SSL)
    SET(WITH_SSL_PATH ${WITH_SSL} CACHE PATH "path to custom SSL installation")
    SET(WITH_SSL_PATH ${WITH_SSL})
  ENDIF()

  IF(WITH_SSL STREQUAL "system" OR
      WITH_SSL STREQUAL "yes" OR
      WITH_SSL_PATH
      )
    # Treat "system" the same way as -DWITH_SSL=</path/to/custom/openssl>
    # Note: we cannot use FIND_PACKAGE(OpenSSL), as older cmake versions
    # have buggy implementations.
    IF((APPLE OR WIN32) AND NOT WITH_SSL_PATH AND WITH_SSL STREQUAL "system")
      IF(APPLE)
        SET(WITH_SSL_PATH "/usr/local/opt/openssl@1.1")
      ELSE()
        SET(WITH_SSL_PATH "C:/Program Files/OpenSSL-Win64/")
      ENDIF()

      # Note that several packages may come with ssl headers,
      # e.g. Strawberry Perl, so ignore some system paths below.
      IF(WIN32 AND NOT OPENSSL_ROOT_DIR AND WITH_SSL STREQUAL "system")
        FILE(TO_CMAKE_PATH "$ENV{PROGRAMFILES}" _programfiles)
        IF(SIZEOF_VOIDP EQUAL 8)
          FIND_PATH(OPENSSL_WIN64
            NAMES  "include/openssl/ssl.h"
            PATHS "${_programfiles}/OpenSSL-Win64" "C:/OpenSSL-Win64/"
            NO_SYSTEM_ENVIRONMENT_PATH
            NO_CMAKE_SYSTEM_PATH
            )
          IF(OPENSSL_WIN64)
            SET(OPENSSL_ROOT_DIR ${OPENSSL_WIN64})
            FIND_LIBRARY(OPENSSL_LIBRARY
              NAMES libssl_static
              HINTS ${OPENSSL_ROOT_DIR}/lib)
            FIND_LIBRARY(CRYPTO_LIBRARY
              NAMES libcrypto_static
              HINTS ${OPENSSL_ROOT_DIR}/lib)
            IF(OPENSSL_LIBRARY AND CRYPTO_LIBRARY)
              SET(FOUND_STATIC_SSL_LIBS 1)
            ENDIF()
          ENDIF()
        ENDIF()
      ENDIF()
    ENDIF()

    # First search in WITH_SSL_PATH.
    FIND_PATH(OPENSSL_ROOT_DIR
      NAMES include/openssl/ssl.h
      NO_CMAKE_PATH
      NO_CMAKE_ENVIRONMENT_PATH
      HINTS ${WITH_SSL_PATH}
    )
    # Then search in standard places (if not found above).
    FIND_PATH(OPENSSL_ROOT_DIR
      NAMES include/openssl/ssl.h
    )

    FIND_PATH(OPENSSL_INCLUDE_DIR
      NAMES openssl/ssl.h
      HINTS ${OPENSSL_ROOT_DIR}/include
    )

    IF (WIN32)
      FIND_FILE(OPENSSL_APPLINK_C
        NAMES openssl/applink.c
        NO_DEFAULT_PATH
        HINTS ${OPENSSL_ROOT_DIR}/include
      )
      MESSAGE(STATUS "OPENSSL_APPLINK_C ${OPENSSL_APPLINK_C}")
    ENDIF()

    # On mac this list is <.dylib;.so;.a>
    # We prefer static libraries, so we reverse it here.
    IF (WITH_SSL_PATH)
      LIST(REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
      MESSAGE(STATUS "suffixes <${CMAKE_FIND_LIBRARY_SUFFIXES}>")
    ENDIF()

    FIND_LIBRARY(OPENSSL_LIBRARY
                 NAMES ssl libssl ssleay32 ssleay32MD
                 HINTS ${OPENSSL_ROOT_DIR}/lib)
    FIND_LIBRARY(CRYPTO_LIBRARY
                 NAMES crypto libcrypto libeay32
                 HINTS ${OPENSSL_ROOT_DIR}/lib)
    IF (WITH_SSL_PATH)
      LIST(REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
    ENDIF()

    IF(OPENSSL_INCLUDE_DIR)
      # Verify version number. Version information looks like:
      #   #define OPENSSL_VERSION_NUMBER 0x1000103fL
      # Encoded as MNNFFPPS: major minor fix patch status
      FILE(STRINGS "${OPENSSL_INCLUDE_DIR}/openssl/opensslv.h"
        OPENSSL_VERSION_NUMBER
        REGEX "^#[ ]*define[\t ]+OPENSSL_VERSION_NUMBER[\t ]+0x[0-9].*"
        )
      STRING(REGEX REPLACE
        "^.*OPENSSL_VERSION_NUMBER[\t ]+0x([0-9]).*$" "\\1"
        OPENSSL_MAJOR_VERSION "${OPENSSL_VERSION_NUMBER}"
        )
      STRING(REGEX REPLACE
        "^.*OPENSSL_VERSION_NUMBER[\t ]+0x[0-9]([0-9][0-9]).*$" "\\1"
        OPENSSL_MINOR_VERSION "${OPENSSL_VERSION_NUMBER}"
        )
      STRING(REGEX REPLACE
        "^.*OPENSSL_VERSION_NUMBER[\t ]+0x[0-9][0-9][0-9]([0-9][0-9]).*$" "\\1"
        OPENSSL_FIX_VERSION "${OPENSSL_VERSION_NUMBER}"
        )
    ENDIF()
    SET(OPENSSL_VERSION
      "${OPENSSL_MAJOR_VERSION}.${OPENSSL_MINOR_VERSION}.${OPENSSL_FIX_VERSION}"
      )
    SET(OPENSSL_VERSION ${OPENSSL_VERSION} CACHE INTERNAL "")

    IF("${OPENSSL_VERSION}" VERSION_GREATER "1.1.0")
       ADD_DEFINITIONS(-DHAVE_TLSv13)
       SET(HAVE_TLSv13 1)
       IF(SOLARIS)
         SET(FORCE_SSL_SOLARIS "-Wl,--undefined,address_of_sk_new_null")
       ENDIF()
    ENDIF()
    IF(OPENSSL_INCLUDE_DIR AND
       OPENSSL_LIBRARY   AND
       CRYPTO_LIBRARY      AND
       OPENSSL_MAJOR_VERSION STREQUAL "1"
      )
      SET(OPENSSL_FOUND TRUE)
    ELSE()
      SET(OPENSSL_FOUND FALSE)
    ENDIF()

    # If we are invoked with -DWITH_SSL=/path/to/custom/openssl
    # and we have found static libraries, then link them statically
    # into our executables and libraries.
    # Adding IMPORTED_LOCATION allows MERGE_STATIC_LIBS
    # to merge imported libraries as well as our own libraries.
    SET(MY_CRYPTO_LIBRARY "${CRYPTO_LIBRARY}")
    SET(MY_OPENSSL_LIBRARY "${OPENSSL_LIBRARY}")
    IF (WITH_SSL_PATH)
      GET_FILENAME_COMPONENT(CRYPTO_EXT "${CRYPTO_LIBRARY}" EXT)
      GET_FILENAME_COMPONENT(OPENSSL_EXT "${OPENSSL_LIBRARY}" EXT)
      IF (CRYPTO_EXT STREQUAL ".a" OR CRYPTO_EXT STREQUAL ".lib")
        SET(MY_CRYPTO_LIBRARY imported_crypto)
        ADD_IMPORTED_LIBRARY(imported_crypto "${CRYPTO_LIBRARY}")
      ENDIF()
      IF (OPENSSL_EXT STREQUAL ".a" OR OPENSSL_EXT STREQUAL ".lib")
        SET(MY_OPENSSL_LIBRARY imported_openssl)
        ADD_IMPORTED_LIBRARY(imported_openssl "${OPENSSL_LIBRARY}")
      ENDIF()
      IF(CRYPTO_EXT STREQUAL ".a" OR OPENSSL_EXT STREQUAL ".a")
        SET(STATIC_SSL_LIBRARY 1)
      ENDIF()
    ENDIF()

    MESSAGE(STATUS "OPENSSL_INCLUDE_DIR = ${OPENSSL_INCLUDE_DIR}")
    MESSAGE(STATUS "OPENSSL_LIBRARY = ${OPENSSL_LIBRARY}")
    MESSAGE(STATUS "CRYPTO_LIBRARY = ${CRYPTO_LIBRARY}")
    MESSAGE(STATUS "OPENSSL_MAJOR_VERSION = ${OPENSSL_MAJOR_VERSION}")
    MESSAGE(STATUS "OPENSSL_MINOR_VERSION = ${OPENSSL_MINOR_VERSION}")
    MESSAGE(STATUS "OPENSSL_FIX_VERSION = ${OPENSSL_FIX_VERSION}")

    # Static SSL libraries? we require at least 1.1.1
    IF(STATIC_SSL_LIBRARY OR (WIN32 AND WITH_SSL_PATH))
      IF(OPENSSL_VERSION VERSION_LESS "1.1.1")
        RESET_SSL_VARIABLES()
        MESSAGE(FATAL_ERROR "SSL version must be at least 1.1.1")
      ENDIF()
      SET(SSL_DEFINES "-DHAVE_STATIC_OPENSSL")
    ENDIF()

    INCLUDE(CheckSymbolExists)

    CMAKE_PUSH_CHECK_STATE()
    SET(CMAKE_REQUIRED_INCLUDES ${OPENSSL_INCLUDE_DIR})
    CHECK_SYMBOL_EXISTS(SHA512_DIGEST_LENGTH "openssl/sha.h"
                        HAVE_SHA512_DIGEST_LENGTH)
    CMAKE_POP_CHECK_STATE()

    IF(OPENSSL_FOUND AND HAVE_SHA512_DIGEST_LENGTH)
      SET(SSL_SOURCES "")
      SET(SSL_LIBRARIES ${MY_OPENSSL_LIBRARY} ${MY_CRYPTO_LIBRARY})
      IF(SOLARIS)
        SET(SSL_LIBRARIES ${SSL_LIBRARIES} ${LIBSOCKET})
      ENDIF()
      IF(LINUX)
        SET(SSL_LIBRARIES ${SSL_LIBRARIES} ${LIBDL})
      ENDIF()
      MESSAGE(STATUS "SSL_LIBRARIES = ${SSL_LIBRARIES}")
      IF(WIN32 AND WITH_SSL STREQUAL "system" AND NOT FOUND_STATIC_SSL_LIBS)
        MESSAGE(STATUS "Please do\nPATH=\"${WITH_SSL_PATH}bin\":$PATH")
        FILE(TO_NATIVE_PATH "${WITH_SSL_PATH}" WITH_SSL_PATH_XX)
        MESSAGE(STATUS "or\nPATH=\"${WITH_SSL_PATH_XX}bin\":$PATH")
      ENDIF()
      SET(SSL_INCLUDE_DIRS ${OPENSSL_INCLUDE_DIR})
      SET(SSL_INTERNAL_INCLUDE_DIRS "")
      STRING_APPEND(SSL_DEFINES " -DHAVE_OPENSSL")
    ELSE()
      RESET_SSL_VARIABLES()
      FATAL_SSL_NOT_FOUND_ERROR(
        "Cannot find appropriate system libraries for WITH_SSL=${WITH_SSL}.")
    ENDIF()
  ELSE()
    RESET_SSL_VARIABLES()
    FATAL_SSL_NOT_FOUND_ERROR(
      "Wrong option or path for WITH_SSL=${WITH_SSL}.")
  ENDIF()
ENDMACRO()
