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

# Clean up RPATH for the debug verson of group_replication.so after INSTALL.

# See the generated cmake_install.cmake files:
IF("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xServerx" OR
    NOT CMAKE_INSTALL_COMPONENT)
  # Continue
ELSE()
  RETURN()
ENDIF()

# The debug plugin is in INSTALL_PLUGINDIR/debug/group_replication.so
FOREACH(PATH
    "lib64/mysql/plugin/debug/group_replication.so"
    "lib/mysql/plugin/debug/group_replication.so"
    "lib/plugin/debug/group_replication.so"
    )
  SET(FULL_PATH "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/${PATH}")
  MESSAGE(STATUS "Looking for ${FULL_PATH}")
  IF(EXISTS ${FULL_PATH})
    SET(DEBUG_PLUGIN ${FULL_PATH})
    BREAK()
  ENDIF()
ENDFOREACH()

IF(NOT DEBUG_PLUGIN)
  MESSAGE(WARNING "Could not find debug version of group_replication.so")
  RETURN()
ENDIF()

# Use patchelf to see if patching already done,
# and to find the RPATH to remove.
FIND_PROGRAM(PATCHELF_EXECUTABLE patchelf)
IF(NOT PATCHELF_EXECUTABLE)
  MESSAGE(WARNING "Could not find patchelf utility.")
  RETURN()
ENDIF()

EXECUTE_PROCESS(COMMAND ${PATCHELF_EXECUTABLE} --print-rpath ${DEBUG_PLUGIN}
  OUTPUT_VARIABLE PATCHELF_PATH
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE PATCHELF_RESULT
  )

IF(NOT PATCHELF_PATH MATCHES "debug/library_output_directory:")
  MESSAGE(STATUS "RPATH_CHANGE already done for ${DEBUG_PLUGIN}")
  RETURN()
ENDIF()

# We add -Wl,-rpath,'\$ORIGIN/../../private' in the top level cmake file.
# When linking with our bundled (shared) protobuf library, cmake will add
# ":<path to library_output_directory in debug build>:"
STRING(REGEX MATCH ":(.*)/library_output_directory:" UNUSED ${PATCHELF_PATH})
IF(CMAKE_MATCH_1)
  SET(REMOVE_OLD_RPATH "${CMAKE_MATCH_1}/library_output_directory:")
ELSE()
  MESSAGE(WARNING "Could not find RPATH for ${DEBUG_PLUGIN}")
  RETURN()
ENDIF()

# See the generated cmake_install.cmake files:
FILE(RPATH_CHANGE
  FILE "${DEBUG_PLUGIN}"
  OLD_RPATH "${REMOVE_OLD_RPATH}"
  NEW_RPATH ""
  )
