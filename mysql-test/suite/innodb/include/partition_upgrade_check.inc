# WL 13352: SCHEMA, DDL, DML, CHECK for upgrade of partitioned tables
# Caller need to set check_option: CREATE, DROP, CHECK, DDL, DML, EXPORT, IMPORT
#                set check_dd: 1, 0 DD table can be checked on debug build
#                set NO_EXT_PATH: 1, 0 No file outside data directory
#                set EXTERNAL_DIRECTORY:  Place for external files
#                set EXPORT_DIRECTORY:    Place for exported data files
#                set EXTRA_TABLES:        Include non partitioned tables

--let $TABLE_1 = table_part_1
--let $TABLE_2 = table_part_sub_1
--let $TABLE_3 = table_part_sub_2
--let $TABLE_4 = `select "`table_part_\\_special_/_sub_?_3`"`
--let $TABLE_5 = table_part_2

--let $num_tables = 5
--let $num_rows = 10

if ($EXTRA_TABLES)
{
  --let $TABLE_6 = table_part_CAP_6
  --let $TABLE_7 = table_part_NOPART_CAP_7
  --let $num_tables = 7
}

if (!$NO_EXT_PATH)
{
  --let $DIR_CLAUSE= DATA DIRECTORY = '$EXTERNAL_DIRECTORY'
}

if ($NO_EXT_PATH)
{
  --let $DIR_CLAUSE=
}

if ($check_option == 'DROP')
{
  --echo Dropping partitioned tables
  --echo ---------------------------

  --let $loop_idx = 1
  while ($loop_idx <= $num_tables)
  {
    --let $table_name = \$TABLE_$loop_idx
    --eval DROP TABLE $table_name
    --inc $loop_idx
  }

  if ($imported) {
    DROP TABLE table_part_encrypt;
  }

  if ($EXTRA_TABLES)
  {
    DROP TABLESPACE SPACE_part;
  }
}

if (`select '$check_option' = 'EXPORT' || '$check_option' = 'IMPORT'`)
{
  --replace_result $MYSQL_TMP_DIR ""
  eval CREATE TABLE table_part_encrypt(
      id int, value int, name varchar(32), data BLOB, primary key pk(id, value),
      key k1(value)) ENCRYPTION = 'Y'
      partition by range (value) subpartition by hash (id)
      ( partition Part_1 values less than (101)(
            subpartition Sub_1,
            subpartition sUb_2),
        partition pArt_2 values less than (501)(
            subpartition suB_3 $DIR_CLAUSE,
            subpartition SUB_4),
        partition PART_3 values less than MAXVALUE(
            subpartition SUB_5,
            subpartition sub_6));

  INSERT INTO table_part_encrypt SELECT * FROM table_part_sub_2;
}

if ($check_option == 'EXPORT')
{
  FLUSH TABLES table_part_encrypt, table_part_sub_2 FOR EXPORT;

  --copy_files_wildcard $MYSQLD_DATADIR/test $EXPORT_DIRECTORY table_part_encrypt*
  --copy_files_wildcard $EXTERNAL_DIRECTORY/test $EXPORT_DIRECTORY table_part_encrypt*

  --copy_files_wildcard $MYSQLD_DATADIR/test $EXPORT_DIRECTORY table_part_sub_2*
  --copy_files_wildcard $EXTERNAL_DIRECTORY/test $EXPORT_DIRECTORY table_part_sub_2*

  UNLOCK TABLES;

  DROP TABLE table_part_encrypt;
}

if ($check_option == 'IMPORT')
{
  ALTER TABLE table_part_sub_2 DISCARD PARTITION sub_3, sub_5 tablespace;
  ALTER TABLE table_part_encrypt DISCARD PARTITION sub_3, sub_5 tablespace;

  --copy_files_wildcard $EXPORT_DIRECTORY  $MYSQLD_DATADIR/test *_5.*
  --copy_files_wildcard $EXPORT_DIRECTORY $EXTERNAL_DIRECTORY/test *_3.*

  --echo
  --echo List files before import
  --echo ---------------------------
  --list_files $MYSQLD_DATADIR/test
  --echo
  --list_files $EXTERNAL_DIRECTORY/test
  --echo

  ALTER TABLE table_part_sub_2 IMPORT PARTITION sub_3, sub_5 tablespace;
  --echo
  ALTER TABLE table_part_encrypt IMPORT PARTITION sub_3, sub_5 tablespace;

  --echo
  --echo List all files after import
  --echo ---------------------------
  --list_files $MYSQLD_DATADIR/test
  --echo
  --list_files $EXTERNAL_DIRECTORY/test
}

if ($check_option == 'CREATE')
{
  --echo Creating partitioned tables
  --echo ---------------------------

  CREATE TABLE table_part_1(
      id int primary key, value int, name varchar(32), data BLOB,
      key k1(value)) partition by hash (id) partitions 5;
  --echo

  --replace_result $MYSQL_TMP_DIR ""
  eval CREATE TABLE table_part_2(
      id int primary key, value int, name varchar(32), data BLOB,
      key k1(value)) $DIR_CLAUSE
      partition by hash (id) partitions 5;
  --echo

  CREATE TABLE table_part_sub_1(
      id int, value int, name varchar(32), data BLOB, primary key pk(id, value),
      key k1(value)) partition by range (value) subpartition by hash (id)
      subpartitions 2 ( partition Part_1 values less than (101),
                        partition pArt_2 values less than (401),
                        partition part_3 values less than (701),
                        partition PART_4 values less than MAXVALUE);
  --echo

  --replace_result $MYSQL_TMP_DIR ""
  eval CREATE TABLE table_part_sub_2(
      id int, value int, name varchar(32), data BLOB, primary key pk(id, value),
      key k1(value)) partition by range (value) subpartition by hash (id)
      ( partition Part_1 values less than (101)(
            subpartition Sub_1,
            subpartition sUb_2),
        partition pArt_2 values less than (501)(
            subpartition suB_3 $DIR_CLAUSE,
            subpartition SUB_4),
        partition PART_3 values less than MAXVALUE(
            subpartition SUB_5,
            subpartition sub_6));
  --echo

  CREATE TABLE `table_part_\_special_/_sub_?_3`(
      id int, value int, name varchar(32), data BLOB, primary key pk(id, value),
      key k1(value)) partition by range (value) subpartition by hash (id)
      ( partition `Part_?_1` values less than (201)(
            subpartition `Sub_?_1`,
            subpartition `sUb_/_2`),
        partition `pArt_\_2` values less than (501)(
            subpartition `suB_\_3`,
            subpartition `SUB_?\_4`),
        partition `PART_/_3` values less than MAXVALUE(
            subpartition `SUB_?/_5`,
            subpartition `sub_\/?_6`));

  if ($EXTRA_TABLES)
  {
    CREATE TABLESPACE SPACE_part add datafile 'SPACE_part_1.ibd';

    CREATE TABLE table_part_CAP_6(
        id int primary key, value int, name varchar(32), data BLOB,
        key k1(value)) partition by hash (id) partitions 5;

    CREATE TABLE table_part_NOPART_CAP_7(
        id int primary key, value int, name varchar(32), data BLOB,
        key k1(value)) TABLESPACE SPACE_part;
  }

  --let $loop_idx = 1

  while ($loop_idx <= $num_tables)
  {
    --let $table_name = \$TABLE_$loop_idx
    --echo
    --echo Inserting $num_rows rows to $table_name
    --echo ---------------------------------------

    --let $row_idx = 0

    while ($row_idx < $num_rows)
    {
      --let $row_val = `select 100 * $row_idx + 55`
      --let $row_name = "Row - $row_idx"
      --let $row_data = REPEAT('Large Column Data - $row_idx' , 256)

      --eval insert into $table_name VALUES($row_idx, $row_val, $row_name, $row_data)
      --inc $row_idx
    }
    --inc $loop_idx
  }

  --let $part_idx = 1
  while ($part_idx <= 6)
  {
    --eval select id, value, name from $TABLE_3 partition (sub_$part_idx) order by id
    --inc $part_idx
  }
}

if ($check_option == 'CHECK')
{
  --echo Checking partitioned tables
  --echo ---------------------------
  # Switch to case sensitive collation for consistent order.
  SET NAMES utf8mb4 COLLATE utf8mb4_0900_as_cs;

  --let $loop_idx = 1
  --let $max_index = $num_tables

  # After import include also the encrypted table.
  if ($imported) {
    --inc $max_index
  }

  while ($loop_idx <= $max_index)
  {
    --let $table_name = \$TABLE_$loop_idx

    # After import include also the encrypted table.
    if ($imported) {
      if ($loop_idx == $max_index) {
        --let $table_name = table_part_encrypt
      }
    }

    --echo
    --eval CHECK TABLE $table_name
    --echo
    --replace_result $MYSQL_TMP_DIR ""
    --replace_regex /mysql_wl13352_data\/\//mysql_wl13352_data\//
    --eval SHOW CREATE TABLE $table_name
    --echo
    --eval SELECT id, value, name, SUBSTRING(data, 1024, 32) FROM $table_name order by id

    --inc $loop_idx
  }

  --echo
  --echo Checking IFS views
  --echo ------------------

  select table_schema, table_name from information_schema.tables
  where table_name LIKE '%table_part%' order by table_schema, table_name;

  --echo
  select name, space_type from information_schema.innodb_tables
  where name LIKE '%table_part%' order by name, space_type;

  --echo
  select table_schema, table_name, partition_name, subpartition_name
  from information_schema.partitions
  where table_name LIKE '%table_part%'
  order by table_schema, table_name, partition_name, subpartition_name;

  --echo
  select name from information_schema.innodb_tablespaces
  where name LIKE '%table_part%' order by name;

  --echo
  --replace_result $MYSQL_TMP_DIR ""
  --replace_regex /innodb_file_per_table_[0-9]*/innodb_file_per_table/
  select file_type, status, file_name, tablespace_name from information_schema.files
  where file_name LIKE '%table_part%'
  order by file_type, status, file_name, tablespace_name;

  --echo
  --echo Checking Innodb stat tables
  --echo ---------------------------

  select database_name, table_name from mysql.innodb_table_stats
  where table_name like '%table_part%' order by database_name, table_name;

  --echo
  select database_name, table_name, index_name, stat_name from mysql.innodb_index_stats
  where table_name like '%table_part%'
  order by database_name, table_name, index_name, stat_name;

  if ($check_dd)
  {
    --echo
    --echo Checking DD tables
    --echo ------------------

    SET DEBUG='+d,skip_dd_table_access_check';

    --echo
    select t.name as "table", p.name as "partition", p.number
    from mysql.tables t, mysql.table_partitions p
    where p.table_id = t.id
    order by t.name, p.name, p.number;

    --echo
    select name from mysql.tablespaces
    where name like '%table_part%' order by name;

    --echo
    --replace_result $MYSQL_TMP_DIR "" \\test\\ /test/
    select file_name from mysql.tablespace_files
    where file_name like '%table_part%' order by file_name;

    SET DEBUG='-d,skip_dd_table_access_check';
  }
  SET NAMES DEFAULT;
}

if ($check_option == 'DDL')
{
  --echo DDL on partitioned tables
  --echo -------------------------

  --eval ALTER TABLE $TABLE_1 TRUNCATE PARTITION p0
  --echo
  --eval ALTER TABLE $TABLE_1 COALESCE PARTITION 2
  --echo
  --eval SHOW CREATE TABLE $TABLE_1

  --echo
  --eval ALTER TABLE $TABLE_2 TRUNCATE PARTITION part_1
  --echo
  --eval ALTER TABLE $TABLE_3 TRUNCATE PARTITION part_3

  --echo
  --eval CREATE TABLE test_table LIKE $TABLE_3
  --echo
  --eval ALTER TABLE test_table REMOVE PARTITIONING

  --let $row_idx = 2
  --let $row_val = `select 100 * $row_idx + 65`
  --let $row_name = "Row - $row_idx"
  --let $row_data = REPEAT('Large Column Data - $row_idx' , 256)
  --eval insert into test_table values($row_idx, $row_val, $row_name, $row_data)

  --echo
  --eval SELECT id, value, name FROM $TABLE_3 PARTITION (sub_3) order by id
  --echo
  --eval SELECT id, value, name FROM test_table order by id

  --echo
  --eval ALTER TABLE $TABLE_3 EXCHANGE PARTITION sub_3 WITH TABLE test_table

  --echo
  --eval SELECT id, value, name FROM $TABLE_3 PARTITION (sub_3) order by id
  --echo
  --eval SELECT id, value, name FROM test_table order by id

  --echo
  --eval DROP TABLE test_table

  --echo
  --eval ALTER TABLE $TABLE_3 RENAME renamed_table;
  SHOW CREATE TABLE renamed_table;

  --echo
  --eval ALTER TABLE renamed_table RENAME $TABLE_3;

  --let $loop_idx = 1

  while ($loop_idx <= $num_tables)
  {
    --let $table_name = \$TABLE_$loop_idx
    --echo
    --eval ALTER TABLE $table_name ENGINE = InnoDB;
    --echo
    --eval ALTER TABLE $table_name ADD COLUMN new_id int AFTER id
    --echo

    # Only for partitioned table
    if ($loop_idx != 7)
    {
      --eval ALTER TABLE $table_name REMOVE PARTITIONING
    }

    --inc $loop_idx
  }
}

if ($check_option == 'DML')
{
  --echo DML on partitioned tables
  --echo -------------------------

  --let $loop_idx = 1

  while ($loop_idx <= $num_tables)
  {
    --let $table_name = \$TABLE_$loop_idx
    --let $row_idx = 15
    --let $row_val = `select 100 * $row_idx + 55`
    --let $row_name = "Row - $row_idx"
    --let $row_data = REPEAT('Large Column Data - $row_idx' , 256)

    --echo
    --eval insert into $table_name values($row_idx, $row_val, $row_name, $row_data)

    --echo
    --eval update $table_name set name = 'Row - updated' where value > 500

    --echo
    --eval delete from $table_name where id = 7

    --inc $loop_idx
  }
}
