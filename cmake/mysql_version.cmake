# Copyright (c) 2009, 2011, Oracle and/or its affiliates. All rights reserved.
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

#
# Global constants, only to be changed between major releases.
#

SET(SHARED_LIB_MAJOR_VERSION "18")
SET(PROTOCOL_VERSION "10")
SET(DOT_FRM_VERSION "6")

# Generate "something" to trigger cmake rerun when VERSION changes
CONFIGURE_FILE(
  ${CMAKE_SOURCE_DIR}/VERSION
  ${CMAKE_BINARY_DIR}/VERSION.dep
)

# Read value for a variable from VERSION.

MACRO(MYSQL_GET_CONFIG_VALUE keyword var)
 IF(NOT ${var})
   FILE (STRINGS ${CMAKE_SOURCE_DIR}/VERSION str REGEX "^[ ]*${keyword}=")
   IF(str)
     STRING(REPLACE "${keyword}=" "" str ${str})
     STRING(REGEX REPLACE  "[ ].*" ""  str "${str}")
     SET(${var} ${str})
   ENDIF()
 ENDIF()
ENDMACRO()


# Read mysql version for configure script

MACRO(GET_MYSQL_VERSION)
  MYSQL_GET_CONFIG_VALUE("MYSQL_VERSION_MAJOR" MAJOR_VERSION)
  MYSQL_GET_CONFIG_VALUE("MYSQL_VERSION_MINOR" MINOR_VERSION)
  MYSQL_GET_CONFIG_VALUE("MYSQL_VERSION_PATCH" PATCH_VERSION)
  MYSQL_GET_CONFIG_VALUE("MYSQL_VERSION_EXTRA" EXTRA_VERSION)

  IF(NOT MAJOR_VERSION OR NOT MINOR_VERSION OR NOT PATCH_VERSION)
    MESSAGE(FATAL_ERROR "VERSION file cannot be parsed.")
  ENDIF()

  SET(VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}${EXTRA_VERSION}")
  MESSAGE("-- MariaDB ${VERSION}")
  SET(MYSQL_BASE_VERSION "${MAJOR_VERSION}.${MINOR_VERSION}" CACHE INTERNAL "MySQL Base version")
  SET(MYSQL_NO_DASH_VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}")
  # Use NDBVERSION irregardless of whether this is Cluster or not, if not
  # then the regex will be ignored anyway.
  STRING(REGEX REPLACE "^.*-ndb-" "" NDBVERSION "${VERSION}")
  STRING(REPLACE "-" "_" MYSQL_RPM_VERSION "${NDBVERSION}")
  MATH(EXPR MYSQL_VERSION_ID "10000*${MAJOR_VERSION} + 100*${MINOR_VERSION} + ${PATCH_VERSION}")
  MARK_AS_ADVANCED(VERSION MYSQL_VERSION_ID MYSQL_BASE_VERSION)
  SET(CPACK_PACKAGE_VERSION_MAJOR ${MAJOR_VERSION})
  SET(CPACK_PACKAGE_VERSION_MINOR ${MINOR_VERSION})
  SET(CPACK_PACKAGE_VERSION_PATCH ${PATCH_VERSION}${EXTRA_VERSION})
ENDMACRO()

# Get mysql version and other interesting variables
GET_MYSQL_VERSION()

SET(MYSQL_TCP_PORT_DEFAULT "3306")

IF(NOT MYSQL_TCP_PORT)
  SET(MYSQL_TCP_PORT ${MYSQL_TCP_PORT_DEFAULT})
  SET(MYSQL_TCP_PORT_DEFAULT "0")
ELSEIF(MYSQL_TCP_PORT EQUAL MYSQL_TCP_PORT_DEFAULT)
  SET(MYSQL_TCP_PORT_DEFAULT "0")
ENDIF()

IF(NOT COMPILATION_COMMENT)
  SET(COMPILATION_COMMENT "Source distribution")
ENDIF()

INCLUDE(package_name)
IF(NOT CPACK_PACKAGE_FILE_NAME)
  GET_PACKAGE_FILE_NAME(CPACK_PACKAGE_FILE_NAME)
ENDIF()

IF(NOT CPACK_SOURCE_PACKAGE_FILE_NAME)
  SET(CPACK_SOURCE_PACKAGE_FILE_NAME "mariadb-${VERSION}")
  IF("${VERSION}" MATCHES "-ndb-")
    STRING(REGEX REPLACE "^.*-ndb-" "" NDBVERSION "${VERSION}")
    SET(CPACK_SOURCE_PACKAGE_FILE_NAME "mysql-cluster-gpl-${NDBVERSION}")
  ENDIF()
ENDIF()
SET(CPACK_PACKAGE_CONTACT "MariaDB team <info@montyprogram.com>")
SET(CPACK_PACKAGE_VENDOR "Monty Program AB")
SET(CPACK_SOURCE_GENERATOR "TGZ")

# Defintions for windows version resources
SET(PRODUCTNAME "MariaDB Server")
SET(COMPANYNAME ${CPACK_PACKAGE_VENDOR})

# Windows 'date' command has unpredictable output, so cannot rely on it to
# set MYSQL_COPYRIGHT_YEAR - if someone finds a portable way to do so then
# it might be useful
#IF (WIN32)
#  EXECUTE_PROCESS(COMMAND "date" "/T" OUTPUT_VARIABLE TMP_DATE)
#  STRING(REGEX REPLACE "(..)/(..)/..(..).*" "\\3\\2\\1" MYSQL_COPYRIGHT_YEAR ${TMP_DATE})
IF(UNIX)
  EXECUTE_PROCESS(COMMAND "date" "+%Y" OUTPUT_VARIABLE MYSQL_COPYRIGHT_YEAR OUTPUT_STRIP_TRAILING_WHITESPACE)
ENDIF()

# Add version information to the exe and dll files
# Refer to http://msdn.microsoft.com/en-us/library/aa381058(VS.85).aspx
# for more info.
IF(MSVC)
    # Tiny version is used to identify the build, it can be set with cmake -DTINY_VERSION=<number>
    # to bzr revno for example (in the CI builds)
    IF(NOT TINY_VERSION)
      SET(TINY_VERSION "0")
    ENDIF()
  
    GET_FILENAME_COMPONENT(MYSQL_CMAKE_SCRIPT_DIR ${CMAKE_CURRENT_LIST_FILE} PATH)

    SET(FILETYPE VFT_APP)
    CONFIGURE_FILE(${MYSQL_CMAKE_SCRIPT_DIR}/versioninfo.rc.in 
    ${CMAKE_BINARY_DIR}/versioninfo_exe.rc)

    SET(FILETYPE VFT_DLL)
    CONFIGURE_FILE(${MYSQL_CMAKE_SCRIPT_DIR}/versioninfo.rc.in  
      ${CMAKE_BINARY_DIR}/versioninfo_dll.rc)

  FUNCTION(ADD_VERSION_INFO target target_type sources_var)
    IF("${target_type}" MATCHES "SHARED" OR "${target_type}" MATCHES "MODULE")
      SET(rcfile ${CMAKE_BINARY_DIR}/versioninfo_dll.rc)
    ELSEIF("${target_type}" MATCHES "EXE")
      SET(rcfile ${CMAKE_BINARY_DIR}/versioninfo_exe.rc)
    ENDIF()
    SET(${sources_var} ${${sources_var}} ${rcfile} PARENT_SCOPE)
  ENDFUNCTION()
ELSE()
  FUNCTION(ADD_VERSION_INFO)
  ENDFUNCTION()
ENDIF()
