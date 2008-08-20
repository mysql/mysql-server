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
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/zlib
                    ${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbapi
                    ${CMAKE_SOURCE_DIR}/storage/ndb/include/debugger
)

ADD_EXECUTABLE(ndb_test_platform ndb_test_platform.cpp)
ADD_EXECUTABLE(ndb_waiter waiter.cpp)
TARGET_LINK_LIBRARIES(ndb_waiter ndbportlib ndbNDBT)
ADD_EXECUTABLE(ndb_drop_table drop_tab.cpp)
TARGET_LINK_LIBRARIES(ndb_drop_table ndbNDBT ndbgeneral)
ADD_EXECUTABLE(ndb_delete_all delete_all.cpp)
TARGET_LINK_LIBRARIES(ndb_delete_all ndbNDBT)
ADD_EXECUTABLE(ndb_desc desc.cpp)
TARGET_LINK_LIBRARIES(ndb_desc ndbNDBT)
ADD_EXECUTABLE(ndb_drop_index drop_index.cpp)
TARGET_LINK_LIBRARIES(ndb_drop_index ndbNDBT)
ADD_EXECUTABLE(ndb_show_tables listTables.cpp)
TARGET_LINK_LIBRARIES(ndb_show_tables ndbclient ndbNDBT)
ADD_EXECUTABLE(ndb_select_all select_all.cpp)
TARGET_LINK_LIBRARIES(ndb_select_all ndbNDBT)
ADD_EXECUTABLE(ndb_select_count select_count.cpp)
TARGET_LINK_LIBRARIES(ndb_select_count ndbNDBT)
ADD_EXECUTABLE(ndb_restore
               restore/restore_main.cpp
               restore/consumer.cpp
               restore/consumer_restore.cpp
               restore/consumer_printer.cpp
               restore/Restore.cpp)
TARGET_LINK_LIBRARIES(ndb_restore ndbbackup ndbNDBT)
ADD_EXECUTABLE(ndb_config ndb_config.cpp)
TARGET_LINK_LIBRARIES(ndb_config ndbmgmclient ndbconf)

SET(options "-I${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmapi")
SET(options "${options} -I${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmsrv")
SET(options "${options} -I${CMAKE_SOURCE_DIR}/storage/ndb/include/mgmcommon")
#SET(options "${options} -DMYSQLCLUSTERDIR=\"\\\"\\\"\"")
SET_TARGET_PROPERTIES(ndb_config PROPERTIES
                      COMPILE_FLAGS "${options}")
