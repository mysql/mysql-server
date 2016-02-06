# Copyright 2010, Oracle and/or its affiliates. All rights reserved.
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

IF(MSVC_VERSION LESS 1600)
 RETURN()
ENDIF()



SET(MANUFACTURER "MariaDB Corporation Ab")
FIND_PATH(WIX_DIR heat.exe
  "$ENV{ProgramFiles}/WiX Toolset v3.9/bin"
  "$ENV{ProgramFiles}/WiX Toolset v3.10/bin"
)
SET(CPACK_WIX_PACKAGE_BASE_NAME "MariaDB")
IF(CMAKE_SIZEOF_VOID_P EQUAL 4)
 SET(CPACK_WIX_UPGRADE_CODE "49EB7A6A-1CEF-4A1E-9E89-B9A4993963E3")
 SET(CPACK_WIX_PACKAGE_NAME "MariaDB ${MAJOR_VERSION}.${MINOR_VERSION}")
ELSE()
 SET(CPACK_WIX_UPGRADE_CODE "2331E7BD-EE58-431B-9E18-B2B918BCEB1B")
 SET(CPACK_WIX_PACKAGE_NAME "MariaDB ${MAJOR_VERSION}.${MINOR_VERSION} (x64)")
ENDIF()

IF(WIX_DIR)
IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
  SET(WIX_ARCH_SUFFIX "x64")
ELSE()
  SET(WIX_ARCH_SUFFIX "x86")
ENDIF()
# Need some Wix SDK libraries to link with customaction
IF(MSVC_VERSION EQUAL 1600 OR MSVC_VERSION EQUAL 1700 )
  SET(WIX_MSVC_SUFFIX "VS2010")
ELSEIF(MSVC_VERSION EQUAL 1800)
  SET(WIX_MSVC_SUFFIX "VS2013")
ELSEIF (MSVC_VERSION EQUAL 1900)
  SET(WIX_MSVC_SUFFIX "VS2015")
ENDIF()


FIND_LIBRARY(WIX_WCAUTIL_LIBRARY 
  NAMES wcautil${WIX_ARCH_SUFFIX}  wcautil${WIX_MSVC_SUFFIX}${WIX_ARCH_SUFFIX}
  wcautil
  PATHS 
  ${WIX_DIR}/../SDK/${WIX_MSVC_SUFFIX}/lib/${WIX_ARCH_SUFFIX}
  )

FIND_LIBRARY(WIX_DUTIL_LIBRARY 
  NAMES dutil${WIX_ARCH_SUFFIX} 
  dutil
  PATHS 
  ${WIX_DIR}/../SDK/${WIX_MSVC_SUFFIX}/lib/${WIX_ARCH_SUFFIX}
  )
ENDIF()

IF(NOT WIX_DUTIL_LIBRARY)
  MESSAGE(STATUS "Cannot find wix 3, installer project will not be generated")
  IF(BUILD_RELEASE)
      MESSAGE(FATAL_ERROR
        "Can't find Wix. It is necessary for producing official package")
  ENDIF()
  RETURN()
ENDIF()

ADD_SUBDIRECTORY(ca)

# extra.wxs.in needs DATADIR_MYSQL_FILES and DATADIR_PERFORMANCE_SCHEMA_FILES, i.e
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
GET_TARGET_PROPERTY(WIXCA_LOCATION wixca LOCATION)
SET(CPACK_WIX_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/CPackWixConfig.cmake)

GET_TARGET_PROPERTY(upgrade_wizard_location mysql_upgrade_wizard LOCATION)
IF(NOT upgrade_wizard_location)
  SET(EXTRA_WIX_PREPROCESSOR_FLAGS "-dHaveUpgradeWizard=0")
ENDIF()
IF(WITH_INNOBASE_STORAGE_ENGINE OR WITH_INNODB_STORAGE_ENGINE OR WITH_XTRADB_STORAGE_ENGINE)
  SET(EXTRA_WIX_PREPROCESSOR_FLAGS ${EXTRA_WIX_PREPROCESSOR_FLAGS} "-dHaveInnodb=1")
ENDIF()

SET(THIRD_PARTY_FEATURE_CONDITION "")

IF(WITH_THIRD_PARTY)
  SET(THIRD_PARTY_DOWNLOAD_LOCATION "$ENV{TEMP}")
  IF(THIRD_PARTY_DOWNLOAD_LOCATION)
    FILE(TO_CMAKE_PATH "${THIRD_PARTY_DOWNLOAD_LOCATION}" THIRD_PARTY_DOWNLOAD_LOCATION)
  ELSE()
    SET(THIRD_PARTY_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}")
  ENDIF()
ENDIF()

GET_TARGET_PROPERTY(LIBMYSQL_LOCATION libmysql LOCATION)
FOREACH(third_party ${WITH_THIRD_PARTY})
  SET(third_party_install_plugin ${CMAKE_CURRENT_SOURCE_DIR}/${third_party}.cmake)
  IF(NOT EXISTS ${third_party_install_plugin})
    MESSAGE(FATAL_ERROR 
"Third party MSI installation plugin ${third_party_install_plugin} does not exist.
It was expected due to WITH_THIRD_PARTY=${WITH_THIRD_PARTY}"
)
  ENDIF()
  STRING(TOUPPER "${third_party}" upper_third_party)
  IF(NOT THIRD_PARTY_FEATURE_CONDITION )
   SET(THIRD_PARTY_FEATURE_CONDITION "<Condition Level='0'>${upper_third_party}INSTALLED")
  ELSE()
   SET(THIRD_PARTY_FEATURE_CONDITION "AND ${upper_third_party}INSTALLED")
  ENDIF()
ENDFOREACH()

IF(THIRD_PARTY_FEATURE_CONDITION)
  SET(THIRD_PARTY_FEATURE_CONDITION "${THIRD_PARTY_FEATURE_CONDITION}</Condition>")
ENDIF()

IF(NOT CPACK_WIX_UI)
  SET(CPACK_WIX_UI "MyWixUI_Mondo")
ENDIF()
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/create_msi.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/create_msi.cmake
  @ONLY)


IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
  SET(WixWin64 " Win64='yes'")
ELSE()
  SET(WixWin64)
ENDIF()


IF(CMAKE_GENERATOR MATCHES "Visual Studio")
 SET(CONFIG_PARAM "-DCMAKE_INSTALL_CONFIG_NAME=${CMAKE_CFG_INTDIR}")
ENDIF()


ADD_CUSTOM_TARGET(
  MSI
  COMMAND ${CMAKE_COMMAND} 
  ${CONFIG_PARAM} 
  -P  ${CMAKE_CURRENT_BINARY_DIR}/create_msi.cmake
)
ADD_DEPENDENCIES(MSI wixca)

ADD_CUSTOM_TARGET(
  MSI_ESSENTIALS
  COMMAND ${CMAKE_COMMAND} -DESSENTIALS=1 
  ${CONFIG_PARAM}
  -P  ${CMAKE_CURRENT_BINARY_DIR}/create_msi.cmake
)
ADD_DEPENDENCIES(MSI_ESSENTIALS wixca)

