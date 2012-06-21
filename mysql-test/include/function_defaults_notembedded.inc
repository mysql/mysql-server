SET TIME_ZONE = "+00:00";

--echo #
--echo # Test of INSERT DELAYED ... SET ...
--echo #

--echo # 2011-04-19 08:02:40 UTC
SET TIMESTAMP = 1303200160.123456;

eval CREATE TABLE t1 ( a INT, b $timestamp NOT NULL DEFAULT CURRENT_$timestamp ON UPDATE CURRENT_$timestamp);

INSERT DELAYED INTO t1 SET a = 1;
FLUSH TABLE t1;

SELECT * FROM t1;
SELECT * FROM t1 WHERE b = 0;

INSERT DELAYED INTO t1 SET a = 2, b = '1980-01-02 10:20:30.405060';
FLUSH TABLE t1;

SELECT * FROM t1;

DROP TABLE t1;

--echo #
--echo # Test of INSERT DELAYED ... VALUES ...
--echo #

--echo # 2011-04-19 08:04:01 UTC
SET TIMESTAMP = 1303200241.234567;

eval CREATE TABLE t1 ( a INT, b $timestamp NOT NULL DEFAULT CURRENT_$timestamp ON UPDATE CURRENT_$timestamp);

INSERT DELAYED INTO t1 ( a ) VALUES (1);
FLUSH TABLE t1;
SELECT * FROM t1;

INSERT DELAYED INTO t1 VALUES (2, '1977-12-19 12:34:56.789123');
FLUSH TABLE t1;
SELECT * FROM t1;

DROP TABLE t1;

--echo #
--echo # Test of a delayed insert handler servicing two insert operations
--echo # with different sets of active defaults.
--echo #
eval CREATE TABLE t1 ( a INT, b $timestamp NOT NULL DEFAULT CURRENT_$timestamp ON UPDATE CURRENT_$timestamp);

--connect(con1, localhost, root,,)
--echo # 2011-04-19 08:04:01 UTC
SET TIMESTAMP = 1303200241.345678;
SET debug_sync = 'before_write_delayed SIGNAL parked WAIT_FOR go';
--send INSERT DELAYED INTO t1 ( a ) VALUES (1), (2), (3)

--connection default
SET debug_sync = 'now WAIT_FOR parked';

--connect(con2, localhost, root,,)
--echo # 2011-04-19 08:04:01 UTC
SET TIME_ZONE="+03:00";
SET TIMESTAMP = 1303200241.456789;
--send INSERT DELAYED INTO t1 ( a, b ) VALUES (4, '1977-12-19 12:34:56.789123'), (5, '1977-12-19 12:34:57.891234'), (6, '1977-12-19 12:34:58.912345')

--connection default
SET debug_sync = 'now SIGNAL go';

--let $wait_condition= SELECT COUNT(*) = 6 FROM t1
--source include/wait_condition.inc

--sorted_result
SELECT * FROM t1;

--disconnect con1
--disconnect con2

DROP TABLE t1;

--echo #
--echo # Test of early activation of function defaults.
--echo #

eval CREATE TABLE t1 ( a INT, b $timestamp NOT NULL DEFAULT CURRENT_$timestamp ON UPDATE CURRENT_$timestamp);

SET TIMESTAMP = 1317235172.987654; # 2011-09-28 18:39:32 UTC
INSERT DELAYED INTO t1 ( a ) VALUES (1), (2), (3);

SET TIMESTAMP = 385503754.876543; # 1982-03-20 20:22:34 UTC
INSERT DELAYED INTO t1 ( a ) VALUES (4), (5), (6);

FLUSH TABLE t1;
SELECT * FROM t1;

DROP TABLE t1;
