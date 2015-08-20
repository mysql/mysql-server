#
# WL#6501: make truncate table atomic
#

--source include/have_innodb.inc
--source include/have_debug.inc
--source include/big_test.inc

# Valgrind would complain about memory leaks when we crash on purpose.
--source include/not_valgrind.inc
# Embedded server does not support crashing
--source include/not_embedded.inc
# Avoid CrashReporter popup on Mac
--source include/not_crashrep.inc

# suppress expected warnings
call mtr.add_suppression("does not exist in the InnoDB internal");

################################################################################
#
# Will test following scenarios:
# 1. Hit crash point on completing drop of all indexes before creation of index
#    is commenced.
# 2. Hit crash point after data is updated to system-table and in-memory dict.
#
################################################################################

#-----------------------------------------------------------------------------
#
# create test-bed
#
let $per_table = `select @@innodb_file_per_table`;

eval set global innodb_file_per_table = on;
let $WL6501_TMP_DIR = `select @@tmpdir`;
let $WL6501_DATA_DIR = `select @@datadir`;
let SEARCH_FILE = $MYSQLTEST_VARDIR/log/my_restart.err;

#-----------------------------------------------------------------------------
#
# 1. Hit crash point on completing drop of all indexes before creation of index
#    is commenced.
#
--echo "1. Hit crash point on completing drop of all indexes before creation"
--echo "   of index is commenced."
eval set global innodb_file_per_table = $wl6501_file_per_table;
set innodb_strict_mode=off;
--disable_warnings
eval create $wl6501_temp table t (
	i int, f float, c char,
	primary key pk(i), unique findex(f), index ck(c))
	engine = innodb row_format = $wl6501_row_fmt
	key_block_size = $wl6501_kbs;
--enable_warnings
insert into t values (1, 1.1, 'a'), (2, 2.2, 'b'), (3, 3.3, 'c');
select * from t;
check table t;
#
set session debug = "+d,ib_trunc_crash_drop_reinit_done_create_to_start";
--source include/expect_crash.inc
--error 2013
truncate table t;
#
--source include/start_mysqld.inc
check table t;

#-----------------------------------------------------------------------------
#
# 2. Hit crash point after data is updated to system-table and in-memory dict.
#
--echo "2. Hit crash point after data is updated to system-table and"
--echo "   in-memory dict."
eval set global innodb_file_per_table = $wl6501_file_per_table;
set innodb_strict_mode=off;
--disable_warnings
eval create $wl6501_temp table t (
	i int, f float, c char,
	primary key pk(i), unique findex(f), index ck(c))
	engine = innodb row_format = $wl6501_row_fmt
	key_block_size = $wl6501_kbs;
--enable_warnings
insert into t values (1, 1.1, 'a'), (2, 2.2, 'b'), (3, 3.3, 'c');
select * from t;
check table t;
#
set session debug = "+d,ib_trunc_crash_on_updating_dict_sys_info";
--source include/expect_crash.inc
--error 2013
truncate table t;
#
--source include/start_mysqld.inc
check table t;

#-----------------------------------------------------------------------------
#
# remove test-bed
#
eval set global innodb_file_per_table = $per_table;
