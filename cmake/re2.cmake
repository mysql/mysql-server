# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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

# cmake -DWITH_RE2=system|bundled
# bundled is the default

MACRO (FIND_SYSTEM_RE2)
  FIND_PATH(PATH_TO_RE2 NAMES re2/re2.h)
  FIND_LIBRARY(RE2_SYSTEM_LIBRARY NAMES re2)
  IF (PATH_TO_RE2 AND RE2_SYSTEM_LIBRARY)
    SET(SYSTEM_RE2_FOUND 1)
    SET(RE2_INCLUDE_DIR ${PATH_TO_RE2})
    SET(RE2_LIBRARY ${RE2_SYSTEM_LIBRARY})
    MESSAGE(STATUS "RE2_INCLUDE_DIR ${RE2_INCLUDE_DIR}")
    MESSAGE(STATUS "RE2_LIBRARY ${RE2_LIBRARY}")
  ENDIF()
ENDMACRO()

MACRO (MYSQL_USE_BUNDLED_RE2)
  SET(BUILD_BUNDLED_RE2 1)
  SET(RE2_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/extra/re2)
  SET(RE2_LIBRARY re2_lib)
ENDMACRO()

IF (NOT WITH_RE2)
  SET(WITH_RE2 "bundled" CACHE STRING "By default use bundled re2 library")
ENDIF()

MACRO (MYSQL_CHECK_RE2)
  IF (WITH_RE2 STREQUAL "bundled")
    MYSQL_USE_BUNDLED_RE2()
  ELSEIF(WITH_RE2 STREQUAL "system")
    FIND_SYSTEM_RE2()
    IF (NOT SYSTEM_RE2_FOUND)
      MESSAGE(FATAL_ERROR "Cannot find system re2 libraries.")
    ENDIF()
  ELSE()
    MESSAGE(FATAL_ERROR "WITH_RE2 must be bundled or system")
  ENDIF()
ENDMACRO()
