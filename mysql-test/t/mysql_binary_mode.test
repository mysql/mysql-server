
source include/rpl/force_binlog_format_statement.inc;
RESET BINARY LOGS AND GTIDS;

--echo # Bug#33048 Not able to recover binary/blob data correctly using mysqlbinlog
--echo # --------------------------------------------------------------------------
--echo # The test verify that 0x00 and 0x0D0A sequence can be handled correctly by
--echo # mysql
--echo

# zero => 0x00, newline => 0x0D0A, A => 0x41, B => 0x42

# 0x410D0A42 => 'A\r\nB'
let $table_name_right= `SELECT 0x410D0A42`;

# 0x410A42 => 'A\nB'
let $table_name_wrong= `SELECT 0x410A42`;

# 0x410042 => 'A\0B'
let $char= `SELECT 0x410042`;

eval CREATE TABLE `$table_name_right` (c1 CHAR(100));

--echo # It is a faked statement. ASCII 0 is in the original statement, it would
--echo # make the test result to become a binary file which was difficult to get
--echo # the diff result if the original query was logged in the result.
--echo INSERT INTO `A\r\nB` VALUES("A\0B");
--echo
--disable_query_log
eval INSERT INTO `$table_name_right` VALUES("$char");
--enable_query_log

let $char= $table_name_right;
eval INSERT INTO `$table_name_right` VALUES("$char");

eval SELECT HEX(c1) FROM `$table_name_right`;

--echo
let $binlog_file= query_get_value(SHOW BINARY LOG STATUS, File, 1);
FLUSH LOGS;
eval DROP TABLE `$table_name_right`;

--echo
let $MYSQLD_DATADIR= `SELECT @@datadir`;
--exec $MYSQL_BINLOG $MYSQLD_DATADIR/$binlog_file > $MYSQLTEST_VARDIR/tmp/my.sql
RESET BINARY LOGS AND GTIDS;

--echo # '--exec mysql ...' without --binary-mode option
--echo # It creates the table with a wrong table name and generates an error.
--echo # (error output was suppressed to make the test case platform agnostic)

## disabling result log because the error message has the 
## table name in the output which is one byte different ('\r') 
## on unixes and windows.
--disable_result_log
--error 1
--exec $MYSQL test < $MYSQLTEST_VARDIR/tmp/my.sql 2>&1
--enable_result_log

--echo
--echo # It is not in binary_mode, so table name '0x410D0A42' can be translated to
--echo # '0x410A42' by mysql depending on the OS - Windows or Unix-like.
--replace_result $table_name_wrong TABLE_NAME_MASKED $table_name_right TABLE_NAME_MASKED
if (`SELECT CONVERT(@@VERSION_COMPILE_OS USING latin1) IN ('Win32', 'Win64', 'Windows')`)
{
  eval DROP TABLE `$table_name_right`;
}

if (`SELECT CONVERT(@@VERSION_COMPILE_OS USING latin1) NOT IN ('Win32', 'Win64', 'Windows')`)
{
  eval DROP TABLE `$table_name_wrong`;
}

--echo
--echo # In binary_mode, table name '0x410D0A42' and string '0x410042' can be
--echo # handled correctly.
RESET BINARY LOGS AND GTIDS;
--exec $MYSQL --binary-mode test < $MYSQLTEST_VARDIR/tmp/my.sql
eval SELECT HEX(c1) FROM `$table_name_right`;

--echo
eval DROP TABLE `$table_name_right`;

#
#  BUG#12794048 - MAIN.MYSQL_BINARY_MODE FAILS ON WINDOWS RELEASE BUILD 
#
RESET BINARY LOGS AND GTIDS;

#
# This test case tests if the table names and their values
# are handled properly. For that we check 
#

# 0x610D0A62 => 'a\r\nb'
let $tbl= `SELECT 0x610D0A62`;

--disable_result_log
--disable_query_log

--let $binlog_file= query_get_value(SHOW BINARY LOG STATUS, File, 1)

#### case #1: mysqltest 
#### CREATE table and insert value through regular mysqltest session 

--eval CREATE TABLE `$tbl` (c1 CHAR(100))
--eval INSERT INTO `$tbl` VALUES ("$tbl")

--let $table_name=`SELECT table_name FROM information_schema.tables WHERE table_schema='test'`
--let $tbl0= `SELECT HEX(table_name) FROM information_schema.tables WHERE table_schema='test'`
--let $val0= `SELECT HEX(c1) FROM `$table_name` LIMIT 1`

FLUSH LOGS;

--eval DROP TABLE `$table_name`;

#### case #2: mysql --binlog-mode=0 
#### Replay through regular mysql client non-interactive mode

--let $MYSQLD_DATADIR= `SELECT @@datadir`
--let $prefix=`SELECT UUID()`
--let $binlog_uuid_filename= $MYSQLTEST_VARDIR/tmp/$prefix-bin.log
--copy_file $MYSQLD_DATADIR/$binlog_file $binlog_uuid_filename
RESET BINARY LOGS AND GTIDS;

--exec $MYSQL_BINLOG $binlog_uuid_filename  | $MYSQL

--let $table_name=`SELECT table_name FROM information_schema.tables WHERE table_schema='test'`
--let $tbl1= `SELECT hex(table_name) FROM information_schema.tables WHERE table_schema='test'`
--let $val1= `SELECT HEX(c1) FROM `$table_name` LIMIT 1`

--eval DROP TABLE `$table_name`;

#### case #3: mysql --binlog-mode=1
#### Replay through regular mysql client non-interactive mode and with binary mode set

RESET BINARY LOGS AND GTIDS;
--exec $MYSQL_BINLOG $binlog_uuid_filename  | $MYSQL --binary-mode

--let $table_name=`SELECT table_name FROM information_schema.tables WHERE table_schema='test'`
--let $tbl2= `SELECT hex(table_name) FROM information_schema.tables WHERE table_schema='test'`
--let $val2= `SELECT HEX(c1) FROM `$table_name` LIMIT 1`

--eval DROP TABLE `$table_name`;

--enable_result_log
--disable_query_log

##### OUTCOME

--let $assert_text= Table and contents created through mysqltest match 0x610D0A62.
--let $assert_cond=  "$tbl0" = "610D0A62" AND "$val0" = "610D0A62"
--source include/assert.inc

--let $assert_text= Table and contents created while replaying binary log without --binary-mode set match 0x61(0D)0A62.
if (`SELECT CONVERT(@@VERSION_COMPILE_OS USING latin1) IN ('Win32', 'Win64', 'Windows')`)
{
  --let $assert_cond=  "$tbl1" = "610D0A62" AND "$val1" = "610D0A62"
}
if (`SELECT CONVERT(@@VERSION_COMPILE_OS USING latin1) NOT IN ('Win32', 'Win64', 'Windows')`)
{
  --let $assert_cond=  "$tbl1" = "610A62" AND "$val1" = "610A62"
}
--source include/assert.inc

--let $assert_text= Table and contents created while replaying binary log with --binary-mode set match 0x610D0A62.
--let $assert_cond=  "$tbl2" = "610D0A62" AND "$val2" = "610D0A62"
--source include/assert.inc

RESET BINARY LOGS AND GTIDS;
--remove_file $binlog_uuid_filename
--remove_file $MYSQLTEST_VARDIR/tmp/my.sql

--source include/rpl/restore_default_binlog_format.inc
