
#
# This test must examine integrity of system database "mysql"
#

-- disable_query_log
use mysql;
-- enable_query_log
-- source include/system_db_struct.inc
-- disable_query_log
use test;
-- enable_query_log
# keep results same with system_mysql_db_fix
show tables;
SET sql_mode = default;
# End of 4.1 tests
