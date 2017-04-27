include/rpl_init.inc [topology=1->2, 2->3]
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection server_2]
CHANGE MASTER TO MASTER_DELAY= 5;
include/start_slave.inc
[connection server_3]
CHANGE MASTER TO MASTER_DELAY= 5;
include/start_slave.inc
[connection server_1]
CREATE TABLE t1 (a INT);
[connection server_1]
include/rpl_sync.inc
[connection server_2]
include/rpl_sync.inc
[connection server_1]
[connection server_2]
[connection server_3]
include/assert.inc [The difference between the immediate_commit_timestamp of servers 1 and 2 should be at least the SQL_Delay]
include/assert.inc [The difference between the immediate_commit_timestamp of servers 2 and 3 should be at least the SQL_Delay]
[connection server_1]
INSERT INTO t1 VALUES (1);
[connection server_1]
include/rpl_sync.inc
[connection server_2]
include/rpl_sync.inc
[connection server_1]
[connection server_2]
[connection server_3]
include/assert.inc [The difference between the immediate_commit_timestamp of servers 1 and 2 should be at least the SQL_Delay]
include/assert.inc [The difference between the immediate_commit_timestamp of servers 2 and 3 should be at least the SQL_Delay]
[connection server_1]
UPDATE t1 SET a=2;
[connection server_1]
include/rpl_sync.inc
[connection server_2]
include/rpl_sync.inc
[connection server_1]
[connection server_2]
[connection server_3]
include/assert.inc [The difference between the immediate_commit_timestamp of servers 1 and 2 should be at least the SQL_Delay]
include/assert.inc [The difference between the immediate_commit_timestamp of servers 2 and 3 should be at least the SQL_Delay]
[connection server_1]
DELETE FROM t1 WHERE a=2;
[connection server_1]
include/rpl_sync.inc
[connection server_2]
include/rpl_sync.inc
[connection server_1]
[connection server_2]
[connection server_3]
include/assert.inc [The difference between the immediate_commit_timestamp of servers 1 and 2 should be at least the SQL_Delay]
include/assert.inc [The difference between the immediate_commit_timestamp of servers 2 and 3 should be at least the SQL_Delay]
[connection server_1]
START TRANSACTION;
INSERT INTO t1 VALUES (1);
INSERT INTO t1 VALUES (2);
INSERT INTO t1 VALUES (3);
COMMIT;
[connection server_1]
include/rpl_sync.inc
[connection server_2]
include/rpl_sync.inc
[connection server_1]
[connection server_2]
[connection server_3]
include/assert.inc [The difference between the immediate_commit_timestamp of servers 1 and 2 should be at least the SQL_Delay]
include/assert.inc [The difference between the immediate_commit_timestamp of servers 2 and 3 should be at least the SQL_Delay]
[connection server_1]
DROP TABLE t1;
[connection server_1]
include/rpl_sync.inc
[connection server_2]
include/rpl_sync.inc
[connection server_1]
[connection server_2]
[connection server_3]
include/assert.inc [The difference between the immediate_commit_timestamp of servers 1 and 2 should be at least the SQL_Delay]
include/assert.inc [The difference between the immediate_commit_timestamp of servers 2 and 3 should be at least the SQL_Delay]
[connection server_1]
[connection server_2]
include/stop_slave_sql.inc
CHANGE MASTER TO MASTER_DELAY= 0;
include/start_slave_sql.inc
[connection server_3]
include/stop_slave_sql.inc
CHANGE MASTER TO MASTER_DELAY= 0;
include/start_slave_sql.inc
[connection server_1]
include/rpl_end.inc
