# Copyright (c) 2008, 2022, Oracle and/or its affiliates.
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

FOREACH(warning
    "unused-but-set-variable"
    "unused-parameter"
    )
  MY_CHECK_CXX_COMPILER_WARNING("${warning}" HAS_WARN_FLAG)
  IF(HAS_WARN_FLAG)
    STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
  ENDIF()
ENDFOREACH()

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmapi
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbapi
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmclient
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/common/mgmcommon
  )

IF(WITHOUT_SERVER)
  RETURN()
ENDIF()

# Define MYSQLCLUSTERDIR, the default location
# of ndb_mgmd config files
IF(NOT DEFINED DEFAULT_MYSQL_HOME)
  # MySQL does not define DEFAULT_MYSQL_HOME in pre 5.5 version, fall
  # back to our old defaults and be backwards compatible
  IF(WIN32)
    SET(DEFAULT_MYSQL_HOME "C:/mysql" )
  ELSE()
    SET(DEFAULT_MYSQL_HOME ${CMAKE_INSTALL_PREFIX})
  ENDIF()
ENDIF()
SET(clusterdir ${DEFAULT_MYSQL_HOME}/mysql-cluster)
ADD_DEFINITIONS(-DMYSQLCLUSTERDIR=\"${clusterdir}\")
MESSAGE(STATUS "Using MYSQLCLUSTERDIR: '${clusterdir}'")


IF(WIN32)
  # Add the rseource files for logging to event log
  SET(NDB_MGMD_EXTRA_SRC
    ${CMAKE_SOURCE_DIR}/storage/ndb/src/common/logger/message.rc
    ${CMAKE_SOURCE_DIR}/storage/ndb/src/common/logger/MSG00001.bin)
ENDIF()

MYSQL_ADD_EXECUTABLE(ndb_mgmd
  ${NDB_MGMD_EXTRA_SRC}
  ConfigManager.cpp
  MgmtSrvr.cpp
  Services.cpp
  main.cpp

  DESTINATION ${INSTALL_SBINDIR}
  COMPONENT ClusterManagementServer
  ENABLE_EXPORTS
  LINK_LIBRARIES
  ${EDITLINE_LIBRARY}
  ndbclient_static
  ndbmgmcommon
  ndbmgmclient
  )

NDB_ADD_TEST(MgmConfig-t testConfig.cpp
  LIBS ndbmgmapi ndbmgmcommon ndbtrace ndblogger ndbgeneral ndbportlib
  )
