CREATE TABLE t1 (f1 INT PRIMARY KEY, f2 LONGBLOB, KEY(f2(50)))
ENGINE=InnoDB;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` longblob,
  PRIMARY KEY (`f1`),
  KEY `f2` (`f2`(50))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
START TRANSACTION;
INSERT INTO t1 VALUES (1, REPEAT('abcdefghijklmnopqrstuvwxyz1234', 6006));
SET DEBUG = '+d,crash_middle_of_lob_rollback_6';
ROLLBACK;
ERROR HY000: Lost connection to MySQL server during query
# Verify that before the crash, we HAVE NOT reached the index length=4 yet
Pattern "Rolling back LOB for transaction \d+ undo number \d+ : current index length 4\." not found
# restart
SELECT * FROM t1 FOR UPDATE;
f1	f2
DROP TABLE t1;
# Verify that after the restart, we HAVE reached the index length=4
Pattern "Rolling back LOB for transaction \d+ undo number \d+ : current index length 4\." found
