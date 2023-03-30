# Copyright (c) 2009, 2023, Oracle and/or its affiliates.
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

# We support different versions of SSL:
# - "system"  (typically) uses headers/libraries in /usr/include and
#       /usr/lib or /usr/lib64
# - a custom installation of openssl can be used like this
#     - cmake -DCMAKE_PREFIX_PATH=</path/to/custom/openssl> -DWITH_SSL="system"
#   or
#     - cmake -DWITH_SSL=</path/to/custom/openssl>
#
# - openssl11 which is an alternative system SSL package on el7.
#   The RPM package openssl11-devel must be installed.
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
#     find_package(OpenSSL) will locate it
# or
#     https://brew.sh
#     https://formulae.brew.sh/formula/openssl@1.1
#     we give a hint ${HOMEBREW_HOME}/openssl@1.1 to find_package(OpenSSL)
#
# On Windows, we treat this "system" library as if cmake had been
# invoked with -DWITH_SSL=</path/to/custom/openssl>
#
# On macOS we treat it as a system library, which means that the generated
# binaries end up having dependencies on Homebrew libraries.
# Note that 'cmake -DWITH_SSL=<some path>'
# is NOT handled in the same way as 'cmake -DWITH_SSL=system'
# which means that for
# 'cmake -DWITH_SSL=/usr/local/opt/openssl'
#    or, on Apple silicon:
# 'cmake -DWITH_SSL=/opt/homebrew/opt/openssl'
# we will treat the libraries as external, and copy them into our build tree.
#
# On el7:
# pkg-config --libs openssl11
#    -L/usr/lib64/openssl11 -lssl -lcrypto
# pkg-config --cflags openssl11
#    -I/usr/include/openssl11

SET(MIN_OPENSSL_VERSION_REQUIRED "1.0.0")

SET(WITH_SSL_DOC "\nsystem (use the OS openssl library)")
SET(WITH_SSL_DOC "\nopenssl[0-9]+ (use alternative system library)")
STRING_APPEND(WITH_SSL_DOC "\nyes (synonym for system)")
STRING_APPEND(WITH_SSL_DOC "\n</path/to/custom/openssl/installation>")

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
      "Please see https://formulae.brew.sh/formula/openssl@1.1\n")
  ENDIF()
ENDMACRO()

MACRO(RESET_SSL_VARIABLES)
  UNSET(WITH_SSL_PATH)
  UNSET(WITH_SSL_PATH CACHE)
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
ENDMACRO(RESET_SSL_VARIABLES)

# Fetch OpenSSL version number.
# OpenSSL < 3:
# #define OPENSSL_VERSION_NUMBER 0x1000103fL
# Encoded as MNNFFPPS: major minor fix patch status
#
# OpenSSL 3:
# #define OPENSSL_VERSION_NUMBER
#   ( (OPENSSL_VERSION_MAJOR<<28)
#     |(OPENSSL_VERSION_MINOR<<20)
#     |(OPENSSL_VERSION_PATCH<<4)
#     |_OPENSSL_VERSION_PRE_RELEASE )
MACRO(FIND_OPENSSL_VERSION)
  FOREACH(version_part
      OPENSSL_VERSION_MAJOR
      OPENSSL_VERSION_MINOR
      OPENSSL_VERSION_PATCH
      )
    FILE(STRINGS "${OPENSSL_INCLUDE_DIR}/openssl/opensslv.h" ${version_part}
      REGEX "^#[\t ]*define[\t ]+${version_part}[\t ]+([0-9]+).*")
    STRING(REGEX REPLACE
      "^.*${version_part}[\t ]+([0-9]+).*" "\\1"
      ${version_part} "${${version_part}}")
  ENDFOREACH()
  IF(OPENSSL_VERSION_MAJOR VERSION_EQUAL 3)
    # OpenSSL 3
    SET(OPENSSL_FIX_VERSION "${OPENSSL_VERSION_PATCH}")
  ELSE()
    # Verify version number. Version information looks like:
    #   #define OPENSSL_VERSION_NUMBER 0x1000103fL
    # Encoded as MNNFFPPS: major minor fix patch status
    FILE(STRINGS "${OPENSSL_INCLUDE_DIR}/openssl/opensslv.h"
      OPENSSL_VERSION_NUMBER
      REGEX "^#[ ]*define[\t ]+OPENSSL_VERSION_NUMBER[\t ]+0x[0-9].*"
      )
    STRING(REGEX REPLACE
      "^.*OPENSSL_VERSION_NUMBER[\t ]+0x([0-9]).*$" "\\1"
      OPENSSL_VERSION_MAJOR "${OPENSSL_VERSION_NUMBER}"
      )
    STRING(REGEX REPLACE
      "^.*OPENSSL_VERSION_NUMBER[\t ]+0x[0-9]([0-9][0-9]).*$" "\\1"
      OPENSSL_VERSION_MINOR "${OPENSSL_VERSION_NUMBER}"
      )
    STRING(REGEX REPLACE
      "^.*OPENSSL_VERSION_NUMBER[\t ]+0x[0-9][0-9][0-9]([0-9][0-9]).*$" "\\1"
      OPENSSL_FIX_VERSION "${OPENSSL_VERSION_NUMBER}"
      )
  ENDIF()
  SET(OPENSSL_MAJOR_MINOR_FIX_VERSION "${OPENSSL_VERSION_MAJOR}")
  STRING_APPEND(OPENSSL_MAJOR_MINOR_FIX_VERSION ".${OPENSSL_VERSION_MINOR}")
  STRING_APPEND(OPENSSL_MAJOR_MINOR_FIX_VERSION ".${OPENSSL_FIX_VERSION}")
  MESSAGE(STATUS
    "OPENSSL_VERSION (${WITH_SSL}) is ${OPENSSL_MAJOR_MINOR_FIX_VERSION}")
ENDMACRO(FIND_OPENSSL_VERSION)

MACRO(FIND_ALTERNATIVE_SYSTEM_SSL)
  MYSQL_CHECK_PKGCONFIG()
  EXECUTE_PROCESS(COMMAND ${PKG_CONFIG_EXECUTABLE} --cflags ${WITH_SSL}
    OUTPUT_STRIP_TRAILING_WHITESPACE
    OUTPUT_VARIABLE OPENSSL_NN_FLAGS
    RESULT_VARIABLE OPENSSL_NN_FLAGS_RESULT
    ERROR_VARIABLE OPENSSL_NN_FLAGS_ERROR
    )
  IF(NOT OPENSSL_NN_FLAGS_RESULT EQUAL 0)
    MESSAGE(FATAL_ERROR "${OPENSSL_NN_FLAGS_ERROR}")
  ENDIF()
  EXECUTE_PROCESS(COMMAND ${PKG_CONFIG_EXECUTABLE} --libs ${WITH_SSL}
    OUTPUT_STRIP_TRAILING_WHITESPACE
    OUTPUT_VARIABLE OPENSSL_NN_LIBS
    RESULT_VARIABLE OPENSSL_NN_LIBS_RESULT
    ERROR_VARIABLE OPENSSL_NN_LIBS_ERROR
    )
  IF(NOT OPENSSL_NN_LIBS_RESULT EQUAL 0)
    MESSAGE(FATAL_ERROR "${OPENSSL_NN_LIBS_ERROR}")
  ENDIF()
  STRING(REPLACE "-I/" "/" OPENSSL_INCLUDE_DIR ${OPENSSL_NN_FLAGS})
  STRING(REPLACE "-L/" "/" OPENSSL_LIB_DIR ${OPENSSL_NN_LIBS})
  STRING(REPLACE " -lssl" "" OPENSSL_LIB_DIR ${OPENSSL_LIB_DIR})
  STRING(REPLACE " -lcrypto" "" OPENSSL_LIB_DIR ${OPENSSL_LIB_DIR})
  SET(ALTERNATIVE_SYSTEM_SSL 1)

  # First search in the alternative system directory.
  FIND_PATH(OPENSSL_ROOT_DIR
    NAMES openssl/ssl.h
    NO_CMAKE_PATH
    NO_CMAKE_ENVIRONMENT_PATH
    NO_DEFAULT_PATH
    HINTS ${OPENSSL_INCLUDE_DIR}
    )

  FIND_LIBRARY(OPENSSL_LIBRARY
    NAMES ssl
    NO_CMAKE_PATH
    NO_CMAKE_ENVIRONMENT_PATH
    NO_DEFAULT_PATH
    HINTS ${OPENSSL_LIB_DIR}
    )
  FIND_LIBRARY(CRYPTO_LIBRARY
    NAMES crypto
    NO_CMAKE_PATH
    NO_CMAKE_ENVIRONMENT_PATH
    NO_DEFAULT_PATH
    HINTS ${OPENSSL_LIB_DIR}
    )
  IF(NOT OPENSSL_ROOT_DIR OR
      NOT OPENSSL_LIBRARY OR
      NOT CRYPTO_LIBRARY)
    FATAL_SSL_NOT_FOUND_ERROR("Could not find system OpenSSL ${WITH_SSL}")
  ENDIF()
  SET(OPENSSL_SSL_LIBRARY ${OPENSSL_LIBRARY})
  SET(OPENSSL_CRYPTO_LIBRARY ${CRYPTO_LIBRARY})

  # Add support for bundled curl.
  ADD_LIBRARY(OpenSSL::SSL UNKNOWN IMPORTED)
  SET_TARGET_PROPERTIES(OpenSSL::SSL PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}")
  IF(EXISTS "${OPENSSL_SSL_LIBRARY}")
    SET_TARGET_PROPERTIES(OpenSSL::SSL PROPERTIES
      IMPORTED_LINK_INTERFACE_LANGUAGES "C"
      IMPORTED_LOCATION "${OPENSSL_SSL_LIBRARY}")
  ENDIF()

  # Add support for bundled curl.
  ADD_LIBRARY(OpenSSL::Crypto UNKNOWN IMPORTED)
  SET_TARGET_PROPERTIES(OpenSSL::Crypto PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}")
  IF(EXISTS "${OPENSSL_CRYPTO_LIBRARY}")
    SET_TARGET_PROPERTIES(OpenSSL::Crypto PROPERTIES
      IMPORTED_LINK_INTERFACE_LANGUAGES "C"
      IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIBRARY}")
  ENDIF()

ENDMACRO(FIND_ALTERNATIVE_SYSTEM_SSL)

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

  # A legacy option: used to be "system" or "bundled" (in that order)
  IF(WITH_SSL STREQUAL "yes")
    SET(WITH_SSL "system")
    SET(WITH_SSL "system" CACHE INTERNAL "Use system SSL libraries" FORCE)
  ENDIF()

  # On e.g. el7: openssl11, el8: openssl3
  STRING(REGEX MATCH "^openssl([0-9]+)$" UNUSED ${WITH_SSL})
  IF(CMAKE_MATCH_1)
    FIND_ALTERNATIVE_SYSTEM_SSL()
  ENDIF()

  IF(WITH_SSL STREQUAL "system" OR WITH_SSL_PATH OR ALTERNATIVE_SYSTEM_SSL)
    IF((APPLE OR WIN32) AND WITH_SSL STREQUAL "system")
      # FindOpenSSL.cmake knows about
      # http://www.slproweb.com/products/Win32OpenSSL.html
      # and will look for "C:/Program Files/OpenSSL-Win64/" (and others)
      # For APPLE we set the hint ${HOMEBREW_HOME}/openssl@1.1
      IF(LINK_STATIC_RUNTIME_LIBRARIES)
        SET(OPENSSL_MSVC_STATIC_RT ON)
      ENDIF()
      IF(APPLE AND NOT OPENSSL_ROOT_DIR)
        SET(OPENSSL_ROOT_DIR "${HOMEBREW_HOME}/openssl@1.1")
      ENDIF()
      # Treat "system" the same way as -DWITH_SSL=</path/to/custom/openssl>
      IF(WIN32 AND NOT OPENSSL_ROOT_DIR)
        # We want to be able to support 32bit client-only builds
        # FindOpenSSL.cmake will look for 32bit before 64bit ...
        # Note that several packages may come with ssl headers,
        # e.g. Strawberry Perl, so ignore some system paths below.
        FILE(TO_CMAKE_PATH "$ENV{PROGRAMFILES}" _programfiles)
        IF(SIZEOF_VOIDP EQUAL 8)
          FIND_PATH(OPENSSL_WIN32
            NAMES "include/openssl/ssl.h"
            PATHS "${_programfiles}/OpenSSL-Win32" "C:/OpenSSL-Win32/"
            NO_SYSTEM_ENVIRONMENT_PATH
            NO_CMAKE_SYSTEM_PATH
            )
          FIND_PATH(OPENSSL_WIN64
            NAMES  "include/openssl/ssl.h"
            PATHS "${_programfiles}/OpenSSL-Win64" "C:/OpenSSL-Win64/"
            NO_SYSTEM_ENVIRONMENT_PATH
            NO_CMAKE_SYSTEM_PATH
            )
          MESSAGE(STATUS "OPENSSL_WIN32 ${OPENSSL_WIN32}")
          MESSAGE(STATUS "OPENSSL_WIN64 ${OPENSSL_WIN64}")
          IF(OPENSSL_WIN64)
            IF(OPENSSL_WIN32)
              MESSAGE(STATUS "Found both 32bit and 64bit")
            ELSE()
              MESSAGE(STATUS "Found 64bit")
            ENDIF()
            SET(OPENSSL_ROOT_DIR ${OPENSSL_WIN64})
            MESSAGE(STATUS "OPENSSL_ROOT_DIR ${OPENSSL_ROOT_DIR}")
          ENDIF()
        ENDIF()
      ENDIF(WIN32 AND NOT OPENSSL_ROOT_DIR)

      # Input hint is OPENSSL_ROOT_DIR.
      # Will set OPENSSL_FOUND, OPENSSL_INCLUDE_DIR and others.
      FIND_PACKAGE(OpenSSL)

      IF(OPENSSL_FOUND)
        GET_FILENAME_COMPONENT(OPENSSL_ROOT_DIR ${OPENSSL_INCLUDE_DIR} PATH)
        SET(WITH_SSL_PATH "${OPENSSL_ROOT_DIR}" CACHE PATH "Path to system SSL")
      ELSE()
        RESET_SSL_VARIABLES()
        FATAL_SSL_NOT_FOUND_ERROR("Could not find system OpenSSL")
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
      IF(NOT OPENSSL_APPLINK_C)
        RESET_SSL_VARIABLES()
        FATAL_SSL_NOT_FOUND_ERROR(
          "Cannot find applink.c for WITH_SSL=${WITH_SSL}.")
      ENDIF()
    ENDIF()

    FIND_LIBRARY(OPENSSL_LIBRARY
                 NAMES ssl libssl ssleay32 ssleay32MD
                 HINTS ${OPENSSL_ROOT_DIR}/lib ${OPENSSL_ROOT_DIR}/lib64)
    FIND_LIBRARY(CRYPTO_LIBRARY
                 NAMES crypto libcrypto libeay32
                 HINTS ${OPENSSL_ROOT_DIR}/lib ${OPENSSL_ROOT_DIR}/lib64)

    IF(OPENSSL_INCLUDE_DIR)
      FIND_OPENSSL_VERSION()
    ENDIF()
    IF (OPENSSL_MAJOR_MINOR_FIX_VERSION VERSION_LESS
        ${MIN_OPENSSL_VERSION_REQUIRED})
      RESET_SSL_VARIABLES()
      FATAL_SSL_NOT_FOUND_ERROR(
        "Not a supported openssl version in WITH_SSL=${WITH_SSL}.")
    ENDIF()

    IF("${OPENSSL_MAJOR_MINOR_FIX_VERSION}" VERSION_GREATER "1.1.0")
       ADD_DEFINITIONS(-DHAVE_TLSv13)
    ENDIF()

    IF(OPENSSL_INCLUDE_DIR AND
       OPENSSL_LIBRARY     AND
       CRYPTO_LIBRARY
      )
      SET(OPENSSL_FOUND TRUE)
      IF(WITH_SSL_PATH)
        FIND_PROGRAM(OPENSSL_EXECUTABLE openssl
          NO_DEFAULT_PATH
          PATHS "${WITH_SSL_PATH}/bin"
          DOC "path to the openssl executable")
      ELSE()
        FIND_PROGRAM(OPENSSL_EXECUTABLE openssl)
      ENDIF()
      IF(OPENSSL_EXECUTABLE)
        SET(OPENSSL_EXECUTABLE_HAS_ZLIB 0)
        EXECUTE_PROCESS(
          COMMAND ${OPENSSL_EXECUTABLE} "list-cipher-commands"
          OUTPUT_VARIABLE stdout
          ERROR_VARIABLE  stderr
          RESULT_VARIABLE result
          OUTPUT_STRIP_TRAILING_WHITESPACE
          )
#       If previous command failed, try alternative command line (debian)
        IF(NOT result EQUAL 0)
          EXECUTE_PROCESS(
            COMMAND ${OPENSSL_EXECUTABLE} "list" "-cipher-commands"
            OUTPUT_VARIABLE stdout
            ERROR_VARIABLE  stderr
            RESULT_VARIABLE result
            OUTPUT_STRIP_TRAILING_WHITESPACE
            )
        ENDIF()
        IF(result EQUAL 0)
          STRING(REGEX REPLACE "[ \n]+" ";" CIPHER_COMMAND_LIST ${stdout})
          FOREACH(cipher_command ${CIPHER_COMMAND_LIST})
            IF(${cipher_command} STREQUAL "zlib")
              SET(OPENSSL_EXECUTABLE_HAS_ZLIB 1)
            ENDIF()
          ENDFOREACH()
          IF(OPENSSL_EXECUTABLE_HAS_ZLIB)
            MESSAGE(STATUS "The openssl command does support zlib")
          ELSE()
            MESSAGE(STATUS "The openssl command does not support zlib")
          ENDIF()
        ENDIF()
      ENDIF()
    ELSE()
      SET(OPENSSL_FOUND FALSE)
    ENDIF()

    SET(MY_CRYPTO_LIBRARY "${CRYPTO_LIBRARY}")
    SET(MY_OPENSSL_LIBRARY "${OPENSSL_LIBRARY}")

    # The whitespace here C:/Program Files/OpenSSL-Win64
    # creates problems for transitive library dependencies.
    # Copy the .lib files to the build directory, and link with the copies.
    IF(WIN32 AND WITH_SSL STREQUAL "system")
      CONFIGURE_FILE(${MY_CRYPTO_LIBRARY}
        "${CMAKE_BINARY_DIR}/copied_crypto.lib" COPYONLY)
      CONFIGURE_FILE(${MY_OPENSSL_LIBRARY}
        "${CMAKE_BINARY_DIR}/copied_openssl.lib" COPYONLY)
      SET(MY_CRYPTO_LIBRARY  "${CMAKE_BINARY_DIR}/copied_crypto.lib")
      SET(MY_OPENSSL_LIBRARY "${CMAKE_BINARY_DIR}/copied_openssl.lib")
    ENDIF()

    MESSAGE(STATUS "OPENSSL_INCLUDE_DIR = ${OPENSSL_INCLUDE_DIR}")
    MESSAGE(STATUS "OPENSSL_LIBRARY = ${OPENSSL_LIBRARY}")
    MESSAGE(STATUS "CRYPTO_LIBRARY = ${CRYPTO_LIBRARY}")
    MESSAGE(STATUS "OPENSSL_LIB_DIR = ${OPENSSL_LIB_DIR}")
    MESSAGE(STATUS "OPENSSL_VERSION_MAJOR = ${OPENSSL_VERSION_MAJOR}")
    MESSAGE(STATUS "OPENSSL_VERSION_MINOR = ${OPENSSL_VERSION_MINOR}")
    MESSAGE(STATUS "OPENSSL_FIX_VERSION = ${OPENSSL_FIX_VERSION}")

    INCLUDE(CheckSymbolExists)

    CMAKE_PUSH_CHECK_STATE()
    SET(CMAKE_REQUIRED_INCLUDES ${OPENSSL_INCLUDE_DIR})
    CHECK_SYMBOL_EXISTS(SHA512_DIGEST_LENGTH "openssl/sha.h"
                        HAVE_SHA512_DIGEST_LENGTH)
    CMAKE_POP_CHECK_STATE()

    IF(OPENSSL_FOUND AND HAVE_SHA512_DIGEST_LENGTH)
      SET(SSL_LIBRARIES ${MY_OPENSSL_LIBRARY} ${MY_CRYPTO_LIBRARY})
      IF(SOLARIS)
        SET(SSL_LIBRARIES ${SSL_LIBRARIES} ${LIBSOCKET})
      ENDIF()
      IF(LINUX)
        SET(SSL_LIBRARIES ${SSL_LIBRARIES} ${LIBDL})
      ENDIF()
      MESSAGE(STATUS "SSL_LIBRARIES = ${SSL_LIBRARIES}")
      INCLUDE_DIRECTORIES(SYSTEM ${OPENSSL_INCLUDE_DIR})
    ELSE()
      RESET_SSL_VARIABLES()
      FATAL_SSL_NOT_FOUND_ERROR(
        "Cannot find appropriate system libraries for WITH_SSL=${WITH_SSL}.")
    ENDIF()
  ELSEIF(ALTERNATIVE_SYSTEM_SSL)
    MESSAGE(STATUS "Using alternative system OpenSSL: ${WITH_SSL}")
  ELSE()
    RESET_SSL_VARIABLES()
    FATAL_SSL_NOT_FOUND_ERROR(
      "Wrong option or path for WITH_SSL=${WITH_SSL}.")
  ENDIF()
ENDMACRO(MYSQL_CHECK_SSL)

# If cmake is invoked with -DWITH_SSL=</path/to/custom/openssl>
# and we discover that the installation has dynamic libraries,
# then copy the dlls
# to runtime_output_directory (Windows),
# or library_output_directory (Mac/Linux).
# INSTALL(FILES ...) the shared libraries
# to INSTALL_BINDIR      (Windows)
# or INSTALL_LIBDIR      (Mac)
# or INSTALL_PRIV_LIBDIR (Linux)
MACRO(MYSQL_CHECK_SSL_DLLS)
  IF (WITH_SSL_PATH AND (APPLE OR WIN32 OR LINUX_STANDALONE OR LINUX_RPM))
    MESSAGE(STATUS "WITH_SSL_PATH ${WITH_SSL_PATH}")
    IF(LINUX)
      GET_FILENAME_COMPONENT(CRYPTO_EXT "${CRYPTO_LIBRARY}" EXT)
      GET_FILENAME_COMPONENT(OPENSSL_EXT "${OPENSSL_LIBRARY}" EXT)
      MESSAGE(STATUS "CRYPTO_EXT ${CRYPTO_EXT}")
      IF(CRYPTO_EXT STREQUAL ".so" AND OPENSSL_EXT STREQUAL ".so")
        SET(HAVE_CRYPTO_SO 1)
        SET(HAVE_OPENSSL_SO 1)
      ENDIF()
    ENDIF()
    IF(LINUX AND HAVE_CRYPTO_SO AND HAVE_OPENSSL_SO)
      # Save the fact that we have custom libraries to copy/install.
      SET(LINUX_WITH_CUSTOM_LIBRARIES 1 CACHE INTERNAL "")

      SET(CRYPTO_CUSTOM_LIBRARY "${CRYPTO_LIBRARY}" CACHE FILEPATH "")
      SET(OPENSSL_CUSTOM_LIBRARY "${OPENSSL_LIBRARY}" CACHE FILEPATH "")

      COPY_CUSTOM_SHARED_LIBRARY("${CRYPTO_CUSTOM_LIBRARY}" ""
        CRYPTO_LIBRARY crypto_target)
      COPY_CUSTOM_SHARED_LIBRARY("${OPENSSL_CUSTOM_LIBRARY}" ""
        OPENSSL_LIBRARY openssl_target)

      SET(SSL_LIBRARIES ${CRYPTO_LIBRARY} ${OPENSSL_LIBRARY})
      MESSAGE(STATUS "SSL_LIBRARIES = ${SSL_LIBRARIES}")

      ADD_CUSTOM_TARGET(copy_openssl_dlls
        DEPENDS ${crypto_target} ${openssl_target})

      COPY_OPENSSL_BINARY(${OPENSSL_EXECUTABLE} "" "" openssl_exe_target)
      ADD_DEPENDENCIES(${openssl_exe_target} copy_openssl_dlls)

    ENDIF(LINUX AND HAVE_CRYPTO_SO AND HAVE_OPENSSL_SO)

    IF(APPLE)
      GET_FILENAME_COMPONENT(CRYPTO_EXT "${CRYPTO_LIBRARY}" EXT)
      GET_FILENAME_COMPONENT(OPENSSL_EXT "${OPENSSL_LIBRARY}" EXT)
      MESSAGE(STATUS "CRYPTO_EXT ${CRYPTO_EXT}")
      IF(CRYPTO_EXT STREQUAL ".dylib" AND OPENSSL_EXT STREQUAL ".dylib")
        SET(HAVE_CRYPTO_DYLIB 1)
        SET(HAVE_OPENSSL_DYLIB 1)
        IF(WITH_SSL STREQUAL "system")
          MESSAGE(STATUS "Using system OpenSSL from Homebrew")
        ELSE()
          SET(APPLE_WITH_CUSTOM_SSL 1)
        ENDIF()
      ENDIF()
    ENDIF(APPLE)

    IF(APPLE_WITH_CUSTOM_SSL)
      # CRYPTO_LIBRARY is .../lib/libcrypto.dylib
      # CRYPTO_VERSION is .../lib/libcrypto.1.0.0.dylib
      EXECUTE_PROCESS(
        COMMAND readlink "${CRYPTO_LIBRARY}" OUTPUT_VARIABLE CRYPTO_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE)
      EXECUTE_PROCESS(
        COMMAND readlink "${OPENSSL_LIBRARY}" OUTPUT_VARIABLE OPENSSL_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE)

      # Replace dependency "/Volumes/.../lib/libcrypto.1.0.0.dylib
      EXECUTE_PROCESS(
        COMMAND otool -L "${OPENSSL_LIBRARY}"
        OUTPUT_VARIABLE OTOOL_OPENSSL_DEPS)
      STRING(REPLACE "\n" ";" DEPS_LIST ${OTOOL_OPENSSL_DEPS})
      FOREACH(LINE ${DEPS_LIST})
        STRING(REGEX MATCH "(/.*/lib/${CRYPTO_VERSION})" XXXXX ${LINE})
        IF(CMAKE_MATCH_1)
          SET(OPENSSL_DEPS "${CMAKE_MATCH_1}")
        ENDIF()
      ENDFOREACH()

      GET_FILENAME_COMPONENT(CRYPTO_DIRECTORY "${CRYPTO_LIBRARY}" DIRECTORY)
      GET_FILENAME_COMPONENT(OPENSSL_DIRECTORY "${OPENSSL_LIBRARY}" DIRECTORY)
      GET_FILENAME_COMPONENT(CRYPTO_NAME "${CRYPTO_LIBRARY}" NAME)
      GET_FILENAME_COMPONENT(OPENSSL_NAME "${OPENSSL_LIBRARY}" NAME)

      SET(CRYPTO_FULL_NAME "${CRYPTO_DIRECTORY}/${CRYPTO_VERSION}")
      SET(OPENSSL_FULL_NAME "${OPENSSL_DIRECTORY}/${OPENSSL_VERSION}")

      # Link with the copied libraries, rather than the original ones.
      SET(SSL_LIBRARIES
        ${CMAKE_BINARY_DIR}/library_output_directory/${CMAKE_CFG_INTDIR}/${OPENSSL_NAME}
        ${CMAKE_BINARY_DIR}/library_output_directory/${CMAKE_CFG_INTDIR}/${CRYPTO_NAME}
        )
      MESSAGE(STATUS)
      MESSAGE(STATUS "SSL_LIBRARIES = ${SSL_LIBRARIES}")

      # Do copying and dependency patching in a sub-process, so that we can
      # skip it if already done.  The BYPRODUCTS argument appears to be
      # necessary to allow Ninja (on macOS) to resolve dependencies on the dll
      # files directly, even if there is an explicit dependency on this target.
      # The BYPRODUCTS option is ignored on non-Ninja generators except to mark
      # byproducts GENERATED.
      ADD_CUSTOM_TARGET(copy_openssl_dlls ALL
        COMMAND ${CMAKE_COMMAND}
        -DCRYPTO_FULL_NAME="${CRYPTO_FULL_NAME}"
        -DCRYPTO_NAME="${CRYPTO_NAME}"
        -DCRYPTO_VERSION="${CRYPTO_VERSION}"
        -DOPENSSL_DEPS="${OPENSSL_DEPS}"
        -DOPENSSL_FULL_NAME="${OPENSSL_FULL_NAME}"
        -DOPENSSL_NAME="${OPENSSL_NAME}"
        -DOPENSSL_VERSION="${OPENSSL_VERSION}"
        -P ${CMAKE_SOURCE_DIR}/cmake/install_name_tool.cmake

        BYPRODUCTS
        "${CMAKE_BINARY_DIR}/library_output_directory/${CRYPTO_NAME}"
        "${CMAKE_BINARY_DIR}/library_output_directory/${OPENSSL_NAME}"

        WORKING_DIRECTORY
        "${CMAKE_BINARY_DIR}/library_output_directory/${CMAKE_CFG_INTDIR}"
        )

      COPY_OPENSSL_BINARY(${OPENSSL_EXECUTABLE}
        ${CRYPTO_VERSION} ${OPENSSL_VERSION}
        openssl_exe_target)
      ADD_DEPENDENCIES(${openssl_exe_target} copy_openssl_dlls)

      # Create symlinks for plugins, see MYSQL_ADD_PLUGIN/install_name_tool
      ADD_CUSTOM_TARGET(link_openssl_dlls ALL
        COMMAND ${CMAKE_COMMAND} -E create_symlink
          "../lib/${CRYPTO_VERSION}" "${CRYPTO_VERSION}"
        COMMAND ${CMAKE_COMMAND} -E create_symlink
          "../lib/${OPENSSL_VERSION}" "${OPENSSL_VERSION}"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/plugin_output_directory"

        BYPRODUCTS
        "${CMAKE_BINARY_DIR}/plugin_output_directory/${CRYPTO_VERSION}"
        "${CMAKE_BINARY_DIR}/plugin_output_directory/${OPENSSL_VERSION}"
        )
      # Create symlinks for plugins built with Xcode
      IF(NOT BUILD_IS_SINGLE_CONFIG)
        ADD_CUSTOM_TARGET(link_openssl_dlls_cmake_cfg_intdir ALL
          COMMAND ${CMAKE_COMMAND} -E create_symlink
          "../../lib/${CMAKE_CFG_INTDIR}/${CRYPTO_VERSION}" "${CRYPTO_VERSION}"
          COMMAND ${CMAKE_COMMAND} -E create_symlink
          "../../lib/${CMAKE_CFG_INTDIR}/${OPENSSL_VERSION}" "${OPENSSL_VERSION}"
          WORKING_DIRECTORY
          "${CMAKE_BINARY_DIR}/plugin_output_directory/${CMAKE_CFG_INTDIR}"

          BYPRODUCTS
          "${CMAKE_BINARY_DIR}/plugin_output_directory/${CMAKE_CFG_INTDIR}/${CRYPTO_VERSION}"
          "${CMAKE_BINARY_DIR}/plugin_output_directory/${CMAKE_CFG_INTDIR}/${OPENSSL_VERSION}"
        )
      ENDIF()

      # Directory layout after 'make install' is different.
      # Create some symlinks from lib/plugin/*.dylib to ../../lib/*.dylib
      FILE(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/plugin_output_directory/plugin")
      ADD_CUSTOM_TARGET(link_openssl_dlls_for_install ALL
        COMMAND ${CMAKE_COMMAND} -E create_symlink
          "../../lib/${CRYPTO_VERSION}" "${CRYPTO_VERSION}"
        COMMAND ${CMAKE_COMMAND} -E create_symlink
          "../../lib/${OPENSSL_VERSION}" "${OPENSSL_VERSION}"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/plugin_output_directory/plugin"
        )
      # See INSTALL_DEBUG_TARGET used for installing debug versions of plugins.
      IF(EXISTS ${DEBUGBUILDDIR})
        FILE(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/plugin_output_directory/plugin/debug")
        ADD_CUSTOM_TARGET(link_openssl_dlls_for_install_debug ALL
          COMMAND ${CMAKE_COMMAND} -E create_symlink
            "../../../lib/${CRYPTO_VERSION}" "${CRYPTO_VERSION}"
          COMMAND ${CMAKE_COMMAND} -E create_symlink
            "../../../lib/${OPENSSL_VERSION}" "${OPENSSL_VERSION}"
          WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/plugin_output_directory/plugin/debug"
        )
      ENDIF()

      MESSAGE(STATUS "INSTALL ${CRYPTO_NAME} to ${INSTALL_LIBDIR}")
      MESSAGE(STATUS "INSTALL ${OPENSSL_NAME} to ${INSTALL_LIBDIR}")

      # ${CMAKE_CFG_INTDIR} does not work with Xcode INSTALL ??
      IF(BUILD_IS_SINGLE_CONFIG)
        INSTALL(FILES
          ${CMAKE_BINARY_DIR}/library_output_directory/${CRYPTO_NAME}
          ${CMAKE_BINARY_DIR}/library_output_directory/${OPENSSL_NAME}
          ${CMAKE_BINARY_DIR}/library_output_directory/${CRYPTO_VERSION}
          ${CMAKE_BINARY_DIR}/library_output_directory/${OPENSSL_VERSION}
          DESTINATION "${INSTALL_LIBDIR}" COMPONENT SharedLibraries
          )
      ELSE()
        FOREACH(cfg Debug Release RelWithDebInfo MinSizeRel)
          INSTALL(FILES
            ${CMAKE_BINARY_DIR}/library_output_directory/${cfg}/${CRYPTO_NAME}
            ${CMAKE_BINARY_DIR}/library_output_directory/${cfg}/${OPENSSL_NAME}
            ${CMAKE_BINARY_DIR}/library_output_directory/${cfg}/${CRYPTO_VERSION}
            ${CMAKE_BINARY_DIR}/library_output_directory/${cfg}/${OPENSSL_VERSION}
            DESTINATION "${INSTALL_LIBDIR}" COMPONENT SharedLibraries
            CONFIGURATIONS ${cfg}
            )
        ENDFOREACH()
      ENDIF()
      INSTALL(FILES
        ${CMAKE_BINARY_DIR}/plugin_output_directory/plugin/${CRYPTO_VERSION}
        ${CMAKE_BINARY_DIR}/plugin_output_directory/plugin/${OPENSSL_VERSION}
        DESTINATION ${INSTALL_PLUGINDIR} COMPONENT SharedLibraries
        )
      # See INSTALL_DEBUG_TARGET used for installing debug versions of plugins.
      IF(EXISTS ${DEBUGBUILDDIR})
        INSTALL(FILES
          ${CMAKE_BINARY_DIR}/plugin_output_directory/plugin/debug/${CRYPTO_VERSION}
          ${CMAKE_BINARY_DIR}/plugin_output_directory/plugin/debug/${OPENSSL_VERSION}
          DESTINATION ${INSTALL_PLUGINDIR}/debug COMPONENT SharedLibraries
          )
      ENDIF()
    ENDIF(APPLE_WITH_CUSTOM_SSL)

    IF(WIN32)
      GET_FILENAME_COMPONENT(CRYPTO_NAME "${CRYPTO_LIBRARY}" NAME_WE)
      GET_FILENAME_COMPONENT(OPENSSL_NAME "${OPENSSL_LIBRARY}" NAME_WE)

      # Different naming scheme for the matching .dll as of SSL 1.1
      # OpenSSL 3.x Look for libcrypto-3-x64.dll or libcrypto-3.dll
      # OpenSSL 1.1 Look for libcrypto-1_1-x64.dll or libcrypto-1_1.dll
      # OpenSSL 1.0 Look for libeay32.dll
      SET(SSL_MSVC_VERSION_SUFFIX)
      SET(SSL_MSVC_ARCH_SUFFIX)
      IF(OPENSSL_VERSION_MAJOR VERSION_EQUAL 1 AND
         OPENSSL_VERSION_MINOR VERSION_EQUAL 1)
        SET(SSL_MSVC_VERSION_SUFFIX "-1_1")
        SET(SSL_MSVC_ARCH_SUFFIX "-x64")
      ENDIF()
      IF(OPENSSL_VERSION_MAJOR VERSION_EQUAL 3)
        SET(SSL_MSVC_VERSION_SUFFIX "-3")
        SET(SSL_MSVC_ARCH_SUFFIX "-x64")
      ENDIF()

      FIND_FILE(HAVE_CRYPTO_DLL
        NAMES
        "${CRYPTO_NAME}${SSL_MSVC_VERSION_SUFFIX}${SSL_MSVC_ARCH_SUFFIX}.dll"
        "${CRYPTO_NAME}${SSL_MSVC_VERSION_SUFFIX}.dll"
        PATHS "${WITH_SSL_PATH}/bin"
        NO_DEFAULT_PATH
        )
      FIND_FILE(HAVE_OPENSSL_DLL
        NAMES
        "${OPENSSL_NAME}${SSL_MSVC_VERSION_SUFFIX}${SSL_MSVC_ARCH_SUFFIX}.dll"
        "${OPENSSL_NAME}${SSL_MSVC_VERSION_SUFFIX}.dll"
        PATHS "${WITH_SSL_PATH}/bin"
        NO_DEFAULT_PATH
        )

      MESSAGE(STATUS "HAVE_CRYPTO_DLL ${HAVE_CRYPTO_DLL}")
      MESSAGE(STATUS "HAVE_OPENSSL_DLL ${HAVE_OPENSSL_DLL}")
      IF(HAVE_CRYPTO_DLL AND HAVE_OPENSSL_DLL)
        COPY_CUSTOM_DLL("${HAVE_CRYPTO_DLL}" OUTPUT_CRYPTO_TARGET)
        COPY_CUSTOM_DLL("${HAVE_OPENSSL_DLL}" OUTPUT_OPENSSL_TARGET)

        ADD_CUSTOM_TARGET(copy_openssl_dlls ALL)
        ADD_DEPENDENCIES(copy_openssl_dlls ${OUTPUT_CRYPTO_TARGET})
        ADD_DEPENDENCIES(copy_openssl_dlls ${OUTPUT_OPENSSL_TARGET})

        COPY_OPENSSL_BINARY(${OPENSSL_EXECUTABLE} "" "" openssl_exe_target)
        ADD_DEPENDENCIES(${openssl_exe_target} copy_openssl_dlls)
      ELSE()
        MESSAGE(STATUS "Cannot find SSL dynamic libraries")
        IF(OPENSSL_VERSION_MAJOR VERSION_EQUAL 1 AND
           OPENSSL_VERSION_MINOR VERSION_EQUAL 1)
          SET(SSL_LIBRARIES ${SSL_LIBRARIES} crypt32.lib)
          MESSAGE(STATUS "SSL_LIBRARIES ${SSL_LIBRARIES}")
        ENDIF()
      ENDIF()
    ENDIF()
  ENDIF()
ENDMACRO(MYSQL_CHECK_SSL_DLLS)

# Downgrade OpenSSL 3 deprecation warnings.
MACRO(DOWNGRADE_OPENSSL3_DEPRECATION_WARNINGS)
  IF(OPENSSL_VERSION_MAJOR VERSION_EQUAL 3)
    IF(MY_COMPILER_IS_GNU_OR_CLANG)
      ADD_COMPILE_FLAGS(${ARGV}
        COMPILE_FLAGS "-Wno-error=deprecated-declarations")
    ELSEIF(WIN32)
      ADD_COMPILE_FLAGS(${ARGV}
        COMPILE_FLAGS "/wd4996")
    ENDIF()
  ENDIF()
ENDMACRO()
