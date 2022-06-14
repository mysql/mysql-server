--echo #
--echo # WL#12261 Control (enforce and disable) table encryption
--echo #

--echo # Check for use of ENCRYPTION clause on tables using engines that
--echo # does not support encryption.

--echo #
--echo # 1. Requesting encryption on SE that does not support encryption.
--echo #
--error ER_CHECK_NOT_IMPLEMENTED
CREATE TABLE t1 (f1 INT NOT NULL) ENGINE=HEAP ENCRYPTION='y';
--error ER_CHECK_NOT_IMPLEMENTED
CREATE TABLE t1 (f1 INT NOT NULL) ENGINE=MyISAM ENCRYPTION='y';
--error ER_CHECK_NOT_IMPLEMENTED
CREATE TABLE t1 (f1 INT NOT NULL) ENGINE=MyISAM ENCRYPTION='w';

--echo # 2.
--echo # Requesting unencrypted tables on SE that does not support
--echo # encryption. This is allowed. Verify that we do not store
--echo # ENCRYPTION clause.
--echo #
CREATE TABLE t1 (f1 INT NOT NULL) ENGINE=HEAP ENCRYPTION='N';
SHOW CREATE TABLE t1;
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
  WHERE TABLE_NAME='t1';
DROP TABLE t1;

CREATE TABLE t1 (f1 INT NOT NULL) ENGINE=HEAP ENCRYPTION='n';
SHOW CREATE TABLE t1;
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
  WHERE TABLE_NAME='t1';
DROP TABLE t1;

CREATE TABLE t1 (f1 INT NOT NULL) ENGINE=MyISAM ENCRYPTION='';
SHOW CREATE TABLE t1;
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
  WHERE TABLE_NAME='t1';
DROP TABLE t1;

--echo # 3.
--echo # Move tables to and from different storage engine.
--echo #

CREATE TABLE t1 (f1 INT NOT NULL) ENGINE=InnoDB ENCRYPTION='y';
SHOW CREATE TABLE t1;

--echo # 4.
--echo # Moving encrypted table to SE that doesn't support encryption.
--error ER_CHECK_NOT_IMPLEMENTED
ALTER TABLE t1 ENGINE=MyISAM;
--error ER_CHECK_NOT_IMPLEMENTED
ALTER TABLE t1 ENGINE=HEAP;

--echo # 5.
--echo # Moving encrypted table to SE that doesn't support encryption.
--echo # with a explicit request to decrypt the table is allowed.
ALTER TABLE t1 ENGINE=MyISAM ENCRYPTION='n';
SHOW CREATE TABLE t1;
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
  WHERE TABLE_NAME='t1';
ALTER TABLE t1 ENGINE=Heap ENCRYPTION='N';
SHOW CREATE TABLE t1;
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
  WHERE TABLE_NAME='t1';
ALTER TABLE t1 ENGINE=CSV ENCRYPTION='';
SHOW CREATE TABLE t1;
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
  WHERE TABLE_NAME='t1';

--echo # 6.
--echo # Moving unencrypted table from SE that doesn't support encryption
--echo # to SE that does support encryption is allowed.
ALTER TABLE t1 ENGINE=InnoDB ENCRYPTION='y';
SHOW CREATE TABLE t1;
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
  WHERE TABLE_NAME='t1';
DROP TABLE t1;

--echo # Same as 4/5/6 using general tablespace:
--echo #
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='y';
CREATE TABLE t1 (f1 INT NOT NULL) TABLESPACE=ts1 ENCRYPTION='y';

--echo # 4.
--echo # Moving encrypted table to SE that doesn't support encryption.
--error ER_CHECK_NOT_IMPLEMENTED
ALTER TABLE t1 ENGINE=MyISAM;
--error ER_CHECK_NOT_IMPLEMENTED
ALTER TABLE t1 ENGINE=HEAP;

--echo # 5.
--echo # Moving encrypted table to SE that doesn't support encryption.
--echo # with a explicit request to decrypt the table is allowed.
ALTER TABLE t1 ENGINE=MyISAM ENCRYPTION='n';
SHOW CREATE TABLE t1;
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
  WHERE TABLE_NAME='t1';

--echo # 6.
--echo # Moving unencrypted table from SE that doesn't support encryption
--echo # to SE that does support encryption is allowed.
ALTER TABLE t1 ENGINE=InnoDB TABLESPACE=innodb_file_per_table ENCRYPTION='n';
SHOW CREATE TABLE t1;
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
  WHERE TABLE_NAME='t1';
ALTER TABLE t1 ENCRYPTION='y';
SHOW CREATE TABLE t1;
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
  WHERE TABLE_NAME='t1';
DROP TABLE t1;
DROP TABLESPACE ts1;
