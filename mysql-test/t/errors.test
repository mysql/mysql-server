#
# Test some error conditions
#

--disable_warnings
drop table if exists t1;
--enable_warnings
--error 1146
insert into t1 values(1);
--error 1146
delete from t1;
--error 1146
update t1 set a=1;
create table t1 (a int);
--error 1054
select count(test.t1.b) from t1;
--error 1054
select count(not_existing_database.t1) from t1;
--error 1054
select count(not_existing_database.t1.a) from t1;
--error 1044,1049
select count(not_existing_database.t1.a) from not_existing_database.t1;
--error 1054
select 1 from t1 order by 2;
--error 1054
select 1 from t1 group by 2;
--error 1054
select 1 from t1 order by t1.b;
--error 1054
select count(*),b from t1;
drop table t1;

# End of 4.1 tests

#
# Bug #6080: Error message for a field with a display width that is too long
#
--error 1439
create table t1 (a int(256));
set sql_mode='traditional';
--error 1074
create table t1 (a varchar(66000));
set sql_mode=default;

#
# Bug #27513: mysql 5.0.x + NULL pointer DoS
#
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
CREATE TABLE t1 (a INT);
SELECT a FROM t1 WHERE a IN(1, (SELECT IF(1=0,1,2/0)));
INSERT INTO t1 VALUES(1);
SELECT a FROM t1 WHERE a IN(1, (SELECT IF(1=0,1,2/0)));
INSERT INTO t1 VALUES(2),(3);
SELECT a FROM t1 WHERE a IN(1, (SELECT IF(1=0,1,2/0)));
DROP TABLE t1;
SET sql_mode = default;
#
# Bug #28677: SELECT on missing column gives extra error
#
CREATE TABLE t1( a INT );
--error ER_BAD_FIELD_ERROR
SELECT b FROM t1;
SHOW ERRORS;
--error ER_BAD_FIELD_ERROR
CREATE TABLE t2 SELECT b FROM t1;
SHOW ERRORS;
--error ER_BAD_FIELD_ERROR
INSERT INTO t1 SELECT b FROM t1;
DROP TABLE t1;
# End of 5.0 tests

flush status;
--disable_warnings
drop table if exists t1, t2;
--enable_warnings
create table t1 (a int unique);
create table t2 (a int);
drop function if exists f1;
drop function if exists f2;

delimiter |;

create function f1() returns int
begin
  insert into t1 (a) values (1);
  insert into t1 (a) values (1);
  return 1;
end|
create function f2() returns int
begin
  insert into t2 (a) values (1);
  return 2;
end|
delimiter ;|

flush status;
--error 1062
select f1(), f2();
show status like 'Com_insert';
select * from t1;
select * from t2;
drop table t1;
drop table t2;
drop function f1;
drop function f2;

#
# testing the value encoding in the error messages
#
# should be TR\xC3\x9CE, TRÜE, TRÜE
#
SET NAMES utf8mb3;
--error ER_WRONG_VALUE_FOR_VAR
SET sql_quote_show_create= _binary x'5452C39C45';
--error ER_WRONG_VALUE_FOR_VAR
SET sql_quote_show_create= _utf8mb3 x'5452C39C45';
--error ER_WRONG_VALUE_FOR_VAR
SET sql_quote_show_create=_latin1 x'5452DC45';
--error ER_WRONG_VALUE_FOR_VAR
SET sql_quote_show_create='TRÃœE';
--error ER_WRONG_VALUE_FOR_VAR
SET sql_quote_show_create=TRÃœE;

SET NAMES latin1;
--error ER_WRONG_VALUE_FOR_VAR
SET sql_quote_show_create= _binary x'5452C39C45';
--error ER_WRONG_VALUE_FOR_VAR
SET sql_quote_show_create= _utf8mb3 x'5452C39C45';
--error ER_WRONG_VALUE_FOR_VAR
SET sql_quote_show_create=_latin1 x'5452DC45';
--error ER_WRONG_VALUE_FOR_VAR
SET sql_quote_show_create='TRÜE';
--error ER_WRONG_VALUE_FOR_VAR
SET sql_quote_show_create=TRÜE;

SET NAMES binary;
--error ER_WRONG_VALUE_FOR_VAR
SET sql_quote_show_create= _binary x'5452C39C45';
--error ER_WRONG_VALUE_FOR_VAR
SET sql_quote_show_create= _utf8mb3 x'5452C39C45';
--error ER_WRONG_VALUE_FOR_VAR
SET sql_quote_show_create=_latin1 x'5452DC45';

--echo #
--echo # Bug#52430 Incorrect key in the error message for duplicate key error involving BINARY type
--echo #
CREATE TABLE t1(c1 BINARY(10), c2 BINARY(10), c3 BINARY(10),
PRIMARY KEY(c1,c2,c3));
INSERT INTO t1 (c1,c2,c3) VALUES('abc','abc','abc');
--error ER_DUP_ENTRY
INSERT INTO t1 (c1,c2,c3) VALUES('abc','abc','abc');
DROP TABLE t1;

CREATE TABLE t1 (f1 VARBINARY(19) PRIMARY KEY);
INSERT INTO t1 VALUES ('abc\0\0');
--error ER_DUP_ENTRY
INSERT INTO t1 VALUES ('abc\0\0');
DROP TABLE t1;

--echo #
--echo # Bug#57882: Item_func_conv_charset::val_str(String*): 
--echo #            Assertion `fixed == 1' failed
--echo #

--error ER_DATA_OUT_OF_RANGE
SELECT (CONVERT('0' USING latin1) IN (CHAR(COT('v') USING utf8mb3),''));

SET NAMES utf8mb3 COLLATE utf8mb3_latvian_ci ;
--error ER_DATA_OUT_OF_RANGE
SELECT UPDATEXML(-73 * -2465717823867977728,@@global.auto_increment_increment,null);

--echo #
--echo # End Bug#57882
--echo #

#
# Bug #13031606 VALUES() IN A SELECT STATEMENT CRASHES SERVER
#
CREATE TABLE t1 (a INT);
CREATE TABLE t2(a INT PRIMARY KEY, b INT);
--error ER_BAD_FIELD_ERROR
SELECT '' AS b FROM t1 GROUP BY VALUES(b);
--error ER_BAD_FIELD_ERROR
REPLACE t2(b) SELECT '' AS b FROM t1 GROUP BY VALUES(b);
--error ER_BAD_FIELD_ERROR
UPDATE t2 SET a=(SELECT '' AS b FROM t1 GROUP BY VALUES(b));
--error ER_BAD_FIELD_ERROR
INSERT INTO t2 VALUES (1,0) ON DUPLICATE KEY UPDATE
  b=(SELECT '' AS b FROM t1 GROUP BY VALUES(b));
INSERT INTO t2(a,b) VALUES (1,0) ON DUPLICATE KEY UPDATE
  b=(SELECT VALUES(a)+2 FROM t1);
DROP TABLE t1, t2;

--echo #
--echo # Bug#54812: assert in Diagnostics_area::set_ok_status during EXPLAIN
--echo #

CREATE USER nopriv_user@localhost;

connection default;
--echo connection: default

--disable_warnings
DROP TABLE IF EXISTS t1,t2,t3;
DROP FUNCTION IF EXISTS f;
--enable_warnings

CREATE TABLE t1 (key1 INT PRIMARY KEY);
CREATE TABLE t2 (key2 INT);
INSERT INTO t1 VALUES (1),(2);

CREATE FUNCTION f() RETURNS INT RETURN 1;

GRANT FILE ON *.* TO 'nopriv_user'@'localhost';

FLUSH PRIVILEGES;

connect (con1,localhost,nopriv_user,,);
connection con1;
--echo connection: con1

let outfile=$MYSQLTEST_VARDIR/tmp/mytest;
--error 0,1
--remove_file $outfile
--replace_result $outfile <outfile>
--error ER_PROCACCESS_DENIED_ERROR
eval SELECT MAX(key1) FROM t1 WHERE f() < 1 INTO OUTFILE '$outfile';
# A removal of the outfile is necessary, at least today (2010-12-07), because
# the outfile is created even if the SELECT statement fails.
# If the server is improved in the future this may not happen.
# ==> Do not fail if the outfile does not exist.
--error 0,1
--remove_file $outfile

--error ER_PROCACCESS_DENIED_ERROR
INSERT INTO t2 SELECT MAX(key1) FROM t1 WHERE f() < 1;

--error ER_PROCACCESS_DENIED_ERROR
SELECT MAX(key1) INTO @dummy FROM t1 WHERE f() < 1;

--error ER_PROCACCESS_DENIED_ERROR
CREATE TABLE t3 (i INT) AS SELECT MAX(key1) FROM t1 WHERE f() < 1;

disconnect con1;
--source include/wait_until_disconnected.inc

connection default;
--echo connection: default

DROP TABLE t1,t2;
DROP FUNCTION f;
DROP USER nopriv_user@localhost;

--echo #
--echo # End Bug#54812
--echo #

--echo #
--echo # Bug #32162954: REGRESSION: ASSERTION `!THD()->IS_ERROR()' FAILED. IN AGGREGATEITERATOR::READ()
--echo #

--error ER_DATA_OUT_OF_RANGE
do count(rand(st_latfromgeohash(st_geohash(point(5920138304254667057,24370),41))));
--error ER_DATA_OUT_OF_RANGE
do ((1)between(cot(_hebrew  ' $[  d'))and(var_pop((1))over()));
--error ER_GIS_INVALID_DATA
do ((st_isvalid(1))=(std(format(-29867 ,54,'es_DO')) over()));

--echo #
--echo # Bug #32206756: ASSERTION `!CURRENT_THD->IS_ERROR()' FAILED IN SQL/ITEM_CMPFUNC.CC
--echo #

CREATE TABLE t1 (a varchar(1), b varchar(1));

CREATE TABLE t2 (pk integer, a varchar(1), b varchar(1), c date, primary key(pk));
CREATE INDEX idx1 ON t2 (b);

INSERT INTO t1 VALUES ('d','7');
INSERT INTO t2 VALUES (1,'q','7','1970-01-01');
INSERT INTO t2 VALUES (2,'l','7','1970-01-01');

--error ER_TRUNCATED_WRONG_VALUE
CREATE TABLE insert_select AS
SELECT t2.c AS field3
FROM t1, t2
WHERE t1.b = t2.b AND t1.a <> t2.pk;

DROP TABLE t1, t2;
