include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]

#1. Create tables and a trigger t1_b on master.
[connection master]
CREATE TABLE t1 (a INT PRIMARY KEY);
CREATE TABLE t2 (a INT PRIMARY KEY, b INT DEFAULT 0);
create trigger t1_b
after insert on t1
for each row
BEGIN
INSERT INTO t2 VALUES (New.a, @value1);
END|
include/rpl_sync.inc

#2. Check that action_order attribute of trigger t1_b is same on both servers.
include/assert.inc [action_order for trigger t1_b must be 1.]
[connection slave]
include/assert.inc [action_order for trigger t1_b must be 1.]
[connection master]

#3. Create another trigger t1_a on t1 for same action event and timing.
create trigger t1_a
after insert on t1
for each row PRECEDES t1_b
BEGIN
SET @value1:= New.a * 2;
END|
include/rpl_sync.inc

#4. Check that CREATED, ACTION_ORDER attributes are same on both servers.
include/assert.inc [action_order for trigger t1_a must be 1.]
include/assert.inc [action_order for trigger t1_b must be 2.]
[connection slave]
include/assert.inc [action_order for trigger t1_a must be 1.]
include/assert.inc [action_order for trigger t1_b must be 2.]
include/assert.inc [Created attribute for a trigger must be same on both the servers]

#5. Insert some data to the table on master.
[connection master]
INSERT INTO t1 (a) values (1),(2);
DROP TRIGGER test.t1_b;
INSERT INTO t1 (a) values (3);

#6. Ensure that data is consistent on both the servers.
include/rpl_sync.inc
include/diff_tables.inc [master:t1,slave:t1]
include/diff_tables.inc [master:t2,slave:t2]

#7. Clean-up
DROP TRIGGER test.t1_a;
DROP TABLE t1;
DROP TABLE t2;
include/rpl_end.inc
