include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CREATE TABLE t1 (f1 INT NOT NULL AUTO_INCREMENT PRIMARY KEY, f2 INT);
INSERT /*+ SET_VAR(auto_increment_increment=10)  SET_VAR(tmp_table_size=1024)*/ 
INTO t1 VALUES (NULL, @@tmp_table_size), (NULL, @@tmp_table_size);
Warnings:
Note	1592	Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT. Statement is unsafe because it uses a system variable that may have a different value on the slave.
include/sync_slave_sql_with_master.inc
SELECT * FROM t1;
f1	f2
1	16777216
11	16777216
DROP TABLE t1;
include/rpl_sync.inc
include/rpl_end.inc
