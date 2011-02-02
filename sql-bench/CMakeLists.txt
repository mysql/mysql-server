# Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

# Install sql-bench files

IF(NOT INSTALL_SQLBENCHDIR)
 RETURN()
ENDIF()

IF(INSTALL_SQLBENCHDIR STREQUAL ".")
  SET(prefix)
ELSE()
 SET(prefix ${INSTALL_SQLBENCHDIR}/)
ENDIF()

INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Data/ATIS
  DESTINATION  ${prefix}sql-bench/Data COMPONENT SqlBench)

INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Data/Wisconsin
  DESTINATION  ${prefix}sql-bench/Data COMPONENT SqlBench)

INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/limits
  DESTINATION  ${prefix}sql-bench COMPONENT SqlBench)

FILE(GLOB all_files ${CMAKE_CURRENT_SOURCE_DIR}/*)

FOREACH(file ${all_files})
  IF(NOT IS_DIRECTORY ${file} AND NOT ${file} MATCHES "Make|as3ap|/example$" )
  GET_FILENAME_COMPONENT(ext ${file} EXT)
  GET_FILENAME_COMPONENT(name ${file} NAME)
  SET(target ${name})
  IF(ext MATCHES ".sh$")
    # Those are perl files actually
    STRING(REPLACE ".sh" "" target ${target} )
    IF(WIN32)
      IF(NOT ext MATCHES ".pl")
        SET(target "${target}.pl")
      ENDIF()
    ENDIF()
  ENDIF()
  SET(target "${CMAKE_CURRENT_BINARY_DIR}/${target}")
  CONFIGURE_FILE(${file} ${target} COPYONLY)
  IF (ext MATCHES ".bat")
    IF(WIN32) 
      INSTALL(FILES ${target} DESTINATION ${prefix}sql-bench COMPONENT SqlBench)
    ENDIF()
  ELSE()
    INSTALL(FILES ${target} DESTINATION ${prefix}sql-bench COMPONENT SqlBench)
  ENDIF()
  ENDIF()
ENDFOREACH()
