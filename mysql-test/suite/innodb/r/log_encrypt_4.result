# create bootstrap file
# Stop the MTR default DB server
# Run the bootstrap command of datadir1
# Start the DB server with datadir1
SELECT @@global.innodb_redo_log_encrypt ;
@@global.innodb_redo_log_encrypt
1
USE test;
CREATE TABLE tab1(c1 INT, c2 VARCHAR(30));
INSERT INTO tab1 VALUES(1, 'Test consistency undo*');
SELECT * FROM tab1;
c1	c2
1	Test consistency undo*
CREATE TABLE tab2(c1 INT, c2 VARCHAR(30)) ENCRYPTION="Y";
INSERT INTO tab2 VALUES(1, 'Test consistency undo*');
SELECT * FROM tab2;
c1	c2
1	Test consistency undo*
DROP TABLE tab1,tab2;
# restart:
# Stop the MTR default DB server
# Run the bootstrap command of datadir2, it should fail since the keyring is not loaded.
# Run the bootstrap command of datadir2
# Start the DB server with datadir2
SELECT @@global.innodb_redo_log_encrypt ;
@@global.innodb_redo_log_encrypt
1
USE test;
CREATE TABLE tab1(c1 INT, c2 VARCHAR(30));
INSERT INTO tab1 VALUES(1, 'Test consistency undo*');
SELECT * FROM tab1;
c1	c2
1	Test consistency undo*
CREATE TABLE tab2(c1 INT, c2 VARCHAR(30)) ENCRYPTION="Y";
INSERT INTO tab2 VALUES(1, 'Test consistency undo*');
SELECT * FROM tab2;
c1	c2
1	Test consistency undo*
DROP TABLE tab1,tab2;
# restart:
# Stop the MTR default DB server
# Run the bootstrap command of datadir3
# Start the DB server with datadir3 and keyring loaded.
SELECT @@global.innodb_redo_log_encrypt ;
@@global.innodb_redo_log_encrypt
0
USE test;
CREATE TABLE tab1(c1 INT, c2 VARCHAR(30));
INSERT INTO tab1 VALUES(1, 'Test consistency undo*');
SELECT * FROM tab1;
c1	c2
1	Test consistency undo*
CREATE TABLE tab2(c1 INT, c2 VARCHAR(30)) ENCRYPTION="Y";
DROP TABLE tab1;
# restart:
