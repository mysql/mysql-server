# ----------------------------------------------------------------------
# Setup
# Creating local configuration file for keyring component: component_keyring_file
# Creating manifest file for current MySQL server instance
# Re-starting mysql server with manifest file
# ----------------------------------------------------------------------
DROP TABLE IF EXISTS t1;
SET GLOBAL innodb_file_per_table = 1;
SELECT @@innodb_file_per_table;
@@innodb_file_per_table
1
# Create a table with encryption
CREATE TABLE t1(c1 INT, c2 char(20)) ENCRYPTION="Y" ENGINE = InnoDB;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` int DEFAULT NULL,
  `c2` char(20) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ENCRYPTION='Y'
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
INSERT INTO t1 select * from t1;
INSERT INTO t1 select * from t1;
INSERT INTO t1 select * from t1;
INSERT INTO t1 select * from t1;
INSERT INTO t1 select * from t1;
INSERT INTO t1 select * from t1;
SELECT * FROM t1 LIMIT 10;
c1	c2
0	aaaaa
1	bbbbb
2	ccccc
3	ddddd
4	eeeee
5	fffff
6	ggggg
7	hhhhh
8	iiiii
9	jjjjj
# Test export/import encrypted tablespace.
FLUSH TABLES test.t1 FOR EXPORT;
UNLOCK TABLES;
ALTER TABLE test.t1 DISCARD TABLESPACE;
ALTER TABLE test.t1 IMPORT TABLESPACE;
CHECK TABLE test.t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
SHOW CREATE TABLE test.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` int DEFAULT NULL,
  `c2` char(20) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ENCRYPTION='Y'
SELECT * FROM test.t1 LIMIT 10;
c1	c2
0	aaaaa
1	bbbbb
2	ccccc
3	ddddd
4	eeeee
5	fffff
6	ggggg
7	hhhhh
8	iiiii
9	jjjjj
SELECT * FROM test.t1 LIMIT 10;
c1	c2
0	aaaaa
1	bbbbb
2	ccccc
3	ddddd
4	eeeee
5	fffff
6	ggggg
7	hhhhh
8	iiiii
9	jjjjj
ALTER TABLE test.t1 DISCARD TABLESPACE;
# Try import in another session.
ALTER TABLE test.t1 IMPORT TABLESPACE;
SELECT * FROM test.t1 LIMIT 10;
c1	c2
0	aaaaa
1	bbbbb
2	ccccc
3	ddddd
4	eeeee
5	fffff
6	ggggg
7	hhhhh
8	iiiii
9	jjjjj
ALTER TABLE test.t1 DISCARD TABLESPACE;
# Import without .cfg file is posible
# copying .cfp and .ibd file
ALTER TABLE test.t1 IMPORT TABLESPACE;
Warnings:
Warning	1810	InnoDB: IO Read error: (2, No such file or directory) Error opening './test/t1.cfg', will attempt to import without schema verification
SELECT * FROM test.t1 LIMIT 10;
c1	c2
0	aaaaa
1	bbbbb
2	ccccc
3	ddddd
4	eeeee
5	fffff
6	ggggg
7	hhhhh
8	iiiii
9	jjjjj
ALTER INSTANCE ROTATE INNODB MASTER KEY;
SELECT * FROM test.t1 LIMIT 10;
c1	c2
0	aaaaa
1	bbbbb
2	ccccc
3	ddddd
4	eeeee
5	fffff
6	ggggg
7	hhhhh
8	iiiii
9	jjjjj
ALTER TABLE test.t1 DISCARD TABLESPACE;
# Import without .cfp file
# copying .cfg and .ibd file
ALTER TABLE test.t1 IMPORT TABLESPACE;
ERROR HY000: Schema mismatch (Table is in an encrypted tablespace, but the encryption meta-data file cannot be found while importing.)
ALTER INSTANCE ROTATE INNODB MASTER KEY;
# Import without .idb file
# copying .cfp and .cfg file
ALTER TABLE test.t1 IMPORT TABLESPACE;
ERROR HY000: Internal error: Cannot reset LSNs in table `test`.`t1` : Tablespace not found
ALTER INSTANCE ROTATE INNODB MASTER KEY;
# Schema mismatch dest table without encryption - fix result
DROP TABLE t1;
CREATE TABLE test.t1(c1 INT, c2 char(20)) ENGINE = InnoDB;
ALTER TABLE test.t1 DISCARD TABLESPACE;
ALTER TABLE test.t1 IMPORT TABLESPACE;
ERROR HY000: Schema mismatch (Encryption attribute in the file does not match the dictionary.)
# Import got expected error.
DROP TABLE t1;
CREATE TABLE test.t1(c1 INT, c2 char(20)) ENCRYPTION="Y" ENGINE = InnoDB;
ALTER TABLE test.t1 DISCARD TABLESPACE;
SET SESSION DEBUG="+d, fsp_header_rotate_encryption_failure";
ALTER TABLE test.t1 IMPORT TABLESPACE;
ERROR HY000: Got error 168 - 'Unknown (generic) error from engine' from storage engine
Warnings:
Note	1051	Unknown table 'test.t1'
# ----------------------------------------------------------------------
# Teardown
# Removing manifest file for current MySQL server instance
# Removing local keyring file for keyring component: component_keyring_file
# Removing local configuration file for keyring component: component_keyring_file
# Restarting server without the manifest file
# ----------------------------------------------------------------------
