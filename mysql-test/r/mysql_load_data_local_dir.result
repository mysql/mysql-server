# create the test directory
# create the wrong test directory
# create the test file
# create the wrong test file
# create a file in wrong dir
# setup
CREATE TABLE test.wl13168(id INT PRIMARY KEY, data VARCHAR(50));
# FR2/FR1.2: specify test_dir: should work
# verify that the data are loaded
SELECT * FROM test.wl13168 ORDER BY id;
id	data
1	a
2	b
DELETE FROM test.wl13168;
# FR1.1: specify empty (default): should fail
ERROR 2068 (HY000) at line 1: LOAD DATA LOCAL INFILE file request rejected due to restrictions on access.
SELECT * FROM test.wl13168 ORDER BY id;
id	data
# FR1.1: specify explicit empty: should fail
ERROR 2068 (HY000) at line 1: LOAD DATA LOCAL INFILE file request rejected due to restrictions on access.
SELECT * FROM test.wl13168 ORDER BY id;
id	data
# FR1.1: specify explicit empty and local-infile: should work
SELECT * FROM test.wl13168 ORDER BY id;
id	data
1	a
2	b
DELETE FROM test.wl13168;
# FR1.1: specify wrong file: should fail
ERROR 2068 (HY000) at line 1: LOAD DATA LOCAL INFILE file request rejected due to restrictions on access.
SELECT * FROM test.wl13168 ORDER BY id;
id	data
# FR1.1: specify wrong dir: should fail
ERROR 2068 (HY000) at line 1: LOAD DATA LOCAL INFILE file request rejected due to restrictions on access.
SELECT * FROM test.wl13168 ORDER BY id;
id	data
# FR1.1: specify wrong dir and local-infile: should work
SELECT * FROM test.wl13168 ORDER BY id;
id	data
1	a
2	b
DELETE FROM test.wl13168;
# Expect no error on MacOS and error on others
# FR1.5: specify wrong case dir: should fail except on MacOS
DELETE FROM test.wl13168;
# FR1.5: specify wrong case file: should fail except on MacOS
DELETE FROM test.wl13168;
# Expect no error on windows and error on others
# FR2.1: specify non-existent dir: expect success on windows and failure on unix
SET @@global.local_infile = 0;
# FR2/FR1.2: specify test_dir: should fail
ERROR 3948 (42000) at line 1: Loading local data is disabled; this must be enabled on both the client and server sides
SELECT * FROM test.wl13168 ORDER BY id;
id	data
# cleanup
DROP TABLE test.wl13168;
SET @@global.local_infile = 1;
