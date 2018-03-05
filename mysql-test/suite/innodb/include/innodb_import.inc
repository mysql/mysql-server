let MYSQLD_DATADIR =`SELECT @@datadir`;
SET @old_innodb_file_per_table = @@innodb_file_per_table;

if ($partitioning)
{
  let $c1_part=PARTITION BY HASH(c1) PARTITIONS 3;
  let $c18_part=PARTITION BY RANGE (col18)
SUBPARTITION BY HASH (col18) SUBPARTITIONS 3
(PARTITION `First_partition` VALUES LESS THAN (10),
 PARTITION `Last_partition` VALUES LESS THAN MAXVALUE);
  let $col7_part=PARTITION BY KEY (col_7_binary) PARTITIONS 3;
  let $col1_part=PARTITION BY HASH (col_1_int) PARTITIONS 3;
  let $col2_part=PARTITION BY KEY (col_2_varchar) PARTITIONS 3;
  let $i_part=PARTITION BY HASH (i) PARTITIONS 3;
  let $col1_key_part=PARTITION BY KEY (col_1) PARTITIONS 3;
  let $col18_part=PARTITION BY HASH (col18) PARTITIONS 3;
  # Set environment variable for perl snippets
  let PARTITIONS=1;
}
SET GLOBAL innodb_file_per_table = 1;
SELECT @@innodb_file_per_table;

--echo # Following testcases are created from JET cases (where import
--echo # export instance are different servers)
--echo # Here test will be run on same import and export instance.

--echo #
--echo # case 1 Simple export, discard and import.
--echo #
eval CREATE TABLE t1 (c1 INT) ENGINE = InnoDB
$c1_part;
INSERT INTO t1 VALUES (1),(123),(331);
SELECT c1 FROM t1;
FLUSH TABLES t1 FOR EXPORT;
SELECT * FROM t1 ORDER BY c1;

perl;
require 'include/innodb-util.inc';
if ($ENV{'PARTITIONS'})
{
ib_backup_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
} else {
ib_backup_tablespaces("test", "t1");
}
EOF

UNLOCK TABLES;

DROP TABLE t1;
eval CREATE TABLE t1 (c1 INT) ENGINE = InnoDB
$c1_part;
ALTER TABLE t1 DISCARD TABLESPACE;


perl;
require 'include/innodb-util.inc';
if ($ENV{'PARTITIONS'})
{
ib_discard_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
ib_restore_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
} else {
ib_discard_tablespaces("test", "t1");
ib_restore_tablespaces("test", "t1");
}
EOF

ALTER TABLE t1 IMPORT TABLESPACE;
SELECT * FROM t1 ORDER BY c1;
DROP TABLE t1;

--echo #
--echo # case 2 With indexes and more datatypes.
--echo #
eval CREATE TABLE t1 (
col1 BIT(1),
col2 BOOLEAN,
col3 TINYINT,
col4 SMALLINT,
col5 MEDIUMINT,
col6 INT,
col7 BIGINT,
col8 FLOAT (14,3),
col9 DOUBLE (14,3),
col10 VARCHAR(20),
col11 TEXT,
col12 ENUM('a','b','c'),
col13 TEXT,
col14 CHAR(20),
col15 VARBINARY (400),
col16 BINARY(40),
col17 BLOB (400),
col18 INT NOT NULL PRIMARY KEY,
col19 DATE,
col20 DATETIME,
col21 TIMESTAMP NULL,
col22 TIME,
col23 YEAR) ENGINE = InnoDB
$c18_part;

CREATE INDEX idx1 ON t1(col18);
CREATE INDEX prefix_idx ON t1(col14 (10));
if (!$partitioning)
{
CREATE UNIQUE INDEX idx2 ON t1(col12);
CREATE UNIQUE INDEX idx3 ON t1(col8);
}
INSERT INTO t1 VALUES
(1,1,-128,32767,-8388608,2147483647,-9223372036854775808, 92233720368.222,
-92233720368.222,'aaa', + 'aaaaaaaaaa','b','bbbbb','ccccc',
REPEAT('d',40),REPEAT('d',40),REPEAT('d',40),1,'1000-01-01',
'3000-12-31 23:59:59.99','1990-01-01 00:00:01.00',
'01:59:59.00','1901');
INSERT INTO t1 VALUES
(NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
NULL,NULL,NULL,3,NULL,NULL,NULL,NULL,NULL);
--error ER_BAD_NULL_ERROR
INSERT INTO  t1 VALUES (NULL,NULL,NULL,NULL,NULL,NULL,NULL,
NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL);
--error ER_DUP_ENTRY
INSERT INTO  t1 VALUES (NULL,NULL,NULL,NULL,NULL,NULL,NULL,
NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,1,NULL,NULL,NULL,NULL,NULL);

FLUSH TABLES t1 WITH READ LOCK;
SELECT * FROM t1;

perl;
require 'include/innodb-util.inc';
if ($ENV{'PARTITIONS'})
{
ib_backup_tablespaces("test",
"t1#P#First_partition#SP#First_partitionsp0",
"t1#P#First_partition#SP#First_partitionsp1",
"t1#P#First_partition#SP#First_partitionsp2",
"t1#P#Last_partition#SP#Last_partitionsp0",
"t1#P#Last_partition#SP#Last_partitionsp1",
"t1#P#Last_partition#SP#Last_partitionsp2");
} else {
ib_backup_tablespaces("test", "t1");
}
EOF
UNLOCK TABLES;

DROP TABLE t1;

eval CREATE TABLE t1 (
col1 BIT(1),
col2 BOOLEAN,
col3 TINYINT,
col4 SMALLINT,
col5 MEDIUMINT,
col6 INT,
col7 BIGINT,
col8 FLOAT (14,3),
col9 DOUBLE (14,3),
col10 VARCHAR(20),
col11 TEXT,
col12 ENUM('a','b','c'),
col13 TEXT,
col14 CHAR(20),
col15 VARBINARY (400),
col16 BINARY(40),
col17 BLOB (400),
col18 INT NOT NULL PRIMARY KEY,
col19 DATE,
col20 DATETIME,
col21 TIMESTAMP NULL,
col22 TIME,
col23 YEAR) ENGINE = InnoDB
$c18_part;

CREATE INDEX idx1 ON t1(col18);
CREATE INDEX prefix_idx ON t1(col14 (10));
if (!$partitioning)
{
CREATE UNIQUE INDEX idx2 ON t1(col12);
CREATE UNIQUE INDEX idx3 ON t1(col8);
}

ALTER TABLE t1 DISCARD TABLESPACE;

perl;
require 'include/innodb-util.inc';
if ($ENV{'PARTITIONS'})
{
ib_discard_tablespaces("test",
"t1#P#First_partition#SP#First_partitionsp0",
"t1#P#First_partition#SP#First_partitionsp1",
"t1#P#First_partition#SP#First_partitionsp2",
"t1#P#Last_partition#SP#Last_partitionsp0",
"t1#P#Last_partition#SP#Last_partitionsp1",
"t1#P#Last_partition#SP#Last_partitionsp2");
ib_restore_tablespaces("test",
"t1#P#First_partition#SP#First_partitionsp0",
"t1#P#First_partition#SP#First_partitionsp1",
"t1#P#First_partition#SP#First_partitionsp2",
"t1#P#Last_partition#SP#Last_partitionsp0",
"t1#P#Last_partition#SP#Last_partitionsp1",
"t1#P#Last_partition#SP#Last_partitionsp2");
} else {
ib_discard_tablespaces("test", "t1");
ib_restore_tablespaces("test", "t1");
}
EOF

ALTER TABLE t1 IMPORT TABLESPACE;
CHECK TABLE t1;

SELECT * FROM t1;

DROP TABLE t1;

--echo #
--echo # case 3 - with blob objects
--echo #

eval CREATE TABLE t1 (
col_1_varbinary VARBINARY (4000),
col_2_varchar VARCHAR (4000),
col_3_text TEXT (4000),
col_4_blob BLOB (4000),
col_5_text TEXT (4000),
col_6_varchar VARCHAR (4000),
col_7_binary BINARY (255)
) ROW_FORMAT=DYNAMIC ENGINE = InnoDB
$col7_part;

INSERT INTO t1 VALUES(
REPEAT('a', 4000),REPEAT('o', 4000),REPEAT('a', 4000), REPEAT('o', 4000),
REPEAT('a', 4000),REPEAT('a', 4000),REPEAT('a', 255));

SELECT  col_1_varbinary = REPEAT("a", 4000),
col_2_varchar = REPEAT("o", 4000),
col_3_text = REPEAT("a", 4000),
col_4_blob = REPEAT("o", 4000),
col_5_text = REPEAT("a", 4000),
col_6_varchar = REPEAT("a", 4000),
col_7_binary = REPEAT("a", 255)
FROM t1;
FLUSH TABLES t1 FOR EXPORT;

perl;
require 'include/innodb-util.inc';
if ($ENV{'PARTITIONS'})
{
ib_backup_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
} else {
ib_backup_tablespaces("test", "t1");
}
EOF

UNLOCK TABLES;

DROP TABLE t1;

eval CREATE TABLE t1 (
col_1_varbinary VARBINARY (4000),
col_2_varchar VARCHAR (4000),
col_3_text TEXT (4000),
col_4_blob BLOB (4000),
col_5_text TEXT (4000),
col_6_varchar VARCHAR (4000),
col_7_binary BINARY (255)
) ROW_FORMAT=DYNAMIC ENGINE = InnoDB
$col7_part;

ALTER TABLE t1 DISCARD TABLESPACE;

perl;
require 'include/innodb-util.inc';
if ($ENV{'PARTITIONS'})
{
ib_discard_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
ib_restore_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
} else {
ib_discard_tablespaces("test", "t1");
ib_restore_tablespaces("test", "t1");
}
EOF

ALTER TABLE t1 IMPORT TABLESPACE;

SELECT  col_1_varbinary = REPEAT("a", 4000),
col_2_varchar = REPEAT("o", 4000),
col_3_text = REPEAT("a", 4000),
col_4_blob = REPEAT("o", 4000),
col_5_text = REPEAT("a", 4000),
col_6_varchar = REPEAT("a", 4000),
col_7_binary = REPEAT("a", 255)
FROM t1;

DROP TABLE t1;

--echo #
--echo # case 4 - transportable tablespace with auto_increment
--echo #
eval CREATE TABLE t1 (
col_1_int INT AUTO_INCREMENT,
col_2_varchar VARCHAR (20),
PRIMARY KEY (col_1_int)) ENGINE = InnoDB
$col1_part;

INSERT INTO t1 VALUES (1,'a1'),(2,'a2'),(3,'a3');
INSERT INTO t1 (col_2_varchar) VALUES ('a4'),('a5'),('a6');

SELECT * FROM t1 ORDER BY col_1_int;
FLUSH TABLES t1 FOR EXPORT;

perl;
require 'include/innodb-util.inc';
if ($ENV{'PARTITIONS'})
{
ib_backup_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
} else {
ib_backup_tablespaces("test", "t1");
}
EOF

UNLOCK TABLES;

DROP TABLE t1;

eval CREATE TABLE t1 (
col_1_int INT AUTO_INCREMENT,
col_2_varchar VARCHAR (20),
PRIMARY KEY (col_1_int)) ENGINE = InnoDB
$col1_part;
ALTER TABLE t1 DISCARD TABLESPACE;


perl;
require 'include/innodb-util.inc';
if ($ENV{'PARTITIONS'})
{
ib_discard_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
ib_restore_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
} else {
ib_discard_tablespaces("test", "t1");
ib_restore_tablespaces("test", "t1");
}
EOF

ALTER TABLE t1 IMPORT TABLESPACE;
SELECT * FROM t1 ORDER BY col_1_int;

# error on inserting duplicate value
--error ER_DUP_ENTRY
INSERT INTO t1 VALUES (1,'a1');
# insert new values
INSERT INTO t1(col_2_varchar) VALUES ('a101'),('a102'),('a103');
SELECT * FROM t1 ORDER BY col_1_int;
# check table can be altered
ALTER TABLE t1 MODIFY col_1_int BIGINT;
SELECT * FROM t1 ORDER BY col_1_int;

DROP TABLE t1;

--echo #
--echo # case 5 - check with primary and foreign key
--echo #
if (!$partitioning)
{
CREATE TABLE t1 (
col_1_int INT,col_2_varchar VARCHAR (20),
PRIMARY KEY (col_2_varchar)) ENGINE = InnoDB;

CREATE TABLE t1_fk (
col_1_int INT,col_2_varchar VARCHAR (20),
PRIMARY KEY (col_1_int),
FOREIGN KEY (col_2_varchar) REFERENCES t1(col_2_varchar)
) ENGINE = InnoDB;

INSERT INTO t1 VALUES (1,'a1'),(2,'a2'),(3,'a3'),(4,'a4'),(5,'a5');
INSERT INTO t1_fk VALUES (1,'a1'),(2,'a2'),(3,'a3');

SELECT * FROM t1;
SELECT * FROM t1_fk;
FLUSH TABLES t1,t1_fk FOR EXPORT;

perl;
require 'include/innodb-util.inc';
ib_backup_tablespaces("test", "t1");
ib_backup_tablespaces("test", "t1_fk");
EOF

UNLOCK TABLES;

DROP TABLE t1_fk,t1;

CREATE TABLE t1 (
col_1_int INT,col_2_varchar VARCHAR (20),
PRIMARY KEY (col_2_varchar)) ENGINE = InnoDB;

CREATE TABLE t1_fk (
col_1_int INT,col_2_varchar VARCHAR (20),
PRIMARY KEY (col_1_int),
FOREIGN KEY (col_2_varchar) REFERENCES t1(col_2_varchar)
) ENGINE = InnoDB;

--echo # Alter table discard table is not allowed with foreign_key_checks = 1
SET foreign_key_checks = 0;
ALTER TABLE t1_fk DISCARD TABLESPACE;
ALTER TABLE t1 DISCARD TABLESPACE;
SET foreign_key_checks = 1;

perl;
require 'include/innodb-util.inc';
ib_discard_tablespaces("test", "t1");
ib_discard_tablespaces("test", "t1_fk");
ib_restore_tablespaces("test", "t1");
ib_restore_tablespaces("test", "t1_fk");
EOF

ALTER TABLE t1 IMPORT TABLESPACE;
ALTER TABLE t1_fk IMPORT TABLESPACE;
SELECT * FROM t1;
SELECT * FROM t1_fk;

--echo # Enter Invalid value: PK-FK relationship violation
--error ER_NO_REFERENCED_ROW_2
INSERT INTO t1_fk VALUES (100,'a100');

SET AUTOCOMMIT = 0;
INSERT INTO t1_fk VALUES (4,'a4'),(5,'a5');
ROLLBACK;
SELECT * FROM t1_fk;

DROP TABLE t1_fk,t1;
SET AUTOCOMMIT = 1;
--remove_file $MYSQLTEST_VARDIR/tmp/t1_fk.cfg
--remove_file $MYSQLTEST_VARDIR/tmp/t1_fk.ibd
}
if ($partitioning)
{
--echo # Foreign Keys not supported by partitioned tables.
}

--echo #
--echo # case 6 - transportable tablespace with transactions
--echo #
eval CREATE TABLE t1 (
col_1_int int,col_2_varchar VARCHAR (20),
PRIMARY KEY (col_2_varchar)) ENGINE = InnoDB
$col2_part;
SET AUTOCOMMIT = 0;
INSERT INTO t1 VALUES (1,'a1'),(2,'a2');
SELECT * FROM t1;
COMMIT;
INSERT INTO t1 VALUES (3,'a3'),(4,'a4');
ROLLBACK;
INSERT INTO t1 VALUES (5,'a5'),(6,'a6');
COMMIT;
SELECT * FROM  t1 ORDER BY col_1_int;
FLUSH TABLES t1 FOR EXPORT;

perl;
require 'include/innodb-util.inc';
if ($ENV{'PARTITIONS'})
{
ib_backup_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
} else {
ib_backup_tablespaces("test", "t1");
}
EOF

UNLOCK TABLES;

DROP TABLE t1;
eval CREATE TABLE t1 (
col_1_int int,col_2_varchar VARCHAR (20),
PRIMARY KEY (col_2_varchar)) ENGINE = InnoDB
$col2_part;
ALTER TABLE t1 DISCARD TABLESPACE;
SET AUTOCOMMIT = 0;

perl;
require 'include/innodb-util.inc';
if ($ENV{'PARTITIONS'})
{
ib_discard_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
ib_restore_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
} else {
ib_discard_tablespaces("test", "t1");
ib_restore_tablespaces("test", "t1");
}
EOF

ALTER TABLE t1 IMPORT TABLESPACE;

INSERT INTO t1 VALUES (7,'a7'),(8,'a8');
COMMIT;
INSERT INTO t1 VALUES (9,'a9'),(10,'a10');
ROLLBACK;
INSERT INTO t1 VALUES (11,'a11'),(12,'a12');
COMMIT;
SELECT * FROM  t1 ORDER BY col_1_int;
SET AUTOCOMMIT = 1;

DROP TABLE t1;

--echo #
--echo # case 7 - Transportable tablespace with transaction
--echo # (earlier failed with jet)
--echo #

eval CREATE TABLE t1 (i int) ENGINE = InnoDB
$i_part;
INSERT INTO t1 VALUES (100),(200),(300);
SELECT * FROM t1 ORDER BY i;
FLUSH TABLES t1 FOR EXPORT;

perl;
require 'include/innodb-util.inc';
if ($ENV{'PARTITIONS'})
{
ib_backup_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
} else {
ib_backup_tablespaces("test", "t1");
}
EOF

UNLOCK TABLES;

DROP TABLE t1;
eval CREATE TABLE t1 (i int) ENGINE = InnoDB
$i_part;
ALTER TABLE t1 DISCARD TABLESPACE;


perl;
require 'include/innodb-util.inc';
if ($ENV{'PARTITIONS'})
{
ib_discard_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
ib_restore_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
} else {
ib_discard_tablespaces("test", "t1");
ib_restore_tablespaces("test", "t1");
}
EOF

ALTER TABLE t1 IMPORT TABLESPACE;
SELECT * FROM t1 ORDER BY i;
SET AUTOCOMMIT = 0;
INSERT INTO t1 VALUES (101),(102),(103);
COMMIT;
SELECT * FROM t1 ORDER BY i;
SET AUTOCOMMIT = 1;
DROP TABLE t1;

--echo #
--echo # case 8 - negative cases
--echo #
eval CREATE TABLE t1 (i int) ENGINE = InnoDB
$i_part;
INSERT INTO t1 VALUES (100),(200),(300);
SELECT * FROM t1 ORDER BY i;
FLUSH TABLES t1 FOR EXPORT;

perl;
require 'include/innodb-util.inc';
if ($ENV{'PARTITIONS'})
{
ib_backup_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
} else {
ib_backup_tablespaces("test", "t1");
}
EOF

UNLOCK TABLES;

--echo # try if we can flush again
FLUSH TABLES t1 FOR EXPORT;
UNLOCK TABLES;

DROP TABLE t1;

--echo # create table with incorrect schema
eval CREATE TABLE t1 (i bigint) ENGINE = InnoDB
$i_part;
ALTER TABLE t1 DISCARD TABLESPACE;

perl;
require 'include/innodb-util.inc';
if ($ENV{'PARTITIONS'})
{
ib_discard_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
ib_restore_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
} else {
ib_discard_tablespaces("test", "t1");
ib_restore_tablespaces("test", "t1");
}
EOF

--echo # error as mismatch in column data type
--error ER_TABLE_SCHEMA_MISMATCH
ALTER TABLE t1 IMPORT TABLESPACE;

--echo # explicitly delete ibd file before creating table with correct schema
perl;
require 'include/innodb-util.inc';
if ($ENV{'PARTITIONS'})
{
ib_unlink_tablespace("test", "t1#P#p0");
ib_unlink_tablespace("test", "t1#P#p1");
ib_unlink_tablespace("test", "t1#P#p2");
} else {
ib_unlink_tablespace("test", "t1");
}
EOF

DROP TABLE t1;

eval CREATE TABLE t1 (i int) ENGINE = InnoDB
$i_part;
ALTER TABLE t1 DISCARD TABLESPACE;

perl;
require 'include/innodb-util.inc';
if ($ENV{'PARTITIONS'})
{
ib_discard_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
ib_restore_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
} else {
ib_discard_tablespaces("test", "t1");
ib_restore_tablespaces("test", "t1");
}
EOF

--echo # Import should succeed
ALTER TABLE t1 IMPORT TABLESPACE;
--echo # Try to import twice
--replace_regex  /#p#/#P#/  /#sp#/#SP#/
--error ER_TABLESPACE_EXISTS
ALTER TABLE t1 IMPORT TABLESPACE;
SELECT * FROM t1 ORDER BY i;

SET AUTOCOMMIT = 0;
INSERT INTO t1 VALUES (101),(102),(103);
COMMIT;
SELECT * FROM t1 ORDER BY i;
DROP TABLE t1;
eval CREATE TABLE t1 (i int) ENGINE = InnoDB
$i_part;
ALTER TABLE t1 DISCARD TABLESPACE;
--echo # do not delete ibd file and try to import
perl;
require 'include/innodb-util.inc';
if ($ENV{'PARTITIONS'})
{
ib_restore_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
} else {
ib_restore_tablespaces("test", "t1");
}
EOF
ALTER TABLE t1 IMPORT TABLESPACE;
DROP TABLE t1;
SET AUTOCOMMIT = 1;

--echo #
--echo # case 9 - empty table import
--echo #
eval CREATE TABLE t1 (c1 INT) ENGINE = InnoDB
$c1_part;
SET AUTOCOMMIT = 0;
INSERT INTO t1 VALUES (1),(123),(331);
SELECT c1 FROM t1;
ROLLBACK;
FLUSH TABLES t1 FOR EXPORT;

perl;
require 'include/innodb-util.inc';
if ($ENV{'PARTITIONS'})
{
ib_backup_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
} else {
ib_backup_tablespaces("test", "t1");
}
EOF

UNLOCK TABLES;

DROP TABLE t1;
eval CREATE TABLE t1 (c1 INT) ENGINE = InnoDB
$c1_part;
ALTER TABLE t1 DISCARD TABLESPACE;


perl;
require 'include/innodb-util.inc';
if ($ENV{'PARTITIONS'})
{
ib_discard_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
ib_restore_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
} else {
ib_discard_tablespaces("test", "t1");
ib_restore_tablespaces("test", "t1");
}
EOF

ALTER TABLE t1 IMPORT TABLESPACE;
SELECT * FROM t1 ORDER BY c1;
DROP TABLE t1;
SET AUTOCOMMIT = 1;

--echo #
--echo # case 10 - Transportable tablespaces with prefix index
--echo #

eval CREATE TABLE t1 (col_1 CHAR (255),
col_2 VARCHAR (255), col_3 VARCHAR (255),
col_4 VARCHAR (255),col_5 VARCHAR (255),
col_6 text (255), col_7 text (255),
col_8 text (255),col_9 text (255),
col_10 BLOB (255),col_11  BLOB (255),
col_12 BLOB (255), col_13 BLOB (255),
col_14 BLOB (255), col_15 int) ENGINE = InnoDB charset latin1
$col1_key_part;

CREATE INDEX prefix_idx ON t1(
col_1 (50),col_2 (50),col_3 (50),
col_4 (50),col_5 (50),col_6 (50),
col_7 (50),col_8 (50),col_9 (50),
col_10 (50),col_11 (50),col_12 (50),
col_13(50));

INSERT INTO t1  VALUES (
REPEAT("col1_00001",10),REPEAT("col2_00001",10),REPEAT("col3_00001",10),
REPEAT("col4_00001",10),REPEAT("col5_00001",10),REPEAT("col6_00001",10),
REPEAT("col7_00001",10),REPEAT("col8_00001",10),REPEAT("col9_00001",10),
REPEAT("col10_00001",10),REPEAT("col11_00001",10),REPEAT("col12_00001",10),
REPEAT("col13_00001",10),REPEAT("col14_00001",10),1);

SELECT
col_1 = REPEAT("col1_00001",10),
col_2 = REPEAT("col2_00001",10),
col_3 = REPEAT("col3_00001",10),
col_4 = REPEAT("col4_00001",10),
col_5 = REPEAT("col5_00001",10),
col_6 = REPEAT("col6_00001",10),
col_7 = REPEAT("col7_00001",10),
col_8 = REPEAT("col8_00001",10),
col_9 = REPEAT("col9_00001",10),
col_10 = REPEAT("col10_00001",10),
col_11 = REPEAT("col11_00001",10),
col_12 = REPEAT("col12_00001",10),
col_13 = REPEAT("col13_00001",10),
col_14 = REPEAT("col14_00001",10),
col_15
FROM t1;

FLUSH TABLES t1 FOR EXPORT;

perl;
require 'include/innodb-util.inc';
if ($ENV{'PARTITIONS'})
{
ib_backup_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
} else {
ib_backup_tablespaces("test", "t1");
}
EOF

UNLOCK TABLES;

DROP TABLE t1;

eval CREATE TABLE t1 (col_1 CHAR (255),
col_2 VARCHAR (255), col_3 VARCHAR (255),
col_4 VARCHAR (255),col_5 VARCHAR (255),
col_6 text (255), col_7 text (255),
col_8 text (255),col_9 text (255),
col_10 BLOB (255),col_11  BLOB (255),
col_12 BLOB (255), col_13 BLOB (255),
col_14 BLOB (255), col_15 int) ENGINE = InnoDB charset latin1
$col1_key_part;

CREATE INDEX prefix_idx ON t1(
col_1 (50),col_2 (50),col_3 (50),
col_4 (50),col_5 (50),col_6 (50),
col_7 (50),col_8 (50),col_9 (50),
col_10 (50),col_11 (50),col_12 (50),
col_13(50));

ALTER TABLE t1 DISCARD TABLESPACE;

perl;
require 'include/innodb-util.inc';
if ($ENV{'PARTITIONS'})
{
ib_discard_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
ib_restore_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
} else {
ib_discard_tablespaces("test", "t1");
ib_restore_tablespaces("test", "t1");
}
EOF

ALTER TABLE t1 IMPORT TABLESPACE;

SELECT
col_1 = REPEAT("col1_00001",10),
col_2 = REPEAT("col2_00001",10),
col_3 = REPEAT("col3_00001",10),
col_4 = REPEAT("col4_00001",10),
col_5 = REPEAT("col5_00001",10),
col_6 = REPEAT("col6_00001",10),
col_7 = REPEAT("col7_00001",10),
col_8 = REPEAT("col8_00001",10),
col_9 = REPEAT("col9_00001",10),
col_10 = REPEAT("col10_00001",10),
col_11 = REPEAT("col11_00001",10),
col_12 = REPEAT("col12_00001",10),
col_13 = REPEAT("col13_00001",10),
col_14 = REPEAT("col14_00001",10),
col_15
FROM t1;

DROP TABLE t1;

--echo #
--echo # case 11 - Transportable tablespaces with secondary index
--echo #

eval CREATE TABLE t1 (col_1 CHAR (255),
col_2 VARCHAR (255), col_3 VARCHAR (255),
col_4 VARCHAR (255),col_5 VARCHAR (255),
col_6 text (255), col_7 text (255),
col_8 text (255),col_9 text (255),
col_10 BLOB (255),col_11  BLOB (255),
col_12 BLOB (255), col_13 BLOB (255),
col_14 BLOB (255), col_15 int) ENGINE = InnoDB charset latin1
$col1_key_part;

CREATE INDEX idx1 ON t1(col_1);
CREATE INDEX idx2 ON t1(col_2);
CREATE INDEX idx3 ON t1(col_3);
CREATE INDEX idx4 ON t1(col_4);
CREATE INDEX idx5 ON t1(col_5);
CREATE INDEX idx6 ON t1(col_6(255));
CREATE INDEX idx7 ON t1(col_7(255));
CREATE INDEX idx8 ON t1(col_8(255));
CREATE INDEX idx9 ON t1(col_9(255));
CREATE INDEX idx10 ON t1(col_10(255));
CREATE INDEX idx11 ON t1(col_11(255));
CREATE INDEX idx12 ON t1(col_12(255));
CREATE INDEX idx13 ON t1(col_13(255));
CREATE INDEX idx14 ON t1(col_14(255));

INSERT INTO t1  VALUES (
REPEAT("col1_00001",10),REPEAT("col2_00001",10),REPEAT("col3_00001",10),
REPEAT("col4_00001",10),REPEAT("col5_00001",10),REPEAT("col6_00001",10),
REPEAT("col7_00001",10),REPEAT("col8_00001",10),REPEAT("col9_00001",10),
REPEAT("col10_00001",10),REPEAT("col11_00001",10),REPEAT("col12_00001",10),
REPEAT("col13_00001",10),REPEAT("col14_00001",10),1);

SELECT
col_1 = REPEAT("col1_00001",10),
col_2 = REPEAT("col2_00001",10),
col_3 = REPEAT("col3_00001",10),
col_4 = REPEAT("col4_00001",10),
col_5 = REPEAT("col5_00001",10),
col_6 = REPEAT("col6_00001",10),
col_7 = REPEAT("col7_00001",10),
col_8 = REPEAT("col8_00001",10),
col_9 = REPEAT("col9_00001",10),
col_10 = REPEAT("col10_00001",10),
col_11 = REPEAT("col11_00001",10),
col_12 = REPEAT("col12_00001",10),
col_13 = REPEAT("col13_00001",10),
col_14 = REPEAT("col14_00001",10),
col_15
FROM t1;

FLUSH TABLES t1 FOR EXPORT;

perl;
require 'include/innodb-util.inc';
if ($ENV{'PARTITIONS'})
{
ib_backup_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
} else {
ib_backup_tablespaces("test", "t1");
}
EOF

UNLOCK TABLES;

DROP TABLE t1;

eval CREATE TABLE t1 (col_1 CHAR (255),
col_2 VARCHAR (255), col_3 VARCHAR (255),
col_4 VARCHAR (255),col_5 VARCHAR (255),
col_6 text (255), col_7 text (255),
col_8 text (255),col_9 text (255),
col_10 BLOB (255),col_11  BLOB (255),
col_12 BLOB (255), col_13 BLOB (255),
col_14 BLOB (255), col_15 int) ENGINE = InnoDB charset latin1
$col1_key_part;

CREATE INDEX idx1 ON t1(col_1);
CREATE INDEX idx2 ON t1(col_2);
CREATE INDEX idx3 ON t1(col_3);
CREATE INDEX idx4 ON t1(col_4);
CREATE INDEX idx5 ON t1(col_5);
CREATE INDEX idx6 ON t1(col_6(255));
CREATE INDEX idx7 ON t1(col_7(255));
CREATE INDEX idx8 ON t1(col_8(255));
CREATE INDEX idx9 ON t1(col_9(255));
CREATE INDEX idx10 ON t1(col_10(255));
CREATE INDEX idx11 ON t1(col_11(255));
CREATE INDEX idx12 ON t1(col_12(255));
CREATE INDEX idx13 ON t1(col_13(255));
CREATE INDEX idx14 ON t1(col_14(255));

ALTER TABLE t1 DISCARD TABLESPACE;


perl;
require 'include/innodb-util.inc';
if ($ENV{'PARTITIONS'})
{
ib_discard_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
ib_restore_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
} else {
ib_discard_tablespaces("test", "t1");
ib_restore_tablespaces("test", "t1");
}
EOF

ALTER TABLE t1 IMPORT TABLESPACE;

SELECT
col_1 = REPEAT("col1_00001",10),
col_2 = REPEAT("col2_00001",10),
col_3 = REPEAT("col3_00001",10),
col_4 = REPEAT("col4_00001",10),
col_5 = REPEAT("col5_00001",10),
col_6 = REPEAT("col6_00001",10),
col_7 = REPEAT("col7_00001",10),
col_8 = REPEAT("col8_00001",10),
col_9 = REPEAT("col9_00001",10),
col_10 = REPEAT("col10_00001",10),
col_11 = REPEAT("col11_00001",10),
col_12 = REPEAT("col12_00001",10),
col_13 = REPEAT("col13_00001",10),
col_14 = REPEAT("col14_00001",10),
col_15
FROM t1;

--echo # perform transaction on imported table
SET AUTOCOMMIT = 0;
INSERT INTO t1(col_15) VALUES (15000),(16000);
SELECT col_15 FROM t1 WHERE col_15 > 11000;
ROLLBACK;
SELECT col_15 FROM t1 WHERE col_15 > 11000;
INSERT INTO t1(col_15) VALUES (15000),(16000);
COMMIT;
SELECT col_15 FROM t1 WHERE col_15 > 11000;
--echo # Perform DDL operations on imported table.
ALTER TABLE t1 DROP INDEX idx1;
ALTER TABLE t1 DROP INDEX idx6;
ALTER TABLE t1 DROP INDEX idx10;

SELECT
col_1 = REPEAT("col1_00001",10),
col_2 = REPEAT("col2_00001",10),
col_3 = REPEAT("col3_00001",10),
col_4 = REPEAT("col4_00001",10),
col_5 = REPEAT("col5_00001",10),
col_6 = REPEAT("col6_00001",10),
col_7 = REPEAT("col7_00001",10),
col_8 = REPEAT("col8_00001",10),
col_9 = REPEAT("col9_00001",10),
col_10 = REPEAT("col10_00001",10),
col_11 = REPEAT("col11_00001",10),
col_12 = REPEAT("col12_00001",10),
col_13 = REPEAT("col13_00001",10),
col_14 = REPEAT("col14_00001",10),
col_15
FROM t1;

ALTER TABLE t1 ADD INDEX idx1 (col_1);
ALTER TABLE t1 ADD INDEX idx6 (col_1(255));
ALTER TABLE t1 ADD INDEX idx10 (col_10(255));


SELECT
col_1 = REPEAT("col1_00001",10),
col_2 = REPEAT("col2_00001",10),
col_3 = REPEAT("col3_00001",10),
col_4 = REPEAT("col4_00001",10),
col_5 = REPEAT("col5_00001",10),
col_6 = REPEAT("col6_00001",10),
col_7 = REPEAT("col7_00001",10),
col_8 = REPEAT("col8_00001",10),
col_9 = REPEAT("col9_00001",10),
col_10 = REPEAT("col10_00001",10),
col_11 = REPEAT("col11_00001",10),
col_12 = REPEAT("col12_00001",10),
col_13 = REPEAT("col13_00001",10),
col_14 = REPEAT("col14_00001",10),
col_15
FROM t1;

DROP TABLE t1;
SET AUTOCOMMIT = 1;

--echo #
--echo # case 12 - Transportable tablespaces with trigger / view
--echo #

eval CREATE TABLE t1(col1 bit(1),
col2 boolean,col3 tinyint, col4 smallint,
col5 mediumint,col6 int, col7 bigint,
col8 float (14,3),col9 double (14,3),
col10 VARCHAR(20) CHARACTER SET utf8,
col11 TEXT CHARACTER SET binary,
col12 ENUM('a','b','c') CHARACTER SET binary,
col13 TEXT CHARACTER SET latin1 COLLATE latin1_general_cs,
col14 CHAR(20), col15 VARBINARY (400),
col16 BINARY(40), col17 BLOB (400),
col18 int not null primary key,
col19 DATE,col20 DATETIME, col21 TIMESTAMP NULL,
col22 TIME, col23 YEAR) ENGINE = InnoDB
$col18_part;

--echo # table for trigger action
CREATE TABLE trigger_table (i int) ENGINE = InnoDB;
# define trigger
CREATE  TRIGGER tri AFTER INSERT ON  t1
FOR EACH ROW INSERT INTO trigger_table VALUES(NEW.col18);
# define view
CREATE OR REPLACE VIEW VW1 AS SELECT * FROM t1;

CREATE INDEX idx1 ON t1(col18);
CREATE INDEX prefix_idx ON t1(col14 (10));
if (!$partitioning)
{
CREATE UNIQUE INDEX idx2 ON t1(col12);
CREATE UNIQUE INDEX idx3 ON t1(col8);
}


INSERT INTO t1 VALUES (
1,1,-128,32767,-8388608,2147483647,-9223372036854775808,92233720368.222,
-92233720368.222,'aaa','aaaaaaaaaa','b','bbbbb','ccccc',REPEAT('d',40),
REPEAT('d',40),REPEAT('d',40),1,'1000-01-01','3000-12-31 23:59:59.99',
'1990-01-01 00:00:01.00','01:59:59.00','1901');

INSERT INTO t1 VALUES (
NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
NULL,NULL,3,NULL,NULL,NULL,NULL,NULL);

--error ER_BAD_NULL_ERROR
INSERT INTO t1 VALUES (
NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL);

--error ER_DUP_ENTRY
INSERT INTO t1 VALUES (
NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
NULL,NULL,1,NULL,NULL,NULL,NULL,NULL);

SELECT * FROM t1;
SELECT * FROM trigger_table;
SELECT * FROM VW1;
FLUSH TABLES t1 FOR EXPORT;

perl;
require 'include/innodb-util.inc';
if ($ENV{'PARTITIONS'})
{
ib_backup_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
} else {
ib_backup_tablespaces("test", "t1");
}
EOF

UNLOCK TABLES;

--echo # trigger is also dropped when table is dropped
DROP TABLE t1;

eval CREATE TABLE t1(col1 bit(1),
col2 boolean,col3 tinyint, col4 smallint,
col5 mediumint,col6 int, col7 bigint,
col8 float (14,3),col9 double (14,3),
col10 VARCHAR(20) CHARACTER SET utf8,
col11 TEXT CHARACTER SET binary,
col12 ENUM('a','b','c') CHARACTER SET binary,
col13 TEXT CHARACTER SET latin1 COLLATE latin1_general_cs,
col14 CHAR(20), col15 VARBINARY (400),
col16 BINARY(40), col17 BLOB (400),
col18 int not null primary key,
col19 DATE,col20 DATETIME, col21 TIMESTAMP NULL,
col22 TIME, col23 YEAR) ENGINE = InnoDB
$col18_part;

CREATE INDEX idx1 ON t1(col18);
CREATE INDEX prefix_idx ON t1(col14 (10));
if (!$partitioning)
{
CREATE UNIQUE INDEX idx2 ON t1(col12);
CREATE UNIQUE INDEX idx3 ON t1(col8);
}

ALTER TABLE t1 DISCARD TABLESPACE;

perl;
require 'include/innodb-util.inc';
if ($ENV{'PARTITIONS'})
{
ib_discard_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
ib_restore_tablespaces("test", "t1#P#p0", "t1#P#p1", "t1#P#p2");
} else {
ib_discard_tablespaces("test", "t1");
ib_restore_tablespaces("test", "t1");
}
EOF

ALTER TABLE t1 IMPORT TABLESPACE;

SELECT * FROM t1;
SELECT * FROM trigger_table;
SELECT * FROM VW1;

# trigger table is not updated as trigger got dropped
INSERT INTO t1(col18) VALUES (5);
# validate data in table not updated
SELECT * FROM trigger_table;

UPDATE t1 SET col18=10 WHERE col18=1;

# view shows updated data
SELECT * FROM VW1;

SELECT * FROM t1 WHERE col18=10;
ALTER TABLE t1 ADD COLUMN col24 varbinary(40) default null;
INSERT INTO t1(col18,col24) VALUES (6,REPEAT('a',10));
SELECT col24,col18 FROM t1 WHERE col18 in (6,1,10) ORDER BY col18;
ALTER TABLE t1 DROP INDEX prefix_idx;
SELECT col18,col14 FROM t1 WHERE col14 like '_ccc%';
ALTER TABLE t1 ADD INDEX prefix_idx (col24(10));
SELECT col18,col24 FROM t1 WHERE col24 like '_a_a%';

DROP VIEW VW1;
DROP TABLE t1;
DROP TABLE trigger_table;

--disable_query_log
call mtr.add_suppression("Got error -1 when reading table '.*'");
call mtr.add_suppression("Error: tablespace id and flags in file '.*'.*");
call mtr.add_suppression("The table .* doesn't have a corresponding tablespace, it was discarded");
call mtr.add_suppression("Unable to import tablespace .* because it already exists.  Please DISCARD the tablespace before IMPORT.");
--enable_query_log

--remove_files_wildcard $MYSQLTEST_VARDIR/tmp t1*.ibd
--remove_files_wildcard $MYSQLTEST_VARDIR/tmp t1*.cfg

SET GLOBAL innodb_file_per_table = @old_innodb_file_per_table;
