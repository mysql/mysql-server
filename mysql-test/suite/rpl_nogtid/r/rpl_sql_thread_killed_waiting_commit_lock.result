include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
call mtr.add_suppression("Query execution was interrupted.");
call mtr.add_suppression("The slave coordinator and worker threads are stopped, possibly leaving data in inconsistent state");
# Let master and slave synced with t1 table created
CREATE TABLE t1 (f INT) ENGINE= INNODB;
include/sync_slave_sql_with_master.inc
# Setup GLOBAL.DEBUG at slave to reach commit
[connection slave]
SET @@GLOBAL.DEBUG= '+d,dbug.reached_commit';
# Do some DML operation on master so that it will be blocked on
# slave as global read lock is in place.
[connection master]
INSERT INTO t1 VALUES (10);
# Issue FLUSH TABLES WITH READ LOCK after Reached is signaled
[connection slave]
SET DEBUG_SYNC='now WAIT_FOR Reached';
FLUSH TABLES WITH READ LOCK;
# Let sql thread continue to try to obtain commit lock
SET DEBUG_SYNC= 'now SIGNAL signal.commit_continue';
# Wait until sql thread enters "Waiting for commit lock" state
# Kill sql thread
include/wait_for_slave_sql_error.inc [errno=1317]
# Restart sql thread to let it finish the replication
SET @@GLOBAL.DEBUG= '-d,dbug.reached_commit';
UNLOCK TABLES;
include/assert.inc [Exec_Master_Log_Pos in SHOW SLAVE STATUS must be equal to mater_log_pos in slave's repository]
include/start_slave_sql.inc
[connection master]
include/sync_slave_sql_with_master.inc
include/diff_tables.inc [master:t1, slave:t1]
[connection master]
DROP TABLE t1;
include/rpl_end.inc
