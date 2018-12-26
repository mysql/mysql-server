SELECT * FROM performance_schema.replication_group_member_stats
LIMIT 1;
CHANNEL_NAME	VIEW_ID	MEMBER_ID	COUNT_TRANSACTIONS_IN_QUEUE	COUNT_TRANSACTIONS_CHECKED	COUNT_CONFLICTS_DETECTED	COUNT_TRANSACTIONS_ROWS_VALIDATING	TRANSACTIONS_COMMITTED_ALL_MEMBERS	LAST_CONFLICT_FREE_TRANSACTION	COUNT_TRANSACTIONS_REMOTE_IN_APPLIER_QUEUE	COUNT_TRANSACTIONS_REMOTE_APPLIED	COUNT_TRANSACTIONS_LOCAL_PROPOSED	COUNT_TRANSACTIONS_LOCAL_ROLLBACK
SELECT * FROM performance_schema.replication_group_member_stats
WHERE channel_name='FOO';
CHANNEL_NAME	VIEW_ID	MEMBER_ID	COUNT_TRANSACTIONS_IN_QUEUE	COUNT_TRANSACTIONS_CHECKED	COUNT_CONFLICTS_DETECTED	COUNT_TRANSACTIONS_ROWS_VALIDATING	TRANSACTIONS_COMMITTED_ALL_MEMBERS	LAST_CONFLICT_FREE_TRANSACTION	COUNT_TRANSACTIONS_REMOTE_IN_APPLIER_QUEUE	COUNT_TRANSACTIONS_REMOTE_APPLIED	COUNT_TRANSACTIONS_LOCAL_PROPOSED	COUNT_TRANSACTIONS_LOCAL_ROLLBACK
INSERT INTO performance_schema.replication_group_member_stats
SET channel_name='FOO', node_id=1;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'replication_group_member_stats'
UPDATE performance_schema.replication_group_member_stats
SET member_id=12;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'replication_group_member_stats'
UPDATE performance_schema.replication_group_member_stats
SET member_id=12 WHERE group_name LIKE "FOO";
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'replication_group_member_stats'
DELETE FROM performance_schema.replication_group_member_stats
WHERE member_id=1;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'replication_group_member_stats'
DELETE FROM performance_schema.replication_group_member_stats;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'replication_group_member_stats'
LOCK TABLES performance_schema.replication_group_member_stats READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'replication_group_member_stats'
UNLOCK TABLES;
LOCK TABLES performance_schema.replication_group_member_stats WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'replication_group_member_stats'
UNLOCK TABLES;
