# Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.
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

IF(NOT WIN32)
  RETURN()
ENDIF()

FIND_PATH(WIX_DIR heat.exe
   $ENV{WIX_DIR}/bin
   $ENV{ProgramFiles}/wix/bin
  "$ENV{ProgramFiles}/Windows Installer XML v3/bin"
  "$ENV{ProgramFiles}/Windows Installer XML v3.5/bin"
  "$ENV{ProgramFiles}/WiX Toolset V3.6/bin"
  "$ENV{ProgramFiles}/WiX Toolset V3.8/bin"
)

IF(NOT WIX_DIR)
  FIND_PATH(WIX_DIR heat.exe
    "$ENV{ProgramFiles}/WiX Toolset V3.10/bin"
  )
ENDIF()

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
# we create a fake one from the plain text COPYING file.
IF(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/COPYING.rtf")
 SET(COPYING_RTF "${CMAKE_CURRENT_SOURCE_DIR}/COPYING.rtf")
ELSE()
  IF(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../LICENSE.mysql")
    SET(LICENSE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../LICENSE.mysql")
  ELSE()
    SET(LICENSE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../COPYING")
  ENDIF()
  FILE(READ ${LICENSE_FILE} CONTENTS)
  STRING(REGEX REPLACE "\n" "\\\\par\n" CONTENTS "${CONTENTS}")
  STRING(REGEX REPLACE "\t" "\\\\tab" CONTENTS "${CONTENTS}")
  FILE(WRITE "${CMAKE_CURRENT_BINARY_DIR}/COPYING.rtf" "{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0\\fnil\\fcharset0 Courier New;}}\\viewkind4\\uc1\\pard\\lang1031\\f0\\fs15")
  FILE(APPEND "${CMAKE_CURRENT_BINARY_DIR}/COPYING.rtf" "${CONTENTS}")
  FILE(APPEND "${CMAKE_CURRENT_BINARY_DIR}/COPYING.rtf" "\n}\n")
  SET(COPYING_RTF "${CMAKE_CURRENT_BINARY_DIR}/COPYING.rtf")
ENDIF()
SET(CPACK_WIX_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/CPackWixConfig.cmake)
SET(CPACK_WIX_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/mysql_server_extra.wxs;${CMAKE_CURRENT_SOURCE_DIR}/custom_ui.wxs")

IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
  SET(WixWin64 " Win64='yes'")
ELSE()
  SET(WixWin64)
  SET(WixWin64 " Win64='no'")
ENDIF()

IF("${VERSION}" MATCHES "-ndb-")
  MESSAGE(STATUS "This is Cluster build")
  # Setup DISPLAY_XXX_VERSION which create_msi.cmake will
  # use as version to "display" when building the msi
  # (i.e replacing for example 5.6.21 with 7.4.3)
  MESSAGE(STATUS "Setting DISPLAY_XXX_VERSION")
  SET(DISPLAY_MAJOR_VERSION ${NDB_VERSION_MAJOR})
  SET(DISPLAY_MINOR_VERSION ${NDB_VERSION_MINOR})
  SET(DISPLAY_PATCH_VERSION ${NDB_VERSION_BUILD})
  MESSAGE(STATUS "DISPLAY_MAJOR_VERSION: ${DISPLAY_MAJOR_VERSION}")
  MESSAGE(STATUS "DISPLAY_MINOR_VERSION: ${DISPLAY_MINOR_VERSION}")
  MESSAGE(STATUS "DISPLAY_PATCH_VERSION: ${DISPLAY_PATCH_VERSION}")
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

