# Copyright (c) 2006, 2013, Oracle and/or its affiliates. All rights reserved.
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
    IF (UNIX AND NOT ${name} MATCHES "README")
      EXECUTE_PROCESS(COMMAND chmod +x ${target})
    ENDIF()
    IF (ext MATCHES ".bat")
      IF(WIN32) 
        INSTALL(PROGRAMS ${target}
          DESTINATION ${prefix}sql-bench COMPONENT SqlBench)
      ENDIF()
    ELSEIF(name MATCHES "README")
      INSTALL(FILES ${target}
        DESTINATION ${prefix}sql-bench COMPONENT SqlBench)
    ELSE()
      INSTALL(PROGRAMS ${target}
        DESTINATION ${prefix}sql-bench COMPONENT SqlBench)
    ENDIF()
  ENDIF()
ENDFOREACH()
