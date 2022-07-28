include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
#
# 1. Create a database and table on the source with a size above 64
#    Check that SHOW BINLOG EVENTS works
CREATE DATABASE 试测试测试测试测试测试测试测试测试测试测试测试测试测试测试测;
CREATE TABLE 试测试测试测试测试测试测试测试测试测试测试测试测试测试测试测.t1 (c1 INT);
INSERT INTO 试测试测试测试测试测试测试测试测试测试测试测试测试测试测试测.t1 VALUES(1);
CREATE TABLE test.测试测试测试测试测试测试测试测试测试测试测试测试测试测试测试 (c1 INT);
INSERT INTO test.测试测试测试测试测试测试测试测试测试测试测试测试测试测试测试 VALUES(1);
SHOW BINLOG EVENTS;
include/sync_slave_sql_with_master.inc
#
# 2. Check the data was replicated
include/assert.inc [The table with a big database name has 1 line]
include/assert.inc [The table with a big name has 1 line]
#
# 3. Cleanup
[connection master]
DROP TABLE 试测试测试测试测试测试测试测试测试测试测试测试测试测试测试测.t1;
DROP DATABASE 试测试测试测试测试测试测试测试测试测试测试测试测试测试测试测;
DROP TABLE test.测试测试测试测试测试测试测试测试测试测试测试测试测试测试测试;
include/rpl_end.inc
