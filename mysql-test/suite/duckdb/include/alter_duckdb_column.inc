--write_file $MYSQL_TMP_DIR/duckdb_column_meta.inc
  CALL dbms_duckdb.query("SELECT table_schema, table_name, column_name, column_default, is_nullable, data_type, COLUMN_COMMENT FROM information_schema.columns WHERE table_name = 't'");
  --sorted_result
  SELECT TABLE_SCHEMA, TABLE_NAME, COLUMN_NAME, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE, COLUMN_COMMENT FROM information_schema.columns WHERE TABLE_NAME = 't';
  --echo
EOF


--echo #
--echo # 1) ADD AND DROP
--echo #
CREATE TABLE t(id INT PRIMARY KEY, a INT, b INT, c INT) ENGINE = DuckDB;
INSERT INTO t VALUES(1, 1, 1, 1);
INSERT INTO t(id) VALUES(2);
SELECT * FROM t;
--source $MYSQL_TMP_DIR/duckdb_column_meta.inc

eval ALTER TABLE t ADD COLUMN d INT NOT NULL DEFAULT 100 COMMENT "col d", 
                   ADD COLUMN e INT NULL DEFAULT 1000 COMMENT "",
                   ADD COLUMN f INT, ALGORITHM = $algorithm;
INSERT INTO t VALUES(3, 3, 3, 3, 3, 3, 3);
INSERT INTO t(id, a, b, c) VALUES(4, 4, 4, 4);
INSERT INTO t VALUES(5, 5, 5, 5, 5, NULL, NULL);
--error ER_BAD_NULL_ERROR
INSERT INTO t VALUES(6, 6, 6, 6, NULL, NULL, NULL);
SELECT * FROM t;
--source $MYSQL_TMP_DIR/duckdb_column_meta.inc

eval ALTER TABLE t DROP COLUMN a, DROP COLUMN d, ALGORITHM = $algorithm;
INSERT INTO t VALUES(6, 6, 6, 6, 6);
INSERT INTO t(id, c) VALUES(7, 7);
SELECT * FROM t;
--source $MYSQL_TMP_DIR/duckdb_column_meta.inc

--echo #
--echo # 3) SET AND DROP DEFAULT VALUE
--echo #

DROP TABLE t;
CREATE TABLE t(id INT PRIMARY KEY, a INT, b INT, c INT) ENGINE = DuckDB;
INSERT INTO t VALUES(1, 1, 1, 1);
INSERT INTO t(id) VALUES(2);
SELECT * FROM t;
--source $MYSQL_TMP_DIR/duckdb_column_meta.inc

eval ALTER TABLE t ALTER COLUMN b SET DEFAULT 100,
                   ALTER COLUMN c SET DEFAULT 100, ALGORITHM = $algorithm;
INSERT INTO t VALUES(3, 3, 3, 3);
INSERT INTO t(id) VALUES(4);
SELECT * FROM t;
--source $MYSQL_TMP_DIR/duckdb_column_meta.inc

eval ALTER TABLE t ALTER COLUMN b DROP DEFAULT, 
                   ALTER COLUMN c SET DEFAULT 1000, ALGORITHM = $algorithm;
INSERT INTO t VALUES(5, 5, 5, 5);
--error ER_NO_DEFAULT_FOR_FIELD
INSERT INTO t(id) VALUES(6);
INSERT INTO t(id, b) VALUES(6, 6);
SELECT * FROM t;
--source $MYSQL_TMP_DIR/duckdb_column_meta.inc

eval ALTER TABLE t ALTER COLUMN b SET DEFAULT 10000, 
                   ALTER COLUMN c DROP DEFAULT, ALGORITHM = $algorithm;
INSERT INTO t VALUES(7, 7, 7, 7);
--error ER_NO_DEFAULT_FOR_FIELD
INSERT INTO t(id) VALUES(8);
INSERT INTO t(id, c) VALUES(8, 8);
SELECT * FROM t;
--source $MYSQL_TMP_DIR/duckdb_column_meta.inc


--echo #
--echo # 3) RENAME, MODIFY AND CHANGE
--echo #
DROP TABLE t;
CREATE TABLE t(id INT PRIMARY KEY, a INT, b INT, c INT) ENGINE = DuckDB;
INSERT INTO t VALUES(1, 1, 1, 1);
INSERT INTO t(id) VALUES(2);
SELECT * FROM t;
--source $MYSQL_TMP_DIR/duckdb_column_meta.inc

eval ALTER TABLE t RENAME COLUMN a TO a1, 
                   RENAME COLUMN b TO b1,
                   RENAME COLUMN c TO c1, ALGORITHM = $algorithm;
INSERT INTO t(id, a1, b1, c1) VALUES(3, 3, 3, 3);
SELECT * FROM t;
--source $MYSQL_TMP_DIR/duckdb_column_meta.inc

# DELETE FROM t WHERE c1 IS NULL;
--disable_result_log
CALL dbms_duckdb.query("DELETE FROM test.t WHERE c1 IS NULL");
--enable_result_log
eval ALTER TABLE t MODIFY COLUMN a1 BIGINT COMMENT "col a1",
                   MODIFY COLUMN b1 BIGINT DEFAULT 100 COMMENT "",
                   MODIFY COLUMN c1 BIGINT NOT NULL DEFAULT 100, ALGORITHM = $algorithm;
INSERT INTO t VALUES(4, 4, 4, 4);
INSERT INTO t(id) VALUES(5);
INSERT INTO t VALUES(6, 6, NULL, 6);
--error ER_BAD_NULL_ERROR
INSERT INTO t VALUES(7, 7, 7, NULL);
SELECT * FROM t;
--source $MYSQL_TMP_DIR/duckdb_column_meta.inc

# DELETE FROM t WHERE b1 IS NULL;
--disable_result_log
CALL dbms_duckdb.query("DELETE FROM test.t WHERE b1 IS NULL");
--enable_result_log
eval ALTER TABLE t CHANGE a1 a INT COMMENT "col a",
                   CHANGE b1 b INT NOT NULL DEFAULT 1000,
                   CHANGE c1 c INT COMMENT "", ALGORITHM = $algorithm;
INSERT INTO t VALUES(8, 8, 8, 8);
INSERT INTO t(id) VALUES(9);
INSERT INTO t VALUES(10, 10, 10, NULL);
--error ER_BAD_NULL_ERROR
INSERT INTO t VALUES(11, 11, NULL, 11);
SELECT * FROM t;
--source $MYSQL_TMP_DIR/duckdb_column_meta.inc

# DELETE FROM t WHERE c IS NULL;
--disable_result_log
CALL dbms_duckdb.query("DELETE FROM test.t WHERE c IS NULL");
--enable_result_log
eval ALTER TABLE t RENAME COLUMN a TO a1,
                   MODIFY COLUMN b BIGINT DEFAULT 10000 COMMENT "col b",
                   CHANGE c c1 BIGINT NOT NULL DEFAULT 10000 COMMENT "col c1", ALGORITHM = $algorithm;
INSERT INTO t VALUES(12, 12, 12, 12);
INSERT INTO t(id) VALUES(13);
INSERT INTO t VALUES(14, 14, NULL, 14);
--error ER_BAD_NULL_ERROR
INSERT INTO t VALUES(15, 15, 15, NULL);
SELECT * FROM t;
--source $MYSQL_TMP_DIR/duckdb_column_meta.inc


--echo #
--echo # 4) DEFAULT NULL
--echo #
DROP TABLE t;
CREATE TABLE t(id INT PRIMARY KEY,
               a INT DEFAULT 1 COMMENT 'col a',
               b INT DEFAULT 1 COMMENT 'col b',
               c INT DEFAULT 1 COMMENT 'col c',
               d INT DEFAULT 1 COMMENT 'col d') ENGINE = DuckDB;
INSERT INTO t VALUES(1, 1, 1, 1, 1);
INSERT INTO t(id) VALUES(2);
SELECT * FROM t;
--source $MYSQL_TMP_DIR/duckdb_column_meta.inc

eval ALTER TABLE t ALTER COLUMN a SET DEFAULT NULL,
                   ALTER COLUMN b DROP DEFAULT,
                   ALTER COLUMN c SET DEFAULT 10,
                   ALTER COLUMN d SET DEFAULT 10, ALGORITHM = $algorithm;
INSERT INTO t(id, b) VALUES(3, 3);
INSERT INTO t(id, b) VALUES(4, NULL);
SELECT * FROM t;
--source $MYSQL_TMP_DIR/duckdb_column_meta.inc

eval ALTER TABLE t MODIFY a BIGINT,
                   CHANGE b b BIGINT DEFAULT NULL,
                   MODIFY COLUMN c BIGINT,
                   CHANGE d d BIGINT DEFAULT NULL, ALGORITHM = $algorithm;
INSERT INTO t(id) VALUES(5);
SELECT * FROM t;
--source $MYSQL_TMP_DIR/duckdb_column_meta.inc


--echo #
--echo # 5) DEFAULT TIMESTAMP
--echo #
DROP TABLE t;
CREATE TABLE t(id INT PRIMARY KEY,
               a TIMESTAMP NOT NULL DEFAULT '1970-01-01 12:00:00',
               b TIMESTAMP NOT NULL DEFAULT '1970-01-01 12:00:00')  ENGINE = DuckDB;
INSERT INTO t(id) VALUES(1);
INSERT INTO t VALUES(2, '2020-01-01 12:00:00', '2020-01-01 12:00:00');
eval ALTER TABLE t MODIFY COLUMN a TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
                   CHANGE COLUMN b b TIMESTAMP NOT NULL DEFAULT NOW(), ALGORITHM = $algorithm;
SET TIMESTAMP = 1303197722.534231;
INSERT INTO t(id) VALUES(3);
SELECT * FROM t;

# 'DEFAULT CURRENT_TIMESTAMP' AND 'ON UPDATE CURRENT_TIMESTAMP'
DROP TABLE t;
SET TIMESTAMP=UNIX_TIMESTAMP('2001-01-01 00:00:00');
CREATE TABLE t(id INT PRIMARY KEY, a TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, b TIMESTAMP DEFAULT NOW()) ENGINE = InnoDB;
INSERT INTO t(id) VALUES (1);
SELECT * FROM t;
# TODO: After DuckDB fix CURRENT_TIMESTAMP
# SET TIMESTAMP=UNIX_TIMESTAMP('2001-02-01 00:00:00');
# eval ALTER TABLE t ADD COLUMN c TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, ALGORITHM = $algorithm;
# INSERT INTO t(id) VALUES (2);
# SELECT * FROM t;
SET TIMESTAMP=UNIX_TIMESTAMP('2001-03-01 00:00:00');
eval ALTER TABLE t MODIFY COLUMN b TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP;
INSERT INTO t(id) VALUES (3);
SELECT * FROM t;


--echo #
--echo # 6) DEFAULT VALUE EXPRESSION
--echo #
DROP TABLE t;
CREATE TABLE t(id INT PRIMARY KEY, a INT DEFAULT (1+1), b VARCHAR(100) DEFAULT ('1+1'), c VARCHAR(100) DEFAULT (1+1)) ENGINE = DuckDB;
INSERT INTO t(id) VALUES(1);
CALL dbms_duckdb.query("INSERT INTO test.t(id) VALUES(2)");

eval ALTER TABLE t ADD COLUMN d INT DEFAULT (2+2),
                   ADD COLUMN e VARCHAR(100) DEFAULT ('2+2'),
                   ADD COLUMN f VARCHAR(100) DEFAULT (2+2), ALGORITHM = $algorithm;
INSERT INTO t(id) VALUES(3);
CALL dbms_duckdb.query("INSERT INTO test.t(id) VALUES(4)");

eval ALTER TABLE t MODIFY COLUMN a INT DEFAULT (3+3),
                   MODIFY COLUMN b VARCHAR(100) DEFAULT ('3+3'),
                   MODIFY COLUMN c VARCHAR(100) DEFAULT (3+3), ALGORITHM = $algorithm;
INSERT INTO t(id) VALUES(5);
CALL dbms_duckdb.query("INSERT INTO test.t(id) VALUES(6)");

SELECT * FROM t;
--source $MYSQL_TMP_DIR/duckdb_column_meta.inc


--echo #
--echo # 7) DEFAULT VALUE OF BIT
--echo #
DROP TABLE t;
CREATE TABLE t (id INT PRIMARY KEY, B0 BIT(1) NOT NULL DEFAULT 0, B1 BIT(32) NOT NULL DEFAULT b'1111') ENGINE=DuckDB;
INSERT INTO t(id) VALUES(1);
CALL DBMS_DUCKDB.QUERY("INSERT INTO test.t(id) VALUES(2)");

eval ALTER TABLE t ADD COLUMN B2 BIT(64) NOT NULL DEFAULT b'11111',
                   ALTER COLUMN B1 SET DEFAULT 3333, ALGORITHM = $algorithm;
INSERT INTO t(id) VALUES(3);
CALL DBMS_DUCKDB.QUERY("INSERT INTO test.t(id) VALUES(4)");

SELECT id, hex(B0), hex(B1), hex(B2) FROM t;
--source $MYSQL_TMP_DIR/duckdb_column_meta.inc


--echo #
--echo # 8) MULTIPLE DDL ABOUT COLUMNS
--echo #
DROP TABLE t;
CREATE TABLE t(id INT PRIMARY KEY,
               a INT DEFAULT 1 COMMENT 'col a',
               b INT DEFAULT 1 COMMENT 'col b', 
               c INT DEFAULT 1 COMMENT 'col c',
               d INT DEFAULT 1 COMMENT 'col d',
               e INT DEFAULT 1 COMMENT 'col e',
               f INT DEFAULT 1 COMMENT 'col f',
               g INT DEFAULT 1 COMMENT 'col g') ENGINE = DuckDB;
--source $MYSQL_TMP_DIR/duckdb_column_meta.inc

eval ALTER TABLE t ADD COLUMN h INT NOT NULL DEFAULT 2 COMMENT 'col h1',
                   DROP COLUMN g,
                   ALTER COLUMN f SET DEFAULT 2,
                   ALTER COLUMN e SET DEFAULT NULL,
                   ALTER COLUMN d DROP DEFAULT,
                   RENAME COLUMN a TO a1,
                   MODIFY COLUMN b BIGINT DEFAULT 2 COMMENT "col b1",
                   CHANGE c c1 BIGINT NOT NULL DEFAULT 2 COMMENT "col c1", ALGORITHM = $algorithm;
--source $MYSQL_TMP_DIR/duckdb_column_meta.inc


--echo #
--echo # 9) Not supported
--echo #
DROP TABLE t;
CREATE TABLE t(id INT PRIMARY KEY, a INT, b INT, c INT) ENGINE = DuckDB;
# INVISIBLE
--error ER_DUCKDB_TABLE_STRUCT_INVALID
CREATE TABLE t1(id INT PRIMARY KEY, a INT INVISIBLE) ENGINE = DuckDB;
--error ER_DUCKDB_TABLE_STRUCT_INVALID
eval ALTER TABLE t ADD COLUMN d INT INVISIBLE, ALGORITHM = $algorithm;
--error ER_DUCKDB_TABLE_STRUCT_INVALID
eval ALTER TABLE t MODIFY COLUMN c INT INVISIBLE, ALGORITHM = $algorithm;
--error ER_DUCKDB_ALTER_OPERATION_NOT_SUPPORTED
eval ALTER TABLE t ALTER COLUMN b SET INVISIBLE, ALGORITHM = $algorithm;

# AUTO_INCREMENT
--error ER_MULTIPLE_PRI_KEY
eval ALTER TABLE t MODIFY COLUMN c INT AUTO_INCREMENT KEY, ALGORITHM = $algorithm;
--error ER_DUCKDB_ALTER_OPERATION_NOT_SUPPORTED
eval ALTER TABLE t ADD COLUMN d INT AUTO_INCREMENT, ALGORITHM = $algorithm;

# ENGINE_ATTRIBUTE
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED # Storage engine 'DUCKDB' does not support ENGINE_ATTRIBUTE.
CREATE TABLE t1(id INT PRIMARY KEY, a INT) ENGINE = DuckDB ENGINE_ATTRIBUTE='{"KEY":"VALUE"}';
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED # Storage engine 'DUCKDB' does not support ENGINE_ATTRIBUTE.
eval ALTER TABLE t ADD COLUMN d INT ENGINE_ATTRIBUTE='{"KEY":"VALUE"}', ALGORITHM = $algorithm;
--error ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED # Storage engine 'DUCKDB' does not support ENGINE_ATTRIBUTE.
eval ALTER TABLE t MODIFY COLUMN c INT ENGINE_ATTRIBUTE='{"KEY":"VALUE"}', ALGORITHM = $algorithm;

if ($copy_ddl == 0)
{
  --error ER_ALTER_OPERATION_NOT_SUPPORTED
  eval ALTER TABLE t ADD COLUMN d INT AFTER a, ALGORITHM = $algorithm;
  --error ER_ALTER_OPERATION_NOT_SUPPORTED
  eval ALTER TABLE t ADD COLUMN d INT FIRST, ALGORITHM = $algorithm;
  --error ER_ALTER_OPERATION_NOT_SUPPORTED
  eval ALTER TABLE t CHANGE b b INT AFTER c, ALGORITHM = $algorithm;
  --error ER_ALTER_OPERATION_NOT_SUPPORTED
  eval ALTER TABLE t MODIFY COLUMN a INT FIRST, ALGORITHM = $algorithm;
}

# GENERATED COLUMN
CREATE TABLE t1(id INT PRIMARY KEY, a INT, b INT GENERATED ALWAYS AS (a - 1) STORED) ENGINE = DuckDB;
INSERT INTO t1(id, a) VALUES(1, 1);
if ($copy_ddl == 0)
{
  --error ER_ALTER_OPERATION_NOT_SUPPORTED
  eval ALTER TABLE t1 ADD COLUMN c INT GENERATED ALWAYS AS (a + 1) STORED, ALGORITHM = $algorithm;
  --error ER_ALTER_OPERATION_NOT_SUPPORTED
  eval ALTER TABLE t1 ADD COLUMN d INT GENERATED ALWAYS AS (a * 5) VIRTUAL, ALGORITHM = $algorithm;
  --error ER_ALTER_OPERATION_NOT_SUPPORTED
  eval ALTER TABLE t1 MODIFY COLUMN b INT GENERATED ALWAYS AS (2 * a + 1) STORED, ALGORITHM = $algorithm;
}
if ($copy_ddl == 1) {
  eval ALTER TABLE t1 ADD COLUMN c INT GENERATED ALWAYS AS (a + 1) STORED, ALGORITHM = $algorithm;
  INSERT INTO t1(id, a) VALUES(2, 2);
  SELECT * FROM t1;
  eval ALTER TABLE t1 ADD COLUMN d INT GENERATED ALWAYS AS (a * 5) VIRTUAL, ALGORITHM = $algorithm;
  INSERT INTO t1(id, a) VALUES(3, 3);
  SELECT * FROM t1;
  eval ALTER TABLE t1 MODIFY COLUMN b INT GENERATED ALWAYS AS (2 * a + 1) STORED, ALGORITHM = $algorithm;
  INSERT INTO t1(id, a) VALUES(4, 4);
}

SELECT * FROM t1;
DROP TABLE t1;


--echo #
--echo # 10) Ignore column format, secondary engine attribute, storage, they can be found in DD.
--echo #
DROP TABLE t;
CREATE TABLE t(id INT PRIMARY KEY, a INT, b INT, c INT) ENGINE = DuckDB;
eval ALTER TABLE t ADD COLUMN d INT COLUMN_FORMAT FIXED STORAGE DISK SECONDARY_ENGINE_ATTRIBUTE='{"KEY":"VALUE"}',
                   ADD COLUMN e INT COLUMN_FORMAT DYNAMIC STORAGE MEMORY SECONDARY_ENGINE_ATTRIBUTE='{"KEY":"VALUE"}', ALGORITHM = $algorithm;

eval ALTER TABLE t MODIFY COLUMN b INT COLUMN_FORMAT FIXED STORAGE DISK SECONDARY_ENGINE_ATTRIBUTE='{"KEY":"VALUE"}',
                   CHANGE c c INT COLUMN_FORMAT DYNAMIC STORAGE MEMORY SECONDARY_ENGINE_ATTRIBUTE='{"KEY":"VALUE"}', ALGORITHM = $algorithm;

SHOW CREATE TABLE t;


--echo #
--echo # 11) Drop primary key column which leads to drop primary key.
--echo #
DROP TABLE t;
CREATE TABLE t(a INT PRIMARY KEY, b INT, c INT) ENGINE = DuckDB;

--error ER_REQUIRES_PRIMARY_KEY
eval ALTER TABLE t DROP COLUMN a, ALGORITHM = $algorithm;


--echo #
--echo # 12) BUG#117725
--echo #
# Error occurs when modifying the same column twice in a DDL
# If MySQL server fix this bug, the behavior of duckdb may change, so we add test cases here
DROP TABLE t;
CREATE TABLE t(a INT PRIMARY KEY, b INT, c INT) ENGINE = DuckDB;

--error ER_BAD_FIELD_ERROR
eval ALTER TABLE t MODIFY COLUMN a INT, MODIFY COLUMN a INT, ALGORITHM = $algorithm;
--error ER_BAD_FIELD_ERROR
eval ALTER TABLE t ALTER COLUMN a SET DEFAULT 10, ALTER COLUMN a DROP DEFAULT, ALGORITHM = $algorithm;
--error ER_BAD_FIELD_ERROR
eval ALTER TABLE t ALTER COLUMN a SET DEFAULT 10, RENAME COLUMN a to c, ALGORITHM = $algorithm;
DROP TABLE t;

# Currently, DuckDB has no index, so we can drop/change columns which are included or before index.
--echo #
--echo # 13) DROP COLUMN IN PRIMARY KEY
--echo #
CREATE TABLE t(a INT, b INT, c INT, PRIMARY KEY(a, b, c)) ENGINE = DuckDB;
eval ALTER TABLE t DROP COLUMN a, ALGORITHM = $algorithm;
eval ALTER TABLE t DROP COLUMN c, ALGORITHM = $algorithm;
SHOW CREATE TABLE t;
--source $MYSQL_TMP_DIR/duckdb_column_meta.inc
DROP TABLE t;


--echo #
--echo # 14) DROP COLUMN BEFORE PRIMARY KEY
--echo #
CREATE TABLE t(a INT, b INT, c INT, d INT, PRIMARY KEY(d, b)) ENGINE = DuckDB;
eval ALTER TABLE t DROP COLUMN a, ALGORITHM = $algorithm;
eval ALTER TABLE t DROP COLUMN c, ALGORITHM = $algorithm;
SHOW CREATE TABLE t;
--source $MYSQL_TMP_DIR/duckdb_column_meta.inc
DROP TABLE t;


--echo #
--echo # 15) CHANGE COLUMN TYPE WITH INDEX
--echo #
CREATE TABLE t(a INT PRIMARY KEY, b INT) ENGINE = DuckDB;
eval ALTER TABLE t MODIFY COLUMN a VARCHAR(10), ALGORITHM = $algorithm;
SHOW CREATE TABLE t;
--source $MYSQL_TMP_DIR/duckdb_column_meta.inc
DROP TABLE t;


--echo #
--echo # 16) Cleanup
--echo #
--remove_file $MYSQL_TMP_DIR/duckdb_column_meta.inc
