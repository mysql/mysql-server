find_library(FUZZINGENGINE_LIB FuzzingEngine)

if(NOT FUZZINGENGINE_LIB)
  MYSQL_ADD_EXECUTABLE(fuzz_real_query fuzz_real_query.cc onefile.cc)
  TARGET_LINK_LIBRARIES(fuzz_real_query mysqlclient)

  MYSQL_ADD_EXECUTABLE(fuzz_stmt_fetch fuzz_stmt_fetch.cc onefile.cc)
  TARGET_LINK_LIBRARIES(fuzz_stmt_fetch mysqlclient)

  MYSQL_ADD_EXECUTABLE(fuzz_stmt_execute fuzz_stmt_execute.cc onefile.cc)
  TARGET_LINK_LIBRARIES(fuzz_stmt_execute mysqlclient)
else()
  MYSQL_ADD_EXECUTABLE(fuzz_real_query fuzz_real_query.cc)
  TARGET_LINK_LIBRARIES(fuzz_real_query mysqlclient)
  TARGET_LINK_LIBRARIES(fuzz_real_query FuzzingEngine)

  MYSQL_ADD_EXECUTABLE(fuzz_stmt_fetch fuzz_stmt_fetch.cc)
  TARGET_LINK_LIBRARIES(fuzz_stmt_fetch mysqlclient)
  TARGET_LINK_LIBRARIES(fuzz_stmt_fetch FuzzingEngine)

  MYSQL_ADD_EXECUTABLE(fuzz_stmt_execute fuzz_stmt_execute.cc)
  TARGET_LINK_LIBRARIES(fuzz_stmt_execute mysqlclient)
  TARGET_LINK_LIBRARIES(fuzz_stmt_execute FuzzingEngine)
endif()
