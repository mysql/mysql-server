# Copyright (c) 2013, 2016, Oracle and/or its affiliates. All rights reserved.
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

MACRO(NDB_ADD_TARGET_PROPERTY TARGET PROPERTY VALUE)

  # Get current value of PROPERTY from TARGET
  GET_TARGET_PROPERTY(curr ${TARGET} ${PROPERTY})
  #MESSAGE(STATUS "${TARGET}.${PROPERTY}: ${curr}")

  # Since GET_TARGET_PROPERTY returns the string $TARGET-NOTFOUND
  # when no such property exists, reset the string to empty to be
  # able to append to list
  IF(NOT curr)
    SET(curr)
  ENDIF()

  # Append VALUE to list
  LIST(APPEND curr ${VALUE})

  # Set new PROPERTY of TARGET
  SET_TARGET_PROPERTIES(${TARGET} PROPERTIES ${PROPERTY} "${curr}")

  #GET_TARGET_PROPERTY(after ${TARGET} ${PROPERTY})
  #MESSAGE(STATUS "${TARGET}.${PROPERTY}: ${after}")
ENDMACRO()



