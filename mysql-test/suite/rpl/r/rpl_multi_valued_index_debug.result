include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
SET SESSION sql_log_bin= 0;
call mtr.add_suppression('.*Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT.*');
SET SESSION sql_log_bin= 1;
include/sync_slave_sql_with_master.inc
CALL mtr.add_suppression(".*Column 1 of table .* cannot be converted from type.*");
CALL mtr.add_suppression(".*The slave coordinator and worker threads are stopped.*");
SET @@global.slave_rows_search_algorithms="TABLE_SCAN,INDEX_SCAN";
Warnings:
Warning	1287	'@@slave_rows_search_algorithms' is deprecated and will be removed in a future release.
[connection master]
FLUSH LOGS;
SET binlog_row_image = MINIMAL;
# Test non-nullable MV index
CREATE TABLE t1 (id INT, c INT, j JSON NOT NULL,
UNIQUE INDEX i1((CAST(CAST(j AS JSON) AS UNSIGNED ARRAY))));
INSERT INTO t1 VALUES (1, 1, '[1,2,3,4]'), (2, 2, '[5,6,7,8]');
UPDATE t1 SET j = '[2,9,10]' WHERE JSON_CONTAINS(CAST(j AS JSON),'2');
include/sync_slave_sql_with_master.inc
[Connection Slave]
SHOW STATUS LIKE 'Replica_rows_last_search_algorithm_used';
Variable_name	Value
Replica_rows_last_search_algorithm_used	TABLE_SCAN
[connection master]
DELETE FROM t1 WHERE JSON_CONTAINS(CAST(j AS JSON),'2');
include/sync_slave_sql_with_master.inc
[Connection Slave]
SHOW STATUS LIKE 'Replica_rows_last_search_algorithm_used';
Variable_name	Value
Replica_rows_last_search_algorithm_used	TABLE_SCAN
[connection master]
DROP TABLE t1;
# Test nullable MV index
CREATE TABLE t1 (id INT, c INT, j JSON,
UNIQUE INDEX i1((CAST(CAST(j AS JSON) AS UNSIGNED ARRAY))));
INSERT INTO t1 VALUES (1, 1, '[1,2,3,4]'), (2, 2, '[5,6,7,8]');
INSERT INTO t1 VALUES (3, 3, NULL);
UPDATE t1 SET j = '[2,9,10]' WHERE JSON_CONTAINS(CAST(j AS JSON),'2');
include/sync_slave_sql_with_master.inc
[Connection Slave]
SHOW STATUS LIKE 'Replica_rows_last_search_algorithm_used';
Variable_name	Value
Replica_rows_last_search_algorithm_used	TABLE_SCAN
[connection master]
DELETE FROM t1 WHERE JSON_CONTAINS(CAST(j AS JSON),'2');
include/sync_slave_sql_with_master.inc
[Connection Slave]
SHOW STATUS LIKE 'Replica_rows_last_search_algorithm_used';
Variable_name	Value
Replica_rows_last_search_algorithm_used	TABLE_SCAN
[connection master]
DROP TABLE t1;
# Test non-unique MV index
CREATE TABLE t1 (id INT, c INT, j JSON NOT NULL,
INDEX i1((CAST(CAST(j AS JSON) AS UNSIGNED ARRAY))));
INSERT INTO t1 VALUES (1, 1, '[1,2,3,4]'), (2, 2, '[5,6,7,8]');
INSERT INTO t1 VALUES (1, 1, '[1,2,3,4]'), (2, 2, '[5,6,7,8]');
UPDATE t1 SET j = '[2,9,10]' WHERE JSON_CONTAINS(CAST(j AS JSON),'2');
include/sync_slave_sql_with_master.inc
[Connection Slave]
SHOW STATUS LIKE 'Replica_rows_last_search_algorithm_used';
Variable_name	Value
Replica_rows_last_search_algorithm_used	TABLE_SCAN
[connection master]
DELETE FROM t1 WHERE JSON_CONTAINS(CAST(j AS JSON),'2');
include/sync_slave_sql_with_master.inc
[Connection Slave]
SHOW STATUS LIKE 'Replica_rows_last_search_algorithm_used';
Variable_name	Value
Replica_rows_last_search_algorithm_used	TABLE_SCAN
[connection master]
DROP TABLE t1;
# Test composite MV index
CREATE TABLE t1 (id INT, c INT, j JSON NOT NULL,
INDEX i1((CAST(CAST(j AS JSON) AS UNSIGNED ARRAY)), id));
INSERT INTO t1 VALUES (1, 1, '[1,2,3,4]'), (2, 2, '[5,6,7,8]');
INSERT INTO t1 VALUES (1, 1, '[1,2,3,4]'), (2, 2, '[5,6,7,8]');
UPDATE t1 SET j = '[2,9,10]' WHERE JSON_CONTAINS(CAST(j AS JSON),'2');
include/sync_slave_sql_with_master.inc
[Connection Slave]
SHOW STATUS LIKE 'Replica_rows_last_search_algorithm_used';
Variable_name	Value
Replica_rows_last_search_algorithm_used	TABLE_SCAN
[connection master]
DELETE FROM t1 WHERE JSON_CONTAINS(CAST(j AS JSON),'2');
include/sync_slave_sql_with_master.inc
[Connection Slave]
SHOW STATUS LIKE 'Replica_rows_last_search_algorithm_used';
Variable_name	Value
Replica_rows_last_search_algorithm_used	TABLE_SCAN
[connection master]
DROP TABLE t1;
SET @@global.slave_rows_search_algorithms= "INTITAL_ROW_SEARCH_CONF";
Warnings:
Warning	1287	'@@slave_rows_search_algorithms' is deprecated and will be removed in a future release.
FLUSH LOGS;
include/sync_slave_sql_with_master.inc
SET @@global.slave_rows_search_algorithms= "INTITAL_ROW_SEARCH_CONF";
Warnings:
Warning	1287	'@@slave_rows_search_algorithms' is deprecated and will be removed in a future release.
include/rpl_end.inc
