#
# Performance Schema, Index validation
#
--echo
--echo ===== SETUP =====
let $get_thread_id=
  SELECT thread_id INTO @my_thread_id
  FROM performance_schema.threads
  WHERE processlist_id = connection_id();

let $get_thread_os_id=
  SELECT thread_os_id INTO @my_thread_os_id
  FROM performance_schema.threads
  WHERE processlist_id = connection_id();

let $get_processlist_id=
  SELECT connection_id() INTO @my_processlist_id;

--disable_query_log

--connection default
--echo ## Connection Default
--echo ## Disable events from the default connection
UPDATE performance_schema.threads SET instrumented = 'NO' WHERE processlist_id = CONNECTION_ID();

## Remove residual entries in the tables performance_schema.users
## and performance_schema.accounts
TRUNCATE TABLE performance_schema.users;
TRUNCATE TABLE performance_schema.accounts;

## Get the thread_id of the default connection
eval $get_thread_id;
let $default_thread_id= `SELECT @my_thread_id`;
eval $get_thread_os_id;
let $default_thread_os_id= `SELECT @my_thread_os_id`;
eval $get_processlist_id;
let $default_processlist_id= `SELECT @my_processlist_id`;

USE test;

--enable_query_log

--echo
--echo ## Create user1, user2, user3
CREATE USER user1@localhost;
CREATE USER user2@localhost;
CREATE USER user3@localhost;
GRANT ALL ON *.* to 'user1'@localhost;
GRANT ALL ON *.* to 'user2'@localhost;
GRANT ALL ON *.* to 'user3'@localhost;

--disable_query_log

--echo
--echo ## Setup test connection 1
connect(con1, localhost, user1,,);
## Get the thread_id of connection 1
eval $get_thread_id;
let $con1_thread_id= `SELECT @my_thread_id`;
eval $get_thread_os_id;
let $con1_thread_os_id= `SELECT @my_thread_os_id`;
eval $get_processlist_id;
let $con1_processlist_id= `SELECT @my_processlist_id`;

--echo ## Setup test connection 2
connect(con2, localhost, user2,,);
## Get the thread_id of connection 2
eval $get_thread_id;
let $con2_thread_id= `SELECT @my_thread_id`;
eval $get_thread_os_id;
let $con2_thread_os_id= `SELECT @my_thread_os_id`;
eval $get_processlist_id;
let $con2_processlist_id= `SELECT @my_processlist_id`;

--echo ## Setup test connection 3
connect(con3, localhost, user3,,);
## Get the thread_id of connection 3
eval $get_thread_id;
let $con3_thread_id= `SELECT @my_thread_id`;
eval $get_thread_os_id;
let $con3_thread_os_id= `SELECT @my_thread_os_id`;
eval $get_processlist_id;
let $con3_processlist_id= `SELECT @my_processlist_id`;

--enable_query_log

--echo
--echo ## Default connection
--connection default

--echo
--echo ## Create data tables
CREATE TABLE test.t1(a INT, b INT, c INT, d INT DEFAULT 0, PRIMARY KEY(a), INDEX index_b(b), INDEX index_cb(c, b));
CREATE TABLE test.t2 LIKE test.t1;
CREATE TABLE test.t3 LIKE test.t1;

--disable_query_log
--disable_result_log

--echo
--echo ## Generate event data from 3 connections

--connection con1
--echo ## Connection 1
SET @uvar_temp = 1;
INSERT INTO test.t1 SET a=101, b=1, c=1;
INSERT INTO test.t2 SET a=102, b=2, c=2;
INSERT INTO test.t2 SET a=103, b=3, c=3;
INSERT INTO test.t3 SET a=104, b=4, c=4;
INSERT INTO test.t3 SET a=105, b=5, c=5;
INSERT INTO test.t3 SET a=106, b=6, c=6;
SELECT * FROM test.t1;
SELECT * FROM test.t2;
SELECT * FROM test.t3;
UPDATE test.t1 SET d=d+1;
UPDATE test.t2 SET d=d+1;
UPDATE test.t3 SET d=d+1;
UPDATE test.t1 SET d=d+1 WHERE a=101;
UPDATE test.t2 SET d=d+1 WHERE a=101;
UPDATE test.t3 SET d=d+1 WHERE a=101;
SELECT * FROM test.t1 FORCE INDEX(index_b) WHERE b=5;
SELECT * FROM test.t2 FORCE INDEX(index_b) WHERE b=5;
SELECT * FROM test.t3 FORCE INDEX(index_b) WHERE b=5;

--connection con2
--echo ## Connection 2
SET @uvar_temp = 2;
INSERT INTO test.t1 SET a=201, b=1, c=1;
INSERT INTO test.t2 SET a=202, b=2, c=2;
INSERT INTO test.t2 SET a=203, b=3, c=3;
INSERT INTO test.t3 SET a=204, b=4, c=4;
INSERT INTO test.t3 SET a=205, b=5, c=5;
INSERT INTO test.t3 SET a=206, b=6, c=6;
SELECT * FROM test.t1;
SELECT * FROM test.t2;
SELECT * FROM test.t3;
UPDATE test.t1 SET d=d+1;
UPDATE test.t2 SET d=d+1;
UPDATE test.t3 SET d=d+1;
UPDATE test.t1 SET d=d+1 WHERE a=201;
UPDATE test.t2 SET d=d+1 WHERE a=201;
UPDATE test.t3 SET d=d+1 WHERE a=201;
SELECT * FROM test.t1 FORCE INDEX(index_b) WHERE b=5;
SELECT * FROM test.t2 FORCE INDEX(index_b) WHERE b=5;
SELECT * FROM test.t3 FORCE INDEX(index_b) WHERE b=5;

--connection con3
--echo ## Connection 3
SET @uvar_temp = 3;
INSERT INTO test.t1 SET a=301, b=1, c=1;
INSERT INTO test.t2 SET a=302, b=2, c=2;
INSERT INTO test.t2 SET a=303, b=3, c=3;
INSERT INTO test.t3 SET a=304, b=4, c=4;
INSERT INTO test.t3 SET a=305, b=5, c=5;
INSERT INTO test.t3 SET a=306, b=6, c=6;
SELECT * FROM test.t1;
SELECT * FROM test.t2;
SELECT * FROM test.t3;
UPDATE test.t1 SET d=d+1;
UPDATE test.t2 SET d=d+1;
UPDATE test.t3 SET d=d+1;
UPDATE test.t1 SET d=d+1 WHERE a=301;
UPDATE test.t2 SET d=d+1 WHERE a=301;
UPDATE test.t3 SET d=d+1 WHERE a=301;
SELECT * FROM test.t1 FORCE INDEX(index_b) WHERE b=5;
SELECT * FROM test.t2 FORCE INDEX(index_b) WHERE b=5;
SELECT * FROM test.t3 FORCE INDEX(index_b) WHERE b=5;

--enable_result_log
--enable_query_log

--connection con2
--echo ## Connection 2
--echo ## Create prepared statements on connection 2
PREPARE st1 FROM 'SELECT SQRT(POW(?,2) + POW(?,2)) AS hypotenuse';
PREPARE st3 FROM 'INSERT INTO test.t1 SELECT * FROM test.t1 WHERE a<=?';
PREPARE st4 FROM '(SELECT a FROM test.t1) UNION (SELECT a+10 FROM test.t1) ORDER BY RAND()*0+a';

--echo ## Create stored procedure connection 2
DELIMITER |;
CREATE PROCEDURE SampleProc1()
BEGIN
  SET @table = 'test.t1';
  SET @s = CONCAT('SELECT * FROM ', @table);
  PREPARE st2 FROM @s;
END|
DELIMITER ;|

--echo
--echo Call SampleProc1() to prepare a statement
CALL SampleProc1();

--disable_query_log
--disable_result_log

## Generate a system error
--error ER_NO_SUCH_TABLE
SELECT * from test.no_table;

--enable_result_log

#DEBUG --echo
#DEBUG SELECT * FROM performance_schema.prepared_statements_instances;

--echo ## Connection default
--connection default

# Set default column names
SET @pfs_columns = "*";
let $target_host = localhost;

--echo
--echo ## Use Connection 2 for default target thread id
eval SET @target_thread_id= $con2_thread_id;
eval SET @default_thread_id= $default_thread_id;

# DEBUG
#--echo ### Default thread: $default_thread_id
#--echo ### Con1 thread id: $con1_thread_id
#--echo ### Con2 thread id: $con2_thread_id (target)
#--echo ### Con3 thread id: $con3_thread_id

--source ../include/idx_compare_setup_index_test.inc

--enable_query_log
