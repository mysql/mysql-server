#
# wl#7277: InnoDB: Bulk Load for Create Index
#

# Create Insert Procedure
DELIMITER |;
CREATE PROCEDURE populate_t1(load_even INT)
BEGIN
	DECLARE i int DEFAULT 1;

	START TRANSACTION;
	WHILE (i <= 10000) DO
                IF i%2 = 0 AND load_even = 1 THEN
		    INSERT INTO t1 VALUES (i, i, CONCAT('a', i));
                END IF;
                IF i%2 != 0 AND load_even != 1 THEN
		    INSERT INTO t1 VALUES (i, i, CONCAT('a', i));
                END IF;
		SET i = i + 1;
	END WHILE;
	COMMIT;
END|
DELIMITER ;|

SELECT @@innodb_fill_factor;

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

  eval CREATE TABLE t1(
	class	INT,
	id	INT,
	title	VARCHAR(100)
  ) ENGINE=InnoDB ROW_FORMAT=$row_format KEY_BLOCK_SIZE=4;
}

-- disable_query_log
# Load half records
CALL populate_t1(1);
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

-- disable_query_log
# Load half records (follow up load)
CALL populate_t1(0);
-- enable_query_log
SELECT COUNT(*) FROM t1;


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
	b BLOB,
	c TEXT) ENGINE=InnoDB ROW_FORMAT=$row_format KEY_BLOCK_SIZE=4;
}

let $cnt= 5000;
-- disable_query_log
WHILE ($cnt>=4950) 
{
EVAL INSERT INTO t1 VALUES
        ($cnt, REPEAT(CONCAT('a', $cnt),2000), CONCAT('a', $cnt));
dec $cnt;
}
-- enable_query_log
ALTER TABLE t1 ADD INDEX `idx` (a,b(5));

SELECT CHAR_LENGTH(b) FROM t1 WHERE a=4975;
SELECT b=REPEAT(CONCAT('a',4975),2000) FROM t1 WHERE a=4975 AND b like 'a4975%';
UPDATE t1 SET b = REPEAT(CONCAT('b',4975),2000) WHERE a=4975 AND b like 'a4975%';
SELECT b=REPEAT(CONCAT('a',4975),2000) FROM t1 WHERE a=4975 AND b like 'a4975%';
SELECT b=REPEAT(CONCAT('b',4975),2000) FROM t1 WHERE a=4975 AND b like 'b4975%';
DELETE FROM t1 WHERE a=4975 AND b like 'b4975%';
SELECT b=REPEAT(CONCAT('b',4975),2000) FROM t1 WHERE a=4975 AND b like 'b4975%';

ALTER TABLE t1 DROP COLUMN c;

CHECK TABLE t1;

SELECT CHAR_LENGTH(b) FROM t1 WHERE a=4975;

DROP TABLE t1;

# Restore global variables
if ($row_format == 'COMPRESSED')
{
  SET GLOBAL innodb_file_per_table=default;
}

DROP PROCEDURE populate_t1;
