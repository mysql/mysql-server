--disable_abort_on_error
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
USE test;

--disable_warnings
DROP TABLE IF EXISTS t1, t2, t4, t10, t11;
--enable_warnings
eval
CREATE TABLE t1  (f1 char(20),f2 char(25),f3 date,f4 int,f5 char(25),f6 int)
charset latin1 ENGINE = $engine_type;
eval
CREATE TABLE t2  (f1 char(20),f2 char(25),f3 date,f4 int,f5 char(25),f6 int)
charset latin1 ENGINE = $engine_type;
eval
CREATE TABLE t4  (f1 char(20),f2 char(25),f3 date,f4 int,f5 char(25),f6 int)
charset latin1 ENGINE = $engine_type;
eval
CREATE TABLE t10 (f1 char(20),f2 char(25),f3 date,f4 int,f5 char(25),f6 int)
charset latin1 ENGINE = $engine_type;
eval
CREATE TABLE t11 (f1 char(20),f2 char(25),f3 date,f4 int,f5 char(25),f6 int)
charset latin1 ENGINE = $engine_type;
--replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
eval
LOAD DATA INFILE '$MYSQLTEST_VARDIR/std_data/funcs_1/t4.txt' INTO TABLE t1
character set latin1;
--replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
eval
LOAD DATA INFILE '$MYSQLTEST_VARDIR/std_data/funcs_1/t4.txt' INTO TABLE t2
character set latin1;
--replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
eval
LOAD DATA INFILE '$MYSQLTEST_VARDIR/std_data/funcs_1/t4.txt' INTO TABLE t4
character set latin1;
--replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
eval
LOAD DATA INFILE '$MYSQLTEST_VARDIR/std_data/funcs_1/t4.txt' INTO TABLE t10
character set latin1;
--replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
eval
LOAD DATA INFILE '$MYSQLTEST_VARDIR/std_data/funcs_1/t4.txt' INTO TABLE t11
character set latin1;


--disable_warnings
drop TABLE if exists t3;
--enable_warnings
eval
CREATE TABLE t3 (f1 char(20), f2 char(20), f3 integer) charset latin1 ENGINE = $engine_type;
--replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
eval
LOAD DATA INFILE '$MYSQLTEST_VARDIR/std_data/funcs_1/t3.txt' INTO TABLE t3
character set latin1;

#---------------------------

--disable_warnings
drop database if exists test4;
--enable_warnings
CREATE database test4;
use test4;

eval
CREATE TABLE t6 (f1 char(20), f2 char(25), f3 date, f4 int, f5 char(25), f6 int)
charset latin1 ENGINE = $engine_type;
--replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
eval
LOAD DATA INFILE '$MYSQLTEST_VARDIR/std_data/funcs_1/t4.txt' INTO TABLE t6
character set latin1;
#---------------------------

use test;

--disable_warnings
drop TABLE if exists t7, t8;
--enable_warnings
eval
CREATE TABLE t7 (f1 char(20),f2 char(25),f3 date,f4 int) charset latin1 ENGINE = $engine_type;
eval
CREATE TABLE t8 (f1 char(20),f2 char(25),f3 date,f4 int) charset latin1 ENGINE = $engine_type;
--replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
eval
LOAD DATA INFILE '$MYSQLTEST_VARDIR/std_data/funcs_1/t7.txt' INTO TABLE t7
character set latin1;
--replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
eval
LOAD DATA INFILE '$MYSQLTEST_VARDIR/std_data/funcs_1/t7.txt' INTO TABLE t8
character set latin1;


--disable_warnings
drop TABLE if exists t9;
--enable_warnings
eval CREATE TABLE t9 (f1 int, f2 char(25), f3 int) charset latin1 ENGINE = $engine_type;
--replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
eval
LOAD DATA INFILE '$MYSQLTEST_VARDIR/std_data/funcs_1/t9.txt' INTO TABLE t9
character set latin1;
SET sql_mode = default;
