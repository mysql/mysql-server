# Inc file to be used with binlog_reset_master_to_option.test
# This tests if a particular binary file is present or not

--let $fname_prefix= `select @@log_bin_basename`
# set the extension expected
--let $expected_filename= $fname_prefix.$binlog_number
# assert that the file exists
file_exists $expected_filename;

# assert that file names in the file system and in SHOW BINARY LOGS
# are the same
--let $show_filename= query_get_value("SHOW BINARY LOGS", Log_name, 1)
--let $show_filename= `select concat("$MYSQLD_DATADIR","$show_filename")`

--let $assert_text= Assert that filename is same in both file system and show binary logs
--let $assert_cond= STRCMP("$expected_filename", "$show_filename") = 0
--source include/assert.inc

--let $assert_text= Assert that file is created and opened
--let $assert_cond= `SELECT OPEN_COUNT FROM performance_schema.file_instances WHERE FILE_NAME LIKE '%$binlog_number%' = 1`
--source include/assert.inc

if (`SELECT COUNT(*) = 1 FROM performance_schema.global_variables WHERE VARIABLE_NAME = 'GTID_MODE' AND VARIABLE_VALUE = 'ON'`)
{
# Check that GTID_EXECUTED and GTID_PURGED is reset to empty.
--let $wait_condition = SELECT VARIABLE_VALUE = "" FROM performance_schema.global_variables WHERE VARIABLE_NAME = 'gtid_purged'
--source include/wait_condition.inc
--let $wait_condition = SELECT VARIABLE_VALUE = "" FROM performance_schema.global_variables WHERE VARIABLE_NAME = 'gtid_executed'
--source include/wait_condition.inc
}
