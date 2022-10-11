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
STOP REPLICA;
eval $test_table_slave;

connection master;
eval $test_table_master;

--let $rpl_only_running_threads= 1
--source include/rpl_reset.inc

eval $test_insert;

connection slave;
--let $slave_sql_errno= convert_error(ER_SERVER_REPLICA_CONVERSION_FAILED) # 13146
--source include/wait_for_slave_sql_error.inc

# The following should be 0
SELECT COUNT(*) FROM t1;
STOP REPLICA;
--source include/rpl_reset.inc
--let $rpl_only_running_threads= 0
