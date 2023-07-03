<<<<<<< HEAD
# Copyright (c) 2010, 2022, Oracle and/or its affiliates.
=======
<<<<<<< HEAD
# Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.
=======
# Copyright (c) 2010, 2023, Oracle and/or its affiliates.
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
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

<<<<<<< HEAD
# WiX wants the license text as rtf; if there is no rtf license, 
# we create a fake one from the plain text LICENSE file.
CREATE_WIX_LICENCE_AND_RTF("${CMAKE_SOURCE_DIR}/LICENSE")
=======
<<<<<<< HEAD
ADD_SUBDIRECTORY(ca)
=======
FIND_PROGRAM(HEAT_EXECUTABLE heat ${WIX_DIR})
FIND_PROGRAM(CANDLE_EXECUTABLE candle ${WIX_DIR})
FIND_PROGRAM(LIGHT_EXECUTABLE light ${WIX_DIR})
>>>>>>> upstream/cluster-7.6

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
<<<<<<< HEAD
# GET_TARGET_PROPERTY(WIXCA_LOCATION wixca LOCATION)
# MESSAGE(STATUS "\nWIXCA_LOCATION ${WIXCA_LOCATION}\n")
>>>>>>> pr/231

=======
>>>>>>> upstream/cluster-7.6
SET(CPACK_WIX_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/CPackWixConfig.cmake)
SET(CPACK_WIX_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/mysql_server_extra.wxs;
  ${CMAKE_CURRENT_SOURCE_DIR}/custom_ui.wxs;
  ${CMAKE_CURRENT_BINARY_DIR}/redist_check.wxs")
SET(CPACK_MSI_SETUP ${CMAKE_CURRENT_SOURCE_DIR}/cpack_msi_setup.cmake)

ADD_CUSTOM_TARGET(
  configure_mysql_server_extra
  COMMAND ${CMAKE_COMMAND}
  -DINPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/mysql_server_extra.wxs.in
  -DOUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/mysql_server_extra.wxs
  -DMAJOR_VERSION=${MAJOR_VERSION}
  -DMINOR_VERSION=${MINOR_VERSION}
  -P ${CMAKE_CURRENT_SOURCE_DIR}/configure_file.cmake
)

ADD_CUSTOM_TARGET(
  configure_redist_check
  COMMAND ${CMAKE_COMMAND}
  -DINPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/redist_check.wxs.in
  -DOUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/redist_check.wxs
  -DLINK_STATIC_RUNTIME_LIBRARIES=${LINK_STATIC_RUNTIME_LIBRARIES}
  -DBUNDLE_RUNTIME_LIBRARIES=${BUNDLE_RUNTIME_LIBRARIES}
  -P ${CMAKE_CURRENT_SOURCE_DIR}/configure_file.cmake
)

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/create_msi.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/create_msi.cmake
  @ONLY)

IF(CMAKE_GENERATOR MATCHES "Visual Studio")
 SET(CONFIG_PARAM "-DCMAKE_INSTALL_CONFIG_NAME=${CMAKE_CFG_INTDIR}")
ENDIF()

<<<<<<< HEAD
=======
<<<<<<< HEAD
=======
>>>>>>> pr/231
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

<<<<<<< HEAD
=======
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
ADD_CUSTOM_TARGET(
  MSI
  COMMAND set VS_UNICODE_OUTPUT=
  COMMAND ${CMAKE_COMMAND} 
  ${CONFIG_PARAM} 
  -P  ${CMAKE_CURRENT_BINARY_DIR}/create_msi.cmake
)
<<<<<<< HEAD
ADD_DEPENDENCIES(MSI configure_mysql_server_extra configure_redist_check)
=======
ADD_DEPENDENCIES(MSI configure_mysql_server_extra)
>>>>>>> pr/231

IF(TARGET remove_INFO_SRC_from_bs)
  ADD_DEPENDENCIES(MSI remove_INFO_SRC_from_bs)
ENDIF()
