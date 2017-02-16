connection master;

CREATE TABLE t(id INT);

connection slave;
SET GLOBAL query_exec_time = 0.1;
--source include/restart_slave_sql.inc

connection slave;

SET SESSION query_exec_time=0.1;

SET GLOBAL QUERY_RESPONSE_TIME_STATS=0;
--eval SET GLOBAL QUERY_RESPONSE_TIME_RANGE_BASE=$base
FLUSH QUERY_RESPONSE_TIME;
SET GLOBAL QUERY_RESPONSE_TIME_STATS=1;

connection master;

SET SESSION query_exec_time = 0.31; INSERT INTO t VALUES(1);
SET SESSION query_exec_time = 0.32; INSERT INTO t VALUES(1);
SET SESSION query_exec_time = 0.33; INSERT INTO t VALUES(1);
SET SESSION query_exec_time = 0.34; INSERT INTO t VALUES(1);
SET SESSION query_exec_time = 0.35; INSERT INTO t VALUES(1);
SET SESSION query_exec_time = 0.36; INSERT INTO t VALUES(1);
SET SESSION query_exec_time = 0.37; INSERT INTO t VALUES(1);
SET SESSION query_exec_time = 0.38; INSERT INTO t VALUES(1);
SET SESSION query_exec_time = 0.39; INSERT INTO t VALUES(1);
SET SESSION query_exec_time = 0.4; INSERT INTO t VALUES(1);
SET SESSION query_exec_time = 1.1; INSERT INTO t VALUES(1);
SET SESSION query_exec_time = 1.2; INSERT INTO t VALUES(1);
SET SESSION query_exec_time = 1.3; INSERT INTO t VALUES(1);
SET SESSION query_exec_time = 1.5; INSERT INTO t VALUES(1);
SET SESSION query_exec_time = 1.4; INSERT INTO t VALUES(1);
SET SESSION query_exec_time = 0.5; INSERT INTO t VALUES(1);
SET SESSION query_exec_time = 2.1; INSERT INTO t VALUES(1);
SET SESSION query_exec_time = 2.3; INSERT INTO t VALUES(1);
SET SESSION query_exec_time = 2.5; INSERT INTO t VALUES(1);
SET SESSION query_exec_time = 3.1; INSERT INTO t VALUES(1);
SET SESSION query_exec_time = 4.1; INSERT INTO t VALUES(1);
SET SESSION query_exec_time = 5.1; INSERT INTO t VALUES(1);

sync_slave_with_master;

connection slave;
SET GLOBAL QUERY_RESPONSE_TIME_STATS=0;
SHOW GLOBAL VARIABLES where Variable_name like 'QUERY_RESPONSE_TIME_RANGE_BASE';
SHOW QUERY_RESPONSE_TIME;
SELECT * FROM INFORMATION_SCHEMA.QUERY_RESPONSE_TIME;

SET GLOBAL QUERY_RESPONSE_TIME_RANGE_BASE=default;
SET GLOBAL QUERY_RESPONSE_TIME_STATS=default;

connection master;
DROP TABLE t;

sync_slave_with_master;
