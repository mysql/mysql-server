# Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

MACRO(NDB_CHECK_MYSQL_INCLUDE_FILE INCLUDE VARIABLE)
  IF("${VARIABLE}" MATCHES "^${VARIABLE}$")
    SET(_msg "Looking for MySQL include file ${INCLUDE}")
    MESSAGE(STATUS "${_msg}")
    IF(EXISTS "${CMAKE_SOURCE_DIR}/include/${INCLUDE}")
      MESSAGE(STATUS "${_msg} - found")
      SET(${VARIABLE} 1 CACHE INTERNAL "Have MySQL include ${INCLUDE}")
    ELSE()
      MESSAGE(STATUS "${_msg} - not found")
      SET(${VARIABLE} "" CACHE INTERNAL "Have MySQL include ${INCLUDE}")
    ENDIF()
  ENDIF()
ENDMACRO()

