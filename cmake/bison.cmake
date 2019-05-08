# Copyright (c) 2009, 2019, Oracle and/or its affiliates. All rights reserved.
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

FIND_PROGRAM(BISON_EXECUTABLE bison DOC "path to the bison executable")
MARK_AS_ADVANCED(BISON_EXECUTABLE "")

IF(NOT BISON_EXECUTABLE)
  MESSAGE(WARNING "Bison executable not found in PATH")
ELSEIF(BISON_EXECUTABLE AND NOT BISON_USABLE)
  # Check version as well
  EXEC_PROGRAM(${BISON_EXECUTABLE} ARGS --version OUTPUT_VARIABLE BISON_OUTPUT)
  # get version information
  STRING(REGEX REPLACE
    "^bison \\(GNU Bison\\) ([0-9]+\\.[0-9]+(\\.[0-9]+)?).*" "\\1"
    BISON_VERSION "${BISON_OUTPUT}")
  MESSAGE(STATUS
    "Found Bison: ${BISON_EXECUTABLE} (found version is ${BISON_VERSION})")
  IF (BISON_VERSION VERSION_LESS "2.1")
    MESSAGE(WARNING "Bison version ${BISON_VERSION} is old. \
      Please update to version 2.1 or higher")
  ELSE()
    SET(BISON_USABLE 1 CACHE INTERNAL "Bison version 2 or higher")
    IF(BISON_VERSION VERSION_LESS "2.4")
      # Don't use --warnings since unsupported
      SET(BISON_FLAGS_WARNINGS "" CACHE INTERNAL "BISON 2.x flags")
    ELSEIF(BISON_VERSION VERSION_LESS "3.0")
      # Enable all warnings
      SET(BISON_FLAGS_WARNINGS
        "--warnings=all"
	CACHE INTERNAL "BISON 2.x flags")
    ELSE()
      # TODO: replace with "--warnings=all"
      # For the backward compatibility with 2.x, suppress warnings:
      # * no-yacc: for --yacc
      # * no-empty-rule: for empty rules without %empty
      # * no-precedence: for useless precedence or/and associativity rules
      SET(BISON_FLAGS_WARNINGS
        "--warnings=all,no-yacc,no-empty-rule,no-precedence"
	CACHE INTERNAL "BISON 3.x flags")
    ENDIF()
  ENDIF()
ENDIF()


# Handle out-of-source build from source package with possibly broken 
# bison. Copy bison output to from source to build directory, if not already 
# there
MACRO(COPY_BISON_OUTPUT input_cc input_h output_cc output_h)
  IF(EXISTS ${input_cc} AND NOT EXISTS ${output_cc})
    CONFIGURE_FILE(${input_cc} ${output_cc} COPYONLY)
    CONFIGURE_FILE(${input_h}  ${output_h}  COPYONLY)
  ENDIF()
ENDMACRO()


# Use bison to generate C++ and header file
MACRO (RUN_BISON input_yy output_cc output_h name_prefix)
  IF(BISON_USABLE)
    ADD_CUSTOM_COMMAND(
      OUTPUT ${output_cc}
             ${output_h}
      COMMAND ${BISON_EXECUTABLE}
       --name-prefix=${name_prefix}
       --yacc
       ${BISON_FLAGS_WARNINGS}
       --output=${output_cc}
       --defines=${output_h}
        ${input_yy}
        DEPENDS ${input_yy}
	)
  ELSE()
    # Bison is missing or not usable, e.g too old
    IF(EXISTS  ${output_cc} AND EXISTS ${output_h})
      IF(${input_yy} IS_NEWER_THAN ${output_cc}  OR  ${input_yy} IS_NEWER_THAN ${output_h})
        # Possibly timestamps are messed up in source distribution.
        MESSAGE(WARNING "No usable bison found, ${input_yy} will not be rebuilt.")
      ENDIF()
    ELSE()
      # Output files are missing, bail out.
      SET(ERRMSG 
         "Bison (GNU parser generator) is required to build MySQL." 
         "Please install bison."
      )
      IF(WIN32)
       SET(ERRMSG ${ERRMSG} 
       "You can download bison from http://gnuwin32.sourceforge.net/packages/bison.htm "
       "Choose 'Complete package, except sources' installation. We recommend to "
       "install bison into a directory without spaces, e.g C:\\GnuWin32.")
      ENDIF()
      MESSAGE(FATAL_ERROR ${ERRMSG})
    ENDIF()
  ENDIF()
ENDMACRO()
