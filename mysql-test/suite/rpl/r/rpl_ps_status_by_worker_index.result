include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CREATE TABLE t1 (c1 INT PRIMARY KEY);
include/sync_slave_sql_with_master.inc
LOCK TABLES t1 READ, performance_schema.threads READ;
[connection master]
INSERT INTO t1 (c1) VALUES (1);
include/sync_slave_io_with_master.inc
# check that a worker thread is waiting using the thread id
# given as index by the table replication_applier_status_by_worker
UNLOCK TABLES;
# check that the number of workers in the replication p_s table matches
# when their thread id from the replication p_s table is used in
# p_s.threads table
[connection master]
DROP TABLE t1;
include/rpl_end.inc
