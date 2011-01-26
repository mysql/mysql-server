# Copyright (C) 2007 MySQL AB
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


INCLUDE(${CMAKE_SOURCE_DIR}/storage/ndb/config/type_ndbapitools.cmake)
INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbapi)

MYSQL_ADD_EXECUTABLE(ndb_waiter
  waiter.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_waiter ndbportlib ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_drop_table
  drop_tab.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_drop_table ndbNDBT ndbgeneral)

MYSQL_ADD_EXECUTABLE(ndb_delete_all
  delete_all.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_delete_all ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_desc
  desc.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_desc ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_drop_index
  drop_index.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_drop_index ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_show_tables
  listTables.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_show_tables ndbclient ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_select_all
  select_all.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_select_all ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_select_count
  select_count.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_select_count ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_restore
  restore/restore_main.cpp
  restore/consumer.cpp
  restore/consumer_restore.cpp
  restore/consumer_printer.cpp
  restore/Restore.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_restore ndbNDBT ndbgeneral)

MYSQL_ADD_EXECUTABLE(ndb_config
  ndb_config.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_config ndbmgmclient ndbconf)

SET(options "-I${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmapi")
SET(options "${options} -I${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmsrv")
SET(options "${options} -I${CMAKE_SOURCE_DIR}/storage/ndb/include/mgmcommon")
SET_TARGET_PROPERTIES(ndb_config PROPERTIES
                      COMPILE_FLAGS "${options}")

# Build ndbinfo_sql and run it to create ndbinfo.sql
ADD_EXECUTABLE(ndbinfo_sql ndbinfo_sql.cpp)
TARGET_LINK_LIBRARIES(ndbinfo_sql ndbclient)
GET_TARGET_PROPERTY(NDBINFO_SQL_EXE ndbinfo_sql LOCATION)
ADD_CUSTOM_COMMAND(OUTPUT ${PROJECT_SOURCE_DIR}/storage/ndb/tools/ndbinfo.sql
                   COMMAND ${NDBINFO_SQL_EXE} ARGS > ndbinfo.sql
                   DEPENDS ndbinfo_sql)
ADD_CUSTOM_TARGET(ndbinfo_sql_run ALL
				  DEPENDS ${PROJECT_SOURCE_DIR}/storage/ndb/tools/ndbinfo.sql)


IF (MYSQL_VERSION_ID LESS "50501")
  # Don't build or install this program anymore in 5.5+
  ADD_EXECUTABLE(ndb_test_platform ndb_test_platform.cpp)
ENDIF()

