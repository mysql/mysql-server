include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CALL mtr.add_suppression("Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT");
call mtr.add_suppression("Slave SQL.*Request to stop slave SQL Thread received while applying a group that has non-transactional changes; waiting for completion of the group");
include/stop_slave.inc
SET @old_log_output= @@log_output;
SET GLOBAL log_output= 'TABLE';
SET @old_long_query_time= @@long_query_time;
SET GLOBAL long_query_time= 2;
TRUNCATE mysql.slow_log;
include/start_slave.inc
CREATE TABLE t1 (a int, b int);
INSERT INTO t1 values(1, 1);
INSERT INTO t1 values(1, sleep(3));
include/sync_slave_sql_with_master.inc
TRUNCATE mysql.slow_log;
SELECT 1, sleep(3);
1	sleep(3)
1	0
SELECT 1;
1
1
TRUNCATE mysql.slow_log;
SET TIMESTAMP= 1;
SELECT 2, sleep(3);
2	sleep(3)
2	0
SELECT 2;
2
2
TRUNCATE mysql.slow_log;
SET @old_slow_query_log= @@slow_query_log;
SET GLOBAL slow_query_log= 'OFF';
SELECT 3, sleep(3);
3	sleep(3)
3	0
SELECT 3;
3
3
TRUNCATE mysql.slow_log;
SET GLOBAL slow_query_log= @old_slow_query_log;
DROP TABLE t1;
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
SET GLOBAL long_query_time= @old_long_query_time;
SET GLOBAL log_output= @old_log_output;
include/start_slave.inc
include/rpl_reset.inc
SET @old_log_output= @@log_output;
SET GLOBAL log_output= 'TABLE';
SET GLOBAL long_query_time= 2;
SET @old_long_query_time= @@long_query_time;
SET SESSION long_query_time= 2;
TRUNCATE mysql.slow_log;
include/stop_slave.inc
SET @old_log_output= @@log_output;
SET GLOBAL log_output= 'TABLE';
SET @old_long_query_time= @@long_query_time;
SET GLOBAL long_query_time= 2;
TRUNCATE mysql.slow_log;
include/start_slave.inc
CREATE TABLE t1 (a int, b int);
********************************************************************
**** INSERT one row that exceeds long_query_time
**** Outcome: query ends up in both master and slave slow log
********************************************************************
INSERT INTO t1 values(1, sleep(3));
include/sync_slave_sql_with_master.inc
### Assertion is good. Both Master and Slave exhibit the 
### same number of queries in slow log: 1
TRUNCATE mysql.slow_log;
TRUNCATE mysql.slow_log;
********************************************************************
**** Now do inserts again, but first add an index to the table.
**** Outcome: Note that the slave contains the same one entry (as 
****          the master does)  whereas before the patch it did not.
********************************************************************
ALTER TABLE t1 ADD INDEX id1(a);
INSERT INTO t1 values(1, sleep(3));
include/sync_slave_sql_with_master.inc
### Assertion is good. Both Master and Slave exhibit the 
### same number of queries in slow log: 1
********************************************************************
**** TRUNCATE the slow log then check whether runtime changes of
**** log_slow_replica_statements work without slave restart.
********************************************************************
SET @old_log_slow_replica_statements= @@global.log_slow_replica_statements;
SET @@global.log_slow_replica_statements = off;
TRUNCATE mysql.slow_log;
INSERT INTO t1 values(1, sleep(3));;
include/sync_slave_sql_with_master.inc
SET @@global.log_slow_replica_statements = on;
INSERT INTO t1 values(1, sleep(3));;
include/sync_slave_sql_with_master.inc
SET @@global.log_output= @old_log_output;
SET @@global.long_query_time= @old_long_query_time;
TRUNCATE TABLE mysql.slow_log;
TRUNCATE TABLE mysql.general_log;
DROP TABLE t1;
include/sync_slave_sql_with_master.inc
SET @@global.log_output= @old_log_output;
TRUNCATE TABLE mysql.general_log;
SET @@global.long_query_time= @old_long_query_time;
SET @@global.log_slow_replica_statements= @old_log_slow_replica_statements;
include/rpl_end.inc
