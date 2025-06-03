IF(LIBFUZZER_COMPILE_FLAGS)
  MYSQL_ADD_EXECUTABLE(fuzz_real_query
    fuzz_real_query.cc
    LINK_LIBRARIES mysqlclient
    SKIP_INSTALL
    )
  LIBFUZZER_ADD_TEST(fuzz_real_query)
ENDIF()
