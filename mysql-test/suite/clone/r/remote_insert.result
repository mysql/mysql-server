INSTALL PLUGIN clone SONAME 'CLONE_PLUGIN';
CREATE PROCEDURE prepare_data(IN val INT)
BEGIN
DECLARE i INT DEFAULT 1;
WHILE i <= val DO
INSERT INTO t1 (b,c) VALUES (REPEAT(a,600), REPEAT(b,600));
INSERT INTO t2 (b,c) VALUES (REPEAT(a,600), REPEAT(b,600));
SET i = i + 1;
END WHILE;
END|
# Case 1 - Normal page archiving process using clone client.
CREATE TABLE t1 (a INT AUTO_INCREMENT, b LONGBLOB, c LONGBLOB, key k1(a));
CREATE TABLE t2 (a INT PRIMARY KEY AUTO_INCREMENT, b LONGBLOB, c LONGBLOB);
SET DEBUG_SYNC = 'clone_file_copy SIGNAL page_signal WAIT_FOR go_page';
SET DEBUG_SYNC = 'clone_page_copy SIGNAL redo_signal WAIT_FOR go_redo';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
SET GLOBAL clone_valid_donor_list = 'HOST:PORT';
CLONE INSTANCE FROM USER@HOST:PORT IDENTIFIED BY '' DATA DIRECTORY = 'CLONE_DATADIR';
SET DEBUG_SYNC = 'now WAIT_FOR page_signal';
CALL prepare_data(50);
SET DEBUG_SYNC = 'now SIGNAL go_page';
SET DEBUG_SYNC = 'now WAIT_FOR redo_signal';
CALL prepare_data(10);
SET DEBUG_SYNC = 'now SIGNAL go_redo';
select count(*) from t1;
count(*)
60
select count(*) from t2;
count(*)
60
# restart: --datadir=CLONE_DATADIR
INSERT INTO t1 (b,c) VALUES (REPEAT(a,600), REPEAT(b,600));
INSERT INTO t2 (b,c) VALUES (REPEAT(a,600), REPEAT(b,600));
select count(*), max(a) from t1;
count(*)	max(a)
61	61
select count(*), max(a) from t2;
count(*)	max(a)
61	61
# restart:
DROP TABLE t1;
DROP TABLE t2;
SET DEBUG_SYNC = 'RESET';
DROP PROCEDURE prepare_data;
UNINSTALL PLUGIN clone;
