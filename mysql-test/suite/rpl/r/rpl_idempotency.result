include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
call mtr.add_suppression("Slave SQL.*Can.t find record in .t[12].* Error_code: 1032");
call mtr.add_suppression("Slave SQL.*Cannot delete or update a parent row: a foreign key constraint fails .* Error_code: 1451");
call mtr.add_suppression("Slave SQL.*Cannot add or update a child row: a foreign key constraint fails .* Error_code: 1452");
call mtr.add_suppression("Slave SQL.*Could not execute Write_rows event on table test.* Duplicate entry .1. for key .PRIMARY.* Error_code: 1062");
CREATE TABLE t1 (a INT PRIMARY KEY);
CREATE TABLE t2 (a INT);
INSERT INTO t1 VALUES (-1),(-2),(-3);
INSERT INTO t2 VALUES (-1),(-2),(-3);
include/sync_slave_sql_with_master.inc
SET @old_slave_exec_mode= @@global.slave_exec_mode;
SET @@global.slave_exec_mode= IDEMPOTENT;
DELETE FROM t1 WHERE a = -2;
DELETE FROM t2 WHERE a = -2;
DELETE FROM t1 WHERE a = -2;
DELETE FROM t2 WHERE a = -2;
SELECT * FROM t1 ORDER BY a;
a
-3
-1
SELECT * FROM t2 ORDER BY a;
a
-3
-1
include/sync_slave_sql_with_master.inc
SELECT * FROM t1 ORDER BY a;
a
-3
-1
SELECT * FROM t2 ORDER BY a;
a
-3
-1
include/check_slave_no_error.inc
INSERT IGNORE INTO t1 VALUES (-2);
INSERT IGNORE INTO t1 VALUES (-2);
SELECT * FROM t1 ORDER BY a;
a
-3
-2
-1
include/sync_slave_sql_with_master.inc
SELECT * FROM t1 ORDER BY a;
a
-3
-2
-1
include/check_slave_no_error.inc
UPDATE t1 SET a = 1 WHERE a = -1;
UPDATE t2 SET a = 1 WHERE a = -1;
UPDATE t1 SET a = 1 WHERE a = -1;
UPDATE t2 SET a = 1 WHERE a = -1;
SELECT * FROM t1 ORDER BY a;
a
-3
-2
1
SELECT * FROM t2 ORDER BY a;
a
-3
1
include/sync_slave_sql_with_master.inc
SELECT * FROM t1 ORDER BY a;
a
-3
-2
1
SELECT * FROM t2 ORDER BY a;
a
-3
1
include/check_slave_no_error.inc
DROP TABLE t1, t2;
include/sync_slave_sql_with_master.inc
SET @@global.slave_exec_mode= @old_slave_exec_mode;
include/rpl_end.inc
