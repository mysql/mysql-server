--source include/have_debug.inc

--echo #
--echo # BUG#27899274 - [MYSQL 8.0 GA DEBUG BUILD] ASSERTION `!IS_SET()' FAILED.
--echo #
--echo #

CREATE TABLE test.t1(col INT);
SET SESSION debug= "+d, enable_stack_overrun_post_alter_commit";
# Suppress exact error message as it contains numbers which may not be identical
# across platforms
--error ER_STACK_OVERRUN_NEED_MORE,ER_STACK_OVERRUN_NEED_MORE
ALTER TABLE test.t1 ADD COLUMN col1 CHAR;
SET SESSION debug= "-d, enable_stack_overrun_post_alter_commit";
DROP TABLE test.t1;

--echo #
--echo # Bug#38625494: Incorrect error handling in open_table_uncached()
--echo #

CREATE TABLE t(x INT);
SET debug = '+d,bug38625494';
--error ER_UNKNOWN_ERROR
ALTER TABLE t ADD COLUMN y INT;
SET debug = '-d,bug38625494';
DROP TABLE t;


--echo #
--echo # Bug#36574259: MySQL converts collection of date data type in ibd
--echo # but data dictionary
--echo #
--echo # Verify collation information in DD.

SET SESSION debug= '+d,skip_dd_table_access_check';
CREATE TABLE t1 (
a datetime
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_as_ci;

--echo # Show DD info on newly created table
SELECT CHARACTER_SET_NAME,COLLATION_NAME from information_schema.columns where
TABLE_NAME = 't1';

SELECT t.name, c.name, c.collation_id, co.name FROM mysql.tables t,
mysql.columns c, mysql.collations co
WHERE c.table_id = t.id AND t.name = 't1' AND c.name = 'a' AND c.collation_id
= co.id;
SELECT co.name = 'latin1_swedish_ci' AS EXPECTED_RESULT
FROM mysql.tables t, mysql.columns c, mysql.collations co
WHERE c.table_id = t.id AND t.name = 't1' AND
c.name = 'a' AND c.collation_id = co.id;

--echo # Alter collation
ALTER TABLE t1 CONVERT TO CHARACTER SET utf8mb4 collate utf8mb4_unicode_ci;

--echo # Verify that collation info in DD
SELECT CHARACTER_SET_NAME,COLLATION_NAME from information_schema.columns where
TABLE_NAME = 't1';

SELECT t.name, c.name, c.collation_id, co.name FROM mysql.tables t,
mysql.columns c, mysql.collations co
WHERE c.table_id = t.id AND t.name = 't1' AND c.name = 'a' AND c.collation_id =
co.id;

SELECT co.name = 'latin1_swedish_ci' AS EXPECTED_RESULT
FROM mysql.tables t, mysql.columns c, mysql.collations co
WHERE c.table_id = t.id AND t.name = 't1' AND
c.name = 'a' AND c.collation_id = co.id;

--echo # Alter engine
ALTER TABLE t1 ENGINE = INNODB;

--echo # Verfy that collation info in DD
SELECT CHARACTER_SET_NAME,COLLATION_NAME from information_schema.columns where TABLE_NAME = 'a';

SELECT t.name, c.name, c.collation_id, co.name FROM mysql.tables t,
mysql.columns c, mysql.collations co
WHERE c.table_id = t.id AND t.name = 't1' AND c.name = 'a' AND c.collation_id =
co.id;

SELECT co.name = 'latin1_swedish_ci' AS EXPECTED_RESULT
FROM mysql.tables t, mysql.columns c, mysql.collations co
WHERE c.table_id = t.id AND t.name = 't1' AND
c.name = 'a' AND c.collation_id = co.id;

SET SESSION debug= '-d,skip_dd_table_access_check';
DROP TABLE t1;

