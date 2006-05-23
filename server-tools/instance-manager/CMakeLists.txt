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
