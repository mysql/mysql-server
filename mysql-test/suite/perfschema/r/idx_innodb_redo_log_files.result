show create table performance_schema.innodb_redo_log_files;
Table	Create Table
innodb_redo_log_files	CREATE TABLE `innodb_redo_log_files` (
  `FILE_ID` bigint NOT NULL COMMENT 'Id of the file.',
  `FILE_NAME` varchar(2000) NOT NULL COMMENT 'Path to the file.',
  `START_LSN` bigint NOT NULL COMMENT 'LSN of the first block in the file.',
  `END_LSN` bigint NOT NULL COMMENT 'LSN after the last block in the file.',
  `SIZE_IN_BYTES` bigint NOT NULL COMMENT 'Size of the file (in bytes).',
  `IS_FULL` tinyint NOT NULL COMMENT '1 iff file has no free space inside.',
  `CONSUMER_LEVEL` int NOT NULL COMMENT 'All redo log consumers registered on smaller levels than this value, have already consumed this file.'
) ENGINE=PERFORMANCE_SCHEMA DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
select * from INFORMATION_SCHEMA.STATISTICS
where TABLE_SCHEMA = "performance_schema"
  and TABLE_NAME = "innodb_redo_log_files";
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	NON_UNIQUE	INDEX_SCHEMA	INDEX_NAME	SEQ_IN_INDEX	COLUMN_NAME	COLLATION	CARDINALITY	SUB_PART	PACKED	NULLABLE	INDEX_TYPE	COMMENT	INDEX_COMMENT	IS_VISIBLE	EXPRESSION
