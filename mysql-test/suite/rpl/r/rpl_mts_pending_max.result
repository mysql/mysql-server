include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
#
#   0) Set SLAVE_PENDING_JOBS_SIZE_MAX to a smaller number (1024)
#      and slave_parallel_workers to 2 to make the test easy.
#
[connection slave]
SET @save.slave_pending_jobs_size_max= @@global.slave_pending_jobs_size_max;
SET @save.slave_parallel_workers= @@global.slave_parallel_workers;
SET @@GLOBAL.slave_pending_jobs_size_max= 1024;
SET @@GLOBAL.slave_parallel_workers= 2;
include/start_slave.inc
#
#
#   1) Create initial data required for the test
#      (two databases (db1, db2) and two tables (db1.t and db2.t)).
#
[connection master]
CREATE DATABASE db1;
CREATE DATABASE db2;
CREATE TABLE db1.t (a TEXT) ENGINE=INNODB;
CREATE TABLE db2.t (a TEXT) ENGINE=INNODB;
include/sync_slave_sql_with_master.inc
#
#   2) Prove that Coordinator will make a big event (bigger in size than
#      SLAVE_PENDING_JOBS_SIZE_MAX) wait for all workers to finish
#      their work (empty their queue) before processing the big event.
#
#
#  1) On Slave, lock one table so that any operation on that
#     will be waiting for the lock to be released.
#
[connection slave1]
LOCK TABLE db1.t WRITE;
#
#  2) Execute query that is going to wait for the table lock.
#
[connection master]
INSERT INTO db1.t VALUES ('small event');
#
#  3) Wait on Slave till a worker picks this event and wait for the
#     lock (which is acquired in step 1)
#
[connection slave]
#
#  4) Now on Master, insert another query that reaches slave.
#
[connection master]
INSERT INTO db2.t VALUES (REPEAT('big event', 1024));
#
#  5) Check that Coordinator waits for the query (at step 2) to be
#     executed before dedicating this new query (at step 4) to
#     one of the workers because of the event size limits.
#
[connection slave]
include/assert.inc [Check that one of the applier worker thread is waiting for the table metadata lock.]
#
#  6) Release the lock acquired in step 1, so that first query will
#     continue it's work and once it is done, second big event
#     will also continue it's work.
#
[connection slave1]
UNLOCK TABLES;
#
#  7) check that slave is able to catch up with master after releasing the
#     lock in step 6.
#
#     7.1) Sync SQL thread with Master.
#
[connection master]
include/sync_slave_sql_with_master.inc
#
#     7.2) Diff all the tables involved in the test to prove
#          that replication worked fine.
#
include/diff_tables.inc [master:db1.t,slave:db1.t]
include/diff_tables.inc [master: db2.t,slave: db2.t]
#
#   3) When a big event is being processed by a worker,
#      Coordinator will make smaller events to wait until the big event
#      is executed completely.
#
#
#
#  1) On Slave, lock one table so that any operation on that
#     will be waiting for the lock to be released.
#
[connection slave1]
LOCK TABLE db1.t WRITE;
#
#  2) Execute query that is going to wait for the table lock.
#
[connection master]
INSERT INTO db1.t VALUES (REPEAT('big event', 1024));
#
#  3) Wait on Slave till a worker picks this event and wait for the
#     lock (which is acquired in step 1)
#
[connection slave]
#
#  4) Now on Master, insert another query that reaches slave.
#
[connection master]
INSERT INTO db2.t VALUES ('small event');
#
#  5) Check that Coordinator waits for the query (at step 2) to be
#     executed before dedicating this new query (at step 4) to
#     one of the workers because of the event size limits.
#
[connection slave]
include/assert.inc [Check that one of the applier worker thread is waiting for the table metadata lock.]
#
#  6) Release the lock acquired in step 1, so that first query will
#     continue it's work and once it is done, second big event
#     will also continue it's work.
#
[connection slave1]
UNLOCK TABLES;
#
#  7) check that slave is able to catch up with master after releasing the
#     lock in step 6.
#
#     7.1) Sync SQL thread with Master.
#
[connection master]
include/sync_slave_sql_with_master.inc
#
#     7.2) Diff all the tables involved in the test to prove
#          that replication worked fine.
#
include/diff_tables.inc [master:db1.t,slave:db1.t]
include/diff_tables.inc [master: db2.t,slave: db2.t]
#
#   4) When a big event is being processed by a worker,
#      Coordinator will make another big event also to wait until the
#      first big event is executed completely.
#
#
#  1) On Slave, lock one table so that any operation on that
#     will be waiting for the lock to be released.
#
[connection slave1]
LOCK TABLE db1.t WRITE;
#
#  2) Execute query that is going to wait for the table lock.
#
[connection master]
INSERT INTO db1.t VALUES (REPEAT('big event', 1024));
#
#  3) Wait on Slave till a worker picks this event and wait for the
#     lock (which is acquired in step 1)
#
[connection slave]
#
#  4) Now on Master, insert another query that reaches slave.
#
[connection master]
INSERT INTO db2.t VALUES (REPEAT('big event', 1024));
#
#  5) Check that Coordinator waits for the query (at step 2) to be
#     executed before dedicating this new query (at step 4) to
#     one of the workers because of the event size limits.
#
[connection slave]
include/assert.inc [Check that one of the applier worker thread is waiting for the table metadata lock.]
#
#  6) Release the lock acquired in step 1, so that first query will
#     continue it's work and once it is done, second big event
#     will also continue it's work.
#
[connection slave1]
UNLOCK TABLES;
#
#  7) check that slave is able to catch up with master after releasing the
#     lock in step 6.
#
#     7.1) Sync SQL thread with Master.
#
[connection master]
include/sync_slave_sql_with_master.inc
#
#     7.2) Diff all the tables involved in the test to prove
#          that replication worked fine.
#
include/diff_tables.inc [master:db1.t,slave:db1.t]
include/diff_tables.inc [master: db2.t,slave: db2.t]
#
#   5) Cleanup (drop tables/databases and reset the variables)
#
[connection master]
DROP DATABASE db1;
DROP DATABASE db2;
include/sync_slave_sql_with_master.inc
include/stop_slave_sql.inc
SET @@global.slave_pending_jobs_size_max=  @save.slave_pending_jobs_size_max;
SET @@GLOBAL.slave_parallel_workers= @save.slave_parallel_workers;
include/start_slave_sql.inc
include/rpl_end.inc
