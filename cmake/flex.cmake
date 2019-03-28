# Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.
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

# File flex.cmake, adapted from bison.cmake
# Tested with Flex 2.5.37

FIND_PROGRAM(FLEX_EXECUTABLE flex DOC "path to the flex executable")
MARK_AS_ADVANCED(FLEX_EXECUTABLE "")

IF(NOT FLEX_EXECUTABLE)
  MESSAGE(WARNING "Flex executable not found in PATH")
ELSEIF(FLEX_EXECUTABLE AND NOT FLEX_USABLE)
  # Check version as well
  EXEC_PROGRAM(${FLEX_EXECUTABLE} ARGS --version OUTPUT_VARIABLE FLEX_VERSION_STR)
   # Get first line in case it's multiline
   STRING(REGEX REPLACE "([^\n]+).*" "\\1" FIRST_LINE "${FLEX_VERSION_STR}")
   # get version information
   STRING(REGEX REPLACE ".* ([0-9]+)\\.([0-9]+)" "\\1" FLEX_VERSION_MAJOR "${FIRST_LINE}")
   STRING(REGEX REPLACE ".* ([0-9]+)\\.([0-9]+)" "\\2" FLEX_VERSION_MINOR "${FIRST_LINE}")
   IF (FLEX_VERSION_MAJOR LESS 2)
     MESSAGE(WARNING "Flex version is old. please update to version 2")
   ELSE()
     SET(FLEX_USABLE 1 CACHE INTERNAL "Flex version 2 or higher")
   ENDIF()
ENDIF()


# Handle out-of-source build from source package with possibly broken 
# flex. Copy flex output from source to build directory, if not already
# there
MACRO(COPY_FLEX_OUTPUT input_cc input_h output_cc output_h)
  IF(EXISTS ${input_cc} AND NOT EXISTS ${output_cc})
    CONFIGURE_FILE(${input_cc} ${output_cc} COPYONLY)
    CONFIGURE_FILE(${input_h}  ${output_h}  COPYONLY)
  ENDIF()
ENDMACRO()


# Use flex to generate C++ and header file
MACRO (RUN_FLEX input_ll output_cc output_h name_prefix)
  IF(FLEX_USABLE)
    ADD_CUSTOM_COMMAND(
      OUTPUT ${output_cc}
             ${output_h}
      COMMAND ${FLEX_EXECUTABLE}
       --prefix=${name_prefix}
       --outfile=${output_cc}
       --header-file=${output_h}
        ${input_ll}
        DEPENDS ${input_ll}
	)
  ELSE()
    # Flex is missing or not usable, e.g too old
    IF(EXISTS  ${output_cc} AND EXISTS ${output_h})
      IF(${input_ll} IS_NEWER_THAN ${output_cc}  OR  ${input_ll} IS_NEWER_THAN ${output_h})
        # Possibly timestamps are messed up in source distribution.
        MESSAGE(WARNING "No usable flex found, ${input_ll} will not be rebuilt.")
      ENDIF()
    ELSE()
      # Output files are missing, bail out.
      SET(ERRMSG 
         "Flex (The Fast Lexical Analyzer) is required to build MySQL. "
         "Please install flex. "
         "You can download flex from https://github.com/westes/flex/releases"
      )
      MESSAGE(FATAL_ERROR ${ERRMSG})
    ENDIF()
  ENDIF()
ENDMACRO()
