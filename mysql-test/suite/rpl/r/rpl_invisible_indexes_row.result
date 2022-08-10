#
# Bug#30072179: USING INVISIBLE INDEX WHEN SLAVE APPLY EVENT
#
include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[Connection Master]
SET sql_log_bin = OFF;
CREATE TABLE t1 ( a INT NOT NULL, b INT NOT NULL, C INT, PRIMARY KEY (a));
SET sql_log_bin = ON;
[Connection Slave]
CREATE TABLE t1 (
a INT NOT NULL,
b INT NOT NULL,
c INT,
PRIMARY KEY (b),
UNIQUE KEY(a) INVISIBLE
);
[Connection Master]
INSERT INTO t1 VALUES (1, 1, 1), (2, 2, 2), (3, 3, 3);
UPDATE t1 SET c = 100 WHERE a = 1;
include/sync_slave_sql_with_master.inc
[Connection Slave]
# Should not have used the a index to syncronize the row.
SELECT 1 AS not_using_index FROM performance_schema.global_status
WHERE variable_name = 'Replica_rows_last_search_algorithm_used'
AND variable_value <> 'INDEX_SCAN';
not_using_index
1
# Make sure it indeed synchronized. This _will_ use the index.
SELECT * FROM t1;
a	b	c
1	1	100
2	2	2
3	3	3
[Connection Master]
DROP TABLE t1;
include/sync_slave_sql_with_master.inc
include/rpl_end.inc
