include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
SET GLOBAL DEBUG ='+d,set_commit_parent_150';
SET @slave_preserver_commit_order_saved= @@GLOBAL.REPLICA_PRESERVE_COMMIT_ORDER;
include/stop_slave.inc
#setup the databases and tables
CREATE DATABASE db2;
CREATE TABLE db2.t (a int) engine=myisam;
SET GLOBAL DEBUG ='-d,set_commit_parent_150';
SET GLOBAL DEBUG ='+d,set_commit_parent_100';
INSERT INTO db2.t values (1),(2),(3);
SET GLOBAL replica_parallel_type='DATABASE';
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
SET GLOBAL replica_preserve_commit_order=0;
include/start_slave.inc
include/sync_slave_sql_with_master.inc
include/diff_tables.inc [master:db2.t, slave:db2.t]
include/stop_slave.inc
SET GLOBAL DEBUG ='-d,set_commit_parent_100';
SET GLOBAL DEBUG ='+d,set_commit_parent_150';
INSERT INTO db2.t values (1),(2),(3);
SET GLOBAL replica_parallel_type='LOGICAL_CLOCK';
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
SET GLOBAL replica_preserve_commit_order=1;
include/start_slave.inc
include/sync_slave_sql_with_master.inc
include/diff_tables.inc [master:db2.t, slave:db2.t]
include/stop_slave.inc
SET GLOBAL DEBUG ='-d,set_commit_parent_150';
SET GLOBAL DEBUG ='+d,set_commit_parent_100';
INSERT INTO db2.t values (1),(2),(3);
SET GLOBAL replica_parallel_type='DATABASE';
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
SET GLOBAL replica_preserve_commit_order=0;
include/start_slave.inc
include/sync_slave_sql_with_master.inc
include/diff_tables.inc [master:db2.t, slave:db2.t]
DROP DATABASE db2;
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
include/start_slave.inc
include/rpl_end.inc
