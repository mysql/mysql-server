# Input: $row_format

--echo # Online rebuild with ROW_FORMAT=$row_format

eval CREATE TABLE t1 (
  id INT NOT NULL,
  c1 VARCHAR(32),
  c2 VARCHAR(86)
) ENGINE=InnoDB ROW_FORMAT=$row_format
  DEFAULT CHARACTER SET utf8mb3;

INSERT INTO t1 VALUES
  (1, 'old', REPEAT('a', 86)),
  (2, 'delete', REPEAT('b', 86));

SELECT TABLE_ID INTO @old_table_id
  FROM INFORMATION_SCHEMA.INNODB_TABLES
  WHERE NAME = 'test/t1';

CONNECT (con1, localhost, root,,);

eval SET DEBUG_SYNC =
  'alter_table_inplace_after_lock_downgrade
   SIGNAL modify_$row_format WAIT_FOR continue_$row_format';

--send
eval ALTER TABLE t1 CONVERT TO CHARACTER SET utf8mb4,
  ADD PRIMARY KEY (id), ALGORITHM = INPLACE, LOCK = NONE;

CONNECTION default;
eval SET DEBUG_SYNC = 'now WAIT_FOR modify_$row_format';

UPDATE t1
  SET c1 = 'updated', c2 = REPEAT('c', 86)
  WHERE id = 1;
DELETE FROM t1 WHERE id = 2;
INSERT INTO t1 VALUES (3, 'inserted', REPEAT('d', 86));

eval SET DEBUG_SYNC = 'now SIGNAL continue_$row_format';

CONNECTION con1;
--reap
SET DEBUG_SYNC = 'RESET';

CONNECTION default;
CHECK TABLE t1;
SELECT id, c1, CHAR_LENGTH(c2), LEFT(c2, 1)
  FROM t1 ORDER BY id;
SELECT @old_table_id <> TABLE_ID AS table_was_rebuilt
  FROM INFORMATION_SCHEMA.INNODB_TABLES
  WHERE NAME = 'test/t1';
SELECT ROW_FORMAT
  FROM INFORMATION_SCHEMA.TABLES
  WHERE TABLE_SCHEMA = 'test' AND TABLE_NAME = 't1';

DISCONNECT con1;
DROP TABLE t1;
