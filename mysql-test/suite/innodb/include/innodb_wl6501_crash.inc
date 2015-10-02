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

# suppress expected warnings.
call mtr.add_suppression("The file '.*' already exists though the corresponding table did not exist in the InnoDB data dictionary");
call mtr.add_suppression("Cannot create file '.*'");
call mtr.add_suppression("InnoDB: Error number 17 means 'File exists'");

################################################################################
#
# Will test following scenarios:
# 1. Hit crash point while writing redo log.
# 2. Hit crash point on completion of redo log write.
# 3. Hit crash point while dropping indexes.
# 4. Hit crash point on completing drop of all indexes before creation of index
#    is commenced.
# 5. Hit crash point while creating indexes.
# 6. Hit crash point after data is updated to system-table and in-memory dict.
# 7. Hit crash point before/after log checkpoint is done.
#
################################################################################

#-----------------------------------------------------------------------------
#
# create test-bed
#
let $per_table = `select @@innodb_file_per_table`;
let $format = `select @@innodb_file_format`;

eval set global innodb_file_per_table = on;
let $WL6501_TMP_DIR = `select @@tmpdir`;
let $WL6501_DATA_DIR = `select @@datadir`;
let SEARCH_FILE = $MYSQLTEST_VARDIR/log/my_restart.err;

#-----------------------------------------------------------------------------
#
# 1. Hit crash point while writing redo log.
#
--echo "1. Hit crash point while writing redo log."
use test;
eval set global innodb_file_per_table = $wl6501_file_per_table;
eval set global innodb_file_format = $wl6501_file_format;
SET innodb_strict_mode=OFF;
--disable_warnings
eval create $wl6501_temp table t (
	i int, f float, c char,
	primary key pk(i), unique findex(f), index ck(c))
	engine=innodb row_format=$wl6501_row_fmt
	key_block_size=$wl6501_kbs;
--enable_warnings
insert into t values (1, 1.1, 'a'), (2, 2.2, 'b'), (3, 3.3, 'c');
select * from t;
check table t;
#
set session debug = "+d,ib_trunc_crash_while_writing_redo_log";
--source include/expect_crash.inc
--error 2013
truncate table t;
#
--source include/start_mysqld.inc
check table t;
select * from t;
select * from t where f < 2.5;
drop table t;

#-----------------------------------------------------------------------------
#
# 2. Hit crash point on completion of redo log write.
#
--echo "2. Hit crash point on completion of redo log write."
use test;
eval set global innodb_file_per_table = $wl6501_file_per_table;
eval set global innodb_file_format = $wl6501_file_format;
SET innodb_strict_mode=OFF;
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
set session debug = "+d,ib_trunc_crash_after_redo_log_write_complete";
--source include/expect_crash.inc
--error 2013
truncate table t;
#
--source include/start_mysqld.inc
check table t;
select * from t;
insert into t values (1, 1.1, 'a'), (2, 2.2, 'b'), (3, 3.3, 'c');
select * from t;
select * from t where f < 2.5;
drop table t;

#-----------------------------------------------------------------------------
#
# 3. Hit crash point while dropping indexes.
#
--echo "3. Hit crash point while dropping indexes."
use test;
eval set global innodb_file_per_table = $wl6501_file_per_table;
eval set global innodb_file_format = $wl6501_file_format;
SET innodb_strict_mode=OFF;
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
set session debug = "+d,ib_trunc_crash_on_drop_of_clust_index";
--source include/expect_crash.inc
--error 2013
truncate table t;
#
--source include/start_mysqld.inc
check table t;
select * from t;
insert into t values (1, 1.1, 'a'), (2, 2.2, 'b'), (3, 3.3, 'c');
select * from t;
select * from t where f < 2.5;
drop table t;
#
#
use test;
eval set global innodb_file_per_table = $wl6501_file_per_table;
eval set global innodb_file_format = $wl6501_file_format;
SET innodb_strict_mode=OFF;
--disable_warnings
eval create $wl6501_temp table t (
	i int, f float, c char,
	primary key pk(i), unique findex(f))
	engine = innodb row_format = $wl6501_row_fmt
	key_block_size = $wl6501_kbs;
--enable_warnings
insert into t values (1, 1.1, 'a'), (2, 2.2, 'b'), (3, 3.3, 'c');
select * from t;
check table t;
#
set session debug = "+d,ib_trunc_crash_on_drop_of_uniq_index";
--source include/expect_crash.inc
--error 2013
truncate table t;
#
--source include/start_mysqld.inc
#
check table t;
select * from t;
insert into t values (1, 1.1, 'a'), (2, 2.2, 'b'), (3, 3.3, 'c');
select * from t;
select * from t where f < 2.5;
drop table t;
#
#
use test;
eval set global innodb_file_per_table = $wl6501_file_per_table;
eval set global innodb_file_format = $wl6501_file_format;
SET innodb_strict_mode=OFF;
--disable_warnings
eval create $wl6501_temp table t (
	i int, f float, c char,
	primary key pk(i), index ck(c))
	engine = innodb row_format = $wl6501_row_fmt
	key_block_size = $wl6501_kbs;
--enable_warnings
insert into t values (1, 1.1, 'a'), (2, 2.2, 'b'), (3, 3.3, 'c');
select * from t;
check table t;
#
set session debug = "+d,ib_trunc_crash_on_drop_of_sec_index";
--source include/expect_crash.inc
--error 2013
truncate table t;
#
--source include/start_mysqld.inc
check table t;
select * from t;
insert into t values (1, 1.1, 'a'), (2, 2.2, 'b'), (3, 3.3, 'c');
select * from t;
select * from t where f < 2.5;
drop table t;

#-----------------------------------------------------------------------------
#
# 4. Hit crash point on completing drop of all indexes before creation of index
#    is commenced.
#
--echo "4. Hit crash point on completing drop of all indexes before creation"
--echo "   of index is commenced."
use test;
eval set global innodb_file_per_table = $wl6501_file_per_table;
eval set global innodb_file_format = $wl6501_file_format;
SET innodb_strict_mode=OFF;
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
select * from t;
insert into t values (1, 1.1, 'a'), (2, 2.2, 'b'), (3, 3.3, 'c');
select * from t;
select * from t where f < 2.5;
drop table t;

#-----------------------------------------------------------------------------
#
# 5. Hit crash point while creating indexes.
#
--echo "5. Hit crash point while creating indexes."
use test;
eval set global innodb_file_per_table = $wl6501_file_per_table;
eval set global innodb_file_format = $wl6501_file_format;
SET innodb_strict_mode=OFF;
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
set session debug = "+d,ib_trunc_crash_on_create_of_clust_index";
--source include/expect_crash.inc
--error 2013
truncate table t;
#
--source include/start_mysqld.inc
check table t;
select * from t;
insert into t values (1, 1.1, 'a'), (2, 2.2, 'b'), (3, 3.3, 'c');
select * from t;
select * from t where f < 2.5;
drop table t;
#
#
use test;
eval set global innodb_file_per_table = $wl6501_file_per_table;
eval set global innodb_file_format = $wl6501_file_format;
SET innodb_strict_mode=OFF;
--disable_warnings
eval create $wl6501_temp table t (
	i int, f float, c char,
	primary key pk(i), unique findex(f))
	engine = innodb row_format = $wl6501_row_fmt
	key_block_size = $wl6501_kbs;
--enable_warnings
insert into t values (1, 1.1, 'a'), (2, 2.2, 'b'), (3, 3.3, 'c');
select * from t;
check table t;
#
set session debug = "+d,ib_trunc_crash_on_create_of_uniq_index";
--source include/expect_crash.inc
--error 2013
truncate table t;
#
--source include/start_mysqld.inc
check table t;
select * from t;
insert into t values (1, 1.1, 'a'), (2, 2.2, 'b'), (3, 3.3, 'c');
select * from t;
select * from t where f < 2.5;
drop table t;
#
#
use test;
eval set global innodb_file_per_table = $wl6501_file_per_table;
eval set global innodb_file_format = $wl6501_file_format;
SET innodb_strict_mode=OFF;
--disable_warnings
eval create $wl6501_temp table t (
	i int, f float, c char,
	primary key pk(i), index ck(c))
	engine = innodb row_format = $wl6501_row_fmt
	key_block_size = $wl6501_kbs;
--enable_warnings
insert into t values (1, 1.1, 'a'), (2, 2.2, 'b'), (3, 3.3, 'c');
select * from t;
check table t;
#
set session debug = "+d,ib_trunc_crash_on_create_of_sec_index";
--source include/expect_crash.inc
--error 2013
truncate table t;
#
--source include/start_mysqld.inc
check table t;
select * from t;
insert into t values (1, 1.1, 'a'), (2, 2.2, 'b'), (3, 3.3, 'c');
select * from t;
select * from t where f < 2.5;
drop table t;

#-----------------------------------------------------------------------------
#
# 6. Hit crash point after data is updated to system-table and in-memory dict.
#
--echo "6. Hit crash point after data is updated to system-table and"
--echo "   in-memory dict."
use test;
eval set global innodb_file_per_table = $wl6501_file_per_table;
eval set global innodb_file_format = $wl6501_file_format;
SET innodb_strict_mode=OFF;
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
select * from t;
insert into t values (1, 1.1, 'a'), (2, 2.2, 'b'), (3, 3.3, 'c');
select * from t;
select * from t where f < 2.5;
drop table t;

#-----------------------------------------------------------------------------
#
# 7. Hit crash point before/after log checkpoint is done.
#
--echo "7. Hit crash point before/after log checkpoint is done."
use test;
eval set global innodb_file_per_table = $wl6501_file_per_table;
eval set global innodb_file_format = $wl6501_file_format;
SET innodb_strict_mode=OFF;
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
set session debug = "+d,ib_trunc_crash_before_log_removal";
--source include/expect_crash.inc
--error 2013
truncate table t;
#
--source include/start_mysqld.inc
check table t;
select * from t;
insert into t values (1, 1.1, 'a'), (2, 2.2, 'b'), (3, 3.3, 'c');
select * from t where f < 2.5;
drop table t;
#
#
use test;
eval set global innodb_file_per_table = $wl6501_file_per_table;
eval set global innodb_file_format = $wl6501_file_format;
SET innodb_strict_mode=OFF;
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
set session debug = "+d,ib_trunc_crash_after_truncate_done";
--source include/expect_crash.inc
--error 2013
truncate table t;
#
--source include/start_mysqld.inc
check table t;
select * from t;
insert into t values (1, 1.1, 'a'), (2, 2.2, 'b'), (3, 3.3, 'c');
select * from t;
select * from t where f < 2.5;
drop table t;


#-----------------------------------------------------------------------------
#
# remove test-bed
#
eval set global innodb_file_format = $format;
eval set global innodb_file_per_table = $per_table;
