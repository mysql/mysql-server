SET GLOBAL innodb_file_per_table = 1;
SELECT @@innodb_file_per_table;
@@innodb_file_per_table
1
DROP DATABASE IF EXISTS tde_db;
CREATE DATABASE tde_db;
USE tde_db;
#
# Set encryption ON for table `mysql`
#
ALTER TABLESPACE mysql ENCRYPTION='Y';
SET debug='+d,skip_dd_table_access_check';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE OPTIONS LIKE '%encryption=Y%';
NAME	OPTIONS
mysql	encryption=Y;
SET debug='-d,skip_dd_table_access_check';
#
# Test crash point `ib_crash_during_rotation_for_encryption`
#
SET SESSION DEBUG="+d,ib_encryption_rotate_crash";
CREATE TABLE tde_db.t1(c1 INT, c2 char(20)) ENCRYPTION="Y" ENGINE = InnoDB;
SET debug='+d,skip_dd_table_access_check';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE OPTIONS LIKE '%encryption=Y%';
NAME	OPTIONS
mysql	encryption=Y;
tde_db/t1	autoextend_size=0;encryption=Y;
SET debug='-d,skip_dd_table_access_check';
SHOW CREATE TABLE tde_db.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` int DEFAULT NULL,
  `c2` char(20) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ENCRYPTION='Y'
INSERT INTO tde_db.t1 VALUES(0, "aaaaa");
INSERT INTO tde_db.t1 VALUES(1, "bbbbb");
INSERT INTO tde_db.t1 VALUES(2, "ccccc");
INSERT INTO tde_db.t1 VALUES(3, "ddddd");
# Execute the statement that causes the crash
ALTER INSTANCE ROTATE INNODB MASTER KEY;
ERROR HY000: Lost connection to MySQL server during query
SET SESSION DEBUG="-d,ib_encryption_rotate_crash";
Pattern "ib_encryption_rotate_crash" found
# Show that encryption is OK
SET debug='+d,skip_dd_table_access_check';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE OPTIONS LIKE '%encryption=Y%';
NAME	OPTIONS
mysql	encryption=Y;
tde_db/t1	autoextend_size=0;encryption=Y;
SET debug='-d,skip_dd_table_access_check';
INSERT INTO tde_db.t1 VALUES(4, "eeeee");
SELECT * FROM tde_db.t1 ORDER BY c1;
c1	c2
0	aaaaa
1	bbbbb
2	ccccc
3	ddddd
4	eeeee
# Create a second table with encryption
CREATE TABLE tde_db.t2(c1 INT, c2 char(20)) ENCRYPTION="Y" ENGINE = InnoDB;
SHOW CREATE TABLE tde_db.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `c1` int DEFAULT NULL,
  `c2` char(20) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ENCRYPTION='Y'
SET debug='+d,skip_dd_table_access_check';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE OPTIONS LIKE '%encryption=Y%';
NAME	OPTIONS
mysql	encryption=Y;
tde_db/t1	autoextend_size=0;encryption=Y;
tde_db/t2	autoextend_size=0;encryption=Y;
SET debug='-d,skip_dd_table_access_check';
INSERT INTO tde_db.t2 VALUES(0, "aaaaa");
INSERT INTO tde_db.t2 VALUES(1, "bbbbb");
# Remove the current auto.cnf file to make sure a new server uuid is
# generated at restart.
# Execute the statement that causes the crash
SET SESSION DEBUG="+d,ib_encryption_rotate_crash";
ALTER INSTANCE ROTATE INNODB MASTER KEY;
ERROR HY000: Lost connection to MySQL server during query
SET SESSION DEBUG="-d,ib_encryption_rotate_crash";
Pattern "ib_encryption_rotate_crash" found
# Show that encryption is OK
SET debug='+d,skip_dd_table_access_check';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE OPTIONS LIKE '%encryption=Y%';
NAME	OPTIONS
mysql	encryption=Y;
tde_db/t1	autoextend_size=0;encryption=Y;
tde_db/t2	autoextend_size=0;encryption=Y;
SET debug='-d,skip_dd_table_access_check';
INSERT INTO tde_db.t1 VALUES(5, "fffff");
SELECT * FROM tde_db.t1 ORDER BY c1;
c1	c2
0	aaaaa
1	bbbbb
2	ccccc
3	ddddd
4	eeeee
5	fffff
SELECT * FROM tde_db.t2 ORDER BY c1;
c1	c2
0	aaaaa
1	bbbbb
# Rotate Encryption again without the Crash
ALTER INSTANCE ROTATE INNODB MASTER KEY;
SELECT * FROM tde_db.t1 ORDER BY c1 ;
c1	c2
0	aaaaa
1	bbbbb
2	ccccc
3	ddddd
4	eeeee
5	fffff
# Show that encryption is OK
SET debug='+d,skip_dd_table_access_check';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE OPTIONS LIKE '%encryption=Y%';
NAME	OPTIONS
mysql	encryption=Y;
tde_db/t1	autoextend_size=0;encryption=Y;
tde_db/t2	autoextend_size=0;encryption=Y;
SET debug='-d,skip_dd_table_access_check';
SELECT * FROM tde_db.t1 ORDER BY c1 ;
c1	c2
0	aaaaa
1	bbbbb
2	ccccc
3	ddddd
4	eeeee
5	fffff
SELECT * FROM tde_db.t2 ORDER BY c1 ;
c1	c2
0	aaaaa
1	bbbbb
DROP TABLE tde_db.t2;
ALTER TABLESPACE mysql ENCRYPTION='N';
SET debug='+d,skip_dd_table_access_check';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE OPTIONS LIKE '%encryption=Y%';
NAME	OPTIONS
tde_db/t1	autoextend_size=0;encryption=Y;
SET debug='-d,skip_dd_table_access_check';
#
# Test crash point `ib_crash_during_create_for_encryption` during a CREATE TABLE
#
SET SESSION DEBUG="+d,ib_crash_during_create_for_encryption";
# Execute the statement that causes the crash
CREATE TABLE tde_db.t2(c1 INT, c2 char(20)) ENCRYPTION="Y" ENGINE = InnoDB;
ERROR HY000: Lost connection to MySQL server during query
SET SESSION DEBUG="-d,ib_crash_during_create_for_encryption";
# Show encrypted tables.
SET debug='+d,skip_dd_table_access_check';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE OPTIONS LIKE '%encryption=Y%';
NAME	OPTIONS
tde_db/t1	autoextend_size=0;encryption=Y;
SET debug='-d,skip_dd_table_access_check';
SELECT * FROM tde_db.t1 ORDER BY c1 LIMIT 5;
c1	c2
0	aaaaa
1	bbbbb
2	ccccc
3	ddddd
4	eeeee
INSERT INTO tde_db.t1 VALUES(6, "ggggg");
SELECT * FROM tde_db.t1 ORDER BY c1 ;
c1	c2
0	aaaaa
1	bbbbb
2	ccccc
3	ddddd
4	eeeee
5	fffff
6	ggggg
SELECT * FROM tde_db.t2;
ERROR 42S02: Table 'tde_db.t2' doesn't exist
#
# Test crash point `ib_crash_during_decrypt_page` during an IMPORT TABLESPACE
#
FLUSH TABLE tde_db.t1 FOR EXPORT;
UNLOCK TABLES;
ALTER TABLE tde_db.t1 DISCARD TABLESPACE;
SET SESSION DEBUG="+d,ib_crash_during_decrypt_page";
# Execute the statement that causes the crash
ALTER TABLE tde_db.t1 IMPORT TABLESPACE;
ERROR HY000: Lost connection to MySQL server during query
SET SESSION DEBUG="-d,ib_crash_during_decrypt_page";
INSERT INTO tde_db.t1 VALUES(7, "hhhhh");
ERROR HY000: Tablespace has been discarded for table 't1'
SELECT * FROM tde_db.t1 ORDER BY c1 ;
ERROR HY000: Tablespace has been discarded for table 't1'
DROP TABLE tde_db.t1;
DROP DATABASE tde_db;
#
# Bug #27307740 [ERROR] [MY-011066] INNODB: CORRUPT LOG RECORD FOUND
# DURING CRASH RECOVERY
#
CREATE DATABASE tde_db;
CREATE TABLE tde_db.test_tbl(c1 int) ENCRYPTION="Y" ENGINE=InnoDB;
SET SESSION DEBUG="+d,keyring_file_backup_fail";
ALTER INSTANCE ROTATE INNODB MASTER KEY;
ERROR HY000: Lost connection to MySQL server during query
SET SESSION DEBUG="-d,keyring_file_backup_fail";
SELECT COUNT(*) FROM tde_db.test_tbl;
COUNT(*)
0
DROP TABLE tde_db.test_tbl;
DROP DATABASE tde_db;
SET GLOBAL innodb_file_per_table=1;
# restart
CREATE TABLE test.t1(c1 INT, c2 char(20)) ENCRYPTION="Y" ENGINE = InnoDB;
DROP TABLE test.t1;
