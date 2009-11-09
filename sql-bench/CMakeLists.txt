# Copyright (C) 2009 Sun Microsystems, Inc
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

# Install sql-bench files
FILE(GLOB all_files 
${CMAKE_SOURCE_DIR}/sql-bench/* 
${CMAKE_SOURCE_DIR}/sql-bench/Data/ATIS/*
${CMAKE_SOURCE_DIR}/sql-bench/Data/Wisconsin/*
${CMAKE_SOURCE_DIR}/sql-bench/Comments/*
${CMAKE_SOURCE_DIR}/sql-bench/limits/*
)

GET_FILENAME_COMPONENT(basedir ${CMAKE_SOURCE_DIR} ABSOLUTE)
FOREACH(file ${all_files})
  IF(NOT IS_DIRECTORY ${file} AND NOT ${file} MATCHES "Make" )
  FILE(RELATIVE_PATH relpath ${basedir} ${file})
  SET(target_relpath ${relpath})
  GET_FILENAME_COMPONENT(ext ${file} EXT)
  GET_FILENAME_COMPONENT(dir ${relpath} PATH)
  IF(ext MATCHES ".sh$")
    # Those are perl files actually
    STRING(REPLACE ".sh" "" target_relpath ${target_relpath} )
    IF(WIN32)
      IF(NOT ext MATCHES ".pl")
        SET(target_relpath "${target_relpath}.pl")
      ENDIF()
    ENDIF()
  ENDIF()
  SET(target "${CMAKE_BINARY_DIR}/${target_relpath}")
  CONFIGURE_FILE(${file} ${target} COPYONLY)
  IF (ext MATCHES ".bat")
    IF(WIN32) 
      INSTALL(FILES ${target} DESTINATION ${dir})
    ENDIF()
  ELSE()
    INSTALL(FILES ${target} DESTINATION ${dir})
  ENDIF()
  ENDIF()
ENDFOREACH()
