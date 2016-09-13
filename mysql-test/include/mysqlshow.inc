# Can't run test of external client with embedded server
-- source include/not_embedded.inc
# Test lists tables in Information_schema, and InnoDB adds some
-- source include/have_innodb.inc
# Don't test when thread_pool active
--source include/not_threadpool.inc

# Test lists tables in Information_schema, and ndb adds some
-- source include/not_ndb_is.inc

--disable_warnings
DROP TABLE IF EXISTS t1,t2,test1,test2;
--enable_warnings

#
## Bug #5036 mysqlshow is missing a column
#
CREATE TABLE t1 (a int);
INSERT INTO t1 VALUES (1),(2),(3);
CREATE TABLE t2 (a int, b int);
show tables;
select "--------------------" as "";
--exec $MYSQL_SHOW test
select "---- -v ------------" as "";
--exec $MYSQL_SHOW test -v
select "---- -v -v ---------" as "";
--exec $MYSQL_SHOW test -v -v
select "----- -t -----------" as "";
--exec $MYSQL_SHOW test -t
select "---- -v -t ---------" as "";
--exec $MYSQL_SHOW test -v -t
select "---- -v -v -t ------" as "";
--exec $MYSQL_SHOW test -v -v -t
DROP TABLE t1, t2;

#
# Bug #19147: mysqlshow INFORMATION_SCHEMA does not work
#
--exec $MYSQL_SHOW information_schema
--exec $MYSQL_SHOW INFORMATION_SCHEMA
--exec $MYSQL_SHOW inf_rmation_schema

#
# WL#3126 TCP address binding for mysql client library;
# - running mysqlshow --protcol=tcp --bind-address=127.0.0.1
#
--exec $MYSQL_SHOW --protocol=tcp --bind-address=127.0.0.1 test

--echo #
--echo # Bug#12688860 : SECURITY RECOMMENDATION: PASSWORDS ON CLI
--echo #

--disable_warnings
DROP DATABASE IF EXISTS b12688860_db;
--enable_warnings

CREATE DATABASE b12688860_db;
CREATE TABLE b12688860_db.b12688860_tab (c1 INT);

--exec $MYSQL_SHOW -uroot --password="" b12688860_db 2>&1

DROP TABLE b12688860_db.b12688860_tab;
DROP DATABASE b12688860_db;

--echo #
--echo # Bug #21246 sql-mode parameter NO_BACKSLASH_ESCAPES
--echo #            breaks parameters with apostrophe's (')
--echo #

--echo # sql_mode = 'NO_BACKSLASH_ESCAPES'
SET @OLD_SQL_MODE = @@SQL_MODE;
SET GLOBAL sql_mode = 'NO_BACKSLASH_ESCAPES';
SHOW GLOBAL VARIABLES LIKE 'sql_mode';

--disable_warnings
DROP TABLE IF EXISTS ```t1`,```t2`;
--enable_warnings

CREATE TABLE ```t1` (a int);
INSERT INTO ```t1` VALUES (1),(2),(3);
CREATE TABLE ```t2` (a int, b int);
show tables;
--exec $MYSQL_SHOW test
--exec $MYSQL_SHOW test "\`t%"

DROP TABLE ```t1`, ```t2`;

--echo # sql_mode = ''
SET GLOBAL sql_mode = '';
SHOW GLOBAL VARIABLES LIKE 'sql_mode';

--disable_warnings
DROP TABLE IF EXISTS ```t1`,```t2`;
--enable_warnings

CREATE TABLE ```t1` (a int);
INSERT INTO ```t1` VALUES (1),(2),(3);
CREATE TABLE ```t2` (a int, b int);
show tables;
--exec $MYSQL_SHOW test
--exec $MYSQL_SHOW test "\`t%"

DROP TABLE ```t1`, ```t2`;
SET GLOBAL sql_mode = @OLD_SQL_MODE;

--echo #
--echo # WL#2284: Increase the length of a user name
--echo #

CREATE USER 'user_with_length_32_abcdefghijkl'@'localhost';
GRANT ALL ON *.* TO 'user_with_length_32_abcdefghijkl'@'localhost';

--exec $MYSQL_SHOW --host=127.0.0.1 -P $MASTER_MYPORT --user=user_with_length_32_abcdefghijkl --protocol=TCP mysql user user

DROP USER 'user_with_length_32_abcdefghijkl'@'localhost';

--echo
--echo End of tests
