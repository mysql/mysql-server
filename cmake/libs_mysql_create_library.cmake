# Copyright (c) 2023, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; IF not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# LIBS_MYSQL_CREATE_LIBRARY(name sources... options/keywords...)
# Create STATIC library from source files passed in function arguments
#
FUNCTION(LIBS_MYSQL_CREATE_LIBRARY TARGET_NAME)
  SET(OPTIONS INSTALL_TARGET)
  SET(ARGS_ONE_VALUE LIB_TYPE)
  SET(ARGS_MULTI_VALUE
    COMPILE_DEFINITIONS # for TARGET_COMPILE_DEFINITIONS
    LINK_LIBRARIES      # for TARGET_LINK_LIBRARIES
    TARGET_SRCS         # source files added to this library
    TARGET_HEADERS      # headers for installation (disabled for now)
    DEPENDENCIES        # for ADD_DEPENDENCIES
  )
  CMAKE_PARSE_ARGUMENTS(MYSQL_LIB "${OPTIONS}" "${ARGS_ONE_VALUE}"
    "${ARGS_MULTI_VALUE}" ${ARGN} )

  # TODO option MYSQL_LIB_INSTALL_TARGET - allow installation of defined
  #   libraries. Variable type: boolean

  # TODO option LIB_TYPE - add support for shared libraries. Variable_type:
  #   enumeration, with supported values: "SHARED" and "STATIC"

  ADD_STATIC_LIBRARY(${TARGET_NAME} ${MYSQL_LIB_TARGET_SRCS})

  IF(MYSQL_LIB_LINK_LIBRARIES)
    TARGET_LINK_LIBRARIES(${TARGET_NAME} PUBLIC ${MYSQL_LIB_LINK_LIBRARIES})
  ENDIF()

  IF(MYSQL_LIB_DEPENDENCIES)
    ADD_DEPENDENCIES(${TARGET_NAME} ${MYSQL_LIB_DEPENDENCIES})
  ENDIF()

  IF(MYSQL_LIB_COMPILE_DEFINITIONS)
    TARGET_COMPILE_DEFINITIONS(${TARGET_NAME} PRIVATE
      ${MYSQL_LIB_COMPILE_DEFINITIONS})
  ENDIF()

ENDFUNCTION()
