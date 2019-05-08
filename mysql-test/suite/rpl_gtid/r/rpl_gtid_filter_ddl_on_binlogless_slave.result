include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CREATE TABLE t1 (c1 INT PRIMARY KEY);
include/sync_slave_sql_with_master.inc
call mtr.add_suppression("You need to use --log-bin to make --binlog-format work.");
include/assert.inc [Table "t1" does not exist, because it is filtered out]
[connection master]
DROP TABLE t1;
include/rpl_end.inc
