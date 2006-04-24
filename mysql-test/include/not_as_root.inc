-- require r/not_as_root.require
disable_query_log;
select $MYSQL_TEST_ROOT as running_as_root;
enable_query_log;
