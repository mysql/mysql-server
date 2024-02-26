#
# WL#13447: Allow mysqldump to set long_query_time when running
#
SET @my_slow_query_log_file = @@GLOBAL.slow_query_log_file;
SET @my_slow_query_log = @@GLOBAL.slow_query_log;
SET GLOBAL slow_query_log_file = "mysqldump_long_query_time-slow.log";
SET GLOBAL slow_query_log = ON;
CREATE DATABASE mysqldump_long_query_time;
USE mysqldump_long_query_time;

## TEST 1: mysqldump with custom flag set to high value
# Check the slow log result. We shouldn't find any query.

## TEST 2: mysqldump with custom flag omitted, global set to 0
SET @my_long_query_time = @@GLOBAL.long_query_time;
SET GLOBAL long_query_time = 0;
CREATE TABLE t1 (i int, c char(255));
INSERT INTO t1 VALUES (0, lpad('a', 250, 'b'));
INSERT INTO t1 SELECT i+1,c FROM t1;
INSERT INTO t1 SELECT i+2,c FROM t1;
INSERT INTO t1 SELECT i+4,c FROM t1;
INSERT INTO t1 SELECT i+8,c FROM t1;
INSERT INTO t1 SELECT i+16,c FROM t1;
# Check the slow log result. One "select" query should be found.
select @@collation_database;

## TEST 3: mysqldump with custom flag set to 0, global set to default
SET @@GLOBAL.long_query_time = @my_long_query_time;

# Check the slow log result.
# Results should equal to previous test, one additional "select" query should appear.
select @@collation_database;
select @@collation_database;

# Cleanup
DROP DATABASE mysqldump_long_query_time;
SET @@GLOBAL.slow_query_log_file = @my_slow_query_log_file;
SET @@GLOBAL.slow_query_log = @my_slow_query_log;
SET @@GLOBAL.long_query_time = @my_long_query_time;
