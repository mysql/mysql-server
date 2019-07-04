#
--source include/have_debug.inc

--echo #
--echo # Bug#28509306 - DIAGNOSTIC AREA NOT POPULATED ON PREPARE STATEMENT ERROR 1615
--echo #
CREATE TABLE t1 (f1 INT);

delimiter |;
CREATE PROCEDURE p1()
BEGIN
  INSERT INTO t1 VALUES(1);
END|
delimiter ;|

PREPARE stmt FROM 'INSERT INTO t1 VALUES (1)';

CALL p1();
EXECUTE stmt;

FLUSH TABLES;
SET DEBUG='+d,simulate_max_reprepare_attempts_hit_case';

--error ER_NEED_REPREPARE
CALL p1();

--echo # Without fix, Sql condition for ER_NEED_REPREPARE error is not pushed to
--echo # the diagnostics area. Hence following SELECT statement returns NULL
--echo # value result set.
--echo # Withfix, Sql condition is pushed and following SELECT statement returns
--echo # expected values.
GET DIAGNOSTICS CONDITION 1 @varErrorMessage = message_text, @varErrorNo = mysql_errno;
SELECT @varErrorMessage, @varErrorNo;

--error ER_NEED_REPREPARE
EXECUTE stmt;

--echo # Without fix, Sql condition for ER_NEED_REPREPARE error is not pushed to
--echo # the diagnostics area. Hence following SELECT statement returns NULL
--echo # value result set.
--echo # Withfix, Sql condition is pushed and following SELECT statement returns
--echo # expected values.
GET DIAGNOSTICS CONDITION 1 @varErrorMessage = message_text, @varErrorNo = mysql_errno;
SELECT @varErrorMessage, @varErrorNo;

DROP TABLE t1;
DROP PROCEDURE p1;
DROP PREPARE stmt;
SET DEBUG='-d,simulate_max_reprepare_attempts_hit_case';
