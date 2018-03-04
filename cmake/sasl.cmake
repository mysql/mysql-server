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

# cmake -DWITH_SASL=system|path
# system is the default
#
# Sets SASL_LIBRARY. If not found, SASL_LIBRARY="".

MACRO (FIND_SYSTEM_SASL)
  FIND_LIBRARY(SASL_SYSTEM_LIBRARY NAMES "sasl2" "sasl")
  IF (SASL_SYSTEM_LIBRARY)
    SET(SYSTEM_SASL_FOUND 1)
    SET(SASL_LIBRARY ${SASL_SYSTEM_LIBRARY})
    MESSAGE(STATUS "SASL_LIBRARY ${SASL_LIBRARY}")
  ENDIF()
ENDMACRO()

IF (NOT WITH_SASL)
  SET(WITH_SASL "system" CACHE STRING "By default use system sasl library")
ENDIF()

MACRO (MYSQL_CHECK_SASL)
  IF (NOT WITH_SASL OR WITH_SASL STREQUAL "system")
    FIND_SYSTEM_SASL()
    IF (NOT SYSTEM_SASL_FOUND)
      MESSAGE(STATUS "Cannot find system sasl libraries.") 
      SET(SASL_LIBRARY "")
    ENDIF()
  ELSE()
    FIND_LIBRARY(SASL_LIBRARY
                 NAMES "sasl2" "sasl"
                 PATHS ${WITH_SASL} ${WITH_SASL}/lib
                 NO_DEFAULT_PATH
                 NO_CMAKE_ENVIRONMENT_PATH
                 NO_SYSTEM_ENVIRONMENT_PATH)
    IF (NOT SASL_LIBRARY)
      MESSAGE(STATUS "Cannot find sasl libraries in ${WITH_SASL}.") 
      SET(SASL_LIBRARY "")
    ELSE()
      MESSAGE(STATUS "SASL_LIBRARY ${SASL_LIBRARY}")
    ENDIF()
  ENDIF()
ENDMACRO()
