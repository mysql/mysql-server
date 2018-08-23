# Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

CMAKE_MINIMUM_REQUIRED(VERSION 2.8.12)

IF(SIZEOF_VOIDP EQUAL 8)
  SET(ARCH_64BIT 1)
ELSE()
  SET(ARCH_64BIT 0)
ENDIF()

INCLUDE(cmake/version.cmake)
SET(CMAKE_MODULE_PATH
  ${CMAKE_MODULE_PATH}
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

IF(${CMAKE_VERSION} VERSION_GREATER "3.0")
  # enable MACOSX_RPATH
  CMAKE_POLICY(SET CMP0042 NEW)
  # warn about non-existing dependencies in add_dependencies()
  CMAKE_POLICY(SET CMP0046 NEW)
  # set the VERSION as documented in PROJECT() command
  CMAKE_POLICY(SET CMP0048 NEW)
  IF(${CMAKE_VERSION} VERSION_GREATER "3.1")
    # only interpret if() arguments as variables or keywords when unquoted
    CMAKE_POLICY(SET CMP0054 NEW)
  ENDIF()
  PROJECT("MySQLRouter" VERSION ${PROJECT_VERSION_TEXT} LANGUAGES C CXX)
ELSE()
  PROJECT("MySQLRouter")
ENDIF()

IF(SOLARIS)
  # disable rapidjson optimisation on Solaris as it breaks
  # shared objects that build with -fPIC
  ADD_DEFINITIONS(-DRAPIDJSON_48BITPOINTER_OPTIMIZATION=0)
ENDIF()

INCLUDE(${CMAKE_SOURCE_DIR}/cmake/rapidjson.cmake)

SET(ROUTER_LICENSE_FILE "${CMAKE_SOURCE_DIR}/router/LICENSE.router")
SET(ROUTER_README_FILE "${CMAKE_SOURCE_DIR}/router/README.router")

SET(DOC_DESTINATION ".")
IF(NOT INSTALL_LAYOUT MATCHES "RPM")
  INSTALL(FILES
      ${ROUTER_LICENSE_FILE}
      ${ROUTER_README_FILE}
      DESTINATION ${DOC_DESTINATION} COMPONENT Router OPTIONAL)
ENDIF()

INCLUDE(cmake/settings.cmake)
INCLUDE(cmake/set_rpath.cmake)

# SSL
MESSAGE(STATUS "MySQL Router SSL_LIBRARIES:    ${SSL_LIBRARIES}")
MESSAGE(STATUS "MySQL Router SSL_DEFINES:      ${SSL_DEFINES}")
MESSAGE(STATUS "MySQL Router SSL_INCLUDE_DIRS: ${SSL_INCLUDE_DIRS}")

# Required tools, libraries, etc..
INCLUDE(cmake/testing.cmake)  # does not enable testing
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/install_layout.cmake)
INCLUDE(cmake/configure.cmake)
INCLUDE(cmake/packaging.cmake)
INCLUDE(cmake/Plugin.cmake)

# Set the proper paths for the mysqlclient library
SET(MySQL_CLIENT_LIB mysqlclient)
SET(MySQL_VERSION_HEADER ${CMAKE_BINARY_DIR}/include/mysql_version.h)
SET(MySQL_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/include)
LIST(APPEND MySQL_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/libbinlogevents/export)
LIST(APPEND MySQL_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/include)

INCLUDE_DIRECTORIES(${PROJECT_BINARY_DIR}/include)

# a meta-target to allow build everything router related, but nothing that
# it doesn't depend on
#
# each target needs to add itself via
#
#   ADD_DEPENDENCIES(${MYSQL_ROUTER_BUILD_ALL_TARGET} ...)
#
SET(MYSQL_ROUTER_BUILD_ALL_TARGET mysqlrouter_all)
ADD_CUSTOM_TARGET(${MYSQL_ROUTER_BUILD_ALL_TARGET})

# Load all modules, including plugins
ADD_SUBDIRECTORY(src)

# Enable testing


IF(WITH_UNIT_TESTS AND GMOCK_FOUND)
  ADD_SUBDIRECTORY(tests)
ENDIF()
