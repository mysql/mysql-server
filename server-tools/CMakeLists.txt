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

SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")

ADD_DEFINITIONS(-DMYSQL_SERVER -DMYSQL_INSTANCE_MANAGER)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/sql
                    ${PROJECT_SOURCE_DIR}/extra/yassl/include)
 
ADD_EXECUTABLE(mysqlmanager buffer.cc command.cc commands.cc guardian.cc instance.cc instance_map.cc
                            instance_options.cc listener.cc log.cc manager.cc messages.cc mysql_connection.cc
                            mysqlmanager.cc options.cc parse.cc parse_output.cc priv.cc protocol.cc
                            thread_registry.cc user_map.cc imservice.cpp windowsservice.cpp
                            user_management_commands.cc
                            ../../sql/net_serv.cc ../../sql-common/pack.c ../../sql/password.c
                            ../../sql/sql_state.c ../../sql-common/client.c ../../libmysql/get_password.c
                            ../../libmysql/errmsg.c)

ADD_DEPENDENCIES(mysqlmanager GenError)
TARGET_LINK_LIBRARIES(mysqlmanager dbug mysys strings taocrypt vio yassl zlib wsock32)
