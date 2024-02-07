# Copyright (c) 2010, 2024, Oracle and/or its affiliates.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

include(wix_setup.cmake)

IF(NOT WIX_DIR)
  RETURN()
ENDIF()

# WiX wants the license text as rtf; if there is no rtf license, 
# we create a fake one from the plain text LICENSE file.
CREATE_WIX_LICENCE_AND_RTF("${CMAKE_SOURCE_DIR}/LICENSE")

SET(CPACK_WIX_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/CPackWixConfig.cmake)
SET(CPACK_WIX_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/mysql_server_extra.wxs;
  ${CMAKE_CURRENT_BINARY_DIR}/custom_ui.wxs;
  ${CMAKE_CURRENT_BINARY_DIR}/redist_check.wxs")
SET(CPACK_MSI_SETUP ${CMAKE_CURRENT_SOURCE_DIR}/cpack_msi_setup.cmake)

CONFIGURE_FILE(
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql_server_extra.wxs.in
  ${CMAKE_CURRENT_BINARY_DIR}/mysql_server_extra.wxs
  @ONLY)

CONFIGURE_FILE(
  ${CMAKE_CURRENT_SOURCE_DIR}/redist_check.wxs.in
  ${CMAKE_CURRENT_BINARY_DIR}/redist_check.wxs
  @ONLY)

CONFIGURE_FILE(
  ${CMAKE_CURRENT_SOURCE_DIR}/custom_ui.wxs.in
  ${CMAKE_CURRENT_BINARY_DIR}/custom_ui.wxs
  @ONLY)

CONFIGURE_FILE(
  ${CMAKE_CURRENT_SOURCE_DIR}/create_msi.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/create_msi.cmake
  @ONLY)

IF(CMAKE_GENERATOR MATCHES "Visual Studio")
 SET(CONFIG_PARAM "-DCMAKE_INSTALL_CONFIG_NAME=${CMAKE_CFG_INTDIR}")
ENDIF()

# The 'dist' target will create Docs/INFO_SRC, and our top level cmake file does:
# INSTALL(DIRECTORY Docs/ DESTINATION ${INSTALL_DOCDIR}
# We install only the copy in ${CMAKE_BINARY_DIR}/Docs/INFO_SRC
IF(DEFINED ENV{PB2WORKDIR})
  IF(EXISTS ${CMAKE_SOURCE_DIR}/Docs/INFO_SRC)
    ADD_CUSTOM_TARGET(remove_INFO_SRC_from_bs
      COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_SOURCE_DIR}/Docs/INFO_SRC
      )
  ENDIF()
ENDIF()

ADD_CUSTOM_TARGET(
  MSI
  COMMAND set VS_UNICODE_OUTPUT=
  COMMAND ${CMAKE_COMMAND} 
  ${CONFIG_PARAM} 
  -P  ${CMAKE_CURRENT_BINARY_DIR}/create_msi.cmake
)

IF(TARGET remove_INFO_SRC_from_bs)
  ADD_DEPENDENCIES(MSI remove_INFO_SRC_from_bs)
ENDIF()
