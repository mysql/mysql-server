--echo #
--echo # Bug#26091333 : ASSERTION `RC == TYPE_OK' FAILED
--echo #

SET timestamp=1000;
SET time_zone='+00:00';
CREATE EVENT event1 ON SCHEDULE EVERY 15 MINUTE STARTS NOW() DO BEGIN END;
DROP EVENT event1;

--source include/count_sessions.inc
connect (con1,localhost,root,,);
SET timestamp=1000;
SET time_zone='+00:00';

connect (con2,localhost,root,,);
SET timestamp=1000;
SET time_zone='+05:30';

connection con1;
CREATE EVENT event1 ON SCHEDULE EVERY 15 MINUTE DO BEGIN END;
--replace_column 19 #
SELECT * FROM INFORMATION_SCHEMA.EVENTS WHERE EVENT_NAME='event1';
CREATE VIEW v1 AS SELECT 1;
SELECT * FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME='v1';
CREATE TABLE t1(a int);
SELECT * FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME='t1';
CREATE FUNCTION hello (s CHAR(20)) RETURNS CHAR(50) DETERMINISTIC RETURN CONCAT('Hello, ',s,'!');
SELECT * FROM INFORMATION_SCHEMA.ROUTINES WHERE ROUTINE_NAME='hello';

disconnect con1;

connection con2;
ALTER EVENT event1 RENAME TO event2;
--replace_column 19 #
SELECT * FROM INFORMATION_SCHEMA.EVENTS WHERE EVENT_NAME='event2';
DROP event event2;
ALTER VIEW v1 AS SELECT 2;
SELECT * FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME='v1';
DROP VIEW v1;
RENAME TABLE t1 TO t2;
SELECT * FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME='t2';
DROP TABLE t2;
ALTER FUNCTION hello comment 'abcd';
SELECT * FROM INFORMATION_SCHEMA.ROUTINES WHERE ROUTINE_NAME='hello';
DROP FUNCTION hello;

disconnect con2;

connection default;

--source include/wait_until_count_sessions.inc

--echo #
--echo # Bug#28492272: Cases where combination of timestamp and time zone
--echo # would result in conversion error when storing timestamp column in DD.
--echo #

--echo # Test case 1: Create table referenced in view when
--echo # time_zone=-6:00 and timestamp=1235;
CREATE TABLE t1(i INT);
CREATE VIEW v1 AS SELECT * FROM t1;
DROP TABLE t1;

SET TIME_ZONE='-6:00';
SET TIMESTAMP=1235;

--echo # Creating a table referenced in an existing view when
--echo # timstamp+time zone is before beginning of epoch must not trigger
--echo # assert.
CREATE TABLE t1(i INT);
SET TIMESTAMP=default;
SET TIME_ZONE=default;
DROP TABLE t1;
DROP VIEW v1;

--echo # Test case 2: Altering view when timestamp=1235 and time_zone=-6:00
CREATE VIEW v1 AS SELECT 5;

SET TIME_ZONE='-6:00';
SET TIMESTAMP=1235;

--echo # Altering a view when timstamp+time zone is before beginning of epoch
--echo # must not trigger assert.
ALTER VIEW v1 AS SELECT 6;
SET TIMESTAMP=default;
SET TIME_ZONE=default;
DROP VIEW v1;

--echo # Test case 3: Altering function when timestamp=1 and time_zone=-12:00
CREATE FUNCTION hello (s CHAR(20)) RETURNS CHAR(50) DETERMINISTIC
RETURN CONCAT('Hello, ',s,'!');
SET TIME_ZONE='-12:00';
SET TIMESTAMP=1;

--echo # Altering a function when timstamp+time zone is before beginning of
--echo # epoch must not trigger assert.
ALTER FUNCTION hello COMMENT 'This is a test';
SET TIME_ZONE= default;
SET TIMESTAMP= default;
DROP FUNCTION hello;
