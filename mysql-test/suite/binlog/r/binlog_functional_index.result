# Check that the hidden generated column is not included in the binlog
# regardless of binlog_row_image
RESET MASTER;
SET @start_row_image_value = @@session.binlog_row_image;
CREATE TABLE t1 (col1 INT, col2 INT NOT NULL, col3 INT, col4 INT, UNIQUE INDEX ((ABS(col2))));
SET @@session.binlog_row_image = MINIMAL;
INSERT INTO t1 VALUES (1, 2, 3, 4), (2, 3, 4, 5);
UPDATE t1 SET col1 = 2 WHERE ABS(col2) = 2;
SET @@session.binlog_row_image = FULL;
INSERT INTO t1 VALUES (10, 20, 30, 40), (20, 30, 40, 50);
UPDATE t1 SET col1 = 20 WHERE ABS(col2) = 20;
SET @@session.binlog_row_image = NOBLOB;
INSERT INTO t1 VALUES (100, 200, 300, 400), (200, 300, 400, 500);
UPDATE t1 SET col1 = 200 WHERE ABS(col2) = 200;
DROP TABLE t1;
FLUSH LOGS;
include/mysqlbinlog.inc
### INSERT INTO `test`.`t1`
### SET
###   @1=1
###   @2=2
###   @3=3
###   @4=4
### INSERT INTO `test`.`t1`
### SET
###   @1=2
###   @2=3
###   @3=4
###   @4=5
### UPDATE `test`.`t1`
### WHERE
###   @1=1
###   @2=2
###   @3=3
###   @4=4
### SET
###   @1=2
### INSERT INTO `test`.`t1`
### SET
###   @1=10
###   @2=20
###   @3=30
###   @4=40
### INSERT INTO `test`.`t1`
### SET
###   @1=20
###   @2=30
###   @3=40
###   @4=50
### UPDATE `test`.`t1`
### WHERE
###   @1=10
###   @2=20
###   @3=30
###   @4=40
### SET
###   @1=20
###   @2=20
###   @3=30
###   @4=40
### INSERT INTO `test`.`t1`
### SET
###   @1=100
###   @2=200
###   @3=300
###   @4=400
### INSERT INTO `test`.`t1`
### SET
###   @1=200
###   @2=300
###   @3=400
###   @4=500
### UPDATE `test`.`t1`
### WHERE
###   @1=100
###   @2=200
###   @3=300
###   @4=400
### SET
###   @1=200
###   @2=200
###   @3=300
###   @4=400
RESET MASTER;
# Check that when the only unique NOT NULL index is a functional index,
# the binary log contains a full before-image regardless of
# binlog_row_image.
CREATE TABLE t1 (
col1 INT NOT NULL
, col2 INT NOT NULL
, col3 INT NOT NULL
, UNIQUE INDEX idx1 ((ABS(col1))));
INSERT INTO t1 VALUES (1, 1, 1);
SET binlog_row_image = MINIMAL;
UPDATE t1 SET col2 = 2 WHERE ABS(col1) = 1;
SET binlog_row_image = FULL;
UPDATE t1 SET col2 = 3 WHERE ABS(col1) = 1;
SET binlog_row_image = NOBLOB;
UPDATE t1 SET col2 = 4 WHERE ABS(col1) = 1;
DROP TABLE t1;
FLUSH LOGS;
SET @@session.binlog_row_image = @start_row_image_value;
include/mysqlbinlog.inc
### INSERT INTO `test`.`t1`
### SET
###   @1=1
###   @2=1
###   @3=1
### UPDATE `test`.`t1`
### WHERE
###   @1=1
###   @2=1
###   @3=1
### SET
###   @2=2
### UPDATE `test`.`t1`
### WHERE
###   @1=1
###   @2=2
###   @3=1
### SET
###   @1=1
###   @2=3
###   @3=1
### UPDATE `test`.`t1`
### WHERE
###   @1=1
###   @2=3
###   @3=1
### SET
###   @1=1
###   @2=4
###   @3=1
