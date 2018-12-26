TABLE: "socket_instances"
INDEXES: PK (OBJECT_INSTANCE_BEGIN), KEY (THREAD_ID), KEY (SOCKET_ID), KEY (IP), KEY (PORT)

===== SETUP =====
## Connection Default
## Disable events from the default connection

## Create user1, user2, user3
CREATE USER user1@localhost;
CREATE USER user2@localhost;
CREATE USER user3@localhost;
GRANT ALL ON *.* to 'user1'@localhost;
GRANT ALL ON *.* to 'user2'@localhost;
GRANT ALL ON *.* to 'user3'@localhost;

## Setup test connection 1
## Setup test connection 2
## Setup test connection 3

## Default connection

## Create data tables
CREATE TABLE test.t1(a INT, b INT, c INT, d INT DEFAULT 0, PRIMARY KEY(a), INDEX index_b(b), INDEX index_cb(c, b));
CREATE TABLE test.t2 LIKE test.t1;
CREATE TABLE test.t3 LIKE test.t1;

## Generate event data from 3 connections
## Connection 1
## Connection 2
## Connection 3
## Connection 2
## Create prepared statements on connection 2
PREPARE st1 FROM 'SELECT SQRT(POW(?,2) + POW(?,2)) AS hypotenuse';
PREPARE st3 FROM 'INSERT INTO test.t1 SELECT * FROM test.t1 WHERE a<=?';
PREPARE st4 FROM '(SELECT a FROM test.t1) UNION (SELECT a+10 FROM test.t1) ORDER BY RAND()*0+a';
## Create stored procedure connection 2
CREATE PROCEDURE SampleProc1()
BEGIN
SET @table = 'test.t1';
SET @s = CONCAT('SELECT * FROM ', @table);
PREPARE st2 FROM @s;
END|

Call SampleProc1() to prepare a statement
CALL SampleProc1();
## Connection default

## Use Connection 2 for default target thread id

## Create index_test() procedure

## Test OBJECT_INSTANCE_BEGIN

===== BEGIN TEST =====
========
SELECT * FROM performance_schema.socket_instances WHERE object_instance_begin is null

OK: Handler_read_key

OK: Result set: Empty  Expected: Empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT * FROM performance_schema.socket_instances WHERE object_instance_begin is not null

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT * FROM performance_schema.socket_instances WHERE object_instance_begin = @object_instance_begin

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT * FROM performance_schema.socket_instances WHERE object_instance_begin != @object_instance_begin

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT * FROM performance_schema.socket_instances WHERE object_instance_begin = '9999999999'

OK: Handler_read_key

OK: Result set: Empty  Expected: Empty

OK: Extra rows: 0  Missing rows: 0

## Test SOCKET_ID

===== BEGIN TEST =====
========
SELECT * FROM performance_schema.socket_instances WHERE SOCKET_ID is null

OK: Handler_read_key

OK: Result set: Empty  Expected: Empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT * FROM performance_schema.socket_instances WHERE SOCKET_ID is not null

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT * FROM performance_schema.socket_instances WHERE SOCKET_ID = @target_id

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT * FROM performance_schema.socket_instances WHERE SOCKET_ID = @target_id ORDER BY SOCKET_ID

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT * FROM performance_schema.socket_instances WHERE SOCKET_ID = @target_id ORDER BY SOCKET_ID DESC

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT * FROM performance_schema.socket_instances WHERE SOCKET_ID = 9999999 ORDER BY SOCKET_ID

OK: Handler_read_key

OK: Result set: Empty  Expected: Empty

OK: Extra rows: 0  Missing rows: 0

## Test IP, PORT

===== BEGIN TEST =====
========
SELECT * FROM performance_schema.socket_instances WHERE IP is null

OK: Handler_read_key

OK: Result set: Empty  Expected: Empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT * FROM performance_schema.socket_instances WHERE IP is not null

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT * FROM performance_schema.socket_instances WHERE IP = '::' and port is null

OK: Handler_read_key

OK: Result set: Empty  Expected: Empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT * FROM performance_schema.socket_instances WHERE IP = '::' and port is not null

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT * FROM performance_schema.socket_instances WHERE IP = '::'

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT * FROM performance_schema.socket_instances WHERE IP = 'impossible'

OK: Handler_read_key

OK: Result set: Empty  Expected: Empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT * FROM performance_schema.socket_instances WHERE IP = '::' AND port = @target_port

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT * FROM performance_schema.socket_instances WHERE IP = '::' AND port = 9999999

OK: Handler_read_key

OK: Result set: Empty  Expected: Empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT * FROM performance_schema.socket_instances WHERE IP = 'impossible' AND port = @target_port

OK: Handler_read_key

OK: Result set: Empty  Expected: Empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT * FROM performance_schema.socket_instances WHERE port = @target_port

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0

===== CLEAN UP =====
## Default connection
DEALLOCATE PREPARE st1;
DEALLOCATE PREPARE st2;
DEALLOCATE PREPARE st3;
DEALLOCATE PREPARE st4;
DROP PROCEDURE SampleProc1;
DROP PROCEDURE index_test;
DROP TABLE IF EXISTS test.indexes_off;
DROP TABLE IF EXISTS test.indexes_on;
DROP TABLE test.t1;
DROP TABLE test.t2;
DROP TABLE test.t3;
REVOKE ALL PRIVILEGES, GRANT OPTION FROM user1@localhost;
REVOKE ALL PRIVILEGES, GRANT OPTION FROM user2@localhost;
REVOKE ALL PRIVILEGES, GRANT OPTION FROM user3@localhost;
DROP USER user1@localhost;
DROP USER user2@localhost;
DROP USER user3@localhost;
UPDATE performance_schema.threads SET INSTRUMENTED = 'YES';
