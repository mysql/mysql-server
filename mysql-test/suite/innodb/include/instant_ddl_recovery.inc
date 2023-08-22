##########################################################################
# Test script to test RECOVERY for table having INSTANT
# ADD/DROP columns.
#
# $row_format is to be set to the row_format on which test is to be run.
##########################################################################

--echo # ------------------------------------------------------------
--echo # Create a table with 3 columns. [id, c1, c2]
--echo # ------------------------------------------------------------
--eval CREATE TABLE t1(id INT PRIMARY KEY, c1 CHAR(10), c2 CHAR(10)) ROW_FORMAT=$row_format;
--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc

INSERT INTO t1 VALUES (1, "r1c1", "r1c2");
SELECT * FROM t1;

--echo # INSTANT ADD a column
Alter table t1 add column c3 char(10), algorithm=instant;
--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc
INSERT INTO t1 VALUES (2, "r2c1", "r2c2", "r2c3");
SELECT * FROM t1;

--echo # INSTANT DROP a column
Alter table t1 drop column c2 , algorithm=instant;
--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc
INSERT INTO t1 VALUES (3, "r3c1", "r3c3");
SELECT * FROM t1;

--echo # Make sure nothing gets flushed on disk
SET GLOBAL innodb_log_checkpoint_now = ON;
SET GLOBAL innodb_page_cleaner_disabled_debug = 1;
SET GLOBAL innodb_dict_stats_disabled_debug = 1;
SET GLOBAL innodb_master_thread_disabled_debug = 1;

--echo # Make inplace update
UPDATE t1 SET c1="r0c0";

--echo # Make not-inplace update
UPDATE t1 SET c3="r1c3" WHERE id=1;

--echo # Insert a new record
INSERT INTO t1 VALUES (4, "r4c1", "r4c3");

--echo # Delete a record
DELETE FROM t1 WHERE id=2;

# kill and restart the server
--source include/kill_and_restart_mysqld.inc

#restart the server

--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc

SELECT * FROM t1;

--echo ###########
--echo # CLEANUP #
--echo ###########
DROP TABLE t1;

#
# Bug#35183686 ALTER_STORED_COLUMN_ORDER with algorithm=INSTANT makes wrong INSERT REDO log
#
--eval CREATE TABLE t1 (`c` text, `b` blob NOT NULL, `nc03242` longblob, `d` int DEFAULT NULL, UNIQUE KEY `b` (`b`(99))) ENGINE=InnoDB ROW_FORMAT=$row_format

alter table t1
  change c c text after d,
  add column nc05984 bool,
algorithm=instant;

SET GLOBAL innodb_log_checkpoint_now = ON;
SET GLOBAL innodb_page_cleaner_disabled_debug = 1;
SET GLOBAL innodb_dict_stats_disabled_debug = 1;
SET GLOBAL innodb_master_thread_disabled_debug = 1;

INSERT INTO t1 SET b='ulccclraaacaucrouorouoooolrlo', d=6;

--source include/kill_and_restart_mysqld.inc

SELECT * FROM t1;

DROP TABLE t1;
