# Copyright (c) 2009 Sun Microsystems, Inc.
# Use is subject to license terms.
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

IF(CMAKE_SYSTEM_NAME MATCHES "SunOS")
 # On Solaris, /opt/csw often contains a newer bison 
 IF(NOT BISON_EXECUTABLE AND EXISTS /opt/csw/bin/bison)
   SET(BISON_EXECUTABLE /opt/csw/bin/bison)
 ENDIF()
ENDIF()
FIND_PROGRAM(BISON_EXECUTABLE bison DOC "path to the bison executable")
MARK_AS_ADVANCED(BISON_EXECUTABLE "")
IF(NOT BISON_EXECUTABLE)
  MESSAGE("Warning: Bison executable not found in PATH")
ELSEIF(BISON_EXECUTABLE AND NOT BISON_USABLE)
  # Check version as well
  EXEC_PROGRAM(${BISON_EXECUTABLE} ARGS --version OUTPUT_VARIABLE BISON_VERSION_STR)
   # Get first line in case it's multiline
   STRING(REGEX REPLACE "([^\n]+).*" "\\1" FIRST_LINE "${BISON_VERSION_STR}")
   # get version information
   STRING(REGEX REPLACE ".* ([0-9]+)\\.([0-9]+)" "\\1" BISON_VERSION_MAJOR "${FIRST_LINE}")
   STRING(REGEX REPLACE ".* ([0-9]+)\\.([0-9]+)" "\\2" BISON_VERSION_MINOR "${FIRST_LINE}")
   IF (BISON_VERSION_MAJOR LESS 2)
     MESSAGE("Warning: bison version is old. please update to version 2")
   ELSE()
     SET(BISON_USABLE 1 CACHE INTERNAL "Bison version 2 or higher")
   ENDIF()
ENDIF()

# Use bison to generate C++ and header file
MACRO (RUN_BISON input_yy output_cc output_h)
  IF(BISON_TOO_OLD)
    IF(EXISTS ${output_cc} AND EXISTS ${output_h})
      SET(BISON_USABLE FALSE)
    ENDIF()
  ENDIF()
  IF(BISON_USABLE)
    ADD_CUSTOM_COMMAND(
      OUTPUT ${output_cc}
             ${output_h}
      COMMAND ${BISON_EXECUTABLE}  -y -p MYSQL 
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
        MESSAGE("Warning: no usable bison found, ${input_yy} will not be rebuilt.")
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
