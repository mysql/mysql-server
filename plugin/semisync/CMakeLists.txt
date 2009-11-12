# Copyright (C) 2006 MySQL AB
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

# This is CMakeLists.txt for semi-sync replication plugins

SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")

# Add common include directories
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/zlib
                    ${CMAKE_SOURCE_DIR}/sql
                    ${CMAKE_SOURCE_DIR}/regex
                    ${CMAKE_SOURCE_DIR}/extra/yassl/include)

SET(SEMISYNC_MASTER_SOURCES semisync.cc semisync_master.cc semisync_master_plugin.cc)
SET(SEMISYNC_SLAVE_SOURCES semisync.cc semisync_slave.cc semisync_slave_plugin.cc)

ADD_DEFINITIONS(-DMYSQL_DYNAMIC_PLUGIN)
ADD_LIBRARY(semisync_master SHARED ${SEMISYNC_MASTER_SOURCES})
TARGET_LINK_LIBRARIES(semisync_master  mysqld)
ADD_LIBRARY(semisync_slave SHARED ${SEMISYNC_SLAVE_SOURCES})
TARGET_LINK_LIBRARIES(semisync_slave  mysqld)
MESSAGE("build SEMISYNC as DLL")
