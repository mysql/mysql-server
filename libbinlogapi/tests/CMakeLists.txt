# Right now, the tests are built using the static version of the
# replication listener, but we need to decide which one to use.

set(MySQL_SERVER_TESTS test-basic)
set(MySQL_SIMPLE_TESTS test-transport)
#TODO: Add support for tests replaybinlog and replay_sys_vars

foreach(test ${MySQL_BINLOG_TESTS} ${MySQL_SERVER_TESTS} ${MySQL_SIMPLE_TESTS})
  message("Adding test ${test}")
  add_executable(${test} ${test}.cpp)
  target_link_libraries(${test} replication_static gtest mysqlclient)
endforeach()

if(WITH_SERVER_TESTS)
  add_test(ServerTests ${MySQL_SERVER_TESTS})
endif(WITH_SERVER_TESTS)
add_test(BasicTests ${MySQL_SIMPLE_TESTS})

