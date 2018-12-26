--echo 
--echo ####################################
--echo # Check if table rebuilding alter isn't affect if table is created
--echo # with explicit row_format
eval CREATE $TEMPORARY TABLE t1 (a INT PRIMARY KEY, b TEXT) ROW_FORMAT=COMPACT ENGINE=INNODB;
INSERT INTO t1 VALUES (1, 'abc');

--source suite/innodb/include/default_row_format_show.inc

SET GLOBAL innodb_default_row_format=DYNAMIC;
ALTER TABLE t1 DROP PRIMARY KEY, ADD COLUMN c INT PRIMARY KEY;

--echo # Here we expect COMPACT because it was explicitly specified at CREATE
--source suite/innodb/include/default_row_format_show.inc
DROP TABLE t1;

--echo 
--echo ####################################
--echo # Check if table rebuilding alter is affected when there is no
--echo # row_format specified at CREATE TABLE.
SET GLOBAL innodb_default_row_format = COMPACT;
eval CREATE $TEMPORARY TABLE t1 (a INT PRIMARY KEY, b TEXT) ENGINE=INNODB;
INSERT INTO t1 VALUES (1, 'abc');

--source suite/innodb/include/default_row_format_show.inc

SET GLOBAL innodb_default_row_format = DYNAMIC;
ALTER TABLE t1 DROP PRIMARY KEY, ADD COLUMN c INT PRIMARY KEY;

--echo # Here we expect DYNAMIC because there is no explicit ROW_FORMAT and the
--echo # default_row_format is changed to DYNAMIC just before ALTER
--source suite/innodb/include/default_row_format_show.inc
DROP TABLE t1;

--echo 
--echo ####################################
--echo # Check the row_format effect on ALTER, ALGORITHM=COPY
SET GLOBAL innodb_default_row_format = REDUNDANT;
eval CREATE $TEMPORARY TABLE t1 (a INT PRIMARY KEY, b TEXT) ENGINE=INNODB;
INSERT INTO t1 VALUES (1, REPEAT('abc',1000));

--source suite/innodb/include/default_row_format_show.inc

SET GLOBAL innoDB_default_row_format = COMPACT;
ALTER TABLE t1 ADD COLUMN c2 BLOB, ALGORITHM=COPY;

--echo # Because of ALGORITHM=COPY, there is TABLE REBUILD and the table isn't
--echo # created with explicit row_format, so we expect ROW_FORMAT=COMPACT
--source suite/innodb/include/default_row_format_show.inc
DROP TABLE t1;

--echo
--echo ###################################
--echo #  Check the row_format effect on ALTER, ALGORITH=COPY on
--echo # create table with explicit row_format
eval CREATE $TEMPORARY TABLE t1 (a INT PRIMARY KEY, b TEXT) ROW_FORMAT=REDUNDANT ENGINE=INNODB;
INSERT INTO t1 VALUES (1, REPEAT('abc',1000));

--source suite/innodb/include/default_row_format_show.inc

SET GLOBAL innoDB_default_row_format = COMPACT;
ALTER TABLE t1 ADD COLUMN c2 BLOB, ALGORITHM=COPY;

--echo # Because of ALGORITHM=COPY, there is TABLE REBUILD and the table is
--echo # created with explicit row_format, so we expect original
--echo # ROW_FORMAT=REDUNDANT
--source suite/innodb/include/default_row_format_show.inc
DROP TABLE t1;

--echo
--echo ##################################
--echo # Check row_format on ALTER ALGORITHM=INPLACE
SET GLOBAL innodb_default_row_format=COMPACT;

eval CREATE $TEMPORARY TABLE t1 (a INT PRIMARY KEY, b TEXT, KEY k1(b(10))) ENGINE=INNODB;
INSERT INTO t1 VALUES (1, REPEAT('abc',1000));

--source suite/innodb/include/default_row_format_show.inc

SET GLOBAL innodb_default_row_format=DYNAMIC;
ALTER TABLE t1 DROP INDEX k1;

--echo # Because it is in-place operation, there is no rebuild, so the
--echo # original format has to be retained.
--source suite/innodb/include/default_row_format_show.inc
DROP TABLE t1;
