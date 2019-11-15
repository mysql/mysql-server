CREATE TABLE t1 (c1 LONGBLOB) ENGINE=InnoDB;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` longblob
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SET @long_str_1 = REPEAT('abcdefghijklmnopqrstuvwxyz1234', 6006);
SET @long_str_2 = REPEAT('1234abcdefghijklmnopqrstuvwxyz', 6006);
INSERT INTO t1 (c1) VALUES (@long_str_1);
SELECT SUBSTRING(c1, 1, 10) FROM t1;
SUBSTRING(c1, 1, 10)
abcdefghij
START TRANSACTION;
UPDATE t1 SET c1 = @long_str_2;
SELECT SUBSTRING(c1, 1, 10) FROM t1;
SUBSTRING(c1, 1, 10)
1234abcdef
SET DEBUG = '+d,crash_middle_of_lob_rollback_6';
ROLLBACK;
ERROR HY000: Lost connection to MySQL server during query
# Verify that before the crash, we HAVE NOT reached the index length=0 yet
Pattern "Rolling back LOB for transaction \d+ undo number \d+ : current index length 0\." not found
# restart
SELECT SUBSTRING(c1, 1, 10) FROM t1 FOR UPDATE;
SUBSTRING(c1, 1, 10)
abcdefghij
DROP TABLE t1;
# Verify that after the restart, we HAVE reached the index length=0
Pattern "Rolling back LOB for transaction \d+ undo number \d+ : current index length 0\." found
