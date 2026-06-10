# Copyright (c) 2017, 2026, Oracle and/or its affiliates.
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
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

IF(EXISTS "./${library_version}")
  RETURN()
ENDIF()

EXECUTE_PROCESS(
  COMMAND ${CMAKE_COMMAND} -E copy
  "${library_directory}/${library_version}" "./${library_version}"
  COMMAND_ECHO STDOUT
  )

IF(NOT "${library_version}" STREQUAL "${library_name}")
  EXECUTE_PROCESS(
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    "${library_version}" "${library_name}"
    COMMAND_ECHO STDOUT
    )
ENDIF()

# subdir contains sasl .so plugins, no symlink needed
IF(NOT subdir)
  EXECUTE_PROCESS(
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    "../lib/${library_version}" "${library_version}"
    COMMAND_ECHO STDOUT
    WORKING_DIRECTORY ${PLUGIN_DIR}
    )
ENDIF()

EXECUTE_PROCESS(
  COMMAND install_name_tool -id
  "@rpath/${library_version}" "./${library_version}"
  COMMAND_ECHO STDOUT
  )

# Convert it back to a list
STRING(REPLACE " " ";" LIBRARY_DEPS "${LIBRARY_DEPS}")
MESSAGE(STATUS "LIBRARY_DEPS for ${library_version} : ${LIBRARY_DEPS}")
FOREACH(dep ${LIBRARY_DEPS})
  # MESSAGE(STATUS "dep ${dep}")
  STRING(REGEX MATCH "/usr/local/mysql/lib/(.*)$" UNUSED ${dep})
  IF(CMAKE_MATCH_1)
    # MESSAGE(STATUS "dep match ${CMAKE_MATCH_1}")
    # install_name_tool -change old new file
    IF(subdir)
      EXECUTE_PROCESS(COMMAND install_name_tool -change
        "/usr/local/mysql/lib/${CMAKE_MATCH_1}"
        "@loader_path/../${CMAKE_MATCH_1}"
        "./${library_version}"
        COMMAND_ECHO STDOUT
        )
    ELSE()
      EXECUTE_PROCESS(COMMAND install_name_tool -change
        "/usr/local/mysql/lib/${CMAKE_MATCH_1}" "@loader_path/${CMAKE_MATCH_1}"
        "./${library_version}"
        COMMAND_ECHO STDOUT
        )
    ENDIF()
  ENDIF()
ENDFOREACH()
