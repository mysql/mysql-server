include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CREATE TABLE t1 (id int, a int);
CREATE TABLE t2 (id int, b int);
CREATE TABLE t3 (id int, c int);
CREATE TABLE t4 (id int, d int);
CREATE TABLE t5 (id int, e int);
CREATE TABLE t6 (id int, f int);
CREATE TABLE t7 (id int, g int);
CREATE TABLE t8 (id int, h int);
CREATE TABLE t9 (id int, i int);
INSERT INTO t1 VALUES (1, 1), (2, 2), (3, 3);
INSERT INTO t2 VALUES (1, 1), (2, 2), (3, 3);
INSERT INTO t3 VALUES (1, 1), (2, 2), (3, 3);
INSERT INTO t4 VALUES (1, 1), (2, 2), (3, 3);
INSERT INTO t5 VALUES (1, 1), (2, 2), (3, 3);
INSERT INTO t6 VALUES (1, 1), (2, 2), (3, 3);
INSERT INTO t7 VALUES (1, 1), (2, 2), (3, 3);
INSERT INTO t8 VALUES (1, 1), (2, 2), (3, 3);
INSERT INTO t9 VALUES (1, 1), (2, 2), (3, 3);
include/sync_slave_sql_with_master.inc
[on slave]
SHOW TABLES LIKE 't%';
Tables_in_test (t%)
t1
t2
t3
[on master]
UPDATE t7 LEFT JOIN t4 ON (t4.id=t7.id) SET d=0, g=0 where t7.id=1;
UPDATE t7 LEFT JOIN (t4, t5, t6) ON (t7.id=t4.id and t7.id=t5.id and t7.id=t6.id) SET d=0, e=0, f=0, g=0 where t7.id=1;
UPDATE t4 LEFT JOIN (t7, t8, t9) ON (t4.id=t7.id and t4.id=t8.id and t4.id=t9.id) SET d=0, g=0, h=0, i=0 where t4.id=1;
UPDATE t7 LEFT JOIN (t8, t9) ON (t7.id=t8.id and t7.id=t9.id) SET g=0, h=0, i=0 where t7.id=1;
UPDATE t1 LEFT JOIN t4 ON (t1.id=t4.id) SET d=0 where t1.id=1;
UPDATE t1 LEFT JOIN t7 ON (t1.id=t7.id) SET g=0 where t1.id=1;
UPDATE t1 LEFT JOIN (t4, t5, t6) ON (t1.id=t4.id and t1.id=t5.id and t1.id=t6.id) SET d=0, e=0, f=0 where t1.id=1;
UPDATE t1 LEFT JOIN (t4, t5, t8) ON (t1.id=t4.id and t1.id=t5.id and t1.id=t8.id) SET d=0, e=0, h=0 where t1.id=1;
UPDATE t1 LEFT JOIN (t7, t8, t5) ON (t1.id=t7.id and t1.id=t8.id and t1.id=t5.id) SET g=0, h=0, e=0 where t1.id=1;
UPDATE t1 LEFT JOIN (t2, t3, t5) ON (t1.id=t2.id and t1.id=t3.id and t1.id=t5.id) SET e=0 where t1.id=1;
UPDATE t4 LEFT JOIN t1 ON (t1.id=t4.id) SET a=0, d=0 where t4.id=1;
UPDATE t4 LEFT JOIN (t1, t7) ON (t4.id=t1.id and t7.id=t4.id) SET a = 0, d=0, g=0 where t4.id=1;
UPDATE t4 LEFT JOIN (t1, t2, t3) ON (t1.id=t4.id and t2.id=t4.id and t3.id=t4.id) SET a=0, b=0, c=0, d=0 where t4.id=1;
UPDATE t4 LEFT JOIN (t1, t2, t5) ON (t1.id=t4.id and t2.id=t4.id and t5.id=t4.id) SET a=0, b=0, e=0, d=0 where t4.id=1;
UPDATE t4 LEFT JOIN (t1, t6, t7) ON (t4.id=t1.id and t4.id=t6.id and t4.id=t7.id) SET a=0, d=0, f=0, g=0 where t4.id=1;
UPDATE t7 LEFT JOIN (t4, t1, t2) ON (t7.id=t4.id and t7.id=t1.id and t7.id=t2.id) SET a=0, b=0, d=0, g=0 where t7.id=1;
UPDATE t7 LEFT JOIN (t8, t4, t1) ON (t7.id=t8.id and t7.id=t4.id and t7.id=t1.id) SET a=0, d=0, g=0, h=0 where t7.id=1;
include/sync_slave_sql_with_master.inc
call mtr.add_suppression("Slave SQL.*Error .Table .test.t[47]. doesn.t exist. on query.* Error_code: MY-001146");
call mtr.add_suppression("The slave coordinator and worker threads are stopped, possibly leaving data in inconsistent state");
UPDATE t1 LEFT JOIN t4 ON (t1.id=t4.id) SET a=0 where t1.id=1;
include/wait_for_slave_sql_error_and_skip.inc [errno=1146]
UPDATE t1 LEFT JOIN (t4, t7) ON (t1.id=t4.id and t1.id=t7.id) SET a=0 where t1.id=1;
include/wait_for_slave_sql_error_and_skip.inc [errno=1146]
UPDATE t1 LEFT JOIN (t2, t4, t7) ON (t1.id=t2.id and t1.id=t4.id and t1.id=t7.id) SET a=0, b=0 where t1.id=1;
include/wait_for_slave_sql_error_and_skip.inc [errno=1146]
UPDATE t1 LEFT JOIN (t2, t3, t7) ON (t1.id=t2.id and t1.id=t3.id and t1.id=t7.id) SET a=0, b=0, c=0 where t1.id=1;
include/wait_for_slave_sql_error_and_skip.inc [errno=1146]
UPDATE t1 LEFT JOIN t7 ON (t1.id=t7.id) SET a=0, g=0 where t1.id=1;
include/wait_for_slave_sql_error_and_skip.inc [errno=1146]
UPDATE t7 LEFT JOIN t1 ON (t1.id=t7.id) SET a=0, g=0 where t7.id=1;
include/wait_for_slave_sql_error_and_skip.inc [errno=1146]
UPDATE t1 LEFT JOIN (t4, t5, t7) ON (t1.id=t4.id and t1.id=t5.id and t1.id=t7.id) SET a=0, g=0 where t1.id=1;
include/wait_for_slave_sql_error_and_skip.inc [errno=1146]
UPDATE t1 LEFT JOIN (t4, t7, t8) ON (t1.id=t4.id and t1.id=t7.id and t1.id=t8.id) SET a=0, g=0 where t1.id=1;
include/wait_for_slave_sql_error_and_skip.inc [errno=1146]
UPDATE t1 LEFT JOIN (t7, t8, t9) ON (t1.id=t7.id and t1.id=t8.id and t1.id=t9.id) SET a=0, g=0, h=0, i=0 where t1.id=1;
include/wait_for_slave_sql_error_and_skip.inc [errno=1146]
UPDATE t7 LEFT JOIN (t1, t2, t3) ON (t7.id=t1.id and t7.id=t2.id and t7.id=t3.id) SET g=0, a=0, b=0, c=0 where t7.id=1;
include/wait_for_slave_sql_error_and_skip.inc [errno=1146]
UPDATE t7 LEFT JOIN (t4, t5, t3) ON (t7.id=t4.id and t7.id=t5.id and t7.id=t3.id) SET g=0, c=0 where t7.id=1;
include/wait_for_slave_sql_error_and_skip.inc [errno=1146]
UPDATE t7 LEFT JOIN (t8, t9, t3) ON (t7.id=t8.id and t7.id=t9.id and t7.id=t3.id) SET g=0, h=0, i=0, c=0 where t7.id=1;
include/wait_for_slave_sql_error_and_skip.inc [errno=1146]
UPDATE t1 LEFT JOIN t4 ON (t1.id=t4.id) SET a=0, d=0 where t1.id=1;
include/wait_for_slave_sql_error_and_skip.inc [errno=1146]
UPDATE t1 LEFT JOIN (t4, t5, t6) ON (t1.id=t4.id and t1.id=t5.id and t1.id=t6.id) SET a=0, d=0, e=0, f=0 where t1.id=1;
include/wait_for_slave_sql_error_and_skip.inc [errno=1146]
UPDATE t4 LEFT JOIN (t1, t5, t6) ON (t4.id=t1.id and t4.id=t5.id and t4.id=t6.id) SET a=0, e=0, f=0 where t4.id=1;
include/wait_for_slave_sql_error_and_skip.inc [errno=1146]
UPDATE t7 LEFT JOIN (t1, t4, t2) ON (t7.id=t1.id and t7.id=t4.id and t7.id=t2.id) SET a=0, b=0, d=0, g=0 where t7.id=1;
include/wait_for_slave_sql_error_and_skip.inc [errno=1146]
include/sync_slave_sql_with_master.inc
[on slave]
show tables like 't%';
Tables_in_test (t%)
t1
t2
t3
SELECT * FROM t1;
id	a
1	1
2	2
3	3
SELECT * FROM t2;
id	b
1	1
2	2
3	3
SELECT * FROM t3;
id	c
1	1
2	2
3	3
[on master]
drop table if exists t1,t2,t3,t4,t5,t6,t7,t8,t9;
include/sync_slave_sql_with_master.inc
CREATE TABLE test.t5 (a INT AUTO_INCREMENT PRIMARY KEY, b DECIMAL(20,20), c INT);
CREATE TABLE test.t1 (a INT);
INSERT INTO test.t1 VALUES(1);
call mtr.add_suppression("Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT");
include/sync_slave_sql_with_master.inc
call mtr.add_suppression("Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT");
CREATE TABLE test.t_slave (a INT AUTO_INCREMENT PRIMARY KEY, b DECIMAL(20,20), c INT);
CREATE TRIGGER t1_update AFTER UPDATE ON test.t1 FOR EACH ROW
INSERT INTO test.t_slave VALUES(NULL, RAND(), @c);
SET INSERT_ID=2;
SET @c=2;
SET @@rand_seed1=10000000, @@rand_seed2=1000000;
INSERT INTO t5 VALUES (NULL, RAND(), @c);
Warnings:
Note	1592	Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT. Statement is unsafe because it uses a system function that may return a different value on the slave.
SELECT b into @b FROM test.t5;
UPDATE test.t1 SET a=2;
include/sync_slave_sql_with_master.inc
SELECT a AS 'ONE' into @a FROM test.t_slave;
SELECT c AS 'NULL' into @c FROM test.t_slave;
SELECT b into @b FROM test.t_slave;
include/assert.inc [Random values from master and slave must be different]
drop table test.t5;
drop table test.t1;
include/sync_slave_sql_with_master.inc
drop table test.t_slave;
include/rpl_end.inc
