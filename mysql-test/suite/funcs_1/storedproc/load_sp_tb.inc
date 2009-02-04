--echo
--echo --source suite/funcs_1/storedproc/load_sp_tb.inc
--echo --------------------------------------------------------------------------------

# ==============================================================================
#
#  This load script can be called multiple times inside a test script because it
#  first cleans up all objects that will be created.
#  Therefore the same script is used as it will be used at the end of a test.
#
# ==============================================================================

--disable_abort_on_error
--enable_query_log

# use the same .inc to cleanup before and after the test
--source suite/funcs_1/storedproc/cleanup_sp_tb.inc

CREATE DATABASE db_storedproc;
CREATE DATABASE db_storedproc_1;

USE db_storedproc;

--replace_result $engine_type <engine_to_be_tested>
eval
create table t1(f1 char(20),f2 char(25),f3 date,f4 int,f5 char(25),f6 int)
engine = $engine_type;
--replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
eval
load data infile '$MYSQLTEST_VARDIR/std_data/funcs_1/t4.txt' into table t1;

--replace_result $engine_type <engine_to_be_tested>
eval
create table t2(f1 char(20),f2 char(25),f3 date,f4 int,f5 char(25),f6 int)
engine = $engine_type;
--replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
eval
load data infile '$MYSQLTEST_VARDIR/std_data/funcs_1/t4.txt' into table t2;

--replace_result $engine_type <engine_to_be_tested>
eval
create table t3(f1 char(20),f2 char(20),f3 integer) engine = $engine_type;
--replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
eval
load data infile '$MYSQLTEST_VARDIR/std_data/funcs_1/t3.txt' into table t3;

--replace_result $engine_type <engine_to_be_tested>
eval
create table t4(f1 char(20),f2 char(25),f3 date,f4 int,f5 char(25),f6 int)
engine = $engine_type;
--replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
eval
load data infile '$MYSQLTEST_VARDIR/std_data/funcs_1/t4.txt' into table t4;

USE db_storedproc_1;

--replace_result $engine_type <engine_to_be_tested>
eval
create table t6(f1 char(20),f2 char(25),f3 date,f4 int,f5 char(25),f6 int)
engine = $engine_type;
--replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
eval
load data infile '$MYSQLTEST_VARDIR/std_data/funcs_1/t4.txt' into table t6;

USE db_storedproc;

--replace_result $engine_type <engine_to_be_tested>
eval
create table t7 (f1 char(20), f2 char(25), f3 date, f4 int)
engine = $engine_type;
--replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
eval
load data infile '$MYSQLTEST_VARDIR/std_data/funcs_1/t7.txt' into table t7;

--replace_result $engine_type <engine_to_be_tested>
eval
create table t8 (f1 char(20), f2 char(25), f3 date, f4 int)
engine = $engine_type;
--replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
eval
load data infile '$MYSQLTEST_VARDIR/std_data/funcs_1/t7.txt' into table t8;

--replace_result $engine_type <engine_to_be_tested>
eval
create table t9(f1 int, f2 char(25), f3 int) engine = $engine_type;
--replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
eval
load data infile '$MYSQLTEST_VARDIR/std_data/funcs_1/t9.txt' into table t9;

--replace_result $engine_type <engine_to_be_tested>
eval
create table t10(f1 char(20),f2 char(25),f3 date,f4 int,f5 char(25),f6 int)
engine = $engine_type;
--replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
eval
load data infile '$MYSQLTEST_VARDIR/std_data/funcs_1/t4.txt' into table t10;

--replace_result $engine_type <engine_to_be_tested>
eval
create table t11(f1 char(20),f2 char(25),f3 date,f4 int,f5 char(25),f6 int)
engine = $engine_type;
--replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
eval
load data infile '$MYSQLTEST_VARDIR/std_data/funcs_1/t4.txt' into table t11;
