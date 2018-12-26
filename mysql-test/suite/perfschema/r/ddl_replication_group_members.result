ALTER TABLE performance_schema.replication_group_members
ADD COLUMN foo INTEGER;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
TRUNCATE TABLE performance_schema.replication_group_members;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'replication_group_members'
ALTER TABLE performance_schema.replication_group_members
ADD INDEX test_index(Member_Id);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index
ON performance_schema.replication_group_members(Member_Id);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY`
  ON performance_schema.replication_group_members;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
