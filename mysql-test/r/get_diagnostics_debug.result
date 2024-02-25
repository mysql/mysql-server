#
# Bug#28509306 - DIAGNOSTIC AREA NOT POPULATED ON PREPARE STATEMENT ERROR 1615
#
CREATE TABLE t1 (f1 INT);
CREATE PROCEDURE p1()
BEGIN
INSERT INTO t1 VALUES(1);
END|
PREPARE stmt FROM 'INSERT INTO t1 VALUES (1)';
CALL p1();
EXECUTE stmt;
FLUSH TABLES;
SET DEBUG='+d,simulate_max_reprepare_attempts_hit_case';
CALL p1();
ERROR HY000: Prepared statement needs to be re-prepared
# Without fix, Sql condition for ER_NEED_REPREPARE error is not pushed to
# the diagnostics area. Hence following SELECT statement returns NULL
# value result set.
# Withfix, Sql condition is pushed and following SELECT statement returns
# expected values.
GET DIAGNOSTICS CONDITION 1 @varErrorMessage = message_text, @varErrorNo = mysql_errno;
SELECT @varErrorMessage, @varErrorNo;
@varErrorMessage	@varErrorNo
Prepared statement needs to be re-prepared	1615
EXECUTE stmt;
ERROR HY000: Prepared statement needs to be re-prepared
# Without fix, Sql condition for ER_NEED_REPREPARE error is not pushed to
# the diagnostics area. Hence following SELECT statement returns NULL
# value result set.
# Withfix, Sql condition is pushed and following SELECT statement returns
# expected values.
GET DIAGNOSTICS CONDITION 1 @varErrorMessage = message_text, @varErrorNo = mysql_errno;
SELECT @varErrorMessage, @varErrorNo;
@varErrorMessage	@varErrorNo
Prepared statement needs to be re-prepared	1615
DROP TABLE t1;
DROP PROCEDURE p1;
DROP PREPARE stmt;
SET DEBUG='-d,simulate_max_reprepare_attempts_hit_case';
