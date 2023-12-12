--echo # Bug #22477433: TABLE WITH UNKNOWN COLLATION CRASHES MYSQL
--echo #
--echo # Create a table with a certain collation. Restart
--echo # the server with a new --character-sets-dir where
--echo # the collation is not supported. Verify that we
--echo # can do DROP and RENAME TABLE, and that CHECK TABLE
--echo # will report a warning.


--echo #
--echo # New character sets dir:
--replace_result $MYSQL_TEST_DIR MYSQL_TEST_DIR
SHOW VARIABLES LIKE 'character_sets_dir%';

--echo #
--echo # Show new collation available in the new character sets dir:
SHOW COLLATION LIKE 'utf8mb3_phone_ci';

--echo #
--echo # Create two tables using the new collation:
CREATE TABLE t1 (i INTEGER, a VARCHAR(10) COLLATE utf8mb3_phone_ci) COLLATE utf8mb3_phone_ci;
CREATE TABLE t2 (i INTEGER, a VARCHAR(10) COLLATE utf8mb3_phone_ci) COLLATE utf8mb3_phone_ci;

--echo #
--echo # Restart server with original character sets dir:
--let $restart_parameters=restart:--character-sets-dir=$MYSQL_CHARSETSDIR
--replace_result $MYSQL_CHARSETSDIR MYSQL_CHARSETSDIR
--source include/restart_mysqld.inc

--let $old_log_error_verbosity = `select @@global.log_error_verbosity`
SET @@global.log_error_verbosity = 1;

--echo #
--echo # Reverted to old character sets dir:
--replace_result $MYSQL_CHARSETSDIR MYSQL_CHARSETSDIR
SHOW VARIABLES LIKE 'character_sets_dir%';

--echo #
--echo # The newly added collation has been deleted:
SHOW COLLATION LIKE 'utf8mb3_phone_ci';

--echo #
--echo # Check behavior of CHECK TABLE (succeed, but report error):
CHECK TABLE t1;

--echo #
--echo # Check behavior of RENAME TABLE (succeed):
RENAME TABLE t1 TO t1_new;
RENAME TABLE t1_new TO t1;

--echo #
--echo # Check behavior of ALTER TABLE w. COPY (fail):
--error ER_UNKNOWN_COLLATION
ALTER TABLE t1 ADD COLUMN (j INTEGER);

--echo #
--echo # Check behavior of SELECT (fail):
--error ER_UNKNOWN_COLLATION
SELECT * FROM t1;

--echo #
--echo # Check behavior of INSERT (fail):
--error ER_UNKNOWN_COLLATION
INSERT INTO t1 VALUES (1);

--echo #
--echo # Check behavior of SHOW CREATE (fail):
--error ER_UNKNOWN_COLLATION
SHOW CREATE TABLE t1;

--echo #
--echo # Check behavior of DROP TABLE (succeed):
# Note that we introduce the table t2 for this purpose since
# dropping t1 is likely to succeed anyway since the table is
# cached in innodb after being renamed.
DROP TABLE t1;
DROP TABLE t2;

--eval SET @@global.log_error_verbosity= $old_log_error_verbosity

--echo #
--echo # Bug#29904087: WHEN DEFAULT_COLLATION_FOR_UTF8MB4 IS CHANGED,
--echo #               MYSQLPUMP MAY FAIL DUE TO ILLEGAL MIX OF
--echo #               COLLATIONS
--echo #

CREATE TABLE t1 (a CHAR(1)) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;
CREATE TABLE t2 (a CHAR(1)) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;
CREATE FUNCTION f1 (a CHAR(1)) RETURNS CHAR(1) CHARSET utf8mb4 RETURN a;
CREATE VIEW v1 AS SELECT f1(a) AS a FROM t1;
CREATE VIEW v2 AS SELECT 1 FROM v1 JOIN t2 WHERE v1.a = t2.a;

--connect (con1,localhost,root,,test)

SET @@session.default_collation_for_utf8mb4 = utf8mb4_general_ci;
SHOW CREATE VIEW v2;

--connection default
--disconnect con1

#Cleanup
DROP TABLE t1, t2;
DROP VIEW v1, v2;
DROP FUNCTION f1;

# restore default values
--let $restart_parameters = restart:
--source include/restart_mysqld.inc
