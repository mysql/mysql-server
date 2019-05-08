include/master-slave.inc
[connection master]
RESET MASTER;
CREATE TABLE employees (id INT NOT NULL, store_id INT NOT NULL);
Test PARTITION BY
ALTER TABLE employees PARTITION BY RANGE (store_id) (PARTITION p0 VALUES LESS THAN (6), PARTITION p1 VALUES LESS THAN (11), PARTITION p2 VALUES LESS THAN (16));
INSERT INTO employees VALUES (1, 3);
INSERT INTO employees VALUES (2, 7);
INSERT INTO employees VALUES (3, 12);
Test ADD PARTITION
ALTER TABLE employees ADD PARTITION (PARTITION p3 VALUES LESS THAN (20));
INSERT INTO employees VALUES (4, 19);
include/assert.inc [Check that the partition count is increased after ADD PARTITION]
Test DROP PARTITION
ALTER TABLE employees DROP PARTITION p3;
include/assert.inc [Check that the partition count is decreased after ADD PARTITION]
Test TRUNCATE PARTITION
INSERT INTO employees VALUES (2, 4);
ALTER TABLE employees TRUNCATE PARTITION P0;
include/assert.inc [Check that the row count after is zero]
Test REORGANIZE PARTITION
ALTER TABLE employees REORGANIZE PARTITION p1,p2 INTO (PARTITION p3 VALUES LESS THAN (20));
include/assert.inc [Check that the partition count is decreased after REORGANIZE PARTITION]
Test EXCHANGE PARTITION
CREATE TABLE employees1 LIKE employees;
ALTER TABLE employees1 REMOVE PARTITIONING;
ALTER TABLE employees EXCHANGE PARTITION p3 WITH TABLE employees1;
include/assert.inc [Check that the partition count is same after EXCHANGE PARTITION]
DROP TABLE employees1;
Test ANALYZE PARTITION
ALTER TABLE employees ANALYZE PARTITION p0;
Table	Op	Msg_type	Msg_text
test.employees	analyze	status	OK
ALTER TABLE employees ANALYZE PARTITION ALL;
Table	Op	Msg_type	Msg_text
test.employees	analyze	status	OK
Test CHECK PARTITION
ALTER TABLE employees CHECK PARTITION p0;
Table	Op	Msg_type	Msg_text
test.employees	check	status	OK
ALTER TABLE employees CHECK PARTITION ALL;
Table	Op	Msg_type	Msg_text
test.employees	check	status	OK
Test OPTIMIZE PARTITION
ALTER TABLE employees OPTIMIZE PARTITION p3;
Table	Op	Msg_type	Msg_text
test.employees	optimize	note	Table does not support optimize on partitions. All partitions will be rebuilt and analyzed.
test.employees	optimize	status	OK
ALTER TABLE employees OPTIMIZE PARTITION ALL;
Table	Op	Msg_type	Msg_text
test.employees	optimize	note	Table does not support optimize on partitions. All partitions will be rebuilt and analyzed.
test.employees	optimize	status	OK
Test REBUILD PARTITION
ALTER TABLE employees REBUILD PARTITION p3;
ALTER TABLE employees REBUILD PARTITION ALL;
Test REPAIR PARTITION
ALTER TABLE employees REPAIR PARTITION p4;
Table	Op	Msg_type	Msg_text
test.employees	repair	Error	Error in list of partitions to test.employees
test.employees	repair	status	Operation failed
ALTER TABLE employees REPAIR PARTITION ALL;
Table	Op	Msg_type	Msg_text
test.employees	repair	status	OK
include/assert.inc [Check that the partition count is same after REPAIR PARTITION]
Test REMOVE PARTITION
ALTER TABLE employees REMOVE PARTITIONING;
include/assert.inc [Check that the partition count is 0 after REMOVE PARTITIONING]
DROP TABLE employees;
Test DISCARD/IMPORT PARTITION
CREATE TABLE employees (id INT NOT NULL, store_id INT NOT NULL);
ALTER TABLE employees PARTITION BY RANGE (store_id) (PARTITION p0 VALUES LESS THAN (6), PARTITION p1 VALUES LESS THAN (11), PARTITION p2 VALUES LESS THAN (16));
[connection server_2]
CREATE TABLE employees (id INT NOT NULL, store_id INT NOT NULL);
ALTER TABLE employees PARTITION BY RANGE (store_id) (PARTITION p0 VALUES LESS THAN (6), PARTITION p1 VALUES LESS THAN (11), PARTITION p2 VALUES LESS THAN (16));
ALTER TABLE employees DISCARD PARTITION p0 TABLESPACE;
[connection server_1]
FLUSH TABLES employees FOR EXPORT;
[connection server_2]
[connection server_1]
UNLOCK TABLES;
[connection server_2]
ALTER TABLE employees IMPORT PARTITION p0 TABLESPACE;
DROP TABLE employees;
[connection server_1]
DROP TABLE employees;
CREATE TABLE employees (id INT NOT NULL, store_id INT NOT NULL);
Test PARTITION BY
ALTER TABLE employees PARTITION BY HASH(store_id) PARTITIONS 6;
Test COALESCE PARTITION
ALTER TABLE employees COALESCE PARTITION 2;
include/assert.inc [Check that the before count is greater than after count  after COALESCE PARTITION]
DROP TABLE employees;
include/rpl_end.inc
