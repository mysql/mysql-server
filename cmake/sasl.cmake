# Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.
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

# cmake -DWITH_SASL=system|</path/to/custom/installation>
# system is the default

INCLUDE (CheckIncludeFile)
INCLUDE (CheckIncludeFiles)

SET(WITH_SASL_DOC "\nsystem (use the OS sasl library)")
STRING_APPEND(WITH_SASL_DOC ", \n</path/to/custom/installation>")

STRING(REPLACE "\n" "| " WITH_SASL_DOC_STRING "${WITH_SASL_DOC}")

MACRO(RESET_SASL_VARIABLES)
  UNSET(SASL_INCLUDE_DIR)
  UNSET(SASL_INCLUDE_DIR CACHE)
  UNSET(SASL_LIBRARY)
  UNSET(SASL_LIBRARY CACHE)
  UNSET(SASL_ROOT_DIR)
  UNSET(SASL_ROOT_DIR CACHE)
  UNSET(WITH_SASL)
  UNSET(WITH_SASL CACHE)
  UNSET(WITH_SASL_PATH)
  UNSET(WITH_SASL_PATH CACHE)
  UNSET(HAVE_CUSTOM_SASL_SASL_H)
  UNSET(HAVE_CUSTOM_SASL_SASL_H CACHE)
  UNSET(HAVE_SASL_SASL_H)
  UNSET(HAVE_SASL_SASL_H CACHE)
ENDMACRO()

MACRO(FIND_SYSTEM_SASL)
  # Cyrus SASL 2.1.26 on Solaris 11.4 has a bug that requires sys/types.h
  # to be included before checking if sasl/sasl.h exists
  CHECK_INCLUDE_FILES("sys/types.h;sasl/sasl.h" HAVE_SASL_SASL_H)
  FIND_LIBRARY(SASL_SYSTEM_LIBRARY NAMES "sasl2" "sasl")
  IF (SASL_SYSTEM_LIBRARY)
    SET(SASL_LIBRARY ${SASL_SYSTEM_LIBRARY})
    MESSAGE(STATUS "SASL_LIBRARY ${SASL_LIBRARY}")
  ENDIF()
ENDMACRO()

MACRO(FIND_CUSTOM_SASL)
  # First search in WITH_SASL_PATH.
  FIND_PATH(SASL_ROOT_DIR
    NAMES include/sasl/sasl.h
    NO_CMAKE_PATH
    NO_CMAKE_ENVIRONMENT_PATH
    HINTS ${WITH_SASL_PATH}
    )
  # Then search in standard places (if not found above).
  FIND_PATH(SASL_ROOT_DIR
    NAMES include/sasl/sasl.h
    )

  FIND_PATH(SASL_INCLUDE_DIR
    NAMES sasl/sasl.h
    HINTS ${SASL_ROOT_DIR}/include
    )

  # On mac this list is <.dylib;.so;.a>
  # We prefer static libraries, so we reverse it here.
  LIST(REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)

  FIND_LIBRARY(SASL_LIBRARY
    NAMES sasl2 sasl libsasl
    PATHS ${WITH_SASL}/lib
    NO_DEFAULT_PATH
    NO_CMAKE_ENVIRONMENT_PATH
    NO_SYSTEM_ENVIRONMENT_PATH)

  LIST(REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)

  IF(SASL_LIBRARY)
    GET_FILENAME_COMPONENT(SASL_LIBRARY_EXT ${SASL_LIBRARY} EXT)
    IF(SASL_LIBRARY_EXT STREQUAL ".a")
      SET(STATIC_SASL_LIBRARY 1)
    ENDIF()
    MESSAGE(STATUS "SASL_LIBRARY ${SASL_LIBRARY}")
  ENDIF()

  IF(SASL_INCLUDE_DIR)
    INCLUDE_DIRECTORIES(BEFORE SYSTEM "${SASL_INCLUDE_DIR}")

    CMAKE_PUSH_CHECK_STATE()
    SET(CMAKE_REQUIRED_INCLUDES "${SASL_INCLUDE_DIR}")
    # Windows users doing 'git pull' will have cached HAVE_SASL_SASL_H=""
    CHECK_INCLUDE_FILE(sasl/sasl.h HAVE_CUSTOM_SASL_SASL_H)
    IF(HAVE_CUSTOM_SASL_SASL_H)
      SET(HAVE_SASL_SASL_H 1 CACHE INTERNAL "Have include sasl/sasl.h" FORCE)
    ENDIF()
    CMAKE_POP_CHECK_STATE()
  ENDIF()

  IF(WIN32)
    FIND_FILE(SASL_LIBRARY_DLL
      NAMES libsasl.dll
      PATHS ${WITH_SASL}/lib
      NO_CMAKE_PATH
      NO_CMAKE_ENVIRONMENT_PATH
      NO_SYSTEM_ENVIRONMENT_PATH
      )
    FIND_FILE(SASL_SCRAM_PLUGIN
      NAMES saslSCRAM.dll
      PATHS ${WITH_SASL}/lib
      NO_CMAKE_PATH
      NO_CMAKE_ENVIRONMENT_PATH
      NO_SYSTEM_ENVIRONMENT_PATH
      )
  ENDIF()
ENDMACRO()

MACRO(MYSQL_CHECK_SASL)
  IF(NOT WITH_SASL)
    SET(WITH_SASL "system" CACHE STRING "${WITH_SASL_DOC_STRING}" FORCE)
  ENDIF()

  # See if WITH_SASL is of the form </path/to/custom/installation>
  FILE(GLOB WITH_SASL_HEADER ${WITH_SASL}/include/sasl/sasl.h)
  IF (WITH_SASL_HEADER)
    FILE(TO_CMAKE_PATH "${WITH_SASL}" WITH_SASL)
    SET(WITH_SASL_PATH ${WITH_SASL})
  ENDIF()

  IF(WITH_SASL STREQUAL "system")
    FIND_SYSTEM_SASL()
  ELSEIF(WITH_SASL_PATH)
    FIND_CUSTOM_SASL()
  ELSE()
    RESET_SASL_VARIABLES()
    MESSAGE(FATAL_ERROR "Could not find SASL")
  ENDIF()

  IF(HAVE_SASL_SASL_H AND SASL_LIBRARY)
    SET(SASL_FOUND TRUE)
  ELSE()
    SET(SASL_FOUND FALSE)
  ENDIF()

ENDMACRO()
