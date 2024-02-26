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

# We support different versions of ICU:
# - "bundled" uses source code in <source dir>/extra/icu
# - "system"  (typically) uses headers/libraries in /usr/lib and /usr/lib64
# - a custom installation of ICU can be used like this
#     - cmake -DCMAKE_PREFIX_PATH=</path/to/custom/icu> -DWITH_ICU="system"
#   or
#     - cmake -DWITH_ICU=</path/to/custom/icu>
#
# The default value for WITH_ICU is "bundled".

# We create an INTERFACE library called icu_interface,
# and an ALIAS ext::icu.
# It will have the necessary INTERFACE_INCLUDE_DIRECTORIES property,
# so just link with it, no need to do INCLUDE_DIRECTORIES.

# To do: The default should probably be different depending on platform. On
# Windows, it should likely be wherever NuGet puts the libraries.

# The ICU library was introduced to MySQL sources with:
# WL#8987 Add the ICU library to handle RLIKE/REGEXP.
# in a series of patches, the first one titled
# WL#8987: ICU 59.1 source added almost without altering.
# The lowest checked version is 55 on Ubuntu 16.
SET(MIN_ICU_VERSION_REQUIRED "55")

MACRO(FIND_ICU_VERSION ICU_COMMON_DIR)
  # Extract the version number. Major version information looks like:
  #   #define U_ICU_VERSION_MAJOR_NUM nn
  FILE(STRINGS "${ICU_COMMON_DIR}/unicode/uvernum.h"
    ICU_MAJOR_VERSION_INFO
    REGEX "^#[ ]*define[\t ]+U_ICU_VERSION_MAJOR_NUM[\t ]+[0-9]+$"
    )
  STRING(REGEX REPLACE
    "^.*U_ICU_VERSION_MAJOR_NUM[\t ]+([0-9]+)$" "\\1"
    ICU_MAJOR_VERSION ${ICU_MAJOR_VERSION_INFO}
    )

  SET(ICU_VERSION "${ICU_MAJOR_VERSION}")
  SET(ICU_VERSION "${ICU_VERSION}" CACHE INTERNAL "ICU major")

  MESSAGE(STATUS "ICU_VERSION (${WITH_ICU}) is ${ICU_VERSION}")
  MESSAGE(STATUS "ICU_INCLUDE_DIRS ${ICU_INCLUDE_DIRS}")
  MESSAGE(STATUS "ICU_LIBRARIES ${ICU_LIBRARIES}")
ENDMACRO()

#
# install_root is either 'system' or is assumed to be a path.
#
FUNCTION(FIND_ICU install_root)
  IF("${install_root}" STREQUAL "system")
    SET(EXTRA_FIND_LIB_ARGS)
    SET(EXTRA_FIND_INC_ARGS)
    IF(APPLE)
      SET(EXTRA_FIND_LIB_ARGS HINTS "${HOMEBREW_HOME}/icu4c"
        PATH_SUFFIXES "lib")
      SET(EXTRA_FIND_INC_ARGS HINTS "${HOMEBREW_HOME}/icu4c"
        PATH_SUFFIXES "include")
    ENDIF()
  ELSE()
    SET(EXTRA_FIND_LIB_ARGS HINTS "${install_root}"
      PATH_SUFFIXES "lib" "lib64" NO_DEFAULT_PATH)
    SET(EXTRA_FIND_INC_ARGS HINTS "${install_root}"
      PATH_SUFFIXES "include"     NO_DEFAULT_PATH)
  ENDIF()

  FIND_PATH(ICU_INCLUDE_DIR NAMES unicode/regex.h ${EXTRA_FIND_INC_ARGS})
  IF (NOT ICU_INCLUDE_DIR)
    MESSAGE(FATAL_ERROR "Cannot find ICU regular expression headers")
  ENDIF()

  IF(WIN32)
    SET(ICU_LIBS icuuc icuio icudt icuin)
  ELSE()
    SET(ICU_LIBS icuuc icuio icudata icui18n)
  ENDIF()

  SET(ICU_SYSTEM_LIBRARIES)
  FOREACH(ICU_LIB ${ICU_LIBS})
    UNSET(ICU_LIB_PATH CACHE)
    FIND_LIBRARY(ICU_LIB_PATH NAMES ${ICU_LIB} ${EXTRA_FIND_LIB_ARGS})
    IF(NOT ICU_LIB_PATH)
      MESSAGE(FATAL_ERROR "Cannot find the ICU library ${ICU_LIB}")
    ENDIF()
    LIST(APPEND ICU_SYSTEM_LIBRARIES ${ICU_LIB_PATH})
  ENDFOREACH()

  SET(ICU_INCLUDE_DIRS ${ICU_INCLUDE_DIR})
  SET(ICU_LIBRARIES ${ICU_SYSTEM_LIBRARIES})

  SET(ICU_COMMON_DIR ${ICU_INCLUDE_DIR})
  FIND_ICU_VERSION(${ICU_COMMON_DIR})

  ADD_LIBRARY(icu_interface INTERFACE)
  TARGET_LINK_LIBRARIES(icu_interface INTERFACE ${ICU_LIBRARIES})
  IF(NOT ICU_INCLUDE_DIR STREQUAL "/usr/include")
    TARGET_INCLUDE_DIRECTORIES(icu_interface SYSTEM INTERFACE
      ${ICU_INCLUDE_DIR})
  ENDIF()
ENDFUNCTION(FIND_ICU)

SET(ICU_VERSION_DIR "icu-release-73-1")
SET(BUNDLED_ICU_PATH ${CMAKE_SOURCE_DIR}/extra/icu/${ICU_VERSION_DIR})

# ICU data files come in two flavours, big and little endian.
# (Actually, there's an 'e' for EBCDIC version as well.)
IF(SOLARIS_SPARC)
  SET(ICUDT_DIR "icudt73b")
ELSE()
  SET(ICUDT_DIR "icudt73l")
ENDIF()


FUNCTION(MYSQL_USE_BUNDLED_ICU)
  SET(WITH_ICU "bundled" CACHE STRING "Use bundled icu library")

  SET(ICU_SOURCE_DIR ${BUNDLED_ICU_PATH}/source)
  SET(ICU_COMMON_DIR ${ICU_SOURCE_DIR}/common)

  SET(ICU_INCLUDE_DIRS
    ${ICU_COMMON_DIR}
    ${ICU_SOURCE_DIR}/stubdata
    ${ICU_SOURCE_DIR}/i18n
  )
  # Unset the variables used for system icu.
  UNSET(ICU_INCLUDE_DIR)
  UNSET(ICU_INCLUDE_DIR CACHE)
  UNSET(ICU_LIB_PATH)
  UNSET(ICU_LIB_PATH CACHE)

  ADD_SUBDIRECTORY(${CMAKE_SOURCE_DIR}/extra/icu)

  SET(ICU_LIBRARIES icui18n icuuc icustubdata)
  FIND_ICU_VERSION(${ICU_COMMON_DIR})

  ADD_LIBRARY(icu_interface INTERFACE)
  TARGET_LINK_LIBRARIES(icu_interface INTERFACE ${ICU_LIBRARIES})
  TARGET_INCLUDE_DIRECTORIES(icu_interface SYSTEM BEFORE INTERFACE
    ${ICU_INCLUDE_DIRS})

ENDFUNCTION(MYSQL_USE_BUNDLED_ICU)

MACRO (MYSQL_CHECK_ICU)

  IF(NOT WITH_ICU)
    SET(WITH_ICU bundled CACHE STRING
      "By default use bundled icu library")
  ENDIF()

  FILE(TO_CMAKE_PATH "${WITH_ICU}" WITH_ICU)

  IF (WITH_ICU STREQUAL "bundled")
    MYSQL_USE_BUNDLED_ICU()
  ELSEIF(WITH_ICU STREQUAL "system")
    FIND_ICU("system")
  ELSEIF(EXISTS "${WITH_ICU}")
    FIND_ICU("${WITH_ICU}")
  ELSE()
    MESSAGE(FATAL_ERROR "WITH_ICU must be 'bundled', 'system' or a path")
  ENDIF()

  IF(ICU_VERSION VERSION_LESS MIN_ICU_VERSION_REQUIRED)
    MESSAGE(FATAL_ERROR
      "ICU version must be at least ${MIN_ICU_VERSION_REQUIRED}, "
      "found ${ICU_VERSION}.\nPlease use -DWITH_ICU=bundled")
  ENDIF()

  IF(WIN32)
    TARGET_COMPILE_DEFINITIONS(icu_interface INTERFACE
      -DU_STATIC_IMPLEMENTATION)
  ENDIF()
  ADD_LIBRARY(ext::icu ALIAS icu_interface)
ENDMACRO(MYSQL_CHECK_ICU)
