include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
######## Configure ########
SET @@SESSION.BINLOG_ROW_IMAGE = 'MINIMAL';
SET @@SESSION.BINLOG_ROW_VALUE_OPTIONS = 'PARTIAL_JSON';
[connection slave]
SET @old_binlog_row_image= @@GLOBAL.BINLOG_ROW_IMAGE;
SET @old_binlog_row_value_options= @@GLOBAL.BINLOG_ROW_VALUE_OPTIONS;
SET @@GLOBAL.BINLOG_ROW_IMAGE = 'MINIMAL';
SET @@GLOBAL.BINLOG_ROW_VALUE_OPTIONS = 'PARTIAL_JSON';
include/stop_slave.inc
include/start_slave.inc
######## Initialize ########
[connection master]
CREATE TABLE t1 (i INT PRIMARY KEY, j JSON);
CREATE TABLE t2 (i INT PRIMARY KEY, j JSON);
INSERT INTO t1 VALUES (1, '["a-1", "b-1"]');
INSERT INTO t2 VALUES (1, '["c-1", "d-1"]');
include/sync_slave_sql_with_master.inc
######## Test ########
[connection master]
# Both tables with partial update
UPDATE t1, t2 SET t1.j= JSON_SET(t1.j, '$[0]', 'a-2'),
t2.j= JSON_SET(t2.j, '$[0]', 'c-2');
include/sync_slave_sql_with_master.inc
[connection master]
include/diff_tables.inc [master:t1, slave:t1]
include/diff_tables.inc [master:t2, slave:t2]
# First table partial update, second table full update
UPDATE t1, t2 SET t1.j= JSON_SET(t1.j, '$[0]', 'a-3'),
t2.j= '["c-3", "d-1"]';
include/sync_slave_sql_with_master.inc
[connection master]
include/diff_tables.inc [master:t1, slave:t1]
include/diff_tables.inc [master:t2, slave:t2]
# First table full update, second table partial update
UPDATE t1, t2 SET t1.j= '["a-4", "b-1"]',
t2.j= JSON_SET(t2.j, '$[0]', 'c-4');
include/sync_slave_sql_with_master.inc
[connection master]
include/diff_tables.inc [master:t1, slave:t1]
include/diff_tables.inc [master:t2, slave:t2]
# Both tables with full update
UPDATE t1, t2 SET t1.j= '["a-5", "b-1"]',
t2.j= '["c-5", "d-1"]';
include/sync_slave_sql_with_master.inc
[connection master]
include/diff_tables.inc [master:t1, slave:t1]
include/diff_tables.inc [master:t2, slave:t2]
######## Clean up ########
DROP TABLE t1, t2;
include/sync_slave_sql_with_master.inc
SET @@GLOBAL.BINLOG_ROW_VALUE_OPTIONS= @old_binlog_row_value_options;
SET @@GLOBAL.BINLOG_ROW_IMAGE= @old_binlog_row_image;
include/rpl_end.inc
