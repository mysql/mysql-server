# InnoDB transparent tablespace data encryption
# This test case will test basic encryption support features.
# Assumption: Setup and Teardown of keyring will be handled by the driver test

--disable_query_log
call mtr.add_suppression("\\[Warning\\] .* Ignoring tablespace .* because it could not be opened");
call mtr.add_suppression("\\[ERROR\\] .* Failed to find tablespace for table `\.\.*`\.`\.\.*` in the cache.");
call mtr.add_suppression("\\[ERROR\\] .* Operating system error number 2 in a file operation.");
call mtr.add_suppression("\\[ERROR\\] .* The error means the system cannot find the path specified.");
call mtr.add_suppression("\\[ERROR\\] .* Could not find a valid tablespace file for");
call mtr.add_suppression("\\[ERROR\\] .* If you are installing InnoDB, remember that you must create directories yourself, InnoDB does not create them.");
call mtr.add_suppression("\\[ERROR\\] .* Tablespace [0-9]+, name 'test.*t1', unable to open file '.*test.*t1.ibd' - Data structure corruption");
--enable_query_log

let $innodb_file_per_table = `SELECT @@innodb_file_per_table`;

SET GLOBAL innodb_file_per_table = 0;
SELECT @@innodb_file_per_table;
# Test create encrypted table when inndb_file_per_table is 0,
# Should fail, since it will try to create table in system tablespace and
# We cannot create encrypted table in unencrypted system tablespace.
--error ER_INVALID_ENCRYPTION_REQUEST
CREATE TABLE t1(c1 INT, c2 char(20)) ENCRYPTION="Y" ENGINE = InnoDB;
SHOW WARNINGS;

SET GLOBAL innodb_file_per_table = 1;
SELECT @@innodb_file_per_table;

# Test create encrypted table in system tablespace,
# Should fail, 'ENCRYPTION=Y' option is not allowed with system tablespace.
--error ER_INVALID_ENCRYPTION_REQUEST
CREATE TABLE t1(c int) ENCRYPTION="Y" tablespace innodb_system;
SHOW WARNINGS;
CREATE TABLE t1(c int) ENCRYPTION="N" tablespace innodb_system;
DROP TABLE t1;

# Test create temporary encrypted table,
# Should fail, 'ENCRYPTION' clause is not  allowed with temporary tablespace.
--error ER_CANNOT_USE_ENCRYPTION_CLAUSE
CREATE TEMPORARY TABLE t1(c int) ENCRYPTION="Y";
SHOW WARNINGS;
--error ER_CANNOT_USE_ENCRYPTION_CLAUSE
CREATE TEMPORARY TABLE t1(c int) ENCRYPTION="N";
SHOW WARNINGS;

# Test create encrypted table with incorrect option.
--error ER_INVALID_ENCRYPTION_OPTION
CREATE TABLE t1(c int) ENCRYPTION="R" ENGINE = InnoDB;

# Crash/recovery test.
CREATE TABLE t1(c1 INT, c2 char(20)) ENCRYPTION="Y" ENGINE = InnoDB;

INSERT INTO t1 VALUES(0, "aaaaa");
INSERT INTO t1 VALUES(1, "bbbbb");
INSERT INTO t1 VALUES(2, "ccccc");
INSERT INTO t1 VALUES(3, "ddddd");
INSERT INTO t1 VALUES(4, "eeeee");
INSERT INTO t1 VALUES(5, "fffff");
INSERT INTO t1 VALUES(6, "ggggg");
INSERT INTO t1 VALUES(7, "hhhhh");
INSERT INTO t1 VALUES(8, "iiiii");
INSERT INTO t1 VALUES(9, "jjjjj");

# Restart to confirm the encryption info can be retrieved properly.
--source include/kill_mysqld.inc
--let $restart_parameters= restart: $PLUGIN_DIR_OPT
--source include/start_mysqld_no_echo.inc

SELECT * FROM t1 ORDER BY c1 LIMIT 10;

# Test alter table.
# We don't support inplace alter
--error ER_ALTER_OPERATION_NOT_SUPPORTED_REASON
ALTER TABLE t1 ENCRYPTION="N", algorithm=inplace;

# Don't support move it to shared tablespace
--error ER_INVALID_ENCRYPTION_REQUEST
ALTER TABLE t1 TABLESPACE=`innodb_system`;

# Alter using copy algorithm
ALTER TABLE t1 ENCRYPTION="N", algorithm=copy;

SELECT * FROM t1 ORDER BY c1 LIMIT 10;

DROP TABLE t1;

CREATE TABLE t1 (c1 int) ENCRYPTION='N';

--error ER_INVALID_ENCRYPTION_OPTION
ALTER TABLE t1 ENCRYPTION='P',algorithm=copy;

ALTER TABLE t1 ADD KEY k1 (c1) ,algorithm=inplace;

# Alter using inpalce algorithm
--error ER_ALTER_OPERATION_NOT_SUPPORTED_REASON
ALTER TABLE t1 ENCRYPTION='Y',algorithm=inplace;

drop table t1;

# Test compression + encryption.
--disable_warnings
CREATE TABLE t1(c1 INT PRIMARY KEY) COMPRESSION = "ZLIB" ENCRYPTION = "Y" ENGINE = InnoDB;
let COMPR_ZIP_WARN= `SHOW WARNINGS`;
--enable_warnings

INSERT INTO t1 VALUES(0), (1), (2), (3), (4), (5), (6), (7), (8), (9);

SHOW CREATE TABLE t1;

FLUSH TABLES t1 WITH READ LOCK;
UNLOCK TABLES;

SELECT * FROM t1 ORDER BY c1 LIMIT 10;

# Restart to confirm the encryption info can be retrieved properly.
--let $restart_parameters= restart: $PLUGIN_DIR_OPT
--source include/restart_mysqld_no_echo.inc

SELECT * FROM t1 ORDER BY c1;

DROP TABLE t1;

# Test old compression + encryption.
CREATE TABLE t1(c1 int null)  ENCRYPTION='Y' ROW_FORMAT=compressed;

INSERT INTO t1 VALUES(0), (1), (2), (3), (4), (5), (6), (7), (8), (9);

SHOW CREATE TABLE t1;

FLUSH TABLES t1 WITH READ LOCK;
UNLOCK TABLES;

SELECT * FROM t1 ORDER BY c1 LIMIT 10;

# Restart to confirm the encryption info can be retrieved properly.
--let $restart_parameters= restart: $PLUGIN_DIR_OPT
--source include/restart_mysqld_no_echo.inc

SELECT * FROM t1 ORDER BY c1;

DROP TABLE t1;

# Test encryption for rtree.
--disable_warnings
CREATE TABLE t1(c1 INT PRIMARY KEY, g geometry not null, spatial index(g)) ENCRYPTION = "Y" ENGINE = InnoDB;
--enable_warnings

INSERT INTO t1 VALUES(0, POINT(0, 0));
INSERT INTO t1 VALUES(1, POINT(1, 1));
INSERT INTO t1 VALUES(2, POINT(2, 2));
INSERT INTO t1 VALUES(3, POINT(3, 3));
INSERT INTO t1 VALUES(4, POINT(4, 4));
INSERT INTO t1 VALUES(5, POINT(5, 5));
INSERT INTO t1 VALUES(6, POINT(6, 6));
INSERT INTO t1 VALUES(7, POINT(7, 7));
INSERT INTO t1 VALUES(8, POINT(8, 8));
INSERT INTO t1 VALUES(9, POINT(9, 9));

SHOW CREATE TABLE t1;

FLUSH TABLES t1 WITH READ LOCK;
UNLOCK TABLES;

SELECT c1, ST_AsText(g) FROM t1 ORDER BY c1 LIMIT 10;

# Restart to confirm the encryption info can be retrieved properly.
--let $restart_parameters= restart: $PLUGIN_DIR_OPT --innodb_strict_mode=OFF
--source include/restart_mysqld_no_echo.inc

SELECT c1, ST_AsText(g) FROM t1 ORDER BY c1 LIMIT 10;

DROP TABLE t1;

# Test alter table when file per table is OFF and strict mode = 0.
SET GLOBAL innodb_file_per_table=OFF;

CREATE TABLE t1 (c1 int);

--error ER_ILLEGAL_HA_CREATE_OPTION
ALTER TABLE t1 COMPRESSION='zlib';
SHOW WARNINGS;

--error ER_INVALID_ENCRYPTION_REQUEST
ALTER TABLE t1 ENCRYPTION='Y',ALGORITHM=COPY;

# Cleanup
eval SET GLOBAL innodb_file_per_table=$innodb_file_per_table;
DROP TABLE t1;
