include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CREATE TABLE t1(i INT);
DROP TABLE t1;
include/sync_slave_sql_with_master.inc
include/stop_slave_io.inc
include/start_slave_io.inc
Pattern "Lost connection to MySQL server during query" not found
include/stop_slave_sql.inc
include/start_slave_sql.inc
Pattern "Lost connection to MySQL server during query" not found
include/stop_slave.inc
include/start_slave.inc
Pattern "Lost connection to MySQL server during query" not found
include/rpl_end.inc
