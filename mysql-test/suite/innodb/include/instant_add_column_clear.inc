
--echo #
--echo # Scenario 1:
--echo # Create a normal table, rebuild and truncate will clear the instant
--echo # information
--echo #

--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format

INSERT INTO t1 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5);


let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c1 INT DEFAULT 10;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

# An ALTER TABLE which requires rebuild
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c2 INT DEFAULT 20, ADD KEY(c2);

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

# Should be empty now
SELECT count(*) FROM information_schema.innodb_tables WHERE instant_cols != 0;
SELECT count(*) FROM information_schema.innodb_columns WHERE has_default = 1;


INSERT INTO t1(a, b) VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5);

let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c3 INT DEFAULT 10;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

# TRUNCATE TABLE should rebuild the table too
let $new_cols = 1;
let $instant_add_column = TRUNCATE TABLE t1;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

# Should be empty now
SELECT count(*) FROM information_schema.innodb_tables WHERE instant_cols != 0;
SELECT count(*) FROM information_schema.innodb_columns WHERE has_default = 1;


CHECK TABLE t1;

SHOW CREATE TABLE t1;

DROP TABLE t1;



--echo #
--echo # Scenario 2:
--echo # Create a partitioned table, rebuild and truncate will clear the instant
--echo # information
--echo #

--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format PARTITION BY HASH(a) PARTITIONS 3;

INSERT INTO t1 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5), (0, 6), (0, 7), (0, 8);

SELECT count(*) AS `Expect 0` FROM information_schema.innodb_tables WHERE instant_cols != 0;
SELECT count(*) AS `Expect 0` FROM information_schema.innodb_columns WHERE has_default = 1;

ALTER TABLE t1 ADD COLUMN c1 INT DEFAULT 10;

SELECT count(*) AS `Expect 3` FROM information_schema.innodb_tables WHERE instant_cols != 0;
SELECT count(*) AS `Expect 1` FROM information_schema.innodb_columns WHERE has_default = 1;

# This will rebuild all partitions
ALTER TABLE t1 ADD COLUMN c2 INT DEFAULT 20, ALGORITHM = COPY;

SELECT count(*) AS `Expect 0` FROM information_schema.innodb_tables WHERE instant_cols != 0;
SELECT count(*) AS `Expect 0` FROM information_schema.innodb_columns WHERE has_default = 1;

INSERT INTO t1(b, c1, c2) SELECT b, c1, c2 FROM t1;

ALTER TABLE t1 ADD COLUMN c3 INT DEFAULT 30;

SELECT count(*) AS `Expect 3` FROM information_schema.innodb_tables WHERE instant_cols != 0;
SELECT count(*) AS `Expect 1` FROM information_schema.innodb_columns WHERE has_default = 1;

# This will rebuild all partitions
TRUNCATE TABLE t1;

SELECT count(*) AS `Expect 0` FROM information_schema.innodb_tables WHERE instant_cols != 0;
SELECT count(*) AS `Expect 0` FROM information_schema.innodb_columns WHERE has_default = 1;


CHECK TABLE t1;

SHOW CREATE TABLE t1;

DROP TABLE t1;



--echo #
--echo # Scenario 3:
--echo # Create a partitioned table, ALTER TABLE ... PARTITION will clear the
--echo # instant information
--echo #

# Range partitioned table
--eval CREATE TABLE t1 (a INT NOT NULL PRIMARY KEY, b INT) ROW_FORMAT=$row_format PARTITION BY RANGE (a) (PARTITION p1 VALUES LESS THAN (10), PARTITION p2 VALUES LESS THAN (20), PARTITION p3 VALUES LESS THAN (30));

INSERT INTO t1 VALUES(1, 1), (2, 2), (11, 11), (12, 12), (21, 21), (22, 22), (26, 26), (27, 27);

SELECT count(*) AS `Expect 0` FROM information_schema.innodb_tables WHERE instant_cols != 0;
SELECT count(*) AS `Expect 0` FROM information_schema.innodb_columns WHERE has_default = 1;

ALTER TABLE t1 ADD COLUMN c INT DEFAULT 100;

SELECT count(*) AS `Expect 3` FROM information_schema.innodb_tables WHERE instant_cols != 0;
SELECT count(*) AS `Expect 1` FROM information_schema.innodb_columns WHERE has_default = 1;

# The new partition should have no instant column information
ALTER TABLE t1 ADD PARTITION (PARTITION p4 VALUES LESS THAN(40));

SELECT count(*) AS `Expect 3` FROM information_schema.innodb_tables WHERE instant_cols != 0;
SELECT count(*) AS `Expect 1` FROM information_schema.innodb_columns WHERE has_default = 1;
SELECT instant_cols FROM information_schema.innodb_tables WHERE name LIKE '%p4%';

# The new partition should have no instant column information
ALTER TABLE t1 REORGANIZE PARTITION p3 INTO (PARTITION p31 VALUES LESS THAN(25), PARTITION p32 VALUES LESS THAN(30));

SELECT * FROM t1 WHERE a > 20 AND a < 30;
SELECT * FROM t1 WHERE a > 10 AND a < 20;

SELECT count(*) AS `Expect 2` FROM information_schema.innodb_tables WHERE instant_cols != 0;
SELECT count(*) AS `Expect 1` FROM information_schema.innodb_columns WHERE has_default = 1;
SELECT instant_cols FROM information_schema.innodb_tables WHERE name LIKE '%p3%';

# The truncated partition should not have instant information
ALTER TABLE t1 TRUNCATE PARTITION p1;

SELECT * FROM t1 WHERE a < 10;

SELECT count(*) AS `Expect 1` FROM information_schema.innodb_tables WHERE instant_cols != 0;
SELECT count(*) AS `Expect 1` FROM information_schema.innodb_columns WHERE has_default = 1;
SELECT instant_cols FROM information_schema.innodb_tables WHERE name LIKE '%p1%';

# Truncate all partitions, the table instant information should be clean
ALTER TABLE t1 TRUNCATE PARTITION p1, p2, p31, p32, p4;

SELECT count(*) AS `Expect 0` FROM information_schema.innodb_tables WHERE instant_cols != 0;
SELECT count(*) AS `Expect 0` FROM information_schema.innodb_columns WHERE has_default = 1;


INSERT INTO t1(a, b) VALUES(1, 1), (2, 2), (11, 11), (12, 12), (21, 21), (22, 22), (26, 26), (27, 27);

SELECT c FROM t1 GROUP BY c;

ALTER TABLE t1 ADD COLUMN d INT DEFAULT 100;

SELECT count(*) AS `Expect 5` FROM information_schema.innodb_tables WHERE instant_cols != 0;
SELECT count(*) AS `Expect 1` FROM information_schema.innodb_columns WHERE has_default = 1;

ALTER TABLE t1 TRUNCATE PARTITION p1;

SELECT count(*) AS `Expect 4` FROM information_schema.innodb_tables WHERE instant_cols != 0;
SELECT count(*) AS `Expect 1` FROM information_schema.innodb_columns WHERE has_default = 1;
SELECT instant_cols FROM information_schema.innodb_tables WHERE name LIKE '%p1%';


# Drop all partitions except p1, the table information should be clean,
# because p1 is a new partition
ALTER TABLE t1 DROP PARTITION p2, p4;

SELECT count(*) AS `Expect 2` FROM information_schema.innodb_tables WHERE instant_cols != 0;
SELECT count(*) AS `Expect 1` FROM information_schema.innodb_columns WHERE has_default = 1;

ALTER TABLE t1 DROP PARTITION p31, p32;

SELECT count(*) AS `Expect 0` FROM information_schema.innodb_tables WHERE instant_cols != 0;
SELECT count(*) AS `Expect 0` FROM information_schema.innodb_columns WHERE has_default = 1;


CHECK TABLE t1;

SHOW CREATE TABLE t1;

DROP TABLE t1;


# HASH partitioned table
--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format PARTITION BY HASH(a) PARTITIONS 3;

INSERT INTO t1 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5), (0, 6), (0, 7), (0, 8);

SELECT count(*) AS `Expect 0` FROM information_schema.innodb_tables WHERE instant_cols != 0;
SELECT count(*) AS `Expect 0` FROM information_schema.innodb_columns WHERE has_default = 1;

ALTER TABLE t1 ADD COLUMN c1 INT DEFAULT 10;

SELECT count(*) AS `Expect 3` FROM information_schema.innodb_tables WHERE instant_cols != 0;
SELECT count(*) AS `Expect 1` FROM information_schema.innodb_columns WHERE has_default = 1;

# ADD PARTITION will rebuild the whole table, and instant information should be
# clean for each partition
ALTER TABLE t1 ADD PARTITION PARTITIONS 2;

SELECT count(*) AS `Expect 0` FROM information_schema.innodb_tables WHERE instant_cols != 0;
SELECT count(*) AS `Expect 0` FROM information_schema.innodb_columns WHERE has_default = 1;


ALTER TABLE t1 ADD COLUMN c2 INT DEFAULT 20;

SELECT count(*) AS `Expect 5` FROM information_schema.innodb_tables WHERE instant_cols != 0;
SELECT count(*) AS `Expect 1` FROM information_schema.innodb_columns WHERE has_default = 1;

# COALESCE PARTITION will rebuild the whole table, and instant information
# should be clean for each partition
ALTER TABLE t1 COALESCE PARTITION 2;

SELECT count(*) AS `Expect 0` FROM information_schema.innodb_tables WHERE instant_cols != 0;
SELECT count(*) AS `Expect 0` FROM information_schema.innodb_columns WHERE has_default = 1;


# Verify it once more

ALTER TABLE t1 ADD COLUMN c3 INT DEFAULT 30;

SELECT count(*) AS `Expect 3` FROM information_schema.innodb_tables WHERE instant_cols != 0;
SELECT count(*) AS `Expect 1` FROM information_schema.innodb_columns WHERE has_default = 1;

# ADD PARTITION will rebuild the whole table, and instant information should be
# clean for each partition
ALTER TABLE t1 ADD PARTITION PARTITIONS 2;

SELECT count(*) AS `Expect 0` FROM information_schema.innodb_tables WHERE instant_cols != 0;
SELECT count(*) AS `Expect 0` FROM information_schema.innodb_columns WHERE has_default = 1;


SELECT * FROM t1;


CHECK TABLE t1;

SHOW CREATE TABLE t1;

DROP TABLE t1;
