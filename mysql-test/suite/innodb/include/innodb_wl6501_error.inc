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
# 1. Error in assigning undo logs for truncate action.
# 2. Error while preparing for truncate.
# 3. Error while dropping index (stimlate ibd file missing condition).
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
# 1. Error in assigning undo logs for truncate action.
#
--echo "1. Error in assigning undo logs for truncate action."
use test;
eval set global innodb_file_per_table = $wl6501_file_per_table;
eval set global innodb_file_format = $wl6501_file_format;
--replace_regex /[0-9]+/NUMBER/
eval create $wl6501_temp table t (i int)
	engine=innodb row_format=$wl6501_row_fmt
	key_block_size=$wl6501_kbs;
insert into t values (1), (2), (3);
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
use test;
eval set global innodb_file_per_table = $wl6501_file_per_table;
eval set global innodb_file_format = $wl6501_file_format;
--replace_regex /[0-9]+/NUMBER/
eval create $wl6501_temp table t (i int)
	engine=innodb row_format=$wl6501_row_fmt
	key_block_size=$wl6501_kbs;
insert into t values (1), (2), (3);
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
# 3. Error while dropping index (stimlate ibd file missing condition).
#
--echo "3. Error while dropping index (stimlate ibd file missing condition)."
use test;
eval set global innodb_file_per_table = $wl6501_file_per_table;
eval set global innodb_file_format = $wl6501_file_format;
--replace_regex /[0-9]+/NUMBER/
eval create $wl6501_temp table t (i int)
	engine=innodb row_format=$wl6501_row_fmt
	key_block_size=$wl6501_kbs;
insert into t values (1), (2), (3);
select * from t;
#check table t;
#
set session debug = "+d,ib_error_trunc_ibd_file_missing";
--error ER_GET_ERRNO
truncate table t;
set session debug = "-d,ib_error_trunc_ibd_file_missing";
#
#check table t;
--error ER_INDEX_CORRUPT
select * from t;
drop table t;



#-----------------------------------------------------------------------------
#
# remove test-bed
#
eval set global innodb_file_format = $format;
eval set global innodb_file_per_table = $per_table;

