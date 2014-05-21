#
# wl#7277: InnoDB: Bulk Load for Create Index
#

-- source include/have_innodb.inc

# Create Insert Procedure
DELIMITER |;
CREATE PROCEDURE populate_t1()
BEGIN
	DECLARE i int DEFAULT 1;

	START TRANSACTION;
	WHILE (i <= 10000) DO
		INSERT INTO t1 VALUES (i, i, CONCAT('a', i));
		SET i = i + 1;
	END WHILE;
	COMMIT;
END|
DELIMITER ;|

SELECT @@innodb_fill_factor;

if ($row_format == 'DYNAMIC')
{
  SET GLOBAL innodb_file_format=Barracuda;
}

if ($row_format != 'COMPRESSED')
{
  eval CREATE TABLE t1(
	class	INT,
	id	INT,
	title	VARCHAR(100)
  ) ENGINE=InnoDB ROW_FORMAT=$row_format;
}

if ($row_format == 'COMPRESSED')
{
  SET GLOBAL innodb_file_per_table=1;
  SET GLOBAL innodb_file_format=Barracuda;

  eval CREATE TABLE t1(
	class	INT,
	id	INT,
	title	VARCHAR(100)
  ) ENGINE=InnoDB ROW_FORMAT=$row_format KEY_BLOCK_SIZE=4;
}

-- disable_query_log
CALL populate_t1();
-- enable_query_log

SELECT COUNT(*) FROM t1;

/* Create index. */
CREATE INDEX idx_id ON t1(id);

CREATE INDEX idx_title ON t1(title);

/* Check table. */
CHECK TABLE t1;

/* Select by index. */
EXPLAIN SELECT * FROM t1 WHERE id = 10;
EXPLAIN SELECT * FROM t1 WHERE title = 'a10';

SELECT * FROM t1 WHERE id = 10;
SELECT * FROM t1 WHERE title = 'a10';

SELECT * FROM t1 WHERE id = 5000;
SELECT * FROM t1 WHERE title = 'a5000';

SELECT * FROM t1 WHERE id = 10000;
SELECT * FROM t1 WHERE title = 'a10000';

SELECT * FROM t1 WHERE id = 10010;
SELECT * FROM t1 WHERE title = 'a10010';

/*Insert/Update/Delete. */
DELETE FROM t1 WHERE id < 4010 AND id > 3990;
INSERT INTO t1 VALUES(4000, 4000, 'b4000');
UPDATE t1 SET title = CONCAT('b', id) WHERE id < 3010 AND id > 2990;

SELECT * FROM t1 WHERE id = 3000;
SELECT * FROM t1 WHERE title = 'a3000';
SELECT * FROM t1 WHERE title = 'b3000';

SELECT * FROM t1 WHERE id = 4000;
SELECT * FROM t1 WHERE title = 'a4000';
SELECT * FROM t1 WHERE title = 'b4000';

SELECT * FROM t1 WHERE id = 4001;
SELECT * FROM t1 WHERE title = 'a4001';

/* Add column. */
ALTER TABLE t1 ADD COLUMN content TEXT;

CHECK TABLE t1;

SELECT * FROM t1 WHERE id = 10;
SELECT * FROM t1 WHERE title = 'a10';

SELECT * FROM t1 WHERE id = 5000;
SELECT * FROM t1 WHERE title = 'a5000';

SELECT * FROM t1 WHERE id = 10000;
SELECT * FROM t1 WHERE title = 'a10000';

SELECT * FROM t1 WHERE id = 10010;
SELECT * FROM t1 WHERE title = 'a10010';

/* Drop column. */
ALTER TABLE t1 DROP COLUMN content;

CHECK TABLE t1;

SELECT * FROM t1 WHERE id = 10;
SELECT * FROM t1 WHERE title = 'a10';

SELECT * FROM t1 WHERE id = 5000;
SELECT * FROM t1 WHERE title = 'a5000';

SELECT * FROM t1 WHERE id = 10000;
SELECT * FROM t1 WHERE title = 'a10000';

SELECT * FROM t1 WHERE id = 10010;
SELECT * FROM t1 WHERE title = 'a10010';

DROP TABLE t1;

# Test Blob
if ($row_format != 'COMPRESSED') {
  eval CREATE TABLE t1(
	a INT PRIMARY KEY,
	b TEXT,
	c TEXT) ENGINE=InnoDB ROW_FORMAT=$row_format;
}

if ($row_format == 'COMPRESSED') {
  eval CREATE TABLE t1(
	a INT PRIMARY KEY,
	b TEXT,
	c TEXT) ENGINE=InnoDB ROW_FORMAT=$row_format KEY_BLOCK_SIZE=4;
}

INSERT INTO t1 VALUES
        (1, REPEAT('a',10000), 'a'),
        (2, REPEAT('b',20000), 'b'),
        (3, REPEAT('c',40000), 'c'),
        (4, REPEAT('d',60000), 'd');

SELECT CHAR_LENGTH(b) FROM t1;

ALTER TABLE t1 DROP COLUMN c;

CHECK TABLE t1;

SELECT CHAR_LENGTH(b) FROM t1;

DROP TABLE t1;

# Restore global variables
if ($row_format == 'COMPRESSED')
{
  SET GLOBAL innodb_file_per_table=default;
  SET GLOBAL innodb_file_format=default;
}

if ($row_format == 'DYNAMIC')
{
  SET GLOBAL innodb_file_format=default;
}

DROP PROCEDURE populate_t1;
