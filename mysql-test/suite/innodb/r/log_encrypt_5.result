# create bootstrap file
# Stop the MTR default DB server
# Run the bootstrap command of datadir1
DROP DATABASE IF EXISTS tde_db;
CREATE DATABASE tde_db;
USE tde_db;
INSTALL PLUGIN keyring_file SONAME 'keyring_file.so';
SET @@global.keyring_file_data= 'MYSQL_TMP_DIR/my_key_log3';
SELECT
PLUGIN_NAME, PLUGIN_STATUS, PLUGIN_TYPE
FROM INFORMATION_SCHEMA.PLUGINS
WHERE PLUGIN_NAME LIKE '%keyring_file%' ;
PLUGIN_NAME	PLUGIN_STATUS	PLUGIN_TYPE
keyring_file	ACTIVE	KEYRING
SET GLOBAL innodb_redo_log_encrypt = 1;
SET GLOBAL innodb_undo_log_encrypt = 1;
SELECT @@global.innodb_redo_log_encrypt ;
@@global.innodb_redo_log_encrypt
1
CREATE TABLE tde_db.t1 (a BIGINT PRIMARY KEY, b LONGBLOB) ENGINE=InnoDB;
INSERT INTO t1 (a, b) VALUES (1, REPEAT('a', 6*512*512));
SELECT a,LEFT(b,10) FROM tde_db.t1;
a	LEFT(b,10)
1	aaaaaaaaaa
CREATE TABLE tde_db.t2 (a BIGINT PRIMARY KEY, b LONGBLOB)
ENCRYPTION='Y' ENGINE=InnoDB;
INSERT INTO t2 (a, b) VALUES (1, REPEAT('a', 6*512*512));
SELECT a,LEFT(b,10) FROM tde_db.t2;
a	LEFT(b,10)
1	aaaaaaaaaa
SET GLOBAL innodb_redo_log_encrypt = 0;
SET GLOBAL innodb_undo_log_encrypt = 0;
SELECT @@global.innodb_redo_log_encrypt ;
@@global.innodb_redo_log_encrypt
0
CREATE TABLE tde_db.t3 (a BIGINT PRIMARY KEY, b LONGBLOB) ENGINE=InnoDB;
INSERT INTO t3 (a, b) VALUES (1, REPEAT('a', 6*512*512));
SELECT a,LEFT(b,10) FROM tde_db.t3;
a	LEFT(b,10)
1	aaaaaaaaaa
CREATE TABLE tde_db.t4 (a BIGINT PRIMARY KEY, b LONGBLOB)
ENCRYPTION='Y' ENGINE=InnoDB;
INSERT INTO t4 (a, b) VALUES (1, REPEAT('a', 6*512*512));
SELECT a,LEFT(b,10) FROM tde_db.t4;
a	LEFT(b,10)
1	aaaaaaaaaa
SET GLOBAL innodb_redo_log_encrypt = 1;
SET GLOBAL innodb_undo_log_encrypt = 1;
SELECT @@global.innodb_redo_log_encrypt ;
@@global.innodb_redo_log_encrypt
1
CREATE TABLE tde_db.t5 (a BIGINT PRIMARY KEY, b LONGBLOB) ENGINE=InnoDB;
INSERT INTO t5 (a, b) VALUES (1, REPEAT('a', 6*512*512));
SELECT a,LEFT(b,10) FROM tde_db.t5;
a	LEFT(b,10)
1	aaaaaaaaaa
CREATE TABLE tde_db.t6 (a BIGINT PRIMARY KEY, b LONGBLOB)
ENCRYPTION='Y' ENGINE=InnoDB;
INSERT INTO t6 (a, b) VALUES (1, REPEAT('a', 6*512*512));
SELECT a,LEFT(b,10) FROM tde_db.t6;
a	LEFT(b,10)
1	aaaaaaaaaa
START TRANSACTION;
INSERT INTO t5 (a, b) VALUES (2, REPEAT('a', 6*512*512));
INSERT INTO t6 (a, b) VALUES (2, REPEAT('a', 6*512*512));
COMMIT;
START TRANSACTION;
INSERT INTO t5 (a, b) VALUES (3, REPEAT('a', 6*512*512));
INSERT INTO t6 (a, b) VALUES (3, REPEAT('a', 6*512*512));
ROLLBACK;
SELECT a,LEFT(b,10) FROM tde_db.t5;
a	LEFT(b,10)
1	aaaaaaaaaa
2	aaaaaaaaaa
SELECT a,LEFT(b,10) FROM tde_db.t6;
a	LEFT(b,10)
1	aaaaaaaaaa
2	aaaaaaaaaa
UNINSTALL PLUGIN keyring_file;
DROP TABLE tde_db.t1,tde_db.t2,tde_db.t3,tde_db.t4,tde_db.t5,tde_db.t6;
DROP DATABASE tde_db;
