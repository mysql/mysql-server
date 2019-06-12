# Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
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

FUNCTION(GET_FILE_SIZE FILE_NAME OUTPUT_SIZE)
  IF(WIN32)
    FILE(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/cmake/filesize.bat" FILESIZE_BAT)
    FILE(TO_NATIVE_PATH "${FILE_NAME}" NATIVE_FILE_NAME)

    EXECUTE_PROCESS(
      COMMAND "${FILESIZE_BAT}" "${NATIVE_FILE_NAME}"
      RESULT_VARIABLE COMMAND_RESULT
      OUTPUT_VARIABLE RESULT
      OUTPUT_STRIP_TRAILING_WHITESPACE)

  ELSEIF(APPLE OR FREEBSD)
    EXEC_PROGRAM(stat ARGS -f '%z' ${FILE_NAME} OUTPUT_VARIABLE RESULT)
  ELSE()
    EXEC_PROGRAM(stat ARGS -c '%s' ${FILE_NAME} OUTPUT_VARIABLE RESULT)
  ENDIF()
  SET(${OUTPUT_SIZE} ${RESULT} PARENT_SCOPE)
ENDFUNCTION()
