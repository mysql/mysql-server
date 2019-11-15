set global innodb_compression_level = 0;
CREATE TABLE t(j1 JSON) row_format=compressed;
set @data_1 = repeat('abcdefghijklmnopqrstuvwxyz1234', 6006);
set @json_doc_1 = concat('["Sven", "', @data_1, '"]');
set @json_doc_2 = concat('["Glen", "', @data_1, '"]');
show create table t;
Table	Create Table
t	CREATE TABLE `t` (
  `j1` json DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ROW_FORMAT=COMPRESSED
start transaction;
# Insert data
INSERT INTO t VALUES (@json_doc_1);
SET SESSION DEBUG='+d,crash_end_of_lob_rollback_0';
rollback;
ERROR HY000: Lost connection to MySQL server during query
# restart
set @data_1 = repeat('abcdefghijklmnopqrstuvwxyz1234', 6006);
set @json_doc_1 = concat('["Sven", "', @data_1, '"]');
set @json_doc_2 = concat('["Glen", "', @data_1, '"]');
select count(*) from t;
count(*)
0
INSERT INTO t VALUES (@json_doc_1);
start transaction;
# Insert data
INSERT INTO t VALUES (@json_doc_2);
SET SESSION DEBUG='+d,crash_end_of_lob_rollback_0';
rollback;
ERROR HY000: Lost connection to MySQL server during query
# restart
select count(*) from t;
count(*)
1
drop table t;
set global innodb_compression_level = default;
