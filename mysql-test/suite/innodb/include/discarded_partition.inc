# Tool for testing operations on discarded partitions. Caller need to set
# $discarded_partition_option to one of:
# 1. PREPARE - creates 6 tables with different partition layouts and
#    DISCARD/IMPORT TABLESPACE history
# 2. SELECT_TEST - performs SELECT queries on created tables
# 3. REBUILD_PARTITION_TEST - performs ALTER TABLE ... REBUILD PARTITION
# 4. SECONDARY_INDEX_TEST - creates and then drops secondary index
# 5. DROP_TEST - performs drop test on created tables and does cleanup. TODO describe more
# 6. PREPARE_FROM_8020 - similar to PREPARE, but loads data from prepared
#    8.0.20 version of MySQL. Used for upgrade test
# 7. CLEANUP_FROM_8020 - must be called after all tests are done, when prepared from 8.0
#
#  Feel free to add new tests

if($discarded_partition_option == 'PREPARE')
{
  let $MYSQLD_DATADIR = `SELECT @@datadir`;

  CREATE DATABASE partitions;

  CREATE TABLE partitions.export1(c1 INT,c2 INT) PARTITION BY KEY(c1) PARTITIONS 4;
  FLUSH TABLE partitions.export1 FOR EXPORT;
  --copy_file $MYSQLD_DATADIR/partitions/export1#p#p0.cfg $MYSQLD_DATADIR/export1#p#p0.cfg_bak
  --copy_file $MYSQLD_DATADIR/partitions/export1#p#p0.ibd $MYSQLD_DATADIR/export1#p#p0.ibd_bak

  --copy_file $MYSQLD_DATADIR/partitions/export1#p#p1.cfg $MYSQLD_DATADIR/export1#p#p1.cfg_bak
  --copy_file $MYSQLD_DATADIR/partitions/export1#p#p1.ibd $MYSQLD_DATADIR/export1#p#p1.ibd_bak
  UNLOCK TABLES;

  #
  # partitions.t1 is table with just single partition discarded
  #
  CREATE TABLE partitions.t1(c1 INT,c2 INT) PARTITION BY KEY(c1) PARTITIONS 4;
  ALTER TABLE partitions.t1 DISCARD PARTITION p0 TABLESPACE;

  #
  # partitions.t2 is a table, with two partitions discarded and then, both imported
  #
  CREATE TABLE partitions.t2(c1 INT,c2 INT) PARTITION BY KEY(c1) PARTITIONS 4;
  ALTER TABLE partitions.t2 DISCARD PARTITION p0, p1 TABLESPACE;
  --copy_file $MYSQLD_DATADIR/export1#p#p0.cfg_bak $MYSQLD_DATADIR/partitions/t2#p#p0.cfg
  --copy_file $MYSQLD_DATADIR/export1#p#p0.ibd_bak $MYSQLD_DATADIR/partitions/t2#p#p0.ibd

  --copy_file $MYSQLD_DATADIR/export1#p#p1.cfg_bak $MYSQLD_DATADIR/partitions/t2#p#p1.cfg
  --copy_file $MYSQLD_DATADIR/export1#p#p1.ibd_bak $MYSQLD_DATADIR/partitions/t2#p#p1.ibd
  ALTER TABLE partitions.t2 IMPORT PARTITION p0 TABLESPACE;
  ALTER TABLE partitions.t2 IMPORT PARTITION p1 TABLESPACE;

  #
  # partitions.t3 is a table, with two partitions discarded and single partition imported
  #
  CREATE TABLE partitions.t3(c1 INT,c2 INT) PARTITION BY KEY(c1) PARTITIONS 4;
  ALTER TABLE partitions.t3 DISCARD PARTITION p0, p1 TABLESPACE;
  --copy_file $MYSQLD_DATADIR/export1#p#p1.cfg_bak $MYSQLD_DATADIR/partitions/t3#p#p1.cfg
  --copy_file $MYSQLD_DATADIR/export1#p#p1.ibd_bak $MYSQLD_DATADIR/partitions/t3#p#p1.ibd
  ALTER TABLE partitions.t3 IMPORT PARTITION p1 TABLESPACE;


  #
  # Table used to export partitions which are used for imports later in the test
  #
  CREATE TABLE partitions.export2 (c1 INT, c2 INT)
    PARTITION BY RANGE( c2)
    SUBPARTITION BY HASH( c2 )
    SUBPARTITIONS 2 (
        PARTITION p0 VALUES LESS THAN (10),
        PARTITION p1 VALUES LESS THAN (20),
        PARTITION p2 VALUES LESS THAN (30),
        PARTITION p3 VALUES LESS THAN MAXVALUE
  );

  FLUSH TABLE partitions.export2 FOR EXPORT;
  --copy_file $MYSQLD_DATADIR/partitions/export2#p#p0#sp#p0sp0.cfg $MYSQLD_DATADIR/export2#p#p0#sp#p0sp0.cfg_bak
  --copy_file $MYSQLD_DATADIR/partitions/export2#p#p0#sp#p0sp0.ibd $MYSQLD_DATADIR/export2#p#p0#sp#p0sp0.ibd_bak

  --copy_file $MYSQLD_DATADIR/partitions/export2#p#p0#sp#p0sp1.cfg $MYSQLD_DATADIR/export2#p#p0#sp#p0sp1.cfg_bak
  --copy_file $MYSQLD_DATADIR/partitions/export2#p#p0#sp#p0sp1.ibd $MYSQLD_DATADIR/export2#p#p0#sp#p0sp1.ibd_bak

  --copy_file $MYSQLD_DATADIR/partitions/export2#p#p1#sp#p1sp0.cfg $MYSQLD_DATADIR/export2#p#p1#sp#p1sp0.cfg_bak
  --copy_file $MYSQLD_DATADIR/partitions/export2#p#p1#sp#p1sp0.ibd $MYSQLD_DATADIR/export2#p#p1#sp#p1sp0.ibd_bak

  --copy_file $MYSQLD_DATADIR/partitions/export2#p#p2#sp#p2sp0.cfg $MYSQLD_DATADIR/export2#p#p2#sp#p2sp0.cfg_bak
  --copy_file $MYSQLD_DATADIR/partitions/export2#p#p2#sp#p2sp0.ibd $MYSQLD_DATADIR/export2#p#p2#sp#p2sp0.ibd_bak

  UNLOCK TABLES;

  #
  # partitions.t4 is a table with:
  # 1. Partition p0, with two discarded subpartitions
  # 2. Partition p1, with single discarded subpartition
  # 3. Partition p2, with two subpartitions
  # 4. Partition p3, with two subpartitions
  #

  CREATE TABLE partitions.t4 (c1 INT, c2 INT)
    PARTITION BY RANGE( c2 )
    SUBPARTITION BY HASH( c2 )
    SUBPARTITIONS 2 (
        PARTITION p0 VALUES LESS THAN (10),
        PARTITION p1 VALUES LESS THAN (20),
        PARTITION p2 VALUES LESS THAN (30),
        PARTITION p3 VALUES LESS THAN MAXVALUE
    );

  ALTER TABLE partitions.t4 DISCARD PARTITION p0sp0 TABLESPACE;
  ALTER TABLE partitions.t4 DISCARD PARTITION p0sp1 TABLESPACE;
  ALTER TABLE partitions.t4 DISCARD PARTITION p1sp0 TABLESPACE;


  #
  # partitions.t5 is a table with:
  # 1. Partition p0, with two discarded subpartitions, two of which are later imported
  # 2. Partition p1, with two discarded subpartitions, one of which is later imported
  # 3. Partition p2, with single discarded subpartition which later is imported
  # 4. Partition p3, with no discarded subpartitions
  #
  CREATE TABLE partitions.t5 (c1 INT, c2 INT)
    PARTITION BY RANGE( c2 )
    SUBPARTITION BY HASH( c2 )
    SUBPARTITIONS 2 (
        PARTITION p0 VALUES LESS THAN (10),
        PARTITION p1 VALUES LESS THAN (20),
        PARTITION p2 VALUES LESS THAN (30),
        PARTITION p3 VALUES LESS THAN MAXVALUE
    );

  ALTER TABLE partitions.t5 DISCARD PARTITION p0sp0 TABLESPACE;
  ALTER TABLE partitions.t5 DISCARD PARTITION p0sp1 TABLESPACE;
  ALTER TABLE partitions.t5 DISCARD PARTITION p1sp0 TABLESPACE;
  ALTER TABLE partitions.t5 DISCARD PARTITION p1sp1 TABLESPACE;
  ALTER TABLE partitions.t5 DISCARD PARTITION p2sp0 TABLESPACE;

  --copy_file $MYSQLD_DATADIR/export2#p#p0#sp#p0sp0.cfg_bak $MYSQLD_DATADIR/partitions/t5#p#p0#sp#p0sp0.cfg
  --copy_file $MYSQLD_DATADIR/export2#p#p0#sp#p0sp0.ibd_bak $MYSQLD_DATADIR/partitions/t5#p#p0#sp#p0sp0.ibd

  --copy_file $MYSQLD_DATADIR/export2#p#p0#sp#p0sp1.cfg_bak $MYSQLD_DATADIR/partitions/t5#p#p0#sp#p0sp1.cfg
  --copy_file $MYSQLD_DATADIR/export2#p#p0#sp#p0sp1.ibd_bak $MYSQLD_DATADIR/partitions/t5#p#p0#sp#p0sp1.ibd

  --copy_file $MYSQLD_DATADIR/export2#p#p1#sp#p1sp0.cfg_bak $MYSQLD_DATADIR/partitions/t5#p#p1#sp#p1sp0.cfg
  --copy_file $MYSQLD_DATADIR/export2#p#p1#sp#p1sp0.ibd_bak $MYSQLD_DATADIR/partitions/t5#p#p1#sp#p1sp0.ibd

  --copy_file $MYSQLD_DATADIR/export2#p#p2#sp#p2sp0.cfg_bak $MYSQLD_DATADIR/partitions/t5#p#p2#sp#p2sp0.cfg
  --copy_file $MYSQLD_DATADIR/export2#p#p2#sp#p2sp0.ibd_bak $MYSQLD_DATADIR/partitions/t5#p#p2#sp#p2sp0.ibd

  ALTER TABLE partitions.t5 IMPORT PARTITION p0sp0 TABLESPACE;
  ALTER TABLE partitions.t5 IMPORT PARTITION p0sp1 TABLESPACE;
  ALTER TABLE partitions.t5 IMPORT PARTITION p1sp0 TABLESPACE;
  ALTER TABLE partitions.t5 IMPORT PARTITION p2sp0 TABLESPACE;

  #
  # partitions.t6 is a table with:
  # 1. Partition p0 which is discarded with a single query
  # 2. Partition p1 which has single subpartition discarded
  # 3. Partitions p2 and p3 with no discarded subpartitions
  #
  CREATE TABLE partitions.t6 (c1 INT, c2 INT)
    PARTITION BY RANGE( c2 )
    SUBPARTITION BY HASH( c2 )
    SUBPARTITIONS 2 (
        PARTITION p0 VALUES LESS THAN (10),
        PARTITION p1 VALUES LESS THAN (20),
        PARTITION p2 VALUES LESS THAN (30),
        PARTITION p3 VALUES LESS THAN MAXVALUE
    );

  ALTER TABLE partitions.t6 DISCARD PARTITION p0 TABLESPACE;
  ALTER TABLE partitions.t6 DISCARD PARTITION p1sp1 TABLESPACE;

  #
  # partitions.t7 is a table with:
  # 1. Partition p0 which is discarded with a single query, which are later imported
  # 2. Partition p1 which has single subpartition discarded which is later imported
  # 3. Partitions p2 and p3 with no discarded subpartitions
  #
  CREATE TABLE partitions.t7 (c1 INT, c2 INT)
    PARTITION BY RANGE( c2 )
    SUBPARTITION BY HASH( c2 )
    SUBPARTITIONS 2 (
        PARTITION p0 VALUES LESS THAN (10),
        PARTITION p1 VALUES LESS THAN (20),
        PARTITION p2 VALUES LESS THAN (30),
        PARTITION p3 VALUES LESS THAN MAXVALUE
    );

  ALTER TABLE partitions.t7 DISCARD PARTITION p0 TABLESPACE;
  ALTER TABLE partitions.t7 DISCARD PARTITION p1sp0 TABLESPACE;

  --copy_file $MYSQLD_DATADIR/export2#p#p0#sp#p0sp0.cfg_bak $MYSQLD_DATADIR/partitions/t7#p#p0#sp#p0sp0.cfg
  --copy_file $MYSQLD_DATADIR/export2#p#p0#sp#p0sp0.ibd_bak $MYSQLD_DATADIR/partitions/t7#p#p0#sp#p0sp0.ibd

  --copy_file $MYSQLD_DATADIR/export2#p#p0#sp#p0sp1.cfg_bak $MYSQLD_DATADIR/partitions/t7#p#p0#sp#p0sp1.cfg
  --copy_file $MYSQLD_DATADIR/export2#p#p0#sp#p0sp1.ibd_bak $MYSQLD_DATADIR/partitions/t7#p#p0#sp#p0sp1.ibd

  --copy_file $MYSQLD_DATADIR/export2#p#p1#sp#p1sp0.cfg_bak $MYSQLD_DATADIR/partitions/t7#p#p1#sp#p1sp0.cfg
  --copy_file $MYSQLD_DATADIR/export2#p#p1#sp#p1sp0.ibd_bak $MYSQLD_DATADIR/partitions/t7#p#p1#sp#p1sp0.ibd

  ALTER TABLE partitions.t7 IMPORT PARTITION p0sp0 TABLESPACE;
  ALTER TABLE partitions.t7 IMPORT PARTITION p0sp1 TABLESPACE;
  ALTER TABLE partitions.t7 IMPORT PARTITION p1sp0 TABLESPACE;



  --remove_file $MYSQLD_DATADIR/export1#p#p0.cfg_bak
  --remove_file $MYSQLD_DATADIR/export1#p#p0.ibd_bak

  --remove_file $MYSQLD_DATADIR/export1#p#p1.cfg_bak
  --remove_file $MYSQLD_DATADIR/export1#p#p1.ibd_bak

  --remove_file $MYSQLD_DATADIR/export2#p#p0#sp#p0sp0.cfg_bak
  --remove_file $MYSQLD_DATADIR/export2#p#p0#sp#p0sp0.ibd_bak

  --remove_file $MYSQLD_DATADIR/export2#p#p0#sp#p0sp1.cfg_bak
  --remove_file $MYSQLD_DATADIR/export2#p#p0#sp#p0sp1.ibd_bak

  --remove_file $MYSQLD_DATADIR/export2#p#p1#sp#p1sp0.cfg_bak
  --remove_file $MYSQLD_DATADIR/export2#p#p1#sp#p1sp0.ibd_bak

  --remove_file $MYSQLD_DATADIR/export2#p#p2#sp#p2sp0.cfg_bak
  --remove_file $MYSQLD_DATADIR/export2#p#p2#sp#p2sp0.ibd_bak

}

if($discarded_partition_option == 'SELECT_TEST')
{

  #
  # partitions.t1
  #

  --error ER_TABLESPACE_DISCARDED
  SELECT * FROM partitions.t1;

  --error ER_TABLESPACE_DISCARDED
  SELECT * FROM partitions.t1 PARTITION (p0);
  SELECT * FROM partitions.t1 PARTITION (p1);
  SELECT * FROM partitions.t1 PARTITION (p2);
  SELECT * FROM partitions.t1 PARTITION (p3);

  #
  # partitions.t2
  #

  SELECT * FROM partitions.t2;

  SELECT * FROM partitions.t2 PARTITION (p0);
  SELECT * FROM partitions.t2 PARTITION (p1);
  SELECT * FROM partitions.t2 PARTITION (p2);
  SELECT * FROM partitions.t2 PARTITION (p3);

  #
  # partitions.t3
  #

  --error ER_TABLESPACE_DISCARDED
  SELECT * FROM partitions.t3;

  --error ER_TABLESPACE_DISCARDED
  SELECT * FROM partitions.t3 PARTITION (p0);
  SELECT * FROM partitions.t3 PARTITION (p1);
  SELECT * FROM partitions.t3 PARTITION (p2);
  SELECT * FROM partitions.t3 PARTITION (p3);

  #
  # partitions.t4
  #

  --error ER_TABLESPACE_DISCARDED
  SELECT * FROM partitions.t4;

  --error ER_TABLESPACE_DISCARDED
  SELECT * FROM partitions.t4 PARTITION (p0);
  --error ER_TABLESPACE_DISCARDED
  SELECT * FROM partitions.t4 PARTITION (p0sp0);
  --error ER_TABLESPACE_DISCARDED
  SELECT * FROM partitions.t4 PARTITION (p0sp1);

  --error ER_TABLESPACE_DISCARDED
  SELECT * FROM partitions.t4 PARTITION (p1);
  --error ER_TABLESPACE_DISCARDED
  SELECT * FROM partitions.t4 PARTITION (p1sp0);
  SELECT * FROM partitions.t4 PARTITION (p1sp1);

  SELECT * FROM partitions.t4 PARTITION (p2);
  SELECT * FROM partitions.t4 PARTITION (p2sp0);
  SELECT * FROM partitions.t4 PARTITION (p2sp1);

  #
  # partitions.t5
  #

  --error ER_TABLESPACE_DISCARDED
  SELECT * FROM partitions.t5;

  SELECT * FROM partitions.t5 PARTITION (p0);
  SELECT * FROM partitions.t5 PARTITION (p0sp0);
  SELECT * FROM partitions.t5 PARTITION (p0sp1);

  --error ER_TABLESPACE_DISCARDED
  SELECT * FROM partitions.t5 PARTITION (p1);
  SELECT * FROM partitions.t5 PARTITION (p1sp0);
  --error ER_TABLESPACE_DISCARDED
  SELECT * FROM partitions.t5 PARTITION (p1sp1);

  SELECT * FROM partitions.t5 PARTITION (p2);
  SELECT * FROM partitions.t5 PARTITION (p2sp0);
  SELECT * FROM partitions.t5 PARTITION (p2sp1);

  #
  # partitions.t6
  #

  --error ER_TABLESPACE_DISCARDED
  SELECT * FROM partitions.t6;

  --error ER_TABLESPACE_DISCARDED
  SELECT * FROM partitions.t6 PARTITION (p0);
  --error ER_TABLESPACE_DISCARDED
  SELECT * FROM partitions.t6 PARTITION (p0sp0);
  --error ER_TABLESPACE_DISCARDED
  SELECT * FROM partitions.t6 PARTITION (p0sp1);

  --error ER_TABLESPACE_DISCARDED
  SELECT * FROM partitions.t6 PARTITION (p1);
  SELECT * FROM partitions.t6 PARTITION (p1sp0);
  --error ER_TABLESPACE_DISCARDED
  SELECT * FROM partitions.t6 PARTITION (p1sp1);

  SELECT * FROM partitions.t6 PARTITION (p2);
  SELECT * FROM partitions.t6 PARTITION (p2sp0);
  SELECT * FROM partitions.t6 PARTITION (p2sp1);

  #
  # partitions.t7
  #

  SELECT * FROM partitions.t7;

  SELECT * FROM partitions.t7 PARTITION (p0);
  SELECT * FROM partitions.t7 PARTITION (p0sp0);
  SELECT * FROM partitions.t7 PARTITION (p0sp1);

  SELECT * FROM partitions.t7 PARTITION (p1);
  SELECT * FROM partitions.t7 PARTITION (p1sp0);
  SELECT * FROM partitions.t7 PARTITION (p1sp1);

  SELECT * FROM partitions.t7 PARTITION (p2);
  SELECT * FROM partitions.t7 PARTITION (p2sp0);
  SELECT * FROM partitions.t7 PARTITION (p2sp1);
}

if($discarded_partition_option == 'REBUILD_PARTITION_TEST')
{
  # Currently we turned off rebuilding partitions which are discarded, because
  # it results in some locks left (if we quit mysqld without droping Tables)
  #
  #  Once that's fixed, uncomment rebuilding of discarded partitions

  #
  #  partitions.t1
  #

  --error ER_TABLESPACE_DISCARDED
  ALTER TABLE partitions.t1 REBUILD PARTITION p0;
  ALTER TABLE partitions.t1 REBUILD PARTITION p1;
  ALTER TABLE partitions.t1 REBUILD PARTITION p2;
  ALTER TABLE partitions.t1 REBUILD PARTITION p3;

  #
  #  partitions.t2
  #

  ALTER TABLE partitions.t2 REBUILD PARTITION p0;
  ALTER TABLE partitions.t2 REBUILD PARTITION p1;
  ALTER TABLE partitions.t2 REBUILD PARTITION p2;
  ALTER TABLE partitions.t2 REBUILD PARTITION p3;

  #
  #  partitions.t3
  #

  --error ER_TABLESPACE_DISCARDED
  ALTER TABLE partitions.t3 REBUILD PARTITION p0;
  ALTER TABLE partitions.t3 REBUILD PARTITION p1;
  ALTER TABLE partitions.t3 REBUILD PARTITION p2;
  ALTER TABLE partitions.t3 REBUILD PARTITION p3;

  #
  #  partitions.t4
  #

  --error ER_TABLESPACE_DISCARDED
  ALTER TABLE partitions.t4 REBUILD PARTITION p0;
  --error ER_TABLESPACE_DISCARDED
  ALTER TABLE partitions.t4 REBUILD PARTITION p1;
  ALTER TABLE partitions.t4 REBUILD PARTITION p2;


  #
  #  partitions.t5
  #

  ALTER TABLE partitions.t5 REBUILD PARTITION p0;
  --error ER_TABLESPACE_DISCARDED
  ALTER TABLE partitions.t5 REBUILD PARTITION p1;
  ALTER TABLE partitions.t5 REBUILD PARTITION p2;


  #
  #  partitions.t6
  #

  --error ER_TABLESPACE_DISCARDED
  ALTER TABLE partitions.t6 REBUILD PARTITION p0;
  --error ER_TABLESPACE_DISCARDED
  ALTER TABLE partitions.t6 REBUILD PARTITION p1;
  ALTER TABLE partitions.t6 REBUILD PARTITION p2;


  #
  #  partitions.t7
  #

  ALTER TABLE partitions.t7 REBUILD PARTITION p0;
  ALTER TABLE partitions.t7 REBUILD PARTITION p1;
  ALTER TABLE partitions.t7 REBUILD PARTITION p2;
}

if($discarded_partition_option == 'SECONDARY_INDEX_TEST')
{
  CREATE INDEX secondary ON partitions.t1 (c2);
  CREATE INDEX secondary ON partitions.t2 (c2);
  CREATE INDEX secondary ON partitions.t3 (c2);
  CREATE INDEX secondary ON partitions.t4 (c2);
  CREATE INDEX secondary ON partitions.t5 (c2);
  CREATE INDEX secondary ON partitions.t6 (c2);
  CREATE INDEX secondary ON partitions.t7 (c2);

  DROP INDEX secondary ON partitions.t1;
  DROP INDEX secondary ON partitions.t2;
  DROP INDEX secondary ON partitions.t3;
  DROP INDEX secondary ON partitions.t4;
  DROP INDEX secondary ON partitions.t5;
  DROP INDEX secondary ON partitions.t6;
  DROP INDEX secondary ON partitions.t7;
}

if($discarded_partition_option == 'DROP_TEST')
{
  # Dropping table with discarded partition should succeed and clean up all files for given Tables
  # TODO describe more
  DROP TABLE partitions.t1;
  DROP TABLE partitions.t2;
  DROP TABLE partitions.t3;
  DROP TABLE partitions.t4;
  DROP TABLE partitions.t5;
  DROP TABLE partitions.t6;
  DROP TABLE partitions.t7;

  DROP DATABASE partitions;
}

if($discarded_partition_option == 'PREPARE_FROM_8020')
{
  --source include/shutdown_mysqld.inc

  --copy_file $MYSQLTEST_VARDIR/std_data/upgrade/discarded_partition_data_8020.zip $MYSQL_TMP_DIR/discarded_partition_data_8020.zip
  --file_exists $MYSQL_TMP_DIR/discarded_partition_data_8020.zip
  --exec unzip -qo $MYSQL_TMP_DIR/discarded_partition_data_8020.zip -d $MYSQL_TMP_DIR

  let $BUGDATADIR = $MYSQL_TMP_DIR/data_upgrade;

  --replace_result $BUGDATADIR BUGDATADIR
  --let $restart_parameters = restart: --datadir=$BUGDATADIR
  --let $WAIT_COUNTER=3000 #TODO why?

  --source include/start_mysqld.inc
}

if($discarded_partition_option == 'CLEANUP_FROM_8020')
{
  --source include/shutdown_mysqld.inc

  --file_exists $MYSQL_TMP_DIR/discarded_partition_data_8020.zip
  --force-rmdir $MYSQL_TMP_DIR/data_upgrade
  --remove_file $MYSQL_TMP_DIR/discarded_partition_data_8020.zip

  let $restart_parameters =;
  --source include/start_mysqld.inc
}
