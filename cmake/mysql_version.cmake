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

# Read value for a variable from configure.in

MACRO(MYSQL_GET_CONFIG_VALUE keyword var)
 IF(NOT ${var})
   IF (EXISTS ${CMAKE_SOURCE_DIR}/configure.in)
     FILE (STRINGS  ${CMAKE_SOURCE_DIR}/configure.in str  REGEX  "^[ ]*${keyword}=")
	 IF(str)
	   STRING(REPLACE "${keyword}=" "" str ${str})
	   STRING(REGEX REPLACE  "[ ].*" ""  str ${str})
	   SET(${var} ${str} CACHE INTERNAL "Config variable")
	 ENDIF()
   ENDIF()
 ENDIF()
ENDMACRO()


# Read mysql version for configure script

MACRO(GET_MYSQL_VERSION)

  IF(NOT VERSION_STRING)
    IF(EXISTS ${CMAKE_SOURCE_DIR}/configure.in)
    FILE(STRINGS  ${CMAKE_SOURCE_DIR}/configure.in  str REGEX "AM_INIT_AUTOMAKE")
    STRING(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+[-][^ \\)]+" VERSION_STRING "${str}")
    IF(NOT VERSION_STRING)
      FILE(STRINGS  configure.in  str REGEX "AC_INIT\\(")
      STRING(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+[-][^ \\]]+" VERSION_STRING "${str}")
    ENDIF()
	ENDIF()
  ENDIF()

  IF(NOT VERSION_STRING)
    MESSAGE(FATAL_ERROR 
	"VERSION_STRING cannot be parsed, please specify -DVERSION_STRING=major.minor.patch-extra"
	"when calling cmake")
  ENDIF()
  
  SET(VERSION ${VERSION_STRING})
  
  STRING(REGEX REPLACE "([0-9]+)\\.[0-9]+\\.[0-9]+[^ ]+" "\\1" MAJOR_VERSION "${VERSION_STRING}")
  STRING(REGEX REPLACE "[0-9]+\\.([0-9]+)\\.[0-9]+[^ ]+" "\\1" MINOR_VERSION "${VERSION_STRING}")
  STRING(REGEX REPLACE "[0-9]+\\.[0-9]+\\.([0-9]+)[^ ]+" "\\1" PATCH "${VERSION_STRING}")
  SET(MYSQL_BASE_VERSION "${MAJOR_VERSION}.${MINOR_VERSION}" CACHE INTERNAL "MySQL Base version")
  SET(MYSQL_NO_DASH_VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH}")
  MATH(EXPR MYSQL_VERSION_ID "10000*${MAJOR_VERSION} + 100*${MINOR_VERSION} + ${PATCH}")
  MARK_AS_ADVANCED(VERSION MYSQL_VERSION_ID MYSQL_BASE_VERSION)
  SET(CPACK_PACKAGE_VERSION_MAJOR ${MAJOR_VERSION})
  SET(CPACK_PACKAGE_VERSION_MINOR ${MINOR_VERSION})
  SET(CPACK_PACKAGE_VERSION_PATCH ${PATCH})
ENDMACRO()

# Get mysql version and other interesting variables
GET_MYSQL_VERSION()

MYSQL_GET_CONFIG_VALUE("PROTOCOL_VERSION" PROTOCOL_VERSION)
MYSQL_GET_CONFIG_VALUE("DOT_FRM_VERSION" DOT_FRM_VERSION)
MYSQL_GET_CONFIG_VALUE("MYSQL_TCP_PORT_DEFAULT" MYSQL_TCP_PORT_DEFAULT)
MYSQL_GET_CONFIG_VALUE("MYSQL_UNIX_ADDR_DEFAULT" MYSQL_UNIX_ADDR_DEFAULT)
MYSQL_GET_CONFIG_VALUE("SHARED_LIB_MAJOR_VERSION" SHARED_LIB_MAJOR_VERSION)
IF(NOT MYSQL_TCP_PORT_DEFAULT)
 SET(MYSQL_TCP_PORT_DEFAULT "3306")
ENDIF()
IF(NOT MYSQL_TCP_PORT)
  SET(MYSQL_TCP_PORT ${MYSQL_TCP_PORT_DEFAULT})
  SET(MYSQL_TCP_PORT_DEFAULT "0")
ENDIF()
IF(NOT MYSQL_UNIX_ADDR)
  SET(MYSQL_UNIX_ADDR "/tmp/mysql.sock")
ENDIF()
IF(NOT COMPILATION_COMMENT)
  SET(COMPILATION_COMMENT "Source distribution")
ENDIF()



# Use meaningful package name for the binary package
IF(NOT CPACK_PACKAGE_FILE_NAME)
  IF( NOT SYSTEM_NAME_AND_PROCESSOR)
    IF(WIN32)
     # CMake does not set CMAKE_SYSTEM_PROCESSOR correctly on Win64
     # (uses x86). Besides, we try to be compatible with existing naming
     IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
       SET(SYSTEM_NAME_AND_PROCESSOR "winx64")
     ELSE()
       SET(SYSTEM_NAME_AND_PROCESSOR "win32")
     ENDIF()
    ELSE()
	  IF(NOT PLATFORM)
	    SET(PLATFORM ${CMAKE_SYSTEM_NAME})
	  ENDIF()
	  IF(NOT MACHINE)
	    SET(MACHINE ${CMAKE_SYSTEM_PROCESSOR})
		IF(CMAKE_SIZEOF_VOID_P EQUAL 8 AND NOT ${MACHINE} MATCHES "ia64")
		  # On almost every 64 bit machine (except IA64) it is possible
		  # to build 32 bit packages. Add -64bit suffix to differentiate 
		  # between 32 and 64 bit packages.
		  SET(MACHINE ${MACHINE}-64bit)
		ENDIF()
	  ENDIF()
      SET(SYSTEM_NAME_AND_PROCESSOR "${PLATFORM}-${MACHINE}")
    ENDIF()
  ENDIF()
  
  SET(package_name "mysql-${VERSION}-${SYSTEM_NAME_AND_PROCESSOR}" )
  # Sometimes package suffix is added (something like icc-glibc23)
  IF(PACKAGE_SUFFIX)
    SET(package_name "${package_name}-${PACKAGE_SUFFIX}")
  ENDIF()
  STRING(TOLOWER ${package_name} package_name)
  SET(CPACK_PACKAGE_FILE_NAME ${package_name})
ENDIF()

IF(NOT CPACK_SOURCE_PACKAGE_FILE_NAME)
  SET(CPACK_SOURCE_PACKAGE_FILE_NAME "mysql-${VERSION}")
ENDIF()
SET(CPACK_PACKAGE_VENDOR "Sun Microsystems")
SET(CPACK_SOURCE_GENERATOR "TGZ")
SET(CPACK_SOURCE_IGNORE_FILES 
  \\\\.bzr/
  \\\\.bzr-mysql
  .bzrignore
  CMakeCache.txt
  /CMakeFiles/
  /_CPack_Packages/
  $.gz
  $.zip
)
