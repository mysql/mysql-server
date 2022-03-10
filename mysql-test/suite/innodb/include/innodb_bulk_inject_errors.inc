--echo # ###########################################################
--echo # Prepare

if ($row_format != 'COMPRESSED')
{
  # Create general tablespace
  CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENGINE=InnoDB;

  # Create table in file-per-table tablespace.
  eval CREATE TABLE t1
    (c1 VARCHAR(300), c2 TEXT)
    ENGINE=InnoDB
    ROW_FORMAT=$row_format
    TABLESPACE innodb_file_per_table;
}

if ($row_format == 'COMPRESSED')
{
  # Create general tablespace
  CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' FILE_BLOCK_SIZE=4096 ENGINE=InnoDB;

  # Create table in file-per-table tablespace.
  eval CREATE TABLE t1
    (c1 VARCHAR(300), c2 TEXT)
    ENGINE=InnoDB
    ROW_FORMAT=$row_format
    KEY_BLOCK_SIZE=4
    TABLESPACE innodb_file_per_table;
}

# Insert some values (make sure more than one page would be used for records)
--disable_query_log
BEGIN;
LET $insert_idx = 1;
LET $insert_cnt = 100; # 30000 (total size) / 300 (rec size)
WHILE ($insert_idx <= $insert_cnt)
{
    INSERT INTO t1 VALUES (REPEAT('1', 300), REPEAT('a', 16000));
    INC $insert_idx;
}
COMMIT;
--enable_query_log

--echo # ###########################################################
--echo # Inject error during bulk insert

SET debug = '+d,ddl_btree_build_too_big_record';

# Move table to general tablespace
--replace_regex /not counting BLOBs, is [0-9]+/not counting BLOBs, is #value#/
--error ER_TOO_BIG_ROWSIZE
ALTER TABLE t1 TABLESPACE ts1;

SET debug = '-d,ddl_btree_build_too_big_record';

FLUSH TABLES;

--echo # ###########################################################
--echo # Inject error during bulk reserve space

SET debug = '+d,ddl_btree_build_oom';

# Move table to general tablespace
--error ER_OUT_OF_RESOURCES
ALTER TABLE t1 TABLESPACE ts1;

FLUSH TABLES;

SET debug = '-d,ddl_btree_build_oom';

--echo # ###########################################################
--echo # No error - check for final success

# Move table to general tablespace
ALTER TABLE t1 TABLESPACE ts1;

FLUSH TABLES;

--echo # ###########################################################
--echo # Cleanup

DROP TABLE t1;
DROP TABLESPACE ts1;
