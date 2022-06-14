SET GLOBAL innodb_compression_level = 0;
CREATE TABLE t1 (j1 JSON) ENGINE=InnoDB ROW_FORMAT=compressed;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `j1` json DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ROW_FORMAT=COMPRESSED
SET @long_str = REPEAT('abcdefghijklmnopqrstuvwxyz1234', 60006);
SET @medium_str_1 = REPEAT('a', 200);
SET @medium_str_2 = REPEAT('b', 200);
SET @json_doc = CONCAT('["', @long_str, '","', @medium_str_1 ,'" ]');
INSERT INTO t1 (j1) VALUES (@json_doc);
SELECT JSON_EXTRACT(j1, '$[1]') FROM t1;
JSON_EXTRACT(j1, '$[1]')
"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
START TRANSACTION;
UPDATE t1 SET j1 = JSON_REPLACE(j1, '$[1]', @medium_str_2);
SELECT JSON_EXTRACT(j1, '$[1]') FROM t1;
JSON_EXTRACT(j1, '$[1]')
"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
SET DEBUG = '+d,crash_middle_of_lob_rollback_6';
ROLLBACK;
ERROR HY000: Lost connection to MySQL server during query
# Verify that before the crash, we HAVE NOT reached 7-th iteration
Pattern "Rolling back LOB for transaction \d+ undo number \d+ : current index length \d+\. \(iteration 7\)" not found
# restart
SELECT JSON_EXTRACT(j1, '$[1]') FROM t1 FOR UPDATE;
JSON_EXTRACT(j1, '$[1]')
"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
DROP TABLE t1;
SET GLOBAL innodb_compression_level = default;
# Verify that after the restart, we HAVE reached 7-th iteration
Pattern "Rolling back LOB for transaction \d+ undo number \d+ : current index length \d+\. \(iteration 7\)" found
