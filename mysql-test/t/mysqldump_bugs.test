--source include/no_valgrind_without_big.inc
# This test doesn't support GTID
--source include/rpl/set_gtid_mode_off.inc

# Save the initial number of concurrent sessions
--source include/count_sessions.inc


--echo #
--echo # Bug #28380961: CONTRIBUTION BY FACEBOOK: ADD MY_APPEND_EXT FLAG
--echo #  WHEN DETERMINING FILE NAME ...
--echo #

--echo # setup
CREATE TABLE `b28380961.p2` (a INT);
INSERT INTO `b28380961.p2` VALUES (1),(2),(3),(4);

--echo # test
--exec $MYSQL_DUMP --skip-comments --tab=$MYSQLTEST_VARDIR/tmp/ test
--echo # success if files are present
--echo # Dump the sql file
--cat_file $MYSQLTEST_VARDIR/tmp/b28380961.p2.sql
--echo # Dump the txt file
--cat_file $MYSQLTEST_VARDIR/tmp/b28380961.p2.txt
--echo # remove the sql file
--remove_file $MYSQLTEST_VARDIR/tmp/b28380961.p2.sql
--echo # remove the txt file
--remove_file $MYSQLTEST_VARDIR/tmp/b28380961.p2.txt

--echo # cleanup
DROP TABLE `b28380961.p2`;


--echo #
--echo # Bug #28373001: MYSQLDUMP --SKIP-CREATE-OPTIONS FAILS ON MYSQL 8.0 SERVER
--echo #

--echo # Put some data in
CREATE TABLE b28373001 (a INT);
INSERT INTO b28373001 VALUES (1), (2), (3), (4);

--echo # Tes: must pass
--exec $MYSQL_DUMP --skip-create-options --compact test b28373001

--echo # Cleanup
DROP TABLE b28373001;

--echo #
--echo # Bug #32141046: MYSQLDUMP SHOULD DUMP USERS REFERENCED IN DEFINER
--echo #                CLAUSE OF STORED PROGRAMS.
--echo #

CREATE DATABASE IF NOT EXISTS db1;
CREATE DATABASE IF NOT EXISTS db2;
CREATE DATABASE IF NOT EXISTS db3;
CREATE DATABASE IF NOT EXISTS db4;
CREATE DATABASE IF NOT EXISTS mysqj;
CREATE DATABASE IF NOT EXISTS mysqk;
CREATE DATABASE IF NOT EXISTS mysqll;

CREATE USER IF NOT EXISTS view_u1;
CREATE USER IF NOT EXISTS view_u2@my.oracle.com;

CREATE USER IF NOT EXISTS event_u1;
CREATE USER IF NOT EXISTS event_u2@192.1.1.140;

CREATE USER IF NOT EXISTS trig_u1;
CREATE USER IF NOT EXISTS trig_u2@xyz.com;

CREATE USER IF NOT EXISTS proc_u1;
CREATE USER IF NOT EXISTS proc_u2@localhost;

CREATE USER IF NOT EXISTS func_u1;
CREATE USER IF NOT EXISTS func_u2;

CREATE USER IF NOT EXISTS user_not_referenced_as_definer;

USE db1;
CREATE TABLE t1 (i int not null, j int);

CREATE DEFINER=view_u1 VIEW v1 AS SELECT * FROM t1;
CREATE DEFINER=view_u1 VIEW v2 AS SELECT * FROM t1;
CREATE DEFINER=view_u2@my.oracle.com VIEW v3 AS SELECT * FROM t1;
CREATE DEFINER=view_u2@my.oracle.com VIEW v4 AS SELECT * FROM t1;

CREATE DEFINER=event_u1 EVENT e1 ON SCHEDULE AT '2037-01-01 00:00:00' DISABLE DO SET @a = 5;
CREATE DEFINER=event_u1 EVENT e2 ON SCHEDULE AT '2037-01-01 00:00:00' DISABLE DO SET @a = 5;
CREATE DEFINER=event_u2@192.1.1.140 EVENT e3 ON SCHEDULE AT '2037-01-01 00:00:00' DISABLE DO SET @a = 5;
CREATE DEFINER=event_u2@192.1.1.140 EVENT e4 ON SCHEDULE AT '2037-01-01 00:00:00' DISABLE DO SET @a = 5;

CREATE DEFINER=trig_u1 TRIGGER trig1 BEFORE INSERT ON t1 FOR EACH ROW SET @sum = 1;
CREATE DEFINER=trig_u1 TRIGGER trig2 BEFORE INSERT ON t1 FOR EACH ROW SET @sum = 1;
CREATE DEFINER=trig_u2@xyz.com TRIGGER trig3 BEFORE INSERT ON t1 FOR EACH ROW SET @sum = 1;
CREATE DEFINER=trig_u2@xyz.com TRIGGER trig4 BEFORE INSERT ON t1 FOR EACH ROW SET @sum = 1;

CREATE DEFINER=proc_u1 PROCEDURE p1() INSERT INTO t1 VALUES(10);
CREATE DEFINER=proc_u1 PROCEDURE p2() INSERT INTO t1 VALUES(10);
CREATE DEFINER=proc_u2@localhost PROCEDURE p3() INSERT INTO t1 VALUES(10);
CREATE DEFINER=proc_u2@localhost PROCEDURE p4() INSERT INTO t1 VALUES(10);

CREATE DEFINER=func_u1 FUNCTION f1 (s CHAR(20)) RETURNS CHAR(50) DETERMINISTIC RETURN CONCAT('Hello, ',s,'!');
CREATE DEFINER=func_u1 FUNCTION f2 (s CHAR(20)) RETURNS CHAR(50) DETERMINISTIC RETURN CONCAT('Hello, ',s,'!');
CREATE DEFINER=func_u2 FUNCTION f3 (s CHAR(20)) RETURNS CHAR(50) DETERMINISTIC RETURN CONCAT('Hello, ',s,'!');
CREATE DEFINER=func_u2 FUNCTION f4 (s CHAR(20)) RETURNS CHAR(50) DETERMINISTIC RETURN CONCAT('Hello, ',s,'!');

use db2;
CREATE TABLE t1 (i int not null, j int);

--echo # ensure that mysql database is dumped first before any other database is dumped.
--exec $MYSQL_DUMP --all-databases --skip-comments --no-create-info --no-data --skip-triggers

#cleanup
DROP DATABASE IF EXISTS db1;
DROP DATABASE IF EXISTS db2;
DROP DATABASE IF EXISTS db3;
DROP DATABASE IF EXISTS db4;
DROP DATABASE IF EXISTS mysqj;
DROP DATABASE IF EXISTS mysqk;
DROP DATABASE IF EXISTS mysqll;
DROP USER view_u1, view_u2@my.oracle.com, event_u1, event_u2@192.1.1.140, trig_u1, trig_u2@xyz.com,
          proc_u1, proc_u2@localhost, func_u1, func_u2, user_not_referenced_as_definer;
USE test;

--echo #
--echo # WL#13292: Deprecate legacy connection compression parameters
--echo #

USE test;
CREATE TABLE wl13292(a INT PRIMARY KEY);

--echo # exec mysqldump --compress: must have a deprecation warning
--exec $MYSQL_DUMP --compress --skip-create-options --compact test wl13292 2>&1
--echo # exec mysqldump --compression-algorithms: must not have a deprecation warning
--exec $MYSQL_DUMP --compression-algorithms=zlib,uncompressed --skip-create-options --compact test wl13292 2>&1

DROP TABLE wl13292;

--echo #
--echo # Bug#35208605: Mysqldump table with virtual column wrong syntax
--echo #

CREATE DATABASE b35208605;
USE b35208605;
CREATE TABLE x1 (c0 BLOB AS ('a') VIRTUAL, c1 INT);
INSERT INTO x1(c1) VALUES (1);
CREATE TABLE x2 (c1 INT, c0 BLOB AS ('a') VIRTUAL);
INSERT INTO x2(c1) VALUES (1);
CREATE TABLE x3 (c0 BLOB AS ('a') VIRTUAL INVISIBLE, c1 INT);
INSERT INTO x3(c1) VALUES (1);
CREATE TABLE x4 (c1 INT, c0 BLOB AS ('a') VIRTUAL INVISIBLE);
INSERT INTO x4(c1) VALUES (1);
CREATE TABLE x5 (c0 BLOB AS ('a') STORED, c1 INT);
INSERT INTO x5(c1) VALUES (1);
CREATE TABLE x6 (c1 INT, c0 BLOB AS ('a') STORED);
INSERT INTO x6(c1) VALUES (1);
CREATE TABLE x7 (c0 BLOB AS ('a') STORED INVISIBLE, c1 INT);
INSERT INTO x7(c1) VALUES (1);
CREATE TABLE x8 (c1 INT, c0 BLOB AS ('a') STORED INVISIBLE);
INSERT INTO x8(c1) VALUES (1);

--echo # Test: all INSERTs must be valid SQL
--exec $MYSQL_DUMP --skip-comments --compact --result-file=$MYSQLTEST_VARDIR/tmp/b35208605.sql b35208605 2>&1
DROP TABLE x1,x2,x3,x4,x5,x6,x7,x8;
--echo # Test: Must apply OK
--exec $MYSQL b35208605 < $MYSQLTEST_VARDIR/tmp/b35208605.sql 2>&1

DROP TABLE x1,x2,x3,x4,x5,x6,x7,x8;
USE test;
DROP DATABASE b35208605;
remove_file $MYSQLTEST_VARDIR/tmp/b35208605.sql;

--echo #
--echo # Bug #35205310: mysqldump SEGV
--echo #

CREATE SCHEMA B35205310;
USE B35205310;
CREATE TABLE t1 (c0 CHAR);
CREATE UNIQUE INDEX i1 ON t1 (c0, (TIME '-1:0:0'));
USE test;

--echo # test: must recognize a functional index and use expression
--exec $MYSQL_DUMP --order-by-primary --skip-comments --compact --no-create-info --databases B35205310 2>&1

DROP TABLE B35205310.t1;
DROP DATABASE B35205310;

--echo # End of 8.0 tests

--echo #
--echo # Bug#38284832: mysqldump --order-by-primary is sorting by
--echo #   all table indices instead of just the primary key
--echo #

CREATE DATABASE B38284832;

CREATE TABLE B38284832.foo (
  `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `col1` INT UNSIGNED NOT NULL,
  `col2` INT UNSIGNED NOT NULL,
  `data` JSON DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_col1` (`col1`),
  KEY `idx_col2` (`col2`)
);

INSERT INTO B38284832.foo (`col1`, `col2`, `data`)
  VALUES (111, 222, JSON_OBJECT('data', REPEAT('LotsOfData', 250000)));
--echo # Test: should not produce an execution error
--exec $MYSQL_DUMP --order-by-primary --result-file=$MYSQLTEST_VARDIR/tmp/b38284832.sql B38284832 2>&1

DROP DATABASE B38284832;
--remove_file $MYSQLTEST_VARDIR/tmp/b38284832.sql


--echo # End of 8.4 tests

# Wait till we reached the initial number of concurrent sessions
--source include/wait_until_count_sessions.inc


--echo #
--echo # Bug #37353658: Contribution: Remove whitespace between INSERT/REPLACE and IGNORE
--echo #

CREATE DATABASE B37353658;
CREATE TABLE B37353658.t1 (a INT);
INSERT INTO B37353658.t1 VALUES (1),(2),(3),(4);

--echo # Test should not have double space between INSERT and IGNORE
--exec $MYSQL_DUMP --skip-comments --compact --no-create-info --insert-ignore B37353658 t1

DROP DATABASE B37353658;

--echo #End of 9.3 tests

--echo #
--echo # Bug#22240504: MYSQLDUMP --ROUTINES MISQUOTES DATABASE NAMES
--echo #

CREATE DATABASE `B\\22240504`;
CREATE EVENT `B\\22240504`.inner_b22240404 ON SCHEDULE EVERY 1 DAY DO SELECT 1;
CREATE EVENT outer_b22240404 ON SCHEDULE EVERY 1 DAY DO SELECT 1;

--echo # Test dump routines should not fail on USE
--exec $MYSQL_DUMP --skip-comments --compact --routines B\\\\22240504

--echo # Test dump events should not fail on USE
--exec $MYSQL_DUMP --skip-comments --compact --events B\\\\22240504 > $MYSQLTEST_VARDIR/tmp/b22240504_events.sql

--let $assert_text=check for inner_b22240404 present
--let $assert_file=$MYSQLTEST_VARDIR/tmp/b22240504_events.sql
--let $assert_select=inner_b22240404
--let $assert_count=1
--source include/assert_grep.inc

--let $assert_text=check for outer_b22240404 absent
--let $assert_file=$MYSQLTEST_VARDIR/tmp/b22240504_events.sql
--let $assert_select=outer_b22240404
--let $assert_count=0
--source include/assert_grep.inc

--remove_file $MYSQLTEST_VARDIR/tmp/b22240504_events.sql

# Cleanup
DROP EVENT `B\\22240504`.inner_b22240404;
DROP EVENT outer_b22240404;
DROP DATABASE `B\\22240504`;

# Restore the default for gtid_mode which is ON
--source include/rpl/set_gtid_mode_on.inc

