# Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.
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

INCLUDE_DIRECTORIES(
  ${NDB_SOURCE_DIR}/src/mgmapi
  ${NDB_SOURCE_DIR}/src/ndbapi
  ${NDB_SOURCE_DIR}/src/mgmclient)

ADD_CONVENIENCE_LIBRARY(ndbconf
               Config.cpp
               ConfigInfo.cpp
               InitConfigFileParser.cpp
               ${NDB_SOURCE_DIR}/src/kernel/vm/mt_thr_config.cpp
)
TARGET_LINK_LIBRARIES(ndbconf ndbmgmapi)

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
    ${CMAKE_SOURCE_DIR}/storage/ndb/src/common/logger/msg00001.bin)
ENDIF()

MYSQL_ADD_EXECUTABLE(ndb_mgmd
  MgmtSrvr.cpp main.cpp Services.cpp ConfigManager.cpp
  ${NDB_MGMD_EXTRA_SRC}
  DESTINATION ${INSTALL_SBINDIR}
  COMPONENT ClusterManagementServer)
TARGET_LINK_LIBRARIES(ndb_mgmd ndbconf ndbclient_static ndbmgmclient)
IF(UNIX)
  TARGET_LINK_LIBRARIES(ndb_mgmd ${READLINE_LIBRARY})
ENDIF(UNIX)

ADD_EXECUTABLE(MgmConfig-t
               testConfig.cpp)
TARGET_LINK_LIBRARIES(MgmConfig-t
                      ndbmgmapi
                      ndbtrace
                      ndbconf
                      ndblogger
                      ndbgeneral
                      ndbportlib)

