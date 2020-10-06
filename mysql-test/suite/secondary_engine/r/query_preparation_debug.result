CREATE TABLE t(x INT) SECONDARY_ENGINE MOCK;
INSERT INTO t VALUES (1);
ALTER TABLE t SECONDARY_LOAD;
FLUSH STATUS;
PREPARE ps FROM 'SELECT * FROM t';
EXECUTE ps;
x
SHOW SESSION STATUS LIKE 'Secondary_engine_execution_count';
Variable_name	Value
Secondary_engine_execution_count	1
FLUSH STATUS;
SET DEBUG = '+d,secondary_engine_mock_prepare_error';
EXECUTE ps;
x
1
SET DEBUG = '-d,secondary_engine_mock_prepare_error';
SHOW SESSION STATUS LIKE 'Secondary_engine_execution_count';
Variable_name	Value
Secondary_engine_execution_count	0
FLUSH STATUS;
EXECUTE ps;
x
1
SHOW SESSION STATUS LIKE 'Secondary_engine_execution_count';
Variable_name	Value
Secondary_engine_execution_count	0
DROP TABLE t;
#
# WL#12389: Add interface for optimizing queries in secondary engines
#
CREATE TABLE t1(x INT) SECONDARY_ENGINE MOCK;
INSERT INTO t1 VALUES (1), (2), (3);
CREATE TABLE t2(y INT) SECONDARY_ENGINE MOCK;
INSERT INTO t2 VALUES (3), (4), (5);
ALTER TABLE t1 SECONDARY_LOAD;
ALTER TABLE t2 SECONDARY_LOAD;
ANALYZE TABLE t1, t2;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
test.t2	analyze	status	OK
SET DEBUG = "+d,secondary_engine_mock_optimize_error";
FLUSH STATUS;
SELECT * FROM t1;
x
1
2
3
SELECT /*+ SET_VAR(use_secondary_engine = FORCED) */ x FROM t1;
ERROR HY000: Secondary engine operation failed. use_secondary_engine is FORCED but query could not be executed in secondary engine.
SELECT (SELECT y FROM t2 WHERE y = 1) FROM t1;
(SELECT y FROM t2 WHERE y = 1)
NULL
NULL
NULL
SELECT * FROM t1 UNION ALL SELECT * FROM t2;
x
1
2
3
3
4
5
SELECT * FROM t1 UNION DISTINCT SELECT * FROM t2;
x
1
2
3
4
5
SET DEBUG = "-d,secondary_engine_mock_optimize_error";
SHOW STATUS LIKE 'Secondary_engine_execution_count';
Variable_name	Value
Secondary_engine_execution_count	0
FLUSH STATUS;
SELECT * FROM t1;
x
SELECT (SELECT y FROM t2 WHERE y = 1) FROM t1;
(SELECT y FROM t2 WHERE y = 1)
SELECT * FROM t1 UNION ALL SELECT * FROM t2;
x
SELECT * FROM t1 UNION DISTINCT SELECT * FROM t2;
x
SHOW STATUS LIKE 'Secondary_engine_execution_count';
Variable_name	Value
Secondary_engine_execution_count	4
DROP TABLE t1, t2;
CREATE TABLE t(x int) SECONDARY_ENGINE MOCK;
INSERT INTO t VALUES (1), (2), (3);
ALTER TABLE t SECONDARY_LOAD;
SET DEBUG_SYNC = 'before_mock_optimize SIGNAL mock WAIT_FOR continue';
SELECT * FROM t;
SET DEBUG_SYNC = 'now WAIT_FOR mock';
SELECT ID FROM INFORMATION_SCHEMA.PROCESSLIST
WHERE INFO = 'SELECT * FROM t' INTO @thread_id;
KILL QUERY @thread_id;
SET DEBUG_SYNC = 'now SIGNAL continue';
ERROR 70100: Query execution was interrupted
DROP TABLE t;
#
# Bug#29615190: SELECTS ON TABLES OFFLOADED TO SECONDARY ENGINE ARE
#               WRITTEN TWICE TO THE MYSQLD GENERAL LOG
#
CREATE TABLE t(x INT) SECONDARY_ENGINE MOCK;
ALTER TABLE t SECONDARY_LOAD;
SET @saved_general_log = @@global.general_log;
SET @saved_log_output = @@global.log_output;
SET GLOBAL general_log = 1;
SET GLOBAL log_output = 'table';
SET DEBUG = '+d,secondary_engine_mock_optimize_error';
TRUNCATE TABLE mysql.general_log;
SELECT * FROM t;
x
statement_count
1
TRUNCATE TABLE mysql.general_log;
PREPARE ps FROM 'SELECT * FROM t';
SELECT COUNT(*) FROM mysql.general_log WHERE argument = 'SELECT * FROM t';
COUNT(*)
1
EXECUTE ps;
x
SELECT COUNT(*) FROM mysql.general_log WHERE argument = 'SELECT * FROM t';
COUNT(*)
2
DROP PREPARE ps;
SET GLOBAL general_log = @saved_general_log;
SET GLOBAL log_output = @saved_log_output;
SET DEBUG = '-d,secondary_engine_mock_optimize_error';
TRUNCATE TABLE mysql.general_log;
DROP TABLE t;
