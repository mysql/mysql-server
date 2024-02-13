--source include/have_debug.inc
--source include/have_debug_sync.inc
--source include/have_component_keyring_file.inc
--source suite/component_keyring_file/inc/setup_component.inc

--echo #
--echo # WL#3627 Add COLUMN_FORMAT and STORAGE for fields
--echo #

CREATE TABLE t1 (
 a int STORAGE DISK,
 b int STORAGE MEMORY NOT NULL,
 c int COLUMN_FORMAT DYNAMIC,
 d int COLUMN_FORMAT FIXED,
 e int COLUMN_FORMAT DEFAULT,
 f int STORAGE DISK COLUMN_FORMAT DYNAMIC NOT NULL,
 g int STORAGE MEMORY COLUMN_FORMAT DYNAMIC,
 h int STORAGE DISK COLUMN_FORMAT FIXED,
 i int STORAGE MEMORY COLUMN_FORMAT FIXED
);
SHOW CREATE TABLE t1;

# Add new columns with all variations of the new column
# level attributes
ALTER TABLE t1
  ADD COLUMN j int STORAGE DISK,
  ADD COLUMN k int STORAGE MEMORY NOT NULL,
  ADD COLUMN l int COLUMN_FORMAT DYNAMIC,
  ADD COLUMN m int COLUMN_FORMAT FIXED,
  ADD COLUMN n int COLUMN_FORMAT DEFAULT,
  ADD COLUMN o int STORAGE DISK COLUMN_FORMAT DYNAMIC NOT NULL,
  ADD COLUMN p int STORAGE MEMORY COLUMN_FORMAT DYNAMIC,
  ADD COLUMN q int STORAGE DISK COLUMN_FORMAT FIXED,
  ADD COLUMN r int STORAGE MEMORY COLUMN_FORMAT FIXED;
SHOW CREATE TABLE t1;

# Use MODIFY COLUMN to "shift" all new attributes to the next column
ALTER TABLE t1
  MODIFY COLUMN j int STORAGE MEMORY NOT NULL,
  MODIFY COLUMN k int COLUMN_FORMAT DYNAMIC,
  MODIFY COLUMN l int COLUMN_FORMAT FIXED,
  MODIFY COLUMN m int COLUMN_FORMAT DEFAULT,
  MODIFY COLUMN n int STORAGE DISK COLUMN_FORMAT DYNAMIC NOT NULL,
  MODIFY COLUMN o int STORAGE MEMORY COLUMN_FORMAT DYNAMIC,
  MODIFY COLUMN p int STORAGE DISK COLUMN_FORMAT FIXED,
  MODIFY COLUMN q int STORAGE MEMORY COLUMN_FORMAT FIXED,
  MODIFY COLUMN r int STORAGE DISK;
SHOW CREATE TABLE t1;

# Check behaviour of multiple COLUMN_FORMAT and/or STORAGE definitions
ALTER TABLE t1
  MODIFY COLUMN h int COLUMN_FORMAT DYNAMIC COLUMN_FORMAT FIXED,
  MODIFY COLUMN i int COLUMN_FORMAT DYNAMIC COLUMN_FORMAT DEFAULT,
  MODIFY COLUMN j int COLUMN_FORMAT FIXED COLUMN_FORMAT DYNAMIC,
  MODIFY COLUMN k int COLUMN_FORMAT FIXED COLUMN_FORMAT DEFAULT,
  MODIFY COLUMN l int STORAGE DISK STORAGE MEMORY,
  MODIFY COLUMN m int STORAGE DISK STORAGE DEFAULT,
  MODIFY COLUMN n int STORAGE MEMORY STORAGE DISK,
  MODIFY COLUMN o int STORAGE MEMORY STORAGE DEFAULT,
  MODIFY COLUMN p int STORAGE DISK STORAGE MEMORY
                      COLUMN_FORMAT FIXED COLUMN_FORMAT DYNAMIC,
  MODIFY COLUMN q int STORAGE DISK STORAGE MEMORY STORAGE DEFAULT
                      COLUMN_FORMAT FIXED COLUMN_FORMAT DYNAMIC COLUMN_FORMAT DEFAULT,
  MODIFY COLUMN r int STORAGE DEFAULT STORAGE DEFAULT STORAGE MEMORY
                      STORAGE DISK STORAGE MEMORY STORAGE DISK STORAGE DISK;
SHOW CREATE TABLE t1;

DROP TABLE t1;


--echo #
--echo # Bug#21347001   SEGMENTATION FAULT WHILE CREATING GENERAL
--echo #                  TABLESPACE IN DISK FULL LINUX
--echo #
SET SESSION debug="+d,out_of_tablespace_disk";
--error ER_CREATE_FILEGROUP_FAILED
CREATE TABLESPACE `ts6` ADD DATAFILE 'ts6.ibd' ENGINE=INNODB;
SHOW WARNINGS;
SET SESSION debug="-d,out_of_tablespace_disk";


--echo #
--echo # Additional coverage for WL#7743 "New data dictionary: changes
--echo # to DDL-related parts of SE API".
--echo #
--echo # Check that limits on tablespace comment and datafile path lengths
--echo # are enforced.
--echo #
--let $TOO_LONG_COMMENT= `SELECT REPEAT('a', 2049)`
--replace_result $TOO_LONG_COMMENT TOO_LONG_COMMENT
--error ER_TOO_LONG_TABLESPACE_COMMENT
--eval CREATE TABLESPACE ts ADD DATAFILE 'ts.ibd' COMMENT="$TOO_LONG_COMMENT" ENGINE=InnoDB
--let $TOO_LONG_PATH= `SELECT CONCAT(REPEAT('a', 512),'.ibd')`
--replace_result $TOO_LONG_PATH TOO_LONG_PATH
--error ER_PATH_LENGTH
--eval CREATE TABLESPACE ts ADD DATAFILE '$TOO_LONG_PATH' ENGINE=InnoDB
--echo # Also coverage for ALTER TABLESPACE case.
--error ER_TABLESPACE_MISSING_WITH_NAME
ALTER TABLESPACE no_such_ts ADD DATAFILE 'ts.ibd';
CREATE TABLESPACE ts ADD DATAFILE 'ts.ibd' ENGINE=InnoDB;
--let $TOO_LONG_PATH= `SELECT CONCAT(REPEAT('a', 512),'.ibd')`
--replace_result $TOO_LONG_PATH TOO_LONG_PATH
--error ER_PATH_LENGTH
--eval ALTER TABLESPACE ts ADD DATAFILE '$TOO_LONG_PATH'
--error ER_MISSING_TABLESPACE_FILE
ALTER TABLESPACE ts DROP DATAFILE 'no_such_file.ibd';
DROP TABLESPACE ts;

--disable_connect_log
--echo #
--echo # Validate tablespace names in the SE.
--echo #
--echo # 1. Tablespace DDL.
--echo # 1.1 Create/drop predefined tablespaces.

--error ER_WRONG_TABLESPACE_NAME
CREATE TABLESPACE innodb_system ADD DATAFILE 'f.ibd' ENGINE InnoDB;
--error ER_WRONG_TABLESPACE_NAME
CREATE TABLESPACE innodb_file_per_table ADD DATAFILE 'f.ibd' ENGINE InnoDB;
--error ER_WRONG_TABLESPACE_NAME
CREATE TABLESPACE innodb_temporary ADD DATAFILE 'f.ibd' ENGINE InnoDB;
--error ER_WRONG_TABLESPACE_NAME
CREATE TABLESPACE mysql ADD DATAFILE 'f.ibd' ENGINE InnoDB;

--error ER_WRONG_TABLESPACE_NAME
DROP TABLESPACE innodb_system;
--error ER_TABLESPACE_MISSING_WITH_NAME
DROP TABLESPACE innodb_file_per_table;
--error ER_WRONG_TABLESPACE_NAME
DROP TABLESPACE innodb_temporary;
--error ER_WRONG_TABLESPACE_NAME
DROP TABLESPACE mysql;

--echo # 1.2 Create/drop implicit tablespaces.

--error ER_WRONG_TABLESPACE_NAME
CREATE TABLESPACE `innodb_file_per_table.2` ADD DATAFILE 'f.ibd' ENGINE InnoDB;
--error ER_TABLESPACE_MISSING_WITH_NAME
DROP TABLESPACE `innodb_file_per_table.2`;

--error ER_WRONG_TABLESPACE_NAME
CREATE TABLESPACE innodb_file_per_table_whatever ADD DATAFILE 'f.ibd' ENGINE InnoDB;
--error ER_TABLESPACE_MISSING_WITH_NAME
DROP TABLESPACE innodb_file_per_table_whatever;

--error ER_WRONG_TABLESPACE_NAME
CREATE TABLESPACE innodb_file_per_table ADD DATAFILE 'f.ibd' ENGINE InnoDB;
--error ER_TABLESPACE_MISSING_WITH_NAME
DROP TABLESPACE innodb_file_per_table;

--echo # 2. Non partitioned table DDL.
--echo # 2.1 Create table.

CREATE TABLE t1 (i INTEGER) TABLESPACE innodb_file_per_table ENGINE InnoDB;
CREATE TABLE t2 (i INTEGER) TABLESPACE innodb_system ENGINE InnoDB;

SHOW CREATE TABLE t1;
SHOW CREATE TABLE t2;

--error ER_WRONG_TABLESPACE_NAME
CREATE TABLE t_bad (i INTEGER) TABLESPACE `innodb_file_per_table.2` ENGINE InnoDB;

--echo # For t1, changing engine back to InnoDB will re-establish usage of the implicit tablespace:
ALTER TABLE t1 ENGINE InnoDB;
SHOW CREATE TABLE t1;

--echo # Changing both engine and tablespace works:
ALTER TABLE t1 TABLESPACE innodb_system ENGINE InnoDB;
SHOW CREATE TABLE t1;
ALTER TABLE t2 TABLESPACE innodb_file_per_table ENGINE InnoDB;
SHOW CREATE TABLE t2;

--echo # Keeping a valid tablespace through ALTER TABLE:
ALTER TABLE t1 ADD COLUMN (j INTEGER);
CREATE TABLESPACE ts ADD DATAFILE 'f.ibd' ENGINE InnoDB;
ALTER TABLE t1 TABLESPACE ts;
ALTER TABLE t1 ENGINE InnoDB;
SHOW CREATE TABLE t1;
ALTER TABLE t1 ENGINE InnoDB;
SHOW CREATE TABLE t1;

DROP TABLE t1;
DROP TABLE t2;
DROP TABLESPACE ts;

--echo # 3. Partitioned table DDL.
--echo # 3.1 Create table.

--error ER_WRONG_TABLESPACE_NAME
CREATE TABLE t_part_bad (i INTEGER) PARTITION BY RANGE(i)
PARTITIONS 2 (
    PARTITION p0 VALUES LESS THAN(100) TABLESPACE `innodb_file_per_table.2`,
    PARTITION p1 VALUES LESS THAN(200));

CREATE TABLE t_part (i INTEGER) TABLESPACE innodb_file_per_table PARTITION BY RANGE(i)
PARTITIONS 2 (
    PARTITION p0 VALUES LESS THAN(100),
    PARTITION p1 VALUES LESS THAN(200));
SHOW CREATE TABLE t_part;

CREATE TABLE t_subpart (i INTEGER) PARTITION BY RANGE(i)
PARTITIONS 2 SUBPARTITION BY HASH(i) (
    PARTITION p0 VALUES LESS THAN(100) (
      SUBPARTITION sp00,
      SUBPARTITION sp01),
    PARTITION p1 VALUES LESS THAN(200) (
      SUBPARTITION sp10,
      SUBPARTITION sp11));
SHOW CREATE TABLE t_subpart;

--echo # 2.3 Alter table.

# TODO : Enable following once shared tablespaces are allowed in Partitioned
#	 Tables (wl#12034).
# ALTER TABLE t_part TABLESPACE innodb_system;
# SHOW CREATE TABLE t_part;

ALTER TABLE t_subpart TABLESPACE innodb_file_per_table;
SHOW CREATE TABLE t_subpart;

--error ER_WRONG_TABLESPACE_NAME
ALTER TABLE t_part TABLESPACE `innodb_file_per_table.2`;

--error ER_WRONG_TABLESPACE_NAME
ALTER TABLE t_subpart TABLESPACE `innodb_file_per_table.2`;

--error ER_WRONG_TABLESPACE_NAME
ALTER TABLE t_part REORGANIZE PARTITION p1 INTO
  (PARTITION p1 VALUES LESS THAN (300) TABLESPACE `innodb_file_per_table.2`);

--error ER_WRONG_TABLESPACE_NAME
ALTER TABLE t_subpart REORGANIZE PARTITION p1 INTO
  (PARTITION p1 VALUES LESS THAN (300) TABLESPACE `innodb_file_per_table.2`);

--error ER_WRONG_TABLESPACE_NAME
ALTER TABLE t_subpart REORGANIZE PARTITION s11 INTO
  (PARTITION s11 TABLESPACE `innodb_file_per_table.2`);

DROP TABLE t_part;
DROP TABLE t_subpart;


--echo #
--echo # Tescases for wl#8972. Rename a general tablespace
--echo #

CREATE TABLESPACE ts ADD DATAFILE 'f.ibd' ENGINE InnoDB;
CREATE TABLE t1(i INT) TABLESPACE ts;
SHOW CREATE TABLE t1;

--echo # Negative tests

--error ER_TABLESPACE_EXISTS
ALTER TABLESPACE ts RENAME TO innodb_system;

--error ER_WRONG_TABLESPACE_NAME
ALTER TABLESPACE ts RENAME TO innodb_file_per_table;

--error ER_TABLESPACE_EXISTS
ALTER TABLESPACE ts RENAME TO innodb_temporary;

--error ER_TABLESPACE_EXISTS
ALTER TABLESPACE ts RENAME TO mysql;

--error ER_WRONG_TABLESPACE_NAME
ALTER TABLESPACE innodb_system RENAME TO ts3;

--error ER_WRONG_TABLESPACE_NAME
ALTER TABLESPACE innodb_temporary RENAME TO ts3;

--error ER_WRONG_TABLESPACE_NAME
ALTER TABLESPACE mysql RENAME TO ts3;

--error ER_WRONG_TABLESPACE_NAME
ALTER TABLESPACE ts RENAME TO `innodb_file_per_table.2`;

--error ER_WRONG_TABLESPACE_NAME
ALTER TABLESPACE ts RENAME TO innodb_file_per_table_whatever;

--error ER_WRONG_TABLESPACE_NAME
ALTER TABLESPACE ts RENAME TO innodb_file_per_table;


--echo #
--echo # Test cases for Bug #26073851
--echo # NO ERROR OR WARNING WHEN CREATING TABLES IN A RESERVED TABLESPACE
--echo #
CREATE TABLESPACE altering ADD DATAFILE 'altering.ibd' ENGINE InnoDB;
CREATE TABLE altering_table (id int) TABLESPACE altering;

--echo # Negative tests

--echo # Altering tables in the mysql tablespace must be allowed
--echo # because of the mysql upgrade client.
ALTER TABLE mysql.user ENGINE InnoDB TABLESPACE mysql;

--echo # An arbitrary table cannot be altered to be in the mysql tablespace.
--error ER_RESERVED_TABLESPACE_NAME
ALTER TABLE altering_table TABLESPACE mysql;

--echo # An arbitrary table cannot be created in the mysql tablespace.
--error ER_RESERVED_TABLESPACE_NAME
CREATE TABLE foos (id int) TABLESPACE mysql;

--echo # An arbitrary partitioned table cannot have partitions in the mysql tablespace.
--error ER_RESERVED_TABLESPACE_NAME
CREATE TABLE partition_test (a INT, b INT) ENGINE = InnoDB ROW_FORMAT=DYNAMIC
PARTITION BY RANGE(a) SUBPARTITION BY KEY(b) (
PARTITION p1 VALUES LESS THAN (100) TABLESPACE mysql,
PARTITION p2 VALUES LESS THAN (200) TABLESPACE mysql,
PARTITION p3 VALUES LESS THAN (300) TABLESPACE mysql,
PARTITION p4 VALUES LESS THAN (400) TABLESPACE mysql);

--echo # An arbitrary partitioned table cannot have partitions in the mysql tablespace.
--error ER_RESERVED_TABLESPACE_NAME
CREATE TABLE partition_test2 (a INT, b INT) ENGINE = InnoDB ROW_FORMAT=DYNAMIC
PARTITION BY RANGE(a) SUBPARTITION BY KEY(b) (
PARTITION p1 VALUES LESS THAN (100) TABLESPACE altering,
PARTITION p2 VALUES LESS THAN (200) TABLESPACE altering,
PARTITION p3 VALUES LESS THAN (300) TABLESPACE mysql,
PARTITION p4 VALUES LESS THAN (400) TABLESPACE mysql);

# TODO : Enable following once shared tablespaces are allowed in partitioned
#	 Tables (wl#12034).
# --echo # Create a partitioned table to be altered later.
# CREATE TABLE partition_test3 (a INT, b INT) ENGINE = InnoDB ROW_FORMAT=DYNAMIC
# PARTITION BY RANGE(a) SUBPARTITION BY KEY(b) (
# PARTITION p1 VALUES LESS THAN (100) TABLESPACE altering,
# PARTITION p2 VALUES LESS THAN (200) TABLESPACE altering,
# PARTITION p3 VALUES LESS THAN (300) TABLESPACE altering,
# PARTITION p4 VALUES LESS THAN (400) TABLESPACE altering);
#
# --echo # An arbitrary partitioned table cannot have new partitions in the mysql tablespace.
# --error ER_RESERVED_TABLESPACE_NAME
# ALTER TABLE partition_test3 ADD PARTITION
# (PARTITION p5 VALUES LESS THAN (500) TABLESPACE mysql);
#
# --echo # An arbitrary partitioned table cannot have partitions altered to be in the mysql tablespace.
# --error ER_RESERVED_TABLESPACE_NAME
# ALTER TABLE partition_test3 REORGANIZE PARTITION p1 INTO
#   (PARTITION p1 VALUES LESS THAN (100) TABLESPACE `mysql`);

--echo # An arbitrary partitioned table cannot have subpartitions in the mysql tablespace.
--error ER_RESERVED_TABLESPACE_NAME
CREATE TABLE partition_test4 (i INTEGER) PARTITION BY RANGE(i)
PARTITIONS 2 SUBPARTITION BY HASH(i) (
    PARTITION p0 VALUES LESS THAN(100) (
      SUBPARTITION sp00,
      SUBPARTITION sp01),
    PARTITION p1 VALUES LESS THAN(200) (
      SUBPARTITION sp10,
      SUBPARTITION sp11 TABLESPACE mysql));

--echo Clean up
DROP TABLE altering_table;
# DROP TABLE partition_test3;
DROP TABLESPACE altering;

# Check that new name shows up in I_S
SELECT (COUNT(*)=1) FROM INFORMATION_SCHEMA.FILES WHERE TABLESPACE_NAME = 'ts';

ALTER TABLESPACE ts RENAME TO ts2;

SELECT (COUNT(*)=1) FROM INFORMATION_SCHEMA.FILES WHERE TABLESPACE_NAME = 'ts2';
SELECT (COUNT(*)=0) FROM INFORMATION_SCHEMA.FILES WHERE TABLESPACE_NAME = 'ts';

--echo # Should display ts2 as tablespace name
SHOW CREATE TABLE t1;

--echo # Check that table in renamed tablespace is accessible
SELECT * FROM t1;
ALTER TABLE t1 ADD COLUMN j VARCHAR(32);

--echo # Should display ts2 as tablespace name, and new column
SHOW CREATE TABLE t1;

INSERT INTO t1 VALUES (0,'0'),(1,'1'),(2,'2'),(3,'3');

--echo # Check that a new table can be created in the renamed tablespace
CREATE TABLE t2(j int) TABLESPACE ts2;
SHOW CREATE TABLE t2;

INSERT INTO t2 VALUES (0),(1),(2),(3);

DROP TABLE t2;
DROP TABLE t1;
DROP TABLESPACE ts2;

--echo # 2. Tablespace operations without required privileges are rejected

--echo # Create user without any privileges.
CREATE USER noprivs@localhost;
REVOKE ALL ON *.* FROM noprivs@localhost;

--echo # Connect as user noprivs@localhost;
connect (con_no_tblspc,localhost,noprivs,,);

--echo # Should fail due to missing privileges

--error ER_SPECIFIC_ACCESS_DENIED_ERROR
CREATE TABLESPACE ts1 ADD DATAFILE 'df1.ibd';

--error ER_SPECIFIC_ACCESS_DENIED_ERROR
ALTER TABLESPACE ts1 ADD DATAFILE 'df2.ibd';

--error ER_SPECIFIC_ACCESS_DENIED_ERROR
ALTER TABLESPACE ts1 DROP DATAFILE 'df2.ibd';

--error ER_SPECIFIC_ACCESS_DENIED_ERROR
ALTER TABLESPACE ts1 RENAME TO ts2;

--error ER_SPECIFIC_ACCESS_DENIED_ERROR
DROP TABLESPACE ts1;

--error ER_SPECIFIC_ACCESS_DENIED_ERROR
CREATE LOGFILE GROUP lg1
  ADD UNDOFILE 'lg1_undofile.dat'
  INITIAL_SIZE 1M
  UNDO_BUFFER_SIZE = 1M;

--error ER_SPECIFIC_ACCESS_DENIED_ERROR
DROP LOGFILE GROUP lg1;

disconnect con_no_tblspc;

connection default;
DROP USER noprivs@localhost;

--echo # Tablespace operations are autocommited
SET AUTOCOMMIT=OFF;
BEGIN WORK;
CREATE TABLESPACE ts1 ADD DATAFILE 'df1.ibd';
ROLLBACK WORK;

BEGIN WORK;
ALTER TABLESPACE ts1 RENAME TO ts2;
ROLLBACK WORK;

BEGIN WORK;
DROP TABLESPACE ts2;
ROLLBACK WORK;

SET AUTOCOMMIT=ON;

# TODO : Enable following once shared tablespaces are allowed in Partitioned
#	 Tables (wl#12034).
# --echo #
# --echo # Additional test cases requested by QA
# --echo #
#
# --echo # Test ALTER TABLESPACE RENAME with partition tables
# CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' Engine=InnoDB;
#
# --replace_result $MYSQL_TMP_DIR MYSQL_TMP_DIR
# let $restart_parameters = restart: --innodb-directories=$MYSQL_TMP_DIR $PLUGIN_DIR_OPT;
--replace_result $PLUGIN_DIR_OPT PLUGIN_DIR_OPT
# --source include/restart_mysqld.inc
#
# --disable_query_log
# --eval CREATE TABLESPACE ts2 ADD DATAFILE '$MYSQL_TMP_DIR/ts2.ibd' ENGINE=InnoDB;
# --enable_query_log
#
# CREATE TABLE tab (a INT, b INT) ENGINE = InnoDB ROW_FORMAT=DYNAMIC
# PARTITION BY RANGE(a) SUBPARTITION BY KEY(b) (
# PARTITION p1 VALUES LESS THAN (100) TABLESPACE=ts1,
# PARTITION p2 VALUES LESS THAN (200) TABLESPACE=ts2,
# PARTITION p3 VALUES LESS THAN (300) TABLESPACE innodb_file_per_table,
# PARTITION p4 VALUES LESS THAN (400) TABLESPACE innodb_system);
#
# INSERT INTO tab VALUES(99,99);
#
# INSERT INTO tab VALUES(199,199);
#
# INSERT INTO tab VALUES(299,299);
#
# INSERT INTO tab VALUES(399,399);
#
# SHOW CREATE TABLE tab;
#
# ALTER TABLESPACE ts1 RENAME TO ts11 ;
#
# ALTER TABLESPACE ts2 RENAME TO ts22 ;
#
# SHOW CREATE TABLE tab;
#
# DELETE FROM tab;
#
# SELECT COUNT(*) FROM tab;
#
# ALTER TABLE tab ADD PARTITION (PARTITION p5 VALUES LESS THAN (500) TABLESPACE ts22);
#
# INSERT INTO tab VALUES(99,99);
#
# INSERT INTO tab VALUES(199,199);
#
# INSERT INTO tab VALUES(299,299);
#
# INSERT INTO tab VALUES(399,399);
#
# INSERT INTO tab VALUES(499,499);
#
# SELECT COUNT(*) FROM tab;
#
# SHOW CREATE TABLE tab;
#
# ALTER TABLE tab ROW_FORMAT=COMPACT;
#
# ALTER TABLESPACE ts11 RENAME TO ts1 ;
#
# ALTER TABLESPACE ts22 RENAME TO ts2 ;
#
# SHOW CREATE TABLE tab;
#
# ALTER TABLE tab ROW_FORMAT=REDUNDANT;
#
# SHOW CREATE TABLE tab;
#
# --echo # failed: 1478:  Tablespace `ts1` cannot contain a COMPRESSED table
# #ALTER TABLE tab ROW_FORMAT=COMPRESSED;
#
# SHOW CREATE TABLE tab;
#
# DROP TABLE tab;
#
# DROP TABLESPACE ts1;
#
# DROP TABLESPACE ts2;
#
#
# --echo # Check ALTER TABLESPACE RENAME with STORED PROCEDURE
# CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENGINE=InnoDB;
#
# --disable_query_log
# --eval CREATE TABLESPACE ts2 ADD DATAFILE '$MYSQL_TMP_DIR/ts2.ibd' ENGINE=InnoDB;
# --enable_query_log
#
# CREATE TABLE tab1(c1 int) ENGINE=InnoDB TABLESPACE=ts1;
#
# CREATE TABLE tab2(c1 int) ENGINE=InnoDB TABLESPACE=ts2;
#
# delimiter |;
#
# CREATE PROCEDURE proc_alter()
# BEGIN
#
# ALTER TABLESPACE ts1 RENAME TO ts11 ;
#
# INSERT INTO tab1 VALUES(1);
#
# ALTER TABLESPACE ts2 RENAME TO ts22 ;
#
# INSERT INTO tab2 VALUES(1);
#
# END|
# delimiter ;|
#
# CALL proc_alter();
#
# SELECT COUNT(*) FROM tab1;
#
# SELECT COUNT(*) FROM tab2;
#
# SHOW CREATE TABLE tab1;
#
# SHOW CREATE TABLE tab2;
#
# DROP PROCEDURE proc_alter;
#
# DROP TABLE tab1,tab2;
#
# DROP TABLESPACE ts11;
#
# DROP TABLESPACE ts22;
#
#
# --echo #
# --echo # Bug#26428558: INNODB: ASSERTION FAILURE:
# --echo # HA_INNOPART.CC:2431:PART_ELEM->TABLESPACE_NAME == __N
# --echo #
#
# CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' Engine=InnoDB;
#
# CREATE TABLE tab (a INT, b INT) ENGINE=InnoDB #ROW_FORMAT=DYNAMIC
# PARTITION BY RANGE(a) SUBPARTITION BY KEY(b) (
# PARTITION p1 VALUES LESS THAN (100) TABLESPACE=ts1);
#
# SHOW CREATE TABLE tab;
# ALTER TABLESPACE ts1 RENAME TO ts11 ;
#
# --echo # Crash
# SHOW CREATE TABLE tab;
#
# DROP TABLE tab;
# DROP TABLESPACE ts11;


--echo #
--echo # Bug#26435800: DD::PROPERTIES_IMPL::GET_UINT64 (THIS=0X7FAC6822B0A0,
--echo # KEY=...,PROPERTIES_IMPL.H:
--echo #

CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' Engine=InnoDB;
CREATE TABLE t1 (a INT, b INT) ENGINE = InnoDB TABLESPACE=ts1;
ALTER TABLESPACE ts1 RENAME TO ts11;

TRUNCATE TABLE t1; 
DROP TABLE t1;
DROP TABLESPACE ts11;

--echo #
--echo # WL#12236 - CREATE TABLESPACE without DATAFILE clause.
--echo #

CREATE TABLESPACE ts Engine=InnoDB;
CREATE TABLESPACE ts1 Engine=InnoDB;
SELECT NAME FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME LIKE 'ts%';
CREATE TABLE t1(c INT) ENGINE=InnoDB TABLESPACE=ts;
INSERT INTO t1 VALUES(1);
ALTER TABLE t1 TABLESPACE ts1;
SHOW CREATE TABLE t1;
DROP TABLE t1;
DROP TABLESPACE ts1;
DROP TABLESPACE ts;

--echo #
--echo # BUG#28656211 - DEBUG SERVER CRASHES WHEN CALL CREATE TABLESPACE WITHOUT DATAFILE
--echo #                FROM PROCEDURE.
--echo #

DELIMITER |;
CREATE PROCEDURE cr(IN start BIGINT)
BEGIN
SET @idx =start;
WHILE (@idx > 0) DO
CREATE TABLESPACE x;
DROP TABLESPACE x;
SET @idx = @idx - 1;
END WHILE;
END |
DELIMITER ;|
CALL cr(3);
DROP PROCEDURE cr;

--echo #
--echo # perform table join with two different tablespaces
--echo # alter tablespace and then perform table join
--echo #

CREATE TABLESPACE ts1 Engine=InnoDB;
CREATE TABLESPACE ts2 ADD DATAFILE 'ts2.ibd' Engine=InnoDB;
CREATE TABLE t1(c1 INT, c2 CHAR(1)) ENGINE=InnoDB TABLESPACE=ts1;
INSERT INTO t1 VALUES(1,'a');
CREATE TABLE t2(c1 INT, c2 CHAR(1)) ENGINE=InnoDB TABLESPACE=ts2;
INSERT INTO t2 VALUES(1,'b');
SELECT * FROM t1 JOIN t2 WHERE t1.c1 = t2.c1;
ALTER TABLESPACE ts2 RENAME TO ts3;
SELECT * FROM t1 JOIN t2 WHERE t1.c1 = t2.c1;
DROP TABLE t1;
DROP TABLE t2;
DROP TABLESPACE ts1;
DROP TABLESPACE ts3;


--echo #
--echo # tablespace creation fails if user does'nt have CREATE TABLESPACE privilege
--echo #
CREATE USER user1@localhost;
connect (con1, localhost, user1,,);
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
CREATE TABLESPACE ts2 Engine=InnoDB;

connection default;
disconnect con1;
GRANT CREATE TABLESPACE on *.* TO user1@localhost;

connect (con1, localhost, user1,,);
CREATE TABLESPACE ts2 Engine=InnoDB;
CREATE TABLE t2(c INT) ENGINE=InnoDB TABLESPACE=ts2;
DROP TABLE t2;
DROP TABLESPACE ts2;

connection default;
disconnect con1;
DROP USER user1@localhost;

--echo #
--echo # Test case for:
--echo # Bug#29756808 assertion failure: trx0trx.*trx_state_eq(((trx)), trx_state_not_started).*
--echo # Bug#29368581 server crash assertion failure: buf0buf.cc after executing check table
--echo # Bug#29601004 assertion failure: buf0buf.cc.*:found while doing check table extended
--echo # Bug#29600913 assertion failure: fsp0fsp.cc:.*((space->flags & ((~(~0u << 1))<< (((((((0 + 1.*
--echo #

--echo #
--echo # Check that the kill query request is ignored when acquiring
--echo # locks on tables in tablespace, when ALTER TABLESPACE changes
--echo # encryption type.
--echo #

--echo # Create table t1 in tablespace and hold read lock on it.
CREATE TABLESPACE ts ADD DATAFILE 'f.ibd' ENGINE InnoDB;
CREATE TABLE t1 (f1 int) tablespace=ts;
LOCK TABLE t1 read;

connect (con1, localhost, root);
let $ID= `SELECT @id := CONNECTION_ID()`;
--echo # In another thread, wait when ALTER TABLESPACE request for lock
--echo # upgrade.
SET DEBUG_SYNC= 'upgrade_lock_for_tables_in_tablespace_kill_point SIGNAL cond2 WAIT_FOR cond3';
--send ALTER TABLESPACE ts ENCRYPTION='Y'

connection default;
let $ignore= `SELECT @id := $ID`;
--echo # Wait for ALTER to reach above condition and set KILL QUERY request.
SET DEBUG_SYNC= 'now WAIT_FOR cond2';
KILL QUERY @id;
SET DEBUG_SYNC= 'now SIGNAL cond3';
UNLOCK TABLES;

--echo # Verify that ALTER TABLESPACE does not fail with the fix.
connection con1;
--reap
disconnect con1;

--echo # Verify that server restarts after the fix.
connection default;
SET DEBUG_SYNC= "RESET";
--replace_result $PLUGIN_DIR_OPT PLUGIN_DIR_OPT
--source include/restart_mysqld.inc

DROP TABLE t1;
DROP TABLESPACE ts;

--echo #
--echo # Check that the lock_wait_timeout is ignored when acquiring
--echo # locks on tables in tablespace, when ALTER TABLESPACE changes
--echo # encryption type.
--echo #

--echo # Create table t1 in tablespace and hold read lock on it.
CREATE TABLESPACE ts ADD DATAFILE 'f.ibd' ENGINE InnoDB;
CREATE TABLE t1 (f1 INT) TABLESPACE=ts;
LOCK TABLE t1 READ, performance_schema.threads READ;

connect (con1, localhost, root);
set @@session.lock_wait_timeout=1;
let $ID= `SELECT @id := CONNECTION_ID()`;
--send ALTER TABLESPACE ts ENCRYPTION='Y'

connection default;
--sleep 1
let $wait_condition=
    SELECT COUNT(*) = 1 FROM performance_schema.threads
    WHERE PROCESSLIST_STATE = "Waiting for table metadata lock"
    AND PROCESSLIST_ID=$ID;
--source include/wait_condition.inc
UNLOCK TABLES;

--echo # Verify that ALTER TABLESPACE does not fail with the fix.
connection con1;
--reap
disconnect con1;

--echo # Verify that server restarts after the fix.
connection default;
--replace_result $PLUGIN_DIR_OPT PLUGIN_DIR_OPT
--source include/restart_mysqld.inc

DROP TABLE t1;
DROP TABLESPACE ts;

--echo #
--echo # BUG#29959193 - ERROR 1831 TABLESPACE EXISTS: EVEN IF CREATE TABLESPACE COMMAND DIDNT SUCCEED
--echo #

--query_vertical SELECT NAME FROM information_schema.INNODB_TABLESPACES WHERE NAME LIKE 'ts1'
SET SESSION DEBUG="+d, pre_commit_error";
--error ER_UNKNOWN_ERROR
CREATE TABLESPACE ts1;
SET SESSION DEBUG="-d, pre_commit_error";
CREATE TABLESPACE ts1;
--query_vertical SELECT NAME FROM information_schema.INNODB_TABLESPACES WHERE NAME LIKE 'ts1'

SET SESSION DEBUG="+d,pre_commit_error";
--error ER_UNKNOWN_ERROR
ALTER TABLESPACE ts1 RENAME TO ts11;
SET SESSION DEBUG="-d,pre_commit_error";

ALTER TABLESPACE ts1 RENAME TO ts11;
--query_vertical SELECT NAME FROM information_schema.INNODB_TABLESPACES WHERE NAME LIKE 'ts11'

DROP TABLESPACE ts11;

--echo #
--echo # WL#13341: Store options for secondary engines.
--echo # Testing engine and secondary engine attributes on tablespaces.
--echo #

CREATE TABLESPACE ts1 ENGINE_ATTRIBUTE='';
CREATE TABLESPACE ts2 ENGINE_ATTRIBUTE='{"c": "v"}';
SELECT * FROM information_schema.tablespaces_extensions WHERE tablespace_name = 'ts2';

--error ER_INVALID_JSON_ATTRIBUTE
CREATE TABLESPACE ts3 ENGINE_ATTRIBUTE='{"c": v}';

--error ER_INVALID_JSON_ATTRIBUTE
ALTER TABLESPACE ts1 ENGINE_ATTRIBUTE='{"foo": "bar}';

ALTER TABLESPACE ts1 ENGINE_ATTRIBUTE='{"foo": "bar"}';
SELECT * FROM information_schema.tablespaces_extensions WHERE tablespace_name = 'ts1';
ALTER TABLESPACE ts1 RENAME TO ts11;
SELECT * FROM information_schema.tablespaces_extensions WHERE tablespace_name = 'ts11';

DROP TABLESPACE ts2;
DROP TABLESPACE ts11;
--source suite/component_keyring_file/inc/teardown_component.inc
