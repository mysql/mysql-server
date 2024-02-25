# Copyright (c) 2013, 2023, Oracle and/or its affiliates.
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



