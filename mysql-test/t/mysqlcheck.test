
#
# Clean up after previous tests
#

--disable_warnings
DROP TABLE IF EXISTS t1, `t``1`, `t 1`;
drop view if exists v1;
drop database if exists client_test_db;
# Repair any tables in mysql, sometimes the slow_log is marked as crashed
# after server has been killed
--exec $MYSQL_CHECK --repair  --databases mysql > /dev/null 2>&1
--enable_warnings

--disable_query_log
# Create a dummy ndb_binlog_index in case we run without ndb to get similar results
SET @have_ndb= (select count(engine) from information_schema.engines where engine='ndbcluster');
SET @create_cmd="CREATE TABLE mysql.ndb_binlog_index (i INTEGER PRIMARY KEY)
  ENGINE=INNODB STATS_PERSISTENT=0";
SET @drop_cmd="DROP TABLE mysql.ndb_binlog_index";

SET @create = IF(@have_ndb = 0, @create_cmd, 'SET @dummy = 0');
SET @drop = IF(@have_ndb = 0, @drop_cmd, 'SET @dummy = 0');
PREPARE create_stmt FROM @create;
PREPARE drop_stmt FROM @drop;

EXECUTE create_stmt;
DROP PREPARE create_stmt;
--enable_query_log

#
# Bug #13783  mysqlcheck tries to optimize and analyze information_schema
#
--replace_result 'Table is already up to date' OK
# Filter out ndb_binlog_index to mask differences due to running with or
# without ndb.
--replace_regex /mysql.ndb_binlog_index.*\n//
--exec $MYSQL_CHECK --all-databases --analyze
# Filter out ndb_binlog_index to mask differences due to running with or
# without ndb.
--replace_regex /mysql.ndb_binlog_index.*\n//
--exec $MYSQL_CHECK --all-databases --optimize
--replace_result 'Table is already up to date' OK
# Filter out ndb_binlog_index to mask differences due to running with or
# without ndb.
--replace_regex /mysql.ndb_binlog_index.*\n//
--exec $MYSQL_CHECK --analyze --databases test information_schema mysql
# Filter out ndb_binlog_index to mask differences due to running with or
# without ndb.
--replace_regex /mysql.ndb_binlog_index.*\n//
--exec $MYSQL_CHECK --optimize  --databases test information_schema mysql
--exec $MYSQL_CHECK --analyze information_schema schemata
--exec $MYSQL_CHECK --optimize information_schema schemata

# Drop dummy ndb_binlog_index table
--disable_query_log
EXECUTE drop_stmt;
DROP PREPARE drop_stmt;
--enable_query_log

#
# Bug#39541 CHECK TABLE on information_schema myisam tables produces error
#
create view v1 as select * from information_schema.routines;
check table v1, information_schema.routines;
drop view v1;


--echo End of 5.0 tests


#
# WL#3126 TCP address binding for mysql client library;
# - running mysqlcheck --protcol=tcp --bind-address=127.0.0.1
#
--exec $MYSQL_CHECK --protocol=tcp --bind-address=127.0.0.1 --databases test

--echo End of 5.1 tests

--echo #
--echo # Bug #35269: mysqlcheck behaves different depending on order of parameters
--echo #

--error 1
--exec $MYSQL_CHECK -aoc test "#mysql50#t1-1"


--echo #
--echo # Bug#12688860 : SECURITY RECOMMENDATION: PASSWORDS ON CLI
--echo #

--disable_warnings
DROP DATABASE IF EXISTS b12688860_db;
--enable_warnings

CREATE DATABASE b12688860_db;
--exec $MYSQL_CHECK -uroot --password="" b12688860_db 2>&1
DROP DATABASE b12688860_db;

--echo #
--echo # WL#2284: Increase the length of a user name
--echo #

CREATE USER 'user_with_length_32_abcdefghijkl'@'localhost';
GRANT ALL ON *.* TO 'user_with_length_32_abcdefghijkl'@'localhost';

--exec $MYSQL_CHECK --host=127.0.0.1 -P $MASTER_MYPORT --user=user_with_length_32_abcdefghijkl --protocol=TCP mysql user

DROP USER 'user_with_length_32_abcdefghijkl'@'localhost';

--echo #
--echo # WL#13292: Deprecate legacy connection compression parameters
--echo #

--echo # exec mysqlcheck --compress: must have a deprecation warning
--exec $MYSQL_CHECK --compress --databases test 2>&1
--echo # exec mysqlcheck --compression-algorithms: must not have a deprecation warning
--exec $MYSQL_CHECK --compression-algorithms=zlib,uncompressed --databases test 2>&1


--echo
--echo End of tests
