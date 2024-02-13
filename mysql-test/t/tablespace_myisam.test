--source include/force_myisam_default.inc
--source include/have_myisam.inc
--source include/have_debug.inc
--source include/have_component_keyring_file.inc
--source suite/component_keyring_file/inc/setup_component.inc

#
# Check that the table options for TABLESPACE and STORAGE
# are printed in SHOW CREATE TABLE
#

# TABLESPACE only
CREATE TABLE t1(a int) TABLESPACE ts ENGINE=MyISAM;
SHOW CREATE TABLE t1;
DROP TABLE t1;

# TABLESPACE + STORAGE DISK
CREATE TABLE t1(a int) TABLESPACE ts STORAGE DISK ENGINE=MyISAM;
SHOW CREATE TABLE t1;
DROP TABLE t1;

# TABLESPACE + STORAGE MEMORY
CREATE TABLE t1(a int) TABLESPACE ts STORAGE MEMORY ENGINE=MyISAM;
SHOW CREATE TABLE t1;
DROP TABLE t1;

# STORAGE MEMORY only
CREATE TABLE t1(a int) STORAGE MEMORY ENGINE=MyISAM;
SHOW CREATE TABLE t1;
DROP TABLE t1;

# STORAGE DISK only
CREATE TABLE t1(a int) STORAGE DISK ENGINE=MyISAM;
SHOW CREATE TABLE t1;
DROP TABLE t1;

#
# Check that the table options for TABLESPACE and STORAGE
# are kept in an ALTER
#

# TABLESPACE only
CREATE TABLE t1(a int) TABLESPACE ts ENGINE=MyISAM;
ALTER TABLE t1 ADD COLUMN b int;
SHOW CREATE TABLE t1;
DROP TABLE t1;

# TABLESPACE and STORAGE DISK
CREATE TABLE t1(a int) TABLESPACE ts STORAGE DISK ENGINE=MyISAM;
ALTER TABLE t1 ADD COLUMN b int;
SHOW CREATE TABLE t1;
DROP TABLE t1;

#
# Check that the table options for TABLESPACE and STORAGE
# can be changed with an ALTER
#

# TABLESPACE only
CREATE TABLE t1(a int) ENGINE=MyISAM;

ALTER TABLE t1 TABLESPACE ts;
SHOW CREATE TABLE t1;

ALTER TABLE t1 TABLESPACE ts2;
SHOW CREATE TABLE t1;

DROP TABLE t1;

# STORAGE only
CREATE TABLE t1(a int) ENGINE=MyISAM;

ALTER TABLE t1 STORAGE MEMORY;
SHOW CREATE TABLE t1;

ALTER TABLE t1 STORAGE DISK;
SHOW CREATE TABLE t1;

DROP TABLE t1;

# TABLESPACE and STORAGE
CREATE TABLE t1(a int) ENGINE=MyISAM;

ALTER TABLE t1 STORAGE MEMORY TABLESPACE ts;
SHOW CREATE TABLE t1;

ALTER TABLE t1 STORAGE DISK TABLESPACE ts2;
SHOW CREATE TABLE t1;

DROP TABLE t1;

--echo # 2. Non partitioned table DDL.
--echo # 2.1 Create table.
CREATE TABLE t1 (i INTEGER) TABLESPACE innodb_file_per_table ENGINE InnoDB;
CREATE TABLE t2 (i INTEGER) TABLESPACE innodb_system ENGINE InnoDB;

--echo # 2.2 Alter table.

--error ER_WRONG_TABLESPACE_NAME
ALTER TABLE t2 TABLESPACE `innodb_file_per_table.2`;
--echo # This is valid since MyISAM does not care:
ALTER TABLE t2 TABLESPACE `innodb_file_per_table.2` ENGINE MyISAM;
SHOW CREATE TABLE t2;

--echo # Table t1 is carried over to MyISAM using the dummy 'innodb_file_per_table':
ALTER TABLE t1 ENGINE MyISAM;
SHOW CREATE TABLE t1;

--echo # Changing only engine back to InnoDB now will be rejected for t2:
--error ER_WRONG_TABLESPACE_NAME
ALTER TABLE t2 ENGINE InnoDB;
SHOW CREATE TABLE t2;

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
ALTER TABLE t1 ENGINE MyISAM;
SHOW CREATE TABLE t1;
ALTER TABLE t1 ENGINE InnoDB;
SHOW CREATE TABLE t1;

DROP TABLE t1;
DROP TABLE t2;
DROP TABLESPACE ts;

--echo # 1. Verify that ENGINE attribute is not needed for ALTER and DROP
--echo # TABLESPACE
CREATE TABLESPACE ts1 ADD DATAFILE 'df1.ibd' ENGINE=InnoDB;

--echo # No need to add ENGINE - looked up in DD
--error ER_ALTER_FILEGROUP_FAILED
ALTER TABLESPACE ts1 ADD DATAFILE 'df2.ibd';
SHOW WARNINGS;

--echo # Specifying correct ENGINE is allowed, but triggers deprecation
--echo # warning
--error ER_ALTER_FILEGROUP_FAILED
ALTER TABLESPACE ts1 ADD DATAFILE 'df2.ibd' ENGINE=INNODB;
SHOW WARNINGS;

--echo # Specifying a different ENGINE than the one stored in the DD is an
--echo # error
--error ER_TABLESPACE_ENGINE_MISMATCH
ALTER TABLESPACE ts1 ADD DATAFILE 'df2.ibd' ENGINE=MYISAM;

DROP TABLESPACE ts1;
--source suite/component_keyring_file/inc/teardown_component.inc
