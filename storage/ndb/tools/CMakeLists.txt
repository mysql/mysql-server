# Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.
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
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbapi
  ${CMAKE_SOURCE_DIR}/storage/ndb/include/kernel/signaldata)

MYSQL_ADD_EXECUTABLE(ndb_waiter
  waiter.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_waiter ndbclient_static ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_drop_table
  drop_tab.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_drop_table ndbclient_static ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_delete_all
  delete_all.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_delete_all ndbclient_static ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_desc
  desc.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_desc ndbclient_static ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_drop_index
  drop_index.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_drop_index ndbclient_static ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_show_tables
  listTables.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_show_tables ndbclient_static ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_select_all
  select_all.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_select_all ndbclient_static ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_select_count
  select_count.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_select_count ndbclient_static ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_restore
  restore/restore_main.cpp
  restore/consumer.cpp
  restore/consumer_restore.cpp
  restore/consumer_printer.cpp
  restore/Restore.cpp
  ndb_lib_move_data.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_restore ndbclient_static ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_config
  ndb_config.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_config ndbclient_static)

# Build ndbinfo_sql and run it to create ndbinfo.sql
ADD_EXECUTABLE(ndbinfo_sql
  ndbinfo_sql.cpp
)
TARGET_LINK_LIBRARIES(ndbinfo_sql ndbclient_static)

ADD_CUSTOM_COMMAND(TARGET ndbinfo_sql POST_BUILD
  COMMAND $<TARGET_FILE:ndbinfo_sql>
  > ${PROJECT_BINARY_DIR}/storage/ndb/tools/ndbinfo.sql
  )

MYSQL_ADD_EXECUTABLE(ndb_index_stat
  ndb_index_stat.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_index_stat ndbclient_static ndbNDBT)

ADD_EXECUTABLE(ndb_dump_frm_data
  ndb_dump_frm_data.cpp)
TARGET_LINK_LIBRARIES(ndb_dump_frm_data ndbclient_static ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndbinfo_select_all
  ndbinfo_select_all.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndbinfo_select_all ndbclient_static ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_blob_tool
  ndb_blob_tool.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_blob_tool ndbclient_static ndbNDBT)

MYSQL_ADD_EXECUTABLE(ndb_move_data
  ndb_move_data.cpp
  ndb_lib_move_data.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_move_data ndbclient_static ndbNDBT)

IF (MYSQL_VERSION_ID LESS "50501")
  # Don't build or install this program anymore in 5.5+
  ADD_EXECUTABLE(ndb_test_platform ndb_test_platform.cpp)
ENDIF()

# Install the ndb_dist_priv.sql script
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/ndb_dist_priv.sql
        DESTINATION ${INSTALL_MYSQLSHAREDIR})

if(NOT WIN32)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/ndb_error_reporter.pl" "${CMAKE_CURRENT_BINARY_DIR}/ndb_error_reporter" COPYONLY)
  EXECUTE_PROCESS(COMMAND chmod +x ${CMAKE_CURRENT_BINARY_DIR}/ndb_error_reporter)
  INSTALL_SCRIPT(${CMAKE_CURRENT_BINARY_DIR}/ndb_error_reporter
         DESTINATION ${INSTALL_BINDIR}
         COMPONENT ClusterTools)
ENDIF()

INSTALL(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/ndb_size.pl
        DESTINATION ${INSTALL_BINDIR})
