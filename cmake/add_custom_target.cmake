# Copyright (c) 2021, 2023, Oracle and/or its affiliates.
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

# Wrap ADD_CUSTOM_TARGET to get rid of Warning MSB8065: File not created
# Custom build for ... succeeded, but specified output ... has not been created.
#
# For the Visual Studio Generator, we generate an extra
# 'cmake -E touch cmakefiles/${target_name}'
#
# Note that we handle only *one* COMMAND.
#   This is good enough for the current codebase.
FUNCTION(MY_ADD_CUSTOM_TARGET TARGET_NAME)
  CMAKE_PARSE_ARGUMENTS(CUSTOM_ARG
    "ALL"
    "COMMENT;WORKING_DIRECTORY"
    "COMMAND;DEPENDS"
    ${ARGN}
    )
  SET(TARGET_COMMAND)
  IF(CUSTOM_ARG_ALL)
    LIST(APPEND TARGET_COMMAND "ALL")
  ENDIF()
  IF(CUSTOM_ARG_DEPENDS)
    LIST(APPEND TARGET_COMMAND "DEPENDS" ${CUSTOM_ARG_DEPENDS})
  ENDIF()
  IF(CUSTOM_ARG_COMMAND)
    LIST(APPEND TARGET_COMMAND "COMMAND" ${CUSTOM_ARG_COMMAND})
  ENDIF()
  IF(CMAKE_GENERATOR MATCHES "Visual Studio")
    STRING(TOLOWER "${TARGET_NAME}" target_name)
    LIST(APPEND TARGET_COMMAND
      "COMMAND" "${CMAKE_COMMAND}" -E touch cmakefiles/${target_name})
  ENDIF()
  IF(CUSTOM_ARG_COMMENT)
    LIST(APPEND TARGET_COMMAND "COMMENT" ${CUSTOM_ARG_COMMENT})
  ENDIF()
  IF(CUSTOM_ARG_WORKING_DIRECTORY)
    LIST(APPEND
      TARGET_COMMAND "WORKING_DIRECTORY" ${CUSTOM_ARG_WORKING_DIRECTORY})
  ENDIF()

  ADD_CUSTOM_TARGET(${TARGET_NAME} ${TARGET_COMMAND})

ENDFUNCTION(MY_ADD_CUSTOM_TARGET)
