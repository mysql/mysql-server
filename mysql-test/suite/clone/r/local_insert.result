INSTALL PLUGIN clone SONAME 'CLONE_PLUGIN';
CREATE PROCEDURE prepare_data(IN val INT)
BEGIN
DECLARE i INT DEFAULT 1;
WHILE i < val DO
INSERT INTO t1 (b,c) VALUES (REPEAT(a,600), REPEAT(b,600));
INSERT INTO t2 (b,c) VALUES (REPEAT(a,600), REPEAT(b,600));
INSERT INTO t3 (b,c) VALUES (REPEAT(a,600), REPEAT(b,600));
SET i = i + 1;
END WHILE;
END|
# Case 1 - Normal page archiving process using clone client.
CREATE TABLE t1 (a INT NOT NULL PRIMARY KEY AUTO_INCREMENT, b LONGBLOB, c
LONGBLOB);
CREATE TABLE t2 (a INT NOT NULL PRIMARY KEY AUTO_INCREMENT, b LONGBLOB, c
LONGBLOB);
CREATE TABLE t3 (a INT NOT NULL PRIMARY KEY AUTO_INCREMENT, b LONGBLOB, c
LONGBLOB);
SET DEBUG_SYNC = 'clone_file_copy SIGNAL page_signal WAIT_FOR go_page';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
CLONE LOCAL DATA DIRECTORY = 'CLONE_DATADIR';
SET DEBUG_SYNC = 'now WAIT_FOR page_signal';
CALL prepare_data(100);
SET DEBUG_SYNC = 'now SIGNAL go_page';
select count(*) from t1;
count(*)
99
select count(*) from t2;
count(*)
99
select count(*) from t3;
count(*)
99
# restart: --datadir=CLONE_DATADIR
select count(*) from t1;
count(*)
99
select count(*) from t2;
count(*)
99
select count(*) from t3;
count(*)
99
# restart:
DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;
SET DEBUG_SYNC = 'RESET';
DROP PROCEDURE prepare_data;
UNINSTALL PLUGIN clone;
