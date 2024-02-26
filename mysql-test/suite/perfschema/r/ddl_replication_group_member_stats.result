ALTER TABLE performance_schema.replication_group_member_stats
ADD COLUMN foo INTEGER;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
TRUNCATE TABLE performance_schema.replication_group_member_stats;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'replication_group_member_stats'
ALTER TABLE performance_schema.replication_group_member_stats
ADD INDEX test_index(Member_Id);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index
ON performance_schema.replication_group_member_stats(Member_Id);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY`
  ON performance_schema.replication_group_member_stats;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select
AS SELECT * from performance_schema.replication_group_member_stats;
DROP TABLE test.create_select;
