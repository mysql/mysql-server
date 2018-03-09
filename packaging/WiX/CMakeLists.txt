# Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.
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

IF(NOT WIN32)
  RETURN()
ENDIF()

include(wix_setup.cmake)

IF(NOT WIX_DIR)
  RETURN()
ENDIF()

ADD_SUBDIRECTORY(ca)

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
# GET_TARGET_PROPERTY(WIXCA_LOCATION wixca LOCATION)
# MESSAGE(STATUS "\nWIXCA_LOCATION ${WIXCA_LOCATION}\n")

SET(CPACK_WIX_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/CPackWixConfig.cmake)
SET(CPACK_WIX_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/mysql_server_extra.wxs;${CMAKE_CURRENT_SOURCE_DIR}/custom_ui.wxs")
SET(CPACK_MSI_SETUP ${CMAKE_CURRENT_SOURCE_DIR}/cpack_msi_setup.cmake)

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

#CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/mysql_server_extra.wxs.in
#  ${CMAKE_CURRENT_BINARY_DIR}/mysql_server_extra.wxs)

ADD_CUSTOM_TARGET(
  configure_mysql_server_extra
  COMMAND ${CMAKE_COMMAND}
  -DINPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/mysql_server_extra.wxs.in
  -DOUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/mysql_server_extra.wxs
  -DWIXCA_LOCATION=$<TARGET_FILE:wixca>
  -DMAJOR_VERSION=${MAJOR_VERSION}
  -DMINOR_VERSION=${MINOR_VERSION}
  -P ${CMAKE_CURRENT_SOURCE_DIR}/configure_file.cmake
)

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
  -DWIXCA_LOCATION=$<TARGET_FILE:wixca>
  -P  ${CMAKE_CURRENT_BINARY_DIR}/create_msi.cmake
)
ADD_DEPENDENCIES(MSI wixca)
ADD_DEPENDENCIES(MSI configure_mysql_server_extra)
