
--echo #
--echo # BUG#57373: Multi update+InnoDB reports ER_KEY_NOT_FOUND if a
--echo #            table is updated twice
--echo #

# Results differ between storage engines.
# See multi_update_myisam.test for the MyISAM variant of this test
CREATE TABLE t1(
  pk INT,
  a INT,
  b INT,
  PRIMARY KEY (pk)
) ENGINE=InnoDB;

INSERT INTO t1 VALUES (0,0,0);
--error ER_MULTI_UPDATE_KEY_CONFLICT
UPDATE t1 AS A, t1 AS B SET A.pk = 1, B.a = 2;
SELECT * FROM t1;

CREATE VIEW v1 AS SELECT * FROM t1;
--error ER_MULTI_UPDATE_KEY_CONFLICT
UPDATE v1 AS A, t1 AS B SET A.pk = 1, B.a = 2;
SELECT * FROM t1;

UPDATE t1 AS A, t1 AS B SET A.a = 1, B.b = 2;
--echo # Should be (0,1,2)
SELECT * FROM t1;

DROP VIEW v1;
DROP TABLE t1;

--echo #
--echo # BUG#11882110: UPDATE REPORTS ER_KEY_NOT_FOUND IF TABLE IS 
--echo #               UPDATED TWICE
--echo #

# Results differ between storage engines.
# See multi_update.test for the MyISAM variant of this test
CREATE TABLE t1 ( 
  col_int_key int, 
  pk int, 
  col_int int, 
  key(col_int_key), 
  primary key (pk)
) ENGINE=InnoDB;
INSERT INTO t1 VALUES (1,2,3);

--echo
CREATE TABLE t2 ( 
  col_int_key int, 
  pk_1 int, 
  pk_2 int, 
  col_int int, 
  key(col_int_key), 
  primary key (pk_1,pk_2)
) ENGINE=InnoDB;
INSERT INTO t2 VALUES (1,2,3,4);

--echo
--error ER_MULTI_UPDATE_KEY_CONFLICT
UPDATE t1 AS A NATURAL JOIN t1 B SET A.pk=5,B.pk=7;
--echo
SELECT * FROM t1;

--echo
--error ER_MULTI_UPDATE_KEY_CONFLICT
UPDATE t2 AS A NATURAL JOIN t2 B SET A.pk_1=5,B.pk_1=7;
--echo
--error ER_MULTI_UPDATE_KEY_CONFLICT
UPDATE t2 AS A NATURAL JOIN t2 B SET A.pk_2=10,B.pk_2=11;

--echo
SELECT * FROM t2;

DROP TABLE t1,t2;

--echo #
--echo #Bug 11757486 - 49539: NON-DESCRIPTIVE ERR (ERROR 0 FROM STORAGE ENGINE)
--echo #                      WITH MULTI-TABLE UPDATE
--echo #

CREATE TABLE table_11757486 (field1 tinyint) ENGINE=INNODB;
INSERT INTO table_11757486 VALUES (0),(0);
SET SESSION SQL_MODE=default;
UPDATE IGNORE (SELECT 128 as col1) x, table_11757486 SET field1=x.col1;

--error ER_WARN_DATA_OUT_OF_RANGE
UPDATE (SELECT 128 as col1) x, table_11757486 SET field1=x.col1;

UPDATE IGNORE (SELECT 128 as col1) x, table_11757486 SET field1=x.col1;
DROP TABLE table_11757486;

--echo #
--echo # Bug#20748537 INNODB: FAILING ASSERTION: NODE->PCUR->REL_POS == BTR_PCUR_ON
--echo #

CREATE TABLE t1(c1 INT) ENGINE=INNODB;
INSERT INTO t1 VALUES (0),(0),(1);
CREATE TABLE t2(b INT) ENGINE=INNODB;
INSERT INTO t2 VALUES(0),(0);
ANALYZE TABLE t1,t2;

let $query=UPDATE t1,t2 SET t2.b=0;
# No tmp table should be used
eval EXPLAIN $query;

# sql_buffer_result should have no influence
SET @old_buf_result=@@sql_buffer_result;
SET sql_buffer_result=ON;
eval EXPLAIN $query;
eval $query;

SET sql_buffer_result=@old_buf_result;
DROP TABLE t1,t2;

--echo #
--echo # Bug#28716103 - MULTI-TABLE UPDATE OF PRIMARY KEY RESULTS IN ERROR 1032
--echo # "CAN'T FIND RECORD"
--echo #

# Make sure the size of primary key(accounting for bytes per code point)
# is bigger than 512 bytes. This will force the use of a hash field
# and a unique constraint on the hash field in the temporary table.

CREATE TABLE t1 (c1 CHAR(255) NOT NULL);
CREATE TABLE t2 (c1 CHAR(255) NOT NULL, c2 CHAR(255) NOT NULL,
                 c3 CHAR(255) NOT NULL,  PRIMARY KEY (c1, c2, c3));
INSERT INTO t1 VALUES ('x'), ('x');
INSERT INTO t2 VALUES ('x', '', '');
UPDATE t1, t2 SET t2.c2 = 'y', t2.c3 = 'y' WHERE t2.c1 = t1.c1;
SELECT * FROM t2;
DROP TABLE t1, t2;

--echo #
--echo # Bug#30674616 - UPDATE QUERY FAILS WITH "ERROR 1071 (42000): SPECIFIED
--echo # KEY WAS TOO LONG; MAX KEY LENGTH IS 1024 BYTES"
--echo #

# Make sure the size of the primary key is greater than 1024 bytes.
CREATE TABLE t1(c1 VARCHAR(350), c2 VARCHAR(350),
                c3 INT, PRIMARY KEY(c1,c2)) charset=utf8mb4;

# If the tmp storage engine is heap, irrespective of the setting of
# global.tmp_table_size, memory to store 10 rows is always allocated.
# Inserting 11 rows to force a migration to disk.

INSERT INTO t1 VALUES ('a','a',1);
INSERT INTO t1 VALUES ('a','b',2);
INSERT INTO t1 VALUES ('a','c',3);
INSERT INTO t1 VALUES ('a','d',4);
INSERT INTO t1 VALUES ('a','e',5);
INSERT INTO t1 VALUES ('a','f',6);
INSERT INTO t1 VALUES ('a','g',7);
INSERT INTO t1 VALUES ('a','h',8);
INSERT INTO t1 VALUES ('a','i',9);
INSERT INTO t1 VALUES ('a','j',10);
INSERT INTO t1 VALUES ('a','k',11);

# Set the size of the temp table to min value, to force migration to InnoDB
SET @@SESSION.tmp_table_size=1024;
SET @@SESSION.internal_tmp_mem_storage_engine=MEMORY;
UPDATE t1 a, t1 b SET a.c3=22 WHERE a.c1 = b.c1;
SELECT COUNT(*) FROM t1 WHERE c3=22;
# Reset to default values
SET @@SESSION.tmp_table_size=DEFAULT;
SET @@SESSION.internal_tmp_mem_storage_engine=DEFAULT;
DROP TABLE t1;