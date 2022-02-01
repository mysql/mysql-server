# ###########################################################
# Prepare
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENGINE=InnoDB;
CREATE TABLE t1
(c1 VARCHAR(300), c2 TEXT)
ENGINE=InnoDB
ROW_FORMAT=REDUNDANT
TABLESPACE innodb_file_per_table;
# ###########################################################
# Inject error during bulk insert
SET debug = '+d,ib_btr_bulk_insert_inject_error';
ALTER TABLE t1 TABLESPACE ts1;
ERROR 42000: Row size too large. The maximum row size for the used table type, not counting BLOBs, is #value#. This includes storage overhead, check the manual. You have to change some columns to TEXT or BLOBs
SET debug = '-d,ib_btr_bulk_insert_inject_error';
FLUSH TABLES;
# ###########################################################
# Inject error during bulk reserve space
SET debug = '+d,ib_btr_bulk_prepare_space_error';
ALTER TABLE t1 TABLESPACE ts1;
ERROR HY000: Out of memory; check if mysqld or some other process uses all available memory; if not, you may have to use 'ulimit' to allow mysqld to use more memory or you can add more swap space
FLUSH TABLES;
SET debug = '-d,ib_btr_bulk_prepare_space_error';
# ###########################################################
# No error - check for final success
ALTER TABLE t1 TABLESPACE ts1;
FLUSH TABLES;
# ###########################################################
# Cleanup
DROP TABLE t1;
DROP TABLESPACE ts1;
# ###########################################################
# Prepare
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENGINE=InnoDB;
CREATE TABLE t1
(c1 VARCHAR(300), c2 TEXT)
ENGINE=InnoDB
ROW_FORMAT=COMPACT
TABLESPACE innodb_file_per_table;
# ###########################################################
# Inject error during bulk insert
SET debug = '+d,ib_btr_bulk_insert_inject_error';
ALTER TABLE t1 TABLESPACE ts1;
ERROR 42000: Row size too large. The maximum row size for the used table type, not counting BLOBs, is #value#. This includes storage overhead, check the manual. You have to change some columns to TEXT or BLOBs
SET debug = '-d,ib_btr_bulk_insert_inject_error';
FLUSH TABLES;
# ###########################################################
# Inject error during bulk reserve space
SET debug = '+d,ib_btr_bulk_prepare_space_error';
ALTER TABLE t1 TABLESPACE ts1;
ERROR HY000: Out of memory; check if mysqld or some other process uses all available memory; if not, you may have to use 'ulimit' to allow mysqld to use more memory or you can add more swap space
FLUSH TABLES;
SET debug = '-d,ib_btr_bulk_prepare_space_error';
# ###########################################################
# No error - check for final success
ALTER TABLE t1 TABLESPACE ts1;
FLUSH TABLES;
# ###########################################################
# Cleanup
DROP TABLE t1;
DROP TABLESPACE ts1;
# ###########################################################
# Prepare
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENGINE=InnoDB;
CREATE TABLE t1
(c1 VARCHAR(300), c2 TEXT)
ENGINE=InnoDB
ROW_FORMAT=DYNAMIC
TABLESPACE innodb_file_per_table;
# ###########################################################
# Inject error during bulk insert
SET debug = '+d,ib_btr_bulk_insert_inject_error';
ALTER TABLE t1 TABLESPACE ts1;
ERROR 42000: Row size too large. The maximum row size for the used table type, not counting BLOBs, is #value#. This includes storage overhead, check the manual. You have to change some columns to TEXT or BLOBs
SET debug = '-d,ib_btr_bulk_insert_inject_error';
FLUSH TABLES;
# ###########################################################
# Inject error during bulk reserve space
SET debug = '+d,ib_btr_bulk_prepare_space_error';
ALTER TABLE t1 TABLESPACE ts1;
ERROR HY000: Out of memory; check if mysqld or some other process uses all available memory; if not, you may have to use 'ulimit' to allow mysqld to use more memory or you can add more swap space
FLUSH TABLES;
SET debug = '-d,ib_btr_bulk_prepare_space_error';
# ###########################################################
# No error - check for final success
ALTER TABLE t1 TABLESPACE ts1;
FLUSH TABLES;
# ###########################################################
# Cleanup
DROP TABLE t1;
DROP TABLESPACE ts1;
# ###########################################################
# Prepare
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' FILE_BLOCK_SIZE=4096 ENGINE=InnoDB;
CREATE TABLE t1
(c1 VARCHAR(300), c2 TEXT)
ENGINE=InnoDB
ROW_FORMAT=COMPRESSED
KEY_BLOCK_SIZE=4
TABLESPACE innodb_file_per_table;
# ###########################################################
# Inject error during bulk insert
SET debug = '+d,ib_btr_bulk_insert_inject_error';
ALTER TABLE t1 TABLESPACE ts1;
ERROR 42000: Row size too large. The maximum row size for the used table type, not counting BLOBs, is #value#. This includes storage overhead, check the manual. You have to change some columns to TEXT or BLOBs
SET debug = '-d,ib_btr_bulk_insert_inject_error';
FLUSH TABLES;
# ###########################################################
# Inject error during bulk reserve space
SET debug = '+d,ib_btr_bulk_prepare_space_error';
ALTER TABLE t1 TABLESPACE ts1;
ERROR HY000: Out of memory; check if mysqld or some other process uses all available memory; if not, you may have to use 'ulimit' to allow mysqld to use more memory or you can add more swap space
FLUSH TABLES;
SET debug = '-d,ib_btr_bulk_prepare_space_error';
# ###########################################################
# No error - check for final success
ALTER TABLE t1 TABLESPACE ts1;
FLUSH TABLES;
# ###########################################################
# Cleanup
DROP TABLE t1;
DROP TABLESPACE ts1;
