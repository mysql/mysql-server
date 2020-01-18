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

# cmake -DWITH_KERBEROS=system|path|none
# system is the default
# none will diable the kerberos build
#
# Following algorithm will be used to find kerberos library.
# 1. If "WITH_KERBEROS" is set to system or not configured, we will search kerberos lib in system directory.
#    System is used for unix alike OS. For windows we dont have system kerberos library.
# 2. If kerberos library path is provided using "WITH_KERBEROS" option, we will search kerberos library in that path.
#    This will be used to build MySQL with custom kerberos library.
# 3. If kerberos library and header is found, kerberos library path, kerberos header path and KERBEROS_LIB_CONFIGURED will be set.
# 4. For windows, we need to install kerberos library from web, https://web.mit.edu/kerberos/kfw-4.1/kfw-4.1.html.

MACRO (FIND_SYSTEM_KERBEROS)
  FIND_LIBRARY(KERBEROS_SYSTEM_LIBRARY NAMES "krb5")
  IF (KERBEROS_SYSTEM_LIBRARY)
    SET(SYSTEM_KERBEROS_FOUND 1)
    SET(KERBEROS_LIBRARY_PATH ${KERBEROS_SYSTEM_LIBRARY})
    MESSAGE(STATUS "KERBEROS_LIBRARY_PATH ${KERBEROS_LIBRARY_PATH}")
  ENDIF()
ENDMACRO()

IF (NOT WITH_KERBEROS)
  SET(WITH_KERBEROS "system" CACHE STRING "By default use system KERBEROS library")
  SET(WITH_KERBEROS_NOT_SET 1)
ENDIF()

MACRO (MYSQL_CHECK_KERBEROS)
  IF (WITH_KERBEROS STREQUAL "none")
    MESSAGE(STATUS "KERBEROS_LIBRARY path is none, disabling kerberos support.")
    SET(KERBEROS_LIBRARY_PATH "")
    SET(WITH_KERBEROS 0)
    SET(KERBEROS_FOUND 0)
    SET(WITH_KERBEROS_NOT_SET 1)
  ELSEIF (NOT WITH_KERBEROS OR WITH_KERBEROS STREQUAL "system")
    FIND_SYSTEM_KERBEROS()
    IF (NOT SYSTEM_KERBEROS_FOUND)
      MESSAGE(STATUS "Cannot find system KERBEROS libraries.")
      SET(KERBEROS_LIBRARY_PATH "")
    ENDIF()
  ELSE()
    FIND_LIBRARY(KERBEROS_LIBRARY_PATH
                 NAMES "krb5"
                 PATHS ${WITH_KERBEROS} ${WITH_KERBEROS}/lib
                 NO_DEFAULT_PATH
                 NO_CMAKE_ENVIRONMENT_PATH
                 NO_SYSTEM_ENVIRONMENT_PATH)
    IF (NOT KERBEROS_LIBRARY_PATH)
      MESSAGE(STATUS "Cannot find KERBEROS libraries in ${WITH_KERBEROS}.")
      SET(KERBEROS_LIBRARY_PATH "")
    ELSE()
      MESSAGE(STATUS "KERBEROS_LIBRARY_PATH ${KERBEROS_LIBRARY_PATH}")
    ENDIF()
  ENDIF()

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
    IF(FREEBSD)
      MESSAGE(STATUS "KERBEROS BSD INCLUDE_DIRECTORIES")
      INCLUDE_DIRECTORIES(SYSTEM /usr/local/include)
      LIST(APPEND CMAKE_REQUIRED_INCLUDES "/usr/local/include")
    ENDIF()
  ENDIF()

  # If WITH_KERBEROS is set explicitly to system or other path, And not able to find kerberos library, cmake will give fatal error.
  IF(NOT KERBEROS_LIBRARY_PATH AND NOT WITH_KERBEROS_NOT)
    MESSAGE(STATUS  "Cannot find KERBEROS libraries in given path. ")
  ENDIF()

ENDMACRO()
