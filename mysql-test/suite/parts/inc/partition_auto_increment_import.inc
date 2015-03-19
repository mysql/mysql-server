# NOTE: Currently run for InnoDB Storage Engine only

--echo # test auto increment with export import
let $MYSQLD_DATADIR=`SELECT @@datadir`;

eval CREATE TABLE t1 (
  col_1_int int AUTO_INCREMENT,
  col_2_varchar VARCHAR (20),
  primary key (col_1_int))
  ENGINE = $engine
  PARTITION BY HASH (col_1_int)
PARTITIONS 3;

SHOW CREATE TABLE t1;

SELECT AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES WHERE
TABLE_SCHEMA='test' AND TABLE_NAME='t1';

--echo # Insert values in all partitions
INSERT INTO t1 VALUES (1,'a1'),(2,'a2'),(3,'a3'),(4,'a4'),(5,'a5');
SELECT * FROM t1 ORDER BY col_1_int;

--echo # Export table and save the data [auto_inc = 6]
SHOW CREATE TABLE t1;
FLUSH TABLES t1 FOR EXPORT;
--copy_file $MYSQLD_DATADIR/test/t1#P#p0.ibd $MYSQLD_DATADIR/test/t1#P#p0_backup.ibd
--copy_file $MYSQLD_DATADIR/test/t1#P#p1.ibd $MYSQLD_DATADIR/test/t1#P#p1_backup.ibd
--copy_file $MYSQLD_DATADIR/test/t1#P#p2.ibd $MYSQLD_DATADIR/test/t1#P#p2_backup.ibd
--copy_file $MYSQLD_DATADIR/test/t1#P#p0.cfg $MYSQLD_DATADIR/test/t1#P#p0_backup.cfg
--copy_file $MYSQLD_DATADIR/test/t1#P#p1.cfg $MYSQLD_DATADIR/test/t1#P#p1_backup.cfg
--copy_file $MYSQLD_DATADIR/test/t1#P#p2.cfg $MYSQLD_DATADIR/test/t1#P#p2_backup.cfg

UNLOCK TABLES;

--error ER_DUP_ENTRY
INSERT INTO t1 VALUES (5,'a5');

--echo # Check auto increment INSERT
INSERT INTO t1(col_2_varchar) VALUES('a6'),('a7'),('a8');

SELECT * FROM t1 ORDER BY col_1_int;

--echo # Drop and create the table and import old data
DROP TABLE t1;
eval CREATE TABLE t1 (
  col_1_int int AUTO_INCREMENT,
  col_2_varchar VARCHAR (20),
  primary key (col_1_int))
  ENGINE = $engine
  PARTITION BY HASH (col_1_int)
PARTITIONS 3;

ALTER TABLE t1 DISCARD TABLESPACE;
--move_file $MYSQLD_DATADIR/test/t1#P#p0_backup.ibd $MYSQLD_DATADIR/test/t1#P#p0.ibd
--move_file $MYSQLD_DATADIR/test/t1#P#p1_backup.ibd $MYSQLD_DATADIR/test/t1#P#p1.ibd
--move_file $MYSQLD_DATADIR/test/t1#P#p2_backup.ibd $MYSQLD_DATADIR/test/t1#P#p2.ibd
--move_file $MYSQLD_DATADIR/test/t1#P#p0_backup.cfg $MYSQLD_DATADIR/test/t1#P#p0.cfg
--move_file $MYSQLD_DATADIR/test/t1#P#p1_backup.cfg $MYSQLD_DATADIR/test/t1#P#p1.cfg
--move_file $MYSQLD_DATADIR/test/t1#P#p2_backup.cfg $MYSQLD_DATADIR/test/t1#P#p2.cfg

--echo # Import old table and saved data [auto_inc = 6]
ALTER TABLE t1 IMPORT TABLESPACE;
SHOW CREATE TABLE t1;

SELECT * FROM t1 ORDER BY col_1_int;

--error ER_DUP_ENTRY
INSERT INTO t1 VALUES (5,'a5');

--echo # Check auto increment INSERT
INSERT INTO t1(col_2_varchar) VALUES('a6'),('a7'),('a8');

SELECT * FROM t1 ORDER BY col_1_int;

DROP TABLE t1;

--echo # test auto increment with export import and missing cfg file
eval CREATE TABLE t1 (
  col_1_int int AUTO_INCREMENT,
  col_2_varchar VARCHAR (20),
  primary key (col_1_int))
  ENGINE = $engine
  PARTITION BY HASH (col_1_int)
PARTITIONS 3;

SHOW CREATE TABLE t1;

SELECT AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES WHERE
TABLE_SCHEMA='test' AND TABLE_NAME='t1';

--echo # Insert values in all partitions
INSERT INTO t1 VALUES (1,'a1'),(2,'a2'),(3,'a3'),(4,'a4'),(5,'a5');
SELECT * FROM t1 ORDER BY col_1_int;

--echo # Export table and save the .ibd files only [auto_inc = 6]
SHOW CREATE TABLE t1;
FLUSH TABLES t1 FOR EXPORT;
--copy_file $MYSQLD_DATADIR/test/t1#P#p0.ibd $MYSQLD_DATADIR/test/t1#P#p0_backup.ibd
--copy_file $MYSQLD_DATADIR/test/t1#P#p1.ibd $MYSQLD_DATADIR/test/t1#P#p1_backup.ibd
--copy_file $MYSQLD_DATADIR/test/t1#P#p2.ibd $MYSQLD_DATADIR/test/t1#P#p2_backup.ibd

UNLOCK TABLES;

--error ER_DUP_ENTRY
INSERT INTO t1 VALUES (5,'a5');

--echo # Check auto increment INSERT
INSERT INTO t1(col_2_varchar) VALUES('a6'),('a7'),('a8');

SELECT * FROM t1 ORDER BY col_1_int;

--echo # Drop and create the table and import old data
DROP TABLE t1;
eval CREATE TABLE t1 (
  col_1_int int AUTO_INCREMENT,
  col_2_varchar VARCHAR (20),
  primary key (col_1_int))
  ENGINE = $engine
  PARTITION BY HASH (col_1_int)
PARTITIONS 3;

ALTER TABLE t1 DISCARD TABLESPACE;
--move_file $MYSQLD_DATADIR/test/t1#P#p0_backup.ibd $MYSQLD_DATADIR/test/t1#P#p0.ibd
--move_file $MYSQLD_DATADIR/test/t1#P#p1_backup.ibd $MYSQLD_DATADIR/test/t1#P#p1.ibd
--move_file $MYSQLD_DATADIR/test/t1#P#p2_backup.ibd $MYSQLD_DATADIR/test/t1#P#p2.ibd

--echo # Import old table and saved data from .ibd files [auto_inc = 6]
--replace_result #p# #P# #sp# #SP#
--replace_regex /Error opening '.*\/test\//Error opening '.\/test\//
ALTER TABLE t1 IMPORT TABLESPACE;
SHOW CREATE TABLE t1;

SELECT * FROM t1 ORDER BY col_1_int;

--error ER_DUP_ENTRY
INSERT INTO t1 VALUES (5,'a5');

--echo # Check auto increment INSERT
INSERT INTO t1(col_2_varchar) VALUES('a6'),('a7'),('a8');

SELECT * FROM t1 ORDER BY col_1_int;

DROP TABLE t1;
#------------------------------------------------------------------------------#
