SELECT * FROM performance_schema.replication_group_members
LIMIT 1;
SELECT * FROM performance_schema.replication_group_members
WHERE channel_name='FOO';
INSERT INTO performance_schema.replication_group_members
SET channel_name='FOO', member_id=1;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'replication_group_members'
UPDATE performance_schema.replication_group_members
SET member_id=12;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'replication_group_members'
UPDATE performance_schema.replication_group_members
SET member_id=12 WHERE channel_name LIKE "FOO";
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'replication_group_members'
DELETE FROM performance_schema.replication_group_members
WHERE member_id=1;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'replication_group_members'
DELETE FROM performance_schema.replication_group_members;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'replication_group_members'
LOCK TABLES performance_schema.replication_group_members READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'replication_group_members'
UNLOCK TABLES;
LOCK TABLES performance_schema.replication_group_members WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'replication_group_members'
UNLOCK TABLES;
