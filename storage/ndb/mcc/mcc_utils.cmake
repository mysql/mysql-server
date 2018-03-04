# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
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

MACRO(PY_INSTALL)

  MYSQL_PARSE_ARGUMENTS(ARG
    "FILES;SRC_DIR;DESTINATION"
    ""
    ${ARGN}
  )

  SET(PY_FILES ${ARG_FILES})
  SET(PY_SRC_DIR "${ARG_SRC_DIR}")
  SET(PY_DEST_DIR "${ARG_DESTINATION}")

  FOREACH(sfile ${PY_FILES})
	INSTALL(FILES "${PY_SRC_DIR}/${sfile}"
		DESTINATION "${PY_DEST_DIR}"
		COMPONENT ClusterTools)
#	MESSAGE(STATUS "INSTALL: ${PY_SRC_DIR}/${sfile} -> ${PY_DEST_DIR}")
  ENDFOREACH()
  
ENDMACRO()

MACRO(ADD_ZIP_COMMAND ARCHIVE FILELIST)

SET(ZIP_EXECUTABLE "")
FIND_PROGRAM(ZIP_EXECUTABLE wzzip PATHS "$ENV{ProgramFiles}/WinZip")
IF(ZIP_EXECUTABLE)
    MESSAGE(STATUS "Using ${ZIP_EXECUTABLE}")
	ADD_CUSTOM_COMMAND(OUTPUT "${ARCHIVE}" 
	  COMMAND ${ZIP_EXECUTABLE} -P \"${ARCHIVE}\" ${FILELIST}
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
ENDIF(ZIP_EXECUTABLE)

IF(NOT ZIP_EXECUTABLE)
  FIND_PROGRAM(ZIP_EXECUTABLE 7z PATHS "$ENV{ProgramFiles}/7-Zip")
  IF(ZIP_EXECUTABLE)
    MESSAGE(STATUS "Using ${ZIP_EXECUTABLE}")
	ADD_CUSTOM_COMMAND(OUTPUT "${ARCHIVE}" 
	  COMMAND ${ZIP_EXECUTABLE} a -tzip \"${ARCHIVE}\" ${FILELIST}
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  ENDIF(ZIP_EXECUTABLE)
ENDIF(NOT ZIP_EXECUTABLE)

IF(NOT ZIP_EXECUTABLE)
  FIND_PACKAGE(Cygwin)
  IF(CYGWIN_INSTALL_PATH)
  MESSAGE(STATUS "Using cygwin install path with ${ZIP_EXECUTABLE}")
    FIND_PROGRAM(ZIP_EXECUTABLE zip PATHS "${CYGWIN_INSTALL_PATH}/bin")
  ELSE()
  MESSAGE(STATUS "Using other install path ${ZIP_EXECUTABLE}")
    FIND_PROGRAM(ZIP_EXECUTABLE zip PATHS "$ENV{PATH}")
  ENDIF()
  IF(ZIP_EXECUTABLE)
    MESSAGE(STATUS "Using ${ZIP_EXECUTABLE}")
	ADD_CUSTOM_COMMAND(OUTPUT "${ARCHIVE}" 
	  COMMAND ${ZIP_EXECUTABLE} -r ${ARCHIVE} ${FILELIST}
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  ENDIF(ZIP_EXECUTABLE)
ENDIF(NOT ZIP_EXECUTABLE)

IF(NOT ZIP_EXECUTABLE)
  IF(JAVA_ARCHIVE)
    SET(ZIP_EXECUTABLE ${JAVA_ARCHIVE})
    MESSAGE(STATUS "Using ${ZIP_EXECUTABLE}")
	ADD_CUSTOM_COMMAND(OUTPUT "${ARCHIVE}" 
	  COMMAND ${ZIP_EXECUTABLE} cf ${ARCHIVE} ${FILELIST}
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  ENDIF()
ENDIF(NOT ZIP_EXECUTABLE)

ENDMACRO()

