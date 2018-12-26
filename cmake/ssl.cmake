# Copyright (c) 2009, 2018, Oracle and/or its affiliates. All rights reserved.
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
# - "system"  (typically) uses headers/libraries in /usr/lib and /usr/lib64
# - a custom installation of openssl can be used like this
#     - cmake -DCMAKE_PREFIX_PATH=</path/to/custom/openssl> -DWITH_SSL="system"
#   or
#     - cmake -DWITH_SSL=</path/to/custom/openssl>
# - "wolfssl" uses wolfssl source code in <source dir>/extra/wolfssl-<version>
#
# The default value for WITH_SSL is "system"
# set in cmake/build_configurations/feature_set.cmake
#
# WITH_SSL="system" means: use the SSL library that comes with the operating
# system. This typically means you have to do 'yum install openssl-devel'
# or something similar.
#
# For Windows or OsX, WITH_SSL="system" is handled a bit differently:
# We assume you have installed
#     https://slproweb.com/products/Win32OpenSSL.html
#     find_package(OpenSSL) will locate it
# or
#     http://brewformulas.org/Openssl
#     we give a hint /usr/local/opt/openssl to find_package(OpenSSL)
# When the package has been located, we treat it as if cmake had been
# invoked with  -DWITH_SSL=</path/to/custom/openssl>


SET(WITH_SSL_DOC "\nsystem (use the OS openssl library)")
SET(WITH_SSL_DOC
  "${WITH_SSL_DOC}, \nyes (synonym for system)")
SET(WITH_SSL_DOC
  "${WITH_SSL_DOC}, \n</path/to/custom/openssl/installation>")
SET(WITH_SSL_DOC
  "${WITH_SSL_DOC}, \nwolfssl (use wolfSSL. See extra/README-wolfssl.txt on how to set this up)")

STRING(REPLACE "\n" "| " WITH_SSL_DOC_STRING "${WITH_SSL_DOC}")
MACRO (CHANGE_SSL_SETTINGS string)
  SET(WITH_SSL ${string} CACHE STRING ${WITH_SSL_DOC_STRING} FORCE)
ENDMACRO()

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

MACRO (MYSQL_USE_WOLFSSL)
  SET(WOLFSSL_VERSION "3.14.0")
  SET(WOLFSSL_SOURCE_DIR "${CMAKE_SOURCE_DIR}/extra/wolfssl-${WOLFSSL_VERSION}")
  MESSAGE(STATUS "WOLFSSL_SOURCE_DIR = ${WOLFSSL_SOURCE_DIR}")

  SET(INC_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${WOLFSSL_SOURCE_DIR}
    ${WOLFSSL_SOURCE_DIR}/wolfssl
    ${WOLFSSL_SOURCE_DIR}/wolfssl/wolfcrypt
  )
  SET(SSL_LIBRARIES  wolfssl wolfcrypt)
  IF(CMAKE_SYSTEM_NAME MATCHES "SunOS")
    SET(SSL_LIBRARIES ${SSL_LIBRARIES} ${LIBSOCKET})
  ENDIF()
  INCLUDE_DIRECTORIES(SYSTEM ${INC_DIRS})
  SET(SSL_INTERNAL_INCLUDE_DIRS ${WOLFSSL_SOURCE_DIR})
  ADD_DEFINITIONS(
    -DBUILDING_WOLFSSL
    -DHAVE_ECC
    -DHAVE_HASHDRBG
    -DHAVE_WOLFSSL
    -DKEEP_OUR_CERT
    -DMULTI_THREADED
    -DOPENSSL_EXTRA
    -DSESSION_CERT
    -DWC_NO_HARDEN
    -DWOLFSSL_AES_DIRECT
    -DWOLFSSL_ALLOW_TLSV10
    -DWOLFSSL_CERT_EXT
    -DWOLFSSL_MYSQL_COMPATIBLE
    -DWOLFSSL_SHA224
    -DWOLFSSL_SHA384
    -DWOLFSSL_SHA512
    -DWOLFSSL_STATIC_RSA
    -DWOLFSSL_CERT_GEN
    )
  CHANGE_SSL_SETTINGS("wolfssl")
  ADD_SUBDIRECTORY(${WOLFSSL_SOURCE_DIR})
  ADD_SUBDIRECTORY(${WOLFSSL_SOURCE_DIR}/wolfcrypt)
  GET_TARGET_PROPERTY(src wolfssl SOURCES)
  FOREACH(file ${src})
    SET(SSL_SOURCES ${SSL_SOURCES} ${WOLFSSL_SOURCE_DIR}/${file})
  ENDFOREACH()
  GET_TARGET_PROPERTY(src wolfcrypt SOURCES)
  FOREACH(file ${src})
    SET(SSL_SOURCES ${SSL_SOURCES}
      ${WOLFSSL_SOURCE_DIR}/wolfcrypt/${file})
  ENDFOREACH()
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
ENDMACRO()

# MYSQL_CHECK_SSL
#
# Provides the following configure options:
# WITH_SSL=[yes|wolfssl|system|<path/to/custom/installation>]
MACRO (MYSQL_CHECK_SSL)

  IF(NOT WITH_SSL)
    CHANGE_SSL_SETTINGS("system")
  ENDIF()

  IF(WITH_SSL STREQUAL "bundled")
    MESSAGE(WARNING
      "bundled SSL (YaSSL) is no longer supported, changed to system"
      )
    RESET_SSL_VARIABLES()
    CHANGE_SSL_SETTINGS("system")
  ENDIF()

  # See if WITH_SSL is of the form </path/to/custom/installation>
  FILE(GLOB WITH_SSL_HEADER ${WITH_SSL}/include/openssl/ssl.h)
  IF (WITH_SSL_HEADER)
    FILE(TO_CMAKE_PATH "${WITH_SSL}" WITH_SSL)
    SET(WITH_SSL_PATH ${WITH_SSL} CACHE PATH "path to custom SSL installation")
    SET(WITH_SSL_PATH ${WITH_SSL})
  ENDIF()

  IF(WITH_SSL STREQUAL "wolfssl")
    MYSQL_USE_WOLFSSL()
    # Reset some variables, in case we switch from /path/to/ssl to "wolfssl".
    IF (WITH_SSL_PATH)
      UNSET(WITH_SSL_PATH)
      UNSET(WITH_SSL_PATH CACHE)
    ENDIF()
    IF (OPENSSL_ROOT_DIR)
      UNSET(OPENSSL_ROOT_DIR)
      UNSET(OPENSSL_ROOT_DIR CACHE)
    ENDIF()
    IF (OPENSSL_INCLUDE_DIR)
      UNSET(OPENSSL_INCLUDE_DIR)
      UNSET(OPENSSL_INCLUDE_DIR CACHE)
    ENDIF()
    IF (WIN32 AND OPENSSL_APPLINK_C)
      UNSET(OPENSSL_APPLINK_C)
      UNSET(OPENSSL_APPLINK_C CACHE)
    ENDIF()
    IF (OPENSSL_LIBRARY)
      UNSET(OPENSSL_LIBRARY)
      UNSET(OPENSSL_LIBRARY CACHE)
    ENDIF()
    IF (CRYPTO_LIBRARY)
      UNSET(CRYPTO_LIBRARY)
      UNSET(CRYPTO_LIBRARY CACHE)
    ENDIF()
  ELSEIF(WITH_SSL STREQUAL "system" OR
      WITH_SSL STREQUAL "yes" OR
      WITH_SSL_PATH
      )
    # Treat "system" the same way as -DWITH_SSL=</path/to/custom/openssl>
    IF((APPLE OR WIN32) AND WITH_SSL STREQUAL "system")
      # FindOpenSSL.cmake knows about
      # http://www.slproweb.com/products/Win32OpenSSL.html
      # and will look for "C:/OpenSSL-Win64/" (and others)
      # For APPLE we set the hint /usr/local/opt/openssl
      IF(LINK_STATIC_RUNTIME_LIBRARIES)
        SET(OPENSSL_MSVC_STATIC_RT ON)
      ENDIF()
      IF(APPLE AND NOT OPENSSL_ROOT_DIR)
        SET(OPENSSL_ROOT_DIR "/usr/local/opt/openssl")
      ENDIF()
      IF(WIN32 AND NOT OPENSSL_ROOT_DIR)
        # We want to be able to support 32bit client-only builds
        # FindOpenSSL.cmake will look for 32bit before 64bit ...
        FILE(TO_CMAKE_PATH "$ENV{PROGRAMFILES}" _programfiles)
        IF(SIZEOF_VOIDP EQUAL 8)
          FIND_PATH(OPENSSL_WIN32
            NAMES "include/openssl/ssl.h"
            PATHS "${_programfiles}/OpenSSL-Win32" "C:/OpenSSL-Win32/")
          FIND_PATH(OPENSSL_WIN64
            NAMES  "include/openssl/ssl.h"
            PATHS "${_programfiles}/OpenSSL-Win64" "C:/OpenSSL-Win64/")
          MESSAGE(STATUS "OPENSSL_WIN32 ${OPENSSL_WIN32}")
          MESSAGE(STATUS "OPENSSL_WIN64 ${OPENSSL_WIN64}")
          IF(OPENSSL_WIN32 AND OPENSSL_WIN64)
            MESSAGE(STATUS "Found both 32bit and 64bit")
            SET(OPENSSL_ROOT_DIR ${OPENSSL_WIN64})
            MESSAGE(STATUS "OPENSSL_ROOT_DIR ${OPENSSL_ROOT_DIR}")
          ENDIF()
        ENDIF()
      ENDIF()
      FIND_PACKAGE(OpenSSL)
      IF(OPENSSL_FOUND)
        GET_FILENAME_COMPONENT(OPENSSL_ROOT_DIR ${OPENSSL_INCLUDE_DIR} PATH)
        MESSAGE(STATUS "system OpenSSL has root ${OPENSSL_ROOT_DIR}")
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
        HINTS ${OPENSSL_ROOT_DIR}/include
      )
      MESSAGE(STATUS "OPENSSL_APPLINK_C ${OPENSSL_APPLINK_C}")
    ENDIF()

    IF(LINUX AND INSTALL_LAYOUT MATCHES "STANDALONE")
      SET(LINUX_STANDALONE 1)
    ENDIF()

    # On mac this list is <.dylib;.so;.a>
    # On most platforms we still prefer static libraries, so we revert it here.
    IF (WITH_SSL_PATH AND NOT APPLE AND NOT LINUX_STANDALONE)
      LIST(REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
      MESSAGE(STATUS "suffixes <${CMAKE_FIND_LIBRARY_SUFFIXES}>")
    ENDIF()
    FIND_LIBRARY(OPENSSL_LIBRARY
                 NAMES ssl libssl ssleay32 ssleay32MD
                 HINTS ${OPENSSL_ROOT_DIR}/lib)
    FIND_LIBRARY(CRYPTO_LIBRARY
                 NAMES crypto libcrypto libeay32
                 HINTS ${OPENSSL_ROOT_DIR}/lib)
    IF (WITH_SSL_PATH AND NOT APPLE AND NOT LINUX_STANDALONE)
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
    ENDIF()
    IF(OPENSSL_INCLUDE_DIR AND
       OPENSSL_LIBRARY   AND
       CRYPTO_LIBRARY      AND
       OPENSSL_MAJOR_VERSION STREQUAL "1"
      )
      SET(OPENSSL_FOUND TRUE)
      FIND_PROGRAM(OPENSSL_EXECUTABLE openssl
        DOC "path to the openssl executable")
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

    # If we are invoked with -DWITH_SSL=/path/to/custom/openssl
    # and we have found static libraries, then link them statically
    # into our executables and libraries.
    # Adding IMPORTED_LOCATION allows MERGE_CONVENIENCE_LIBRARIES
    # to merge imported libraries as well as our own libraries.
    SET(MY_CRYPTO_LIBRARY "${CRYPTO_LIBRARY}")
    SET(MY_OPENSSL_LIBRARY "${OPENSSL_LIBRARY}")
    IF (WITH_SSL_PATH)
      GET_FILENAME_COMPONENT(CRYPTO_EXT "${CRYPTO_LIBRARY}" EXT)
      GET_FILENAME_COMPONENT(OPENSSL_EXT "${OPENSSL_LIBRARY}" EXT)
      IF (CRYPTO_EXT STREQUAL ".a" OR OPENSSL_EXT STREQUAL ".lib")
        SET(MY_CRYPTO_LIBRARY imported_crypto)
        ADD_IMPORTED_LIBRARY(imported_crypto "${CRYPTO_LIBRARY}")
      ENDIF()
      IF (OPENSSL_EXT STREQUAL ".a" OR OPENSSL_EXT STREQUAL ".lib")
        SET(MY_OPENSSL_LIBRARY imported_openssl)
        ADD_IMPORTED_LIBRARY(imported_openssl "${OPENSSL_LIBRARY}")
      ENDIF()
    ENDIF()

    MESSAGE(STATUS "OPENSSL_INCLUDE_DIR = ${OPENSSL_INCLUDE_DIR}")
    MESSAGE(STATUS "OPENSSL_LIBRARY = ${OPENSSL_LIBRARY}")
    MESSAGE(STATUS "CRYPTO_LIBRARY = ${CRYPTO_LIBRARY}")
    MESSAGE(STATUS "OPENSSL_MAJOR_VERSION = ${OPENSSL_MAJOR_VERSION}")
    MESSAGE(STATUS "OPENSSL_MINOR_VERSION = ${OPENSSL_MINOR_VERSION}")
    # The server hangs in OpenSSL_add_all_algorithms() in ssl_start()
    IF(WIN32 AND OPENSSL_MINOR_VERSION VERSION_EQUAL 1)
      MESSAGE(WARNING "OpenSSL 1.1 is experimental on Windows")
    ENDIF()

    INCLUDE(CheckSymbolExists)
    SET(CMAKE_REQUIRED_INCLUDES ${OPENSSL_INCLUDE_DIR})
    CHECK_SYMBOL_EXISTS(SHA512_DIGEST_LENGTH "openssl/sha.h"
                        HAVE_SHA512_DIGEST_LENGTH)
    IF(OPENSSL_FOUND AND HAVE_SHA512_DIGEST_LENGTH)
      SET(SSL_SOURCES "")
      SET(SSL_LIBRARIES ${MY_OPENSSL_LIBRARY} ${MY_CRYPTO_LIBRARY})
      IF(CMAKE_SYSTEM_NAME MATCHES "SunOS")
        SET(SSL_LIBRARIES ${SSL_LIBRARIES} ${LIBSOCKET})
      ENDIF()
      IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
        SET(SSL_LIBRARIES ${SSL_LIBRARIES} ${LIBDL})
      ENDIF()
      MESSAGE(STATUS "SSL_LIBRARIES = ${SSL_LIBRARIES}")
      INCLUDE_DIRECTORIES(SYSTEM ${OPENSSL_INCLUDE_DIR})
      SET(SSL_INTERNAL_INCLUDE_DIRS "")
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

# If cmake is invoked with -DWITH_SSL=</path/to/custom/openssl>
# and we discover that the installation has dynamic libraries,
# then copy the dlls to runtime_output_directory, and add INSTALL them.
# Currently only relevant for Windows, Mac and Linux.
MACRO(MYSQL_CHECK_SSL_DLLS)
  IF (WITH_SSL_PATH AND (APPLE OR WIN32 OR LINUX_STANDALONE))
    MESSAGE(STATUS "WITH_SSL_PATH ${WITH_SSL_PATH}")
    IF(LINUX_STANDALONE)
      GET_FILENAME_COMPONENT(CRYPTO_EXT "${CRYPTO_LIBRARY}" EXT)
      GET_FILENAME_COMPONENT(OPENSSL_EXT "${OPENSSL_LIBRARY}" EXT)
      MESSAGE(STATUS "CRYPTO_EXT ${CRYPTO_EXT}")
      IF(CRYPTO_EXT STREQUAL ".so" AND OPENSSL_EXT STREQUAL ".so")
        MESSAGE(STATUS "set HAVE_CRYPTO_SO HAVE_OPENSSL_SO")
        SET(HAVE_CRYPTO_SO 1)
        SET(HAVE_OPENSSL_SO 1)
      ENDIF()
    ENDIF()
    IF(LINUX_STANDALONE AND HAVE_CRYPTO_SO AND HAVE_OPENSSL_SO)
      EXECUTE_PROCESS(
        COMMAND readlink "${CRYPTO_LIBRARY}" OUTPUT_VARIABLE CRYPTO_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE)
      EXECUTE_PROCESS(
        COMMAND readlink "${OPENSSL_LIBRARY}" OUTPUT_VARIABLE OPENSSL_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE)
      MESSAGE(STATUS "CRYPTO_VERSION ${CRYPTO_VERSION}")
      MESSAGE(STATUS "OPENSSL_VERSION ${OPENSSL_VERSION}")

      SET(LINUX_INSTALL_RPATH_ORIGIN 1)

      GET_FILENAME_COMPONENT(CRYPTO_DIRECTORY "${CRYPTO_LIBRARY}" DIRECTORY)
      GET_FILENAME_COMPONENT(OPENSSL_DIRECTORY "${OPENSSL_LIBRARY}" DIRECTORY)
      GET_FILENAME_COMPONENT(CRYPTO_NAME "${CRYPTO_LIBRARY}" NAME)
      GET_FILENAME_COMPONENT(OPENSSL_NAME "${OPENSSL_LIBRARY}" NAME)

      SET(CRYPTO_FULL_NAME "${CRYPTO_DIRECTORY}/${CRYPTO_VERSION}")
      SET(OPENSSL_FULL_NAME "${OPENSSL_DIRECTORY}/${OPENSSL_VERSION}")

      ADD_CUSTOM_TARGET(copy_openssl_dlls ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CRYPTO_FULL_NAME}" "./${CRYPTO_VERSION}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${OPENSSL_FULL_NAME}" "./${OPENSSL_VERSION}"
        COMMAND ${CMAKE_COMMAND} -E create_symlink
          "${CRYPTO_VERSION}" "${CRYPTO_NAME}"
        COMMAND ${CMAKE_COMMAND} -E create_symlink
          "${OPENSSL_VERSION}" "${OPENSSL_NAME}"

        WORKING_DIRECTORY
        "${CMAKE_BINARY_DIR}/library_output_directory"
        )

      # Create symlinks for executables
      ADD_CUSTOM_TARGET(link_openssl_dlls_exe ALL
        COMMAND ${CMAKE_COMMAND} -E create_symlink
        "../lib/${CRYPTO_VERSION}" "${CRYPTO_VERSION}"
        COMMAND ${CMAKE_COMMAND} -E create_symlink
        "../lib/${OPENSSL_VERSION}" "${OPENSSL_VERSION}"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/runtime_output_directory"
      )

      # Create symlinks for plugins
      ADD_CUSTOM_TARGET(link_openssl_dlls ALL
        COMMAND ${CMAKE_COMMAND} -E create_symlink
          "../lib/${CRYPTO_VERSION}" "${CRYPTO_VERSION}"
        COMMAND ${CMAKE_COMMAND} -E create_symlink
          "../lib/${OPENSSL_VERSION}" "${OPENSSL_VERSION}"
#         COMMAND ${CMAKE_COMMAND} -E create_symlink
#           "../lib/${CRYPTO_NAME}" "${CRYPTO_NAME}"
#         COMMAND ${CMAKE_COMMAND} -E create_symlink
#           "../lib/${OPENSSL_NAME}" "${OPENSSL_NAME}"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/plugin_output_directory"
        )

      # Directory layout after 'make install' is different.
      # Create some symlinks from lib/plugin/*.so to ../../lib/*.so
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
        FILE(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/plugin_output_directory/plugin/Debug")
        ADD_CUSTOM_TARGET(link_openssl_dlls_for_install_debug ALL
          COMMAND ${CMAKE_COMMAND} -E create_symlink
            "../../../lib/${CRYPTO_VERSION}" "${CRYPTO_VERSION}"
          COMMAND ${CMAKE_COMMAND} -E create_symlink
            "../../../lib/${OPENSSL_VERSION}" "${OPENSSL_VERSION}"
          WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/plugin_output_directory/plugin/Debug"
        )
      ENDIF()

      MESSAGE(STATUS "INSTALL ${CRYPTO_NAME} to ${INSTALL_LIBDIR}")
      MESSAGE(STATUS "INSTALL ${OPENSSL_NAME} to ${INSTALL_LIBDIR}")

      INSTALL(FILES
        ${CMAKE_BINARY_DIR}/runtime_output_directory/${CRYPTO_VERSION}
        ${CMAKE_BINARY_DIR}/runtime_output_directory/${OPENSSL_VERSION}
        DESTINATION "${INSTALL_BINDIR}" COMPONENT SharedLibraries
        )
      INSTALL(FILES
        ${CMAKE_BINARY_DIR}/library_output_directory/${CRYPTO_NAME}
        ${CMAKE_BINARY_DIR}/library_output_directory/${OPENSSL_NAME}
        ${CMAKE_BINARY_DIR}/library_output_directory/${CRYPTO_VERSION}
        ${CMAKE_BINARY_DIR}/library_output_directory/${OPENSSL_VERSION}
        DESTINATION "${INSTALL_LIBDIR}" COMPONENT SharedLibraries
        )
      INSTALL(FILES
        ${CMAKE_BINARY_DIR}/plugin_output_directory/plugin/${CRYPTO_VERSION}
        ${CMAKE_BINARY_DIR}/plugin_output_directory/plugin/${OPENSSL_VERSION}
        DESTINATION "${INSTALL_PLUGINDIR}" COMPONENT SharedLibraries
        )

      # See INSTALL_DEBUG_TARGET used for installing debug versions of plugins.
      IF(EXISTS ${DEBUGBUILDDIR})
        INSTALL(FILES
          ${CMAKE_BINARY_DIR}/plugin_output_directory/plugin/Debug/${CRYPTO_VERSION}
          ${CMAKE_BINARY_DIR}/plugin_output_directory/plugin/Debug/${OPENSSL_VERSION}
          DESTINATION ${INSTALL_PLUGINDIR}/debug COMPONENT SharedLibraries
          )
      ENDIF()

      SET(CMAKE_C_LINK_FLAGS
        "${CMAKE_C_LINK_FLAGS} -Wl,-rpath,'\$ORIGIN/'")
      SET(CMAKE_CXX_LINK_FLAGS
        "${CMAKE_CXX_LINK_FLAGS} -Wl,-rpath,'\$ORIGIN/'")
      SET(CMAKE_MODULE_LINKER_FLAGS
        "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-rpath,'\$ORIGIN/'")
      SET(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-rpath,'\$ORIGIN/'")
      MESSAGE(STATUS
        "CMAKE_C_LINK_FLAGS ${CMAKE_C_LINK_FLAGS}")
      MESSAGE(STATUS
        "CMAKE_CXX_LINK_FLAGS ${CMAKE_CXX_LINK_FLAGS}")
      MESSAGE(STATUS
        "CMAKE_MODULE_LINKER_FLAGS ${CMAKE_MODULE_LINKER_FLAGS}")
      MESSAGE(STATUS
        "CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS}")

    ENDIF()

    IF(APPLE)
      GET_FILENAME_COMPONENT(CRYPTO_EXT "${CRYPTO_LIBRARY}" EXT)
      GET_FILENAME_COMPONENT(OPENSSL_EXT "${OPENSSL_LIBRARY}" EXT)
      MESSAGE(STATUS "CRYPTO_EXT ${CRYPTO_EXT}")
      IF(CRYPTO_EXT STREQUAL ".dylib" AND OPENSSL_EXT STREQUAL ".dylib")
        MESSAGE(STATUS "set HAVE_CRYPTO_DYLIB HAVE_OPENSSL_DYLIB")
        SET(HAVE_CRYPTO_DYLIB 1)
        SET(HAVE_OPENSSL_DYLIB 1)
      ENDIF()
    ENDIF()
    IF(APPLE AND HAVE_CRYPTO_DYLIB AND HAVE_OPENSSL_DYLIB)
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
      MESSAGE(STATUS "SSL_LIBRARIES = ${SSL_LIBRARIES}")

      # Do copying and dependency patching in a sub-process, so that we can
      # skip it if already done.  The BYPRODUCTS argument appears to be
      # necessary to allow Ninja (on MacOS) to resolve dependencies on the dll
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

      # Create symlinks for plugins, see MYSQL_ADD_PLUGIN/install_name_tool
      ADD_CUSTOM_TARGET(link_openssl_dlls ALL
        COMMAND ${CMAKE_COMMAND} -E create_symlink
          "../lib/${CRYPTO_VERSION}" "${CRYPTO_VERSION}"
        COMMAND ${CMAKE_COMMAND} -E create_symlink
          "../lib/${OPENSSL_VERSION}" "${OPENSSL_VERSION}"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/plugin_output_directory"
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
      SET(SUBDIRECTORY "")
      IF(CMAKE_BUILD_TYPE AND NOT BUILD_IS_SINGLE_CONFIG AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        SET(SUBDIRECTORY "Debug")
      ELSEIF(CMAKE_BUILD_TYPE AND NOT BUILD_IS_SINGLE_CONFIG AND CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        SET(SUBDIRECTORY "RelWithDebInfo")
      ENDIF()
      INSTALL(FILES
        ${CMAKE_BINARY_DIR}/library_output_directory/${SUBDIRECTORY}/${CRYPTO_NAME}
        ${CMAKE_BINARY_DIR}/library_output_directory/${SUBDIRECTORY}/${OPENSSL_NAME}
        ${CMAKE_BINARY_DIR}/library_output_directory/${SUBDIRECTORY}/${CRYPTO_VERSION}
        ${CMAKE_BINARY_DIR}/library_output_directory/${SUBDIRECTORY}/${OPENSSL_VERSION}
        DESTINATION "${INSTALL_LIBDIR}" COMPONENT SharedLibraries
        )
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
    ENDIF()

    IF(WIN32)
      GET_FILENAME_COMPONENT(CRYPTO_NAME "${CRYPTO_LIBRARY}" NAME_WE)
      GET_FILENAME_COMPONENT(OPENSSL_NAME "${OPENSSL_LIBRARY}" NAME_WE)

      # Different naming scheme for the matching .dll as of SSL 1.1
      SET(SSL_MSVC_VERSION_SUFFIX)
      SET(SSL_MSVC_ARCH_SUFFIX)
      IF(OPENSSL_MINOR_VERSION VERSION_EQUAL 1)
        SET(SSL_MSVC_VERSION_SUFFIX "-1_1")
        SET(SSL_MSVC_ARCH_SUFFIX "-x64")
      ENDIF()

      # OpenSSL 1.1 Look for libcrypto-1_1-x64.dll or libcrypto-1_1.dll
      # OpenSSL 1.0 Look for libeay32.dll
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
        GET_FILENAME_COMPONENT(CRYPTO_DLL_NAME "${HAVE_CRYPTO_DLL}" NAME)
        GET_FILENAME_COMPONENT(OPENSSL_DLL_NAME "${HAVE_OPENSSL_DLL}" NAME)
        ADD_CUSTOM_TARGET(copy_openssl_dlls ALL
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${HAVE_CRYPTO_DLL}"
          "${CMAKE_BINARY_DIR}/runtime_output_directory/${CMAKE_CFG_INTDIR}/${CRYPTO_DLL_NAME}"
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${HAVE_OPENSSL_DLL}"
          "${CMAKE_BINARY_DIR}/runtime_output_directory/${CMAKE_CFG_INTDIR}/${OPENSSL_DLL_NAME}"
          )
        MESSAGE(STATUS "INSTALL ${HAVE_CRYPTO_DLL} to ${INSTALL_BINDIR}")
        MESSAGE(STATUS "INSTALL ${HAVE_OPENSSL_DLL} to ${INSTALL_BINDIR}")
        INSTALL(FILES
          "${HAVE_CRYPTO_DLL}"
          "${HAVE_OPENSSL_DLL}"
          DESTINATION "${INSTALL_BINDIR}" COMPONENT SharedLibraries)
      ELSE()
        MESSAGE(STATUS "Cannot find SSL dynamic libraries")
      ENDIF()
    ENDIF()
  ENDIF()
ENDMACRO()
