#
# WL#6501: make truncate table atomic
#

--source include/have_debug.inc

--disable_query_log
# suppress expected warnings
call mtr.add_suppression("Unable to truncate FTS index for table");
call mtr.add_suppression("Unable to assign a new identifier to table "
			 "`.*`\.`.*` after truncating it");
call mtr.add_suppression("Flagged corruption of .* in table "
			 "`.*`\.`.*` in TRUNCATE TABLE");
call mtr.add_suppression("Parent table of FTS auxiliary table "
			 ".*\/.* not found");
--enable_query_log
################################################################################
#
# Will test following scenarios:
# 1. Error in assigning undo logs for truncate action.
# 2. Error while preparing for truncate.
# 3. Error while dropping/creating indexes.
# 4. Error while completing truncate of table involving FTS.
# 5. Error while updating sys-tables.
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
set innodb_strict_mode=off;

#-----------------------------------------------------------------------------
#
# 1. Error in assigning undo logs for truncate action.
#
--echo "1. Error in assigning undo logs for truncate action."
eval set global innodb_file_per_table = $wl6501_file_per_table;
--disable_warnings
eval create $wl6501_temp table t (
        i int, f float, c char,
        primary key pk(i), unique findex(f), index ck(c))
        engine = innodb row_format = $wl6501_row_fmt
        key_block_size = $wl6501_kbs;
--enable_warnings
insert into t values (1, 1.1, 'a'), (2, 2.2, 'b'), (3, 3.3, 'c');
select * from t;
#check table t;
#
set session debug = "+d,ib_err_trunc_assigning_undo_log";
--error ER_GET_ERRNO
truncate table t;
set session debug = "-d,ib_err_trunc_assigning_undo_log";
#
#check table t;
select * from t;
drop table t;

#-----------------------------------------------------------------------------
#
# 2. Error while preparing for truncate.
#
--echo "2. Error while preparing for truncate."
eval set global innodb_file_per_table = $wl6501_file_per_table;
--disable_warnings
eval create $wl6501_temp table t (
        i int, f float, c char,
        primary key pk(i), unique findex(f), index ck(c))
        engine = innodb row_format = $wl6501_row_fmt
        key_block_size = $wl6501_kbs;
--enable_warnings
insert into t values (1, 1.1, 'a'), (2, 2.2, 'b'), (3, 3.3, 'c');
select * from t;
#check table t;
#
set session debug = "+d,ib_err_trunc_preparing_for_truncate";
--error ER_GET_ERRNO
truncate table t;
set session debug = "-d,ib_err_trunc_preparing_for_truncate";
#
#check table t;
select * from t;
drop table t;

#-----------------------------------------------------------------------------
#
# 3. Error while dropping/creating indexes
#
--echo "3. Error while dropping/creating indexes"
eval set global innodb_file_per_table = $wl6501_file_per_table;
--disable_warnings
eval create $wl6501_temp table t (
        i int, f float, c char,
        primary key pk(i), unique findex(f), index ck(c))
        engine = innodb row_format = $wl6501_row_fmt
        key_block_size = $wl6501_kbs;
--enable_warnings
insert into t values (1, 1.1, 'a'), (2, 2.2, 'b'), (3, 3.3, 'c');
select * from t;
#check table t;
#
set session debug = "+d,ib_err_trunc_drop_index";
--error ER_GET_ERRNO
truncate table t;
set session debug = "-d,ib_err_trunc_drop_index";
#
#check table t;
--error ER_TABLE_CORRUPT
select * from t;
drop table t;
#
#
eval set global innodb_file_per_table = $wl6501_file_per_table;
--disable_warnings
eval create $wl6501_temp table t (
        i int, f float, c char,
        primary key pk(i), unique findex(f), index ck(c))
        engine = innodb row_format = $wl6501_row_fmt
        key_block_size = $wl6501_kbs;
--enable_warnings
insert into t values (1, 1.1, 'a'), (2, 2.2, 'b'), (3, 3.3, 'c');
select * from t;
#check table t;
#
set session debug = "+d,ib_err_trunc_create_index";
--error ER_GET_ERRNO
truncate table t;
set session debug = "-d,ib_err_trunc_create_index";
#
#check table t;
--error ER_TABLE_CORRUPT
select * from t;
drop table t;
#
#
eval set global innodb_file_per_table = $wl6501_file_per_table;
--disable_warnings
eval create temporary table t (
        i int, f float, c char,
        primary key pk(i), unique findex(f), index ck(c))
        engine = innodb row_format = $wl6501_row_fmt
        key_block_size = $wl6501_kbs;
--enable_warnings
insert into t values (1, 1.1, 'a'), (2, 2.2, 'b'), (3, 3.3, 'c');
select * from t;
#check table t;
#
set session debug = "+d,ib_err_trunc_temp_recreate_index";
--error ER_GET_ERRNO
truncate table t;
set session debug = "-d,ib_err_trunc_temp_recreate_index";
#
#check table t;
--error ER_TABLE_CORRUPT
select * from t;
drop table t;

#-----------------------------------------------------------------------------
#
# 4. Error while completing truncate of table involving FTS.
#
--echo "4. Error while completing truncate of table involving FTS."
eval set global innodb_file_per_table = $wl6501_file_per_table;
--disable_warnings
eval create $wl6501_temp table t (i int, f float, c char(100),
	primary key pk(i), index fk(f), fulltext index ck(c))
	engine=innodb row_format=$wl6501_row_fmt
	key_block_size=$wl6501_kbs;
--enable_warnings
insert into t values (1, 1.1, 'mysql is now oracle company'),
		     (2, 2.2, 'innodb is part of mysql'),
		     (3, 3.3, 'innodb is default storage engine of mysql');
select * from t;
#check table t;
#
set session debug = "+d,ib_err_trunc_during_fts_trunc";
--error ER_GET_ERRNO
truncate table t;
set session debug = "-d,ib_err_trunc_during_fts_trunc";
#
#check table t;
--error ER_TABLE_CORRUPT
select * from t;
drop table t;

#-----------------------------------------------------------------------------
#
# 5. Error while updating sys-tables.
#
--echo "5. Error while updating sys-tables."
eval set global innodb_file_per_table = $wl6501_file_per_table;
--disable_warnings
eval create $wl6501_temp table t (i int, f float, c char(100),
	primary key pk(i), index fk(f), fulltext index ck(c))
	engine=innodb row_format=$wl6501_row_fmt
	key_block_size=$wl6501_kbs;
--enable_warnings
insert into t values (1, 1.1, 'mysql is now oracle company'),
		     (2, 2.2, 'innodb is part of mysql'),
		     (3, 3.3, 'innodb is default storage engine of mysql');
select * from t order by i;
#check table t;
#
set session debug = "+d,ib_err_trunc_during_sys_table_update";
--error ER_GET_ERRNO
truncate table t;
set session debug = "-d,ib_err_trunc_during_sys_table_update";
#
#check table t;
--error ER_TABLE_CORRUPT
select * from t order by i;
drop table t;

#-----------------------------------------------------------------------------
#
# remove test-bed
#
eval set global innodb_file_per_table = $per_table;
