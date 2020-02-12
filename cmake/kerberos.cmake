# Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
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

# cmake -DWITH_KERBEROS=system|<path/to/custom/installation>|none
# system is the default
# none will diable the kerberos build
#
# Following algorithm will be used to find kerberos library.
# 1. If "WITH_KERBEROS" is set to system or not configured,
#    we will search kerberos lib in system directory.
#    System is used for unix alike OS.
#    For windows we dont have system kerberos library.
# 2. If kerberos library path is provided using "WITH_KERBEROS" option,
#    we will search kerberos library in that path.
#    This will be used to build MySQL with custom kerberos library.
# 3. If kerberos library and header is found, kerberos library path,
#    kerberos header path and KERBEROS_LIB_CONFIGURED will be set.
# 4. For windows, we need to install kerberos library from web,
#    https://web.mit.edu/kerberos/kfw-4.1/kfw-4.1.html.

INCLUDE (CheckIncludeFile)
INCLUDE (CheckIncludeFiles)

SET(WITH_KERBEROS_DOC "\nsystem (use the OS sasl library)")
STRING_APPEND(WITH_KERBEROS_DOC ", \n</path/to/custom/installation>")
STRING_APPEND(WITH_KERBEROS_DOC ", \nnone (skip kerberos)>")

STRING(REPLACE "\n" "| " WITH_KERBEROS_DOC_STRING "${WITH_KERBEROS_DOC}")

MACRO(RESET_KERBEROS_VARIABLES)
  UNSET(KERBEROS_INCLUDE_DIR)
  UNSET(KERBEROS_INCLUDE_DIR CACHE)
  UNSET(KERBEROS_SYSTEM_LIBRARY)
  UNSET(KERBEROS_SYSTEM_LIBRARY CACHE)
ENDMACRO()

MACRO (FIND_SYSTEM_KERBEROS)
  FIND_LIBRARY(KERBEROS_SYSTEM_LIBRARY NAMES "krb5")
  IF(LINUX_DEBIAN OR LINUX_UBUNTU)
    FIND_LIBRARY(KERBEROS_SYSTEM_LIBRARY
      NAMES "krb5"
      HINTS /usr/lib/x86_64-linux-gnu)
  ENDIF()
  IF (KERBEROS_SYSTEM_LIBRARY)
    SET(KERBEROS_LIBRARY_PATH ${KERBEROS_SYSTEM_LIBRARY})
    MESSAGE(STATUS "KERBEROS_LIBRARY_PATH ${KERBEROS_LIBRARY_PATH}")

    CMAKE_PUSH_CHECK_STATE()

    IF(SOLARIS)
      INCLUDE_DIRECTORIES(BEFORE SYSTEM /usr/include/kerberosv5)
      SET(CMAKE_REQUIRED_INCLUDES "/usr/include/kerberosv5")
    ELSEIF(FREEBSD)
      # Do *not* INCLUDE_DIRECTORIES /usr/local/include here.
      SET(CMAKE_REQUIRED_INCLUDES "/usr/local/include")
    ELSEIF(LINUX_DEBIAN OR LINUX_UBUNTU)
      INCLUDE_DIRECTORIES(BEFORE SYSTEM /usr/include/mit-krb5)
      SET(CMAKE_REQUIRED_INCLUDES "/usr/include/mit-krb5")
    ENDIF()

    CHECK_INCLUDE_FILE(krb5/krb5.h HAVE_KRB5_KRB5_H)
    IF(HAVE_KRB5_KRB5_H)
      FIND_PATH(KERBEROS_INCLUDE_DIR
        NAMES "krb5/krb5.h"
        HINTS ${CMAKE_REQUIRED_INCLUDES}
        )
    ENDIF()

    CMAKE_POP_CHECK_STATE()

  ENDIF()
ENDMACRO()

# TODO: implement for standalone Linux and Windows.
# Lookup, and copy: libgssapi_krb5.so.2 libkrb5.so.3 libkrb5support.so.0
MACRO(FIND_CUSTOM_KERBEROS)
  FIND_LIBRARY(KERBEROS_LIBRARY_PATH
    NAMES "krb5"
    PATHS ${WITH_KERBEROS} ${WITH_KERBEROS}/lib
    NO_DEFAULT_PATH
    NO_CMAKE_ENVIRONMENT_PATH
    NO_SYSTEM_ENVIRONMENT_PATH)

  # Header file first search in WITH_KERBEROS.
  FIND_PATH(KERBEROS_ROOT_DIR
    NAMES include/krb5.h
    NO_CMAKE_PATH
    NO_CMAKE_ENVIRONMENT_PATH
    HINTS ${WITH_KERBEROS}
  )
  # Header file search in standard places (if not found above).
  FIND_PATH(KERBEROS_ROOT_DIR
    NAMES include/krb5/krb5.h
  )
  FIND_PATH(KERBEROS_INCLUDE_DIR
    NAMES krb5.h
    HINTS ${KERBEROS_ROOT_DIR}/include
  )
  IF(KERBEROS_INCLUDE_DIR AND KERBEROS_LIBRARY_PATH)
    MESSAGE(STATUS "KERBEROS_INCLUDE_DIR ${KERBEROS_INCLUDE_DIR}")
    SET(CMAKE_REQUIRED_INCLUDES ${KERBEROS_INCLUDE_DIR})
    ADD_DEFINITIONS(-DKERBEROS_LIB_CONFIGURED)
    SET(KERBEROS_FOUND 1)
  ENDIF()

ENDMACRO()

MACRO (MYSQL_CHECK_KERBEROS)
  # For standalone linux: custom KERBEROS must match custom builds of
  # LDAP/SASL/SSL. This is not yet supported, so we disable Kerberos.
  IF(LINUX_STANDALONE AND KNOWN_CUSTOM_LIBRARIES)
    SET(WITH_KERBEROS "none")
    SET(WITH_KERBEROS "none" CACHE INTERNAL "")
    RESET_KERBEROS_VARIABLES()
  ENDIF()

  # No Kerberos support for Windows.
  IF(WIN32)
    SET(WITH_KERBEROS "none")
    SET(WITH_KERBEROS "none" CACHE INTERNAL "")
    RESET_KERBEROS_VARIABLES()
  ENDIF()

  IF(NOT WITH_KERBEROS)
    SET(WITH_KERBEROS "system" CACHE STRING "${WITH_KERBEROS_DOC_STRING}" FORCE)
  ENDIF()

  # See if WITH_KERBEROS is of the form </path/to/custom/installation>
  # TODO: not implemented yet. Implement for STANDALONE_LINUX and Windows.
  FILE(GLOB WITH_KERBEROS_HEADER ${WITH_KERBEROS}/include/krb5/krb5.h)
  IF(WITH_KERBEROS_HEADER)
    FILE(TO_CMAKE_PATH "${WITH_KERBEROS}" WITH_KERBEROS)
    SET(WITH_KERBEROS_PATH ${WITH_KERBEROS})
  ENDIF()

  IF(WITH_KERBEROS STREQUAL "system")
    FIND_SYSTEM_KERBEROS()
  ELSEIF(WITH_KERBEROS STREQUAL "none")
    MESSAGE(STATUS "KERBEROS_LIBRARY path is none, disabling kerberos support.")
    SET(KERBEROS_LIBRARY_PATH "")
    SET(WITH_KERBEROS 0)
    SET(KERBEROS_FOUND 0)
    SET(WITH_KERBEROS_NOT_SET 1)
  ELSEIF(WITH_KERBEROS_PATH)
    IF(LINUX_STANDALONE)
      FIND_CUSTOM_KERBEROS()
    ELSE()
      MESSAGE(FATAL_ERROR
        "-DWITH_KERBEROS=<path> not supported on this platform")
    ENDIF()
  ELSE()
    MESSAGE(FATAL_ERROR "Could not find KERBEROS")
  ENDIF()

  IF(KERBEROS_SYSTEM_LIBRARY AND HAVE_KRB5_KRB5_H)
    SET(KERBEROS_FOUND 1)
  ELSE()
    SET(KERBEROS_FOUND 0)
    # TODO: FATAL_ERROR later if WITH_AUTHENTICATION_LDAP == ON
    IF(WITH_KERBEROS)
      MESSAGE(WARNING "Could not find KERBEROS")
    ENDIF()
  ENDIF()

  IF(KERBEROS_FOUND)
    ADD_DEFINITIONS(-DKERBEROS_LIB_CONFIGURED)
  ENDIF()

ENDMACRO()
