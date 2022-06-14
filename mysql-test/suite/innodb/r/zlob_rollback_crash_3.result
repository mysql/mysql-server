SET GLOBAL innodb_compression_level = 0;
CREATE TABLE t1 (f1 INT PRIMARY KEY, f2 LONGBLOB, KEY(f2(50)))
ENGINE=InnoDB ROW_FORMAT=compressed;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` longblob,
  PRIMARY KEY (`f1`),
  KEY `f2` (`f2`(50))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ROW_FORMAT=COMPRESSED
START TRANSACTION;
INSERT INTO t1 VALUES (1, REPEAT('abcdefghijklmnopqrstuvwxyz1234', 4373));
SET DEBUG = '+d,crash_almost_end_of_lob_rollback_0';
ROLLBACK;
ERROR HY000: Lost connection to MySQL server during query
# restart
SELECT * FROM t1;
f1	f2
DROP TABLE t1;
SET GLOBAL innodb_compression_level = default;
