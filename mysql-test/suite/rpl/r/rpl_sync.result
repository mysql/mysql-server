=====Configuring the enviroment=======;
include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
call mtr.add_suppression('Attempting backtrace');
call mtr.add_suppression("Recovery from master pos .* and file master-bin.000001");
call mtr.add_suppression("Binlog has bad magic number;  It's not a binary log file that can be used by this version of MySQL");
CREATE TABLE t1(a INT, PRIMARY KEY(a)) engine=innodb;
insert into t1(a) values(1);
insert into t1(a) values(2);
insert into t1(a) values(3);
=====Inserting data on the master but without the SQL Thread being running=======;
include/sync_slave_sql_with_master.inc
include/stop_slave_sql.inc
insert into t1(a) values(4);
insert into t1(a) values(5);
insert into t1(a) values(6);
=====Removing relay log files and crashing/recoverying the slave=======;
include/stop_slave_io.inc
SET SESSION debug="d,crash_before_rotate_relaylog";
FLUSH LOGS;
ERROR HY000: Lost connection to MySQL server during query
include/rpl_reconnect.inc
=====Dumping and comparing tables=======;
include/start_slave.inc
include/sync_slave_sql_with_master.inc
include/diff_tables.inc [master:t1,slave:t1]
=====Corrupting the master.info=======;
include/stop_slave.inc
FLUSH LOGS;
insert into t1(a) values(7);
insert into t1(a) values(8);
insert into t1(a) values(9);
SET SESSION debug="d,crash_before_rotate_relaylog";
FLUSH LOGS;
ERROR HY000: Lost connection to MySQL server during query
include/rpl_reconnect.inc
=====Dumping and comparing tables=======;
include/start_slave.inc
include/sync_slave_sql_with_master.inc
include/diff_tables.inc [master:t1,slave:t1]
=====Clean up=======;
drop table t1;
include/rpl_end.inc
