#
# wl#7277: InnoDB: Bulk Load for Create Index
#

-- source include/have_innodb.inc

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

-- disable_warnings
LOAD DATA INFILE '../../std_data/wl7277_10000.dat' INTO TABLE t1 FIELDS TERMINATED BY ':';
-- enable_warnings

SELECT COUNT(*) FROM t1;

/* Create index. */
CREATE INDEX idx_id ON t1(id);

CREATE INDEX idx_title ON t1(title);

/* Check table. */
CHECK TABLE t1;

/* Select by index. */
EXPLAIN SELECT * FROM t1 WHERE id = 10;
EXPLAIN SELECT * FROM t1 WHERE title = 'AccessibleComputing';

SELECT * FROM t1 WHERE id = 10;
SELECT * FROM t1 WHERE title = 'AccessibleComputing';

SELECT * FROM t1 WHERE id = 8051;
SELECT * FROM t1 WHERE title = 'History of Dominica';

SELECT * FROM t1 WHERE id = 15517;
SELECT * FROM t1 WHERE title = 'ITSO';

SELECT * FROM t1 WHERE id = 74607;
SELECT * FROM t1 WHERE title = 'Big cats';

/*Insert/Update/Delete. */
INSERT INTO t1 VALUES(33344753, 8050, 'JapanConstitution/PreFace');
UPDATE t1 SET id = 3432, title = 'Isaac Stern' WHERE id = 10;
DELETE FROM t1 WHERE id = 8051;

SELECT * FROM t1 WHERE id = 10;
SELECT * FROM t1 WHERE title = 'AccessibleComputing';

SELECT * FROM t1 WHERE id = 3432;
SELECT * FROM t1 WHERE title = 'Isaac Stern';

SELECT * FROM t1 WHERE id = 8050;
SELECT * FROM t1 WHERE title = 'JapanConstitution/PreFace';

SELECT * FROM t1 WHERE id = 8051;
SELECT * FROM t1 WHERE title = 'History of Dominica';

/* Add column. */
ALTER TABLE t1 ADD COLUMN content TEXT;

CHECK TABLE t1;

SELECT * FROM t1 WHERE id = 10;
SELECT * FROM t1 WHERE title = 'AccessibleComputing';

SELECT * FROM t1 WHERE id = 8051;
SELECT * FROM t1 WHERE title = 'History of Dominica';

SELECT * FROM t1 WHERE id = 15517;
SELECT * FROM t1 WHERE title = 'ITSO';

SELECT * FROM t1 WHERE id = 74607;
SELECT * FROM t1 WHERE title = 'Big cats';

/* Drop column. */
ALTER TABLE t1 DROP COLUMN content;

CHECK TABLE t1;

SELECT * FROM t1 WHERE id = 10;
SELECT * FROM t1 WHERE title = 'AccessibleComputing';

SELECT * FROM t1 WHERE id = 8051;
SELECT * FROM t1 WHERE title = 'History of Dominica';

SELECT * FROM t1 WHERE id = 15517;
SELECT * FROM t1 WHERE title = 'ITSO';

SELECT * FROM t1 WHERE id = 74607;
SELECT * FROM t1 WHERE title = 'Big cats';

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
