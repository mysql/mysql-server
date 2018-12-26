include/rpl_init.inc [topology=1->2->3]
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
CON: 'server_1', IMG: 'NOBLOB', RESTART SLAVE: 'N'
SET SESSION binlog_row_image= 'NOBLOB';
SET GLOBAL binlog_row_image= 'NOBLOB';
FLUSH TABLES;
SHOW VARIABLES LIKE 'binlog_row_image';
Variable_name	Value
binlog_row_image	NOBLOB
CON: 'server_2', IMG: 'NOBLOB', RESTART SLAVE: 'Y'
SET SESSION binlog_row_image= 'NOBLOB';
SET GLOBAL binlog_row_image= 'NOBLOB';
include/stop_slave.inc
include/start_slave.inc
FLUSH TABLES;
SHOW VARIABLES LIKE 'binlog_row_image';
Variable_name	Value
binlog_row_image	NOBLOB
CON: 'server_3', IMG: 'NOBLOB', RESTART SLAVE: 'Y'
SET SESSION binlog_row_image= 'NOBLOB';
SET GLOBAL binlog_row_image= 'NOBLOB';
include/stop_slave.inc
include/start_slave.inc
FLUSH TABLES;
SHOW VARIABLES LIKE 'binlog_row_image';
Variable_name	Value
binlog_row_image	NOBLOB
### engines: MyISAM, MyISAM, MyISAM
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check when there is no key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check even if there is a key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (UK NOT NULL exists in the table) 
### will not break replication
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (PK exists int the table)
### will not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob in a key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a unique (not null) key does not break replication
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a primary key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
### engines: MyISAM, MyISAM, InnoDB
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check when there is no key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check even if there is a key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (UK NOT NULL exists in the table) 
### will not break replication
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (PK exists int the table)
### will not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob in a key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a unique (not null) key does not break replication
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a primary key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
### engines: MyISAM, InnoDB, MyISAM
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check when there is no key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check even if there is a key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (UK NOT NULL exists in the table) 
### will not break replication
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (PK exists int the table)
### will not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob in a key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a unique (not null) key does not break replication
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a primary key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
### engines: MyISAM, InnoDB, InnoDB
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check when there is no key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check even if there is a key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (UK NOT NULL exists in the table) 
### will not break replication
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (PK exists int the table)
### will not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob in a key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a unique (not null) key does not break replication
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a primary key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
### engines: InnoDB, MyISAM, MyISAM
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check when there is no key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check even if there is a key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (UK NOT NULL exists in the table) 
### will not break replication
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (PK exists int the table)
### will not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob in a key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a unique (not null) key does not break replication
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a primary key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
### engines: InnoDB, MyISAM, InnoDB
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check when there is no key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check even if there is a key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (UK NOT NULL exists in the table) 
### will not break replication
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (PK exists int the table)
### will not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob in a key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a unique (not null) key does not break replication
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a primary key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
### engines: InnoDB, InnoDB, MyISAM
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check when there is no key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check even if there is a key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (UK NOT NULL exists in the table) 
### will not break replication
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (PK exists int the table)
### will not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob in a key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a unique (not null) key does not break replication
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a primary key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
### engines: InnoDB, InnoDB, InnoDB
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check when there is no key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check even if there is a key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (UK NOT NULL exists in the table) 
### will not break replication
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (PK exists int the table)
### will not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob in a key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a unique (not null) key does not break replication
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a primary key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
CON: 'server_1', IMG: 'MINIMAL', RESTART SLAVE: 'N'
SET SESSION binlog_row_image= 'MINIMAL';
SET GLOBAL binlog_row_image= 'MINIMAL';
FLUSH TABLES;
SHOW VARIABLES LIKE 'binlog_row_image';
Variable_name	Value
binlog_row_image	MINIMAL
CON: 'server_2', IMG: 'MINIMAL', RESTART SLAVE: 'Y'
SET SESSION binlog_row_image= 'MINIMAL';
SET GLOBAL binlog_row_image= 'MINIMAL';
include/stop_slave.inc
include/start_slave.inc
FLUSH TABLES;
SHOW VARIABLES LIKE 'binlog_row_image';
Variable_name	Value
binlog_row_image	MINIMAL
CON: 'server_3', IMG: 'MINIMAL', RESTART SLAVE: 'Y'
SET SESSION binlog_row_image= 'MINIMAL';
SET GLOBAL binlog_row_image= 'MINIMAL';
include/stop_slave.inc
include/start_slave.inc
FLUSH TABLES;
SHOW VARIABLES LIKE 'binlog_row_image';
Variable_name	Value
binlog_row_image	MINIMAL
### engines: MyISAM, MyISAM, MyISAM
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check when there is no key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check even if there is a key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (UK NOT NULL exists in the table) 
### will not break replication
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (PK exists int the table)
### will not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob in a key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a unique (not null) key does not break replication
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a primary key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
### engines: MyISAM, MyISAM, InnoDB
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check when there is no key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check even if there is a key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (UK NOT NULL exists in the table) 
### will not break replication
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (PK exists int the table)
### will not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob in a key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a unique (not null) key does not break replication
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a primary key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
### engines: MyISAM, InnoDB, MyISAM
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check when there is no key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check even if there is a key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (UK NOT NULL exists in the table) 
### will not break replication
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (PK exists int the table)
### will not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob in a key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a unique (not null) key does not break replication
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a primary key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
### engines: MyISAM, InnoDB, InnoDB
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check when there is no key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check even if there is a key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (UK NOT NULL exists in the table) 
### will not break replication
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (PK exists int the table)
### will not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob in a key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a unique (not null) key does not break replication
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a primary key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
### engines: InnoDB, MyISAM, MyISAM
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check when there is no key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check even if there is a key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (UK NOT NULL exists in the table) 
### will not break replication
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (PK exists int the table)
### will not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob in a key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a unique (not null) key does not break replication
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a primary key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
### engines: InnoDB, MyISAM, InnoDB
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check when there is no key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check even if there is a key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (UK NOT NULL exists in the table) 
### will not break replication
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (PK exists int the table)
### will not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob in a key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a unique (not null) key does not break replication
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a primary key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
### engines: InnoDB, InnoDB, MyISAM
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check when there is no key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check even if there is a key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (UK NOT NULL exists in the table) 
### will not break replication
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (PK exists int the table)
### will not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob in a key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a unique (not null) key does not break replication
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a primary key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
### engines: InnoDB, InnoDB, InnoDB
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check when there is no key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check even if there is a key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (UK NOT NULL exists in the table) 
### will not break replication
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (PK exists int the table)
### will not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob in a key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a unique (not null) key does not break replication
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a primary key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
CON: 'server_1', IMG: 'FULL', RESTART SLAVE: 'N'
SET SESSION binlog_row_image= 'FULL';
SET GLOBAL binlog_row_image= 'FULL';
FLUSH TABLES;
SHOW VARIABLES LIKE 'binlog_row_image';
Variable_name	Value
binlog_row_image	FULL
CON: 'server_2', IMG: 'FULL', RESTART SLAVE: 'Y'
SET SESSION binlog_row_image= 'FULL';
SET GLOBAL binlog_row_image= 'FULL';
include/stop_slave.inc
include/start_slave.inc
FLUSH TABLES;
SHOW VARIABLES LIKE 'binlog_row_image';
Variable_name	Value
binlog_row_image	FULL
CON: 'server_3', IMG: 'FULL', RESTART SLAVE: 'Y'
SET SESSION binlog_row_image= 'FULL';
SET GLOBAL binlog_row_image= 'FULL';
include/stop_slave.inc
include/start_slave.inc
FLUSH TABLES;
SHOW VARIABLES LIKE 'binlog_row_image';
Variable_name	Value
binlog_row_image	FULL
### engines: MyISAM, MyISAM, MyISAM
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check when there is no key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check even if there is a key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (UK NOT NULL exists in the table) 
### will not break replication
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (PK exists int the table)
### will not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob in a key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a unique (not null) key does not break replication
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a primary key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
### engines: MyISAM, MyISAM, InnoDB
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check when there is no key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check even if there is a key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (UK NOT NULL exists in the table) 
### will not break replication
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (PK exists int the table)
### will not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob in a key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a unique (not null) key does not break replication
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a primary key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
### engines: MyISAM, InnoDB, MyISAM
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check when there is no key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check even if there is a key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (UK NOT NULL exists in the table) 
### will not break replication
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (PK exists int the table)
### will not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob in a key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a unique (not null) key does not break replication
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a primary key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
### engines: MyISAM, InnoDB, InnoDB
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check when there is no key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check even if there is a key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (UK NOT NULL exists in the table) 
### will not break replication
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (PK exists int the table)
### will not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob in a key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a unique (not null) key does not break replication
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a primary key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
### engines: InnoDB, MyISAM, MyISAM
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check when there is no key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check even if there is a key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (UK NOT NULL exists in the table) 
### will not break replication
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (PK exists int the table)
### will not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob in a key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a unique (not null) key does not break replication
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a primary key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
### engines: InnoDB, MyISAM, InnoDB
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check when there is no key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check even if there is a key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (UK NOT NULL exists in the table) 
### will not break replication
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (PK exists int the table)
### will not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob in a key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a unique (not null) key does not break replication
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a primary key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
### engines: InnoDB, InnoDB, MyISAM
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check when there is no key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check even if there is a key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (UK NOT NULL exists in the table) 
### will not break replication
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (PK exists int the table)
### will not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob in a key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a unique (not null) key does not break replication
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a primary key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= MyISAM;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
### engines: InnoDB, InnoDB, InnoDB
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check when there is no key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the AI (they are not updated) 
### will not break replication (check even if there is a key in the table)
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (UK NOT NULL exists in the table) 
### will not break replication
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int NOT NULL, c2 blob NOT NULL, c3 int, unique key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that updates without blobs in the BI (PK exists int the table)
### will not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c1)) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob in a key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a unique (not null) key does not break replication
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob NOT NULL, c3 int, unique key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
SET SQL_LOG_BIN=0;
### Asserts that declaring a blob as part of a primary key does not break replication
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
CREATE TABLE t (c1 int, c2 blob, c3 int, primary key(c2(512))) engine= InnoDB;;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
SET SQL_LOG_BIN=1;
INSERT INTO t VALUES (1, "a", 10);
INSERT INTO t VALUES (2, "b", 20);
INSERT INTO t VALUES (3, "c", 30);
include/rpl_sync.inc
UPDATE t SET c1=10 WHERE c2="a";
UPDATE t SET c1=20 WHERE c1=2;
UPDATE t SET c1=30 WHERE c3=30;
UPDATE t SET c3=40 WHERE c1=30;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DELETE FROM t WHERE c2="a";
DELETE FROM t WHERE c1=20;
DELETE FROM t;
include/rpl_sync.inc
include/diff_tables.inc [server_1:test.t, server_2:test.t, server_3:test.t]
DROP TABLE t;
include/rpl_sync.inc
include/rpl_end.inc
