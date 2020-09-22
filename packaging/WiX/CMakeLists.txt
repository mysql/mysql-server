# Copyright (c) 2010, 2020, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

IF(NOT WIN32)
  RETURN()
ENDIF()

FIND_PATH(WIX_DIR heat.exe
   $ENV{WIX_DIR}/bin
   $ENV{WIX_DIR}
   $ENV{ProgramFiles}/wix/bin
   "$ENV{ProgramFiles}/WiX Toolset V3.8/bin"
   "$ENV{ProgramFiles}/WiX Toolset V3.8"
   "$ENV{ProgramFiles}/WiX Toolset V3.10/bin"
   "$ENV{ProgramFiles}/WiX Toolset V3.10"
)

IF(NOT WIX_DIR)
  IF(NOT _WIX_DIR_CHECKED)
    SET(_WIX_DIR_CHECKED 1 CACHE INTERNAL "")
    MESSAGE(STATUS "Cannot find wix 3, installer project will not be generated")
  ENDIF()
  RETURN()
ENDIF()

# mysql_server_extra.wxs.in needs DATADIR_MYSQL_FILES and DATADIR_PERFORMANCE_SCHEMA_FILES, i.e
# Wix-compatible file lists for ${builddir}\sql\data\{mysql,performance_schema}

FOREACH(dir mysql performance_schema)
 FILE(GLOB files ${CMAKE_BINARY_DIR}/sql/data/${dir}/*)
 SET(filelist)
 FOREACH(f ${files})
   IF(NOT f MATCHES ".rule")
   FILE(TO_NATIVE_PATH "${f}" file_native_path)
   GET_FILENAME_COMPONENT(file_name "${f}" NAME)
   SET(filelist 
"${filelist}
<File Id='${file_name}' Source='${file_native_path}'/>")
   ENDIF()
 ENDFOREACH()
 STRING(TOUPPER ${dir} DIR_UPPER)
 SET(DATADIR_${DIR_UPPER}_FILES "${filelist}")
ENDFOREACH()

FILE(GLOB files ${CMAKE_BINARY_DIR}/sql/data/*)
SET(filelist)
FOREACH(f ${files})
  IF(IS_DIRECTORY "${f}")
  ELSE()
    IF(NOT (f MATCHES ".rule" OR f MATCHES "bootstrap.sql"))
      FILE(TO_NATIVE_PATH "${f}" file_native_path)
      GET_FILENAME_COMPONENT(file_name "${f}" NAME)
      SET(filelist 
"${filelist}
<File Id='${file_name}' Source='${file_native_path}'/>")
    ENDIF()
  ENDIF()
ENDFOREACH()
SET(DATADIR_FILES "${filelist}")


FIND_PROGRAM(HEAT_EXECUTABLE heat ${WIX_DIR})
FIND_PROGRAM(CANDLE_EXECUTABLE candle ${WIX_DIR})
FIND_PROGRAM(LIGHT_EXECUTABLE light ${WIX_DIR})

# WiX wants the license text as rtf; if there is no rtf license, 
# we create a fake one from the plain text LICENSE file.
IF(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.rtf")
 SET(LICENSE_RTF "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.rtf")
ELSE()
  SET(LICENSE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../LICENSE")
  FILE(READ ${LICENSE_FILE} CONTENTS)
  STRING(REGEX REPLACE "\n" "\\\\par\n" CONTENTS "${CONTENTS}")
  STRING(REGEX REPLACE "\t" "\\\\tab" CONTENTS "${CONTENTS}")
  FILE(WRITE "${CMAKE_CURRENT_BINARY_DIR}/LICENSE.rtf" "{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0\\fnil\\fcharset0 Courier New;}}\\viewkind4\\uc1\\pard\\lang1031\\f0\\fs15")
  FILE(APPEND "${CMAKE_CURRENT_BINARY_DIR}/LICENSE.rtf" "${CONTENTS}")
  FILE(APPEND "${CMAKE_CURRENT_BINARY_DIR}/LICENSE.rtf" "\n}\n")
  SET(LICENSE_RTF "${CMAKE_CURRENT_BINARY_DIR}/LICENSE.rtf")
ENDIF()
SET(CPACK_WIX_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/CPackWixConfig.cmake)
SET(CPACK_WIX_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/mysql_server_extra.wxs;${CMAKE_CURRENT_SOURCE_DIR}/custom_ui.wxs")

IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
  SET(WixWin64 " Win64='yes'")
ELSE()
  SET(WixWin64)
  SET(WixWin64 " Win64='no'")
ENDIF()

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/mysql_server_extra.wxs.in
  ${CMAKE_CURRENT_BINARY_DIR}/mysql_server_extra.wxs)

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/create_msi.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/create_msi.cmake
  @ONLY)

IF(CMAKE_GENERATOR MATCHES "Visual Studio")
 SET(CONFIG_PARAM "-DCMAKE_INSTALL_CONFIG_NAME=${CMAKE_CFG_INTDIR}")
ENDIF()


ADD_CUSTOM_TARGET(
  MSI
  COMMAND set VS_UNICODE_OUTPUT=
  COMMAND ${CMAKE_COMMAND} 
  ${CONFIG_PARAM} 
  -P  ${CMAKE_CURRENT_BINARY_DIR}/create_msi.cmake
)

ADD_CUSTOM_TARGET(
  MSI_ESSENTIALS
  COMMAND set VS_UNICODE_OUTPUT=
  COMMAND ${CMAKE_COMMAND} -DESSENTIALS=1 
  ${CONFIG_PARAM}
  -P  ${CMAKE_CURRENT_BINARY_DIR}/create_msi.cmake
)

