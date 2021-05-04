SET GLOBAL innodb_redo_log_encrypt = ON;
CREATE TABLE t1(c1 INT, c2 char(20)) ENCRYPTION="Y" ENGINE = InnoDB;
ERROR HY000: Can't find master key from keyring, please check in the server log if a keyring is loaded and initialized successfully.
CREATE TABLE t1(c1 INT, c2 char(20)) ENGINE = InnoDB;
ALTER TABLE t1 ENCRYPTION="Y", algorithm=copy;
ERROR HY000: Can't find master key from keyring, please check in the server log if a keyring is loaded and initialized successfully.
DROP DATABASE IF EXISTS tde_db;
CREATE DATABASE tde_db;
USE tde_db;
SET GLOBAL innodb_redo_log_encrypt = ON;
SHOW WARNINGS;
Level	Code	Message
CREATE TABLE tde_db.t4 (a BIGINT PRIMARY KEY, b LONGBLOB) ENGINE=InnoDB;
INSERT INTO t4 (a, b) VALUES (1, REPEAT('a', 6*512*512));
SELECT a,LEFT(b,10) FROM tde_db.t4;
a	LEFT(b,10)
1	aaaaaaaaaa
# restart
SELECT a,LEFT(b,10) FROM tde_db.t4;
a	LEFT(b,10)
1	aaaaaaaaaa
DROP TABLE tde_db.t4;
DROP TABLE test.t1;
DROP DATABASE tde_db;
