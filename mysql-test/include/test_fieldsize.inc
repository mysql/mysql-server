#
# include/test_fieldsize.inc 
# 
# This include file is designed to create a table with one column
# whose size on the master is greater than that on the slave. The
# test should fail with an error on the slave.
#

connection master;
DROP TABLE IF EXISTS t1;

sync_slave_with_master;
STOP SLAVE;
RESET SLAVE;
eval $test_table_slave;

connection master;
eval $test_table_master;
RESET MASTER;

eval $test_insert;

connection slave;
START SLAVE;
--let $slave_sql_errno= 1535
--let $show_slave_sql_error= 1
--source include/wait_for_slave_sql_error.inc

# The following should be 0
SELECT COUNT(*) FROM t1;
STOP SLAVE;
RESET SLAVE;

connection master;
RESET MASTER;

connection slave;
START SLAVE;

