#-------------------------------------------------------
# Prints instant metadata of partition table and it's
# columns from
#       - DD Tables
#       - Information_schema tables
# NOTE : $table_name needs to be set.
#      : $is_debug to be set for debug build
#-------------------------------------------------------

--disable_query_log
--let $n_parts = `SELECT COUNT(*) from information_schema.innodb_tables where name like "%$table_name%"`

--let $part_no = 0
while ($part_no < $n_parts)
{ 
  --let $part_name = `select NAME from information_schema.innodb_tables where name like "%$table_name%" limit $part_no, 1`
  --let $table_id = `SELECT TABLE_ID FROM INFORMATION_SCHEMA.INNODB_TABLES WHERE NAME="$part_name"`

  if ($is_debug)
  {
    # Print DD metadata only once for 1st partition
    if ($part_no == 0)
    {
      --echo # DD Metadata of columns in table
      SET SESSION debug= '+d,skip_dd_table_access_check';
      eval SELECT ID INTO @tid FROM mysql.tables WHERE NAME='$table_name';
      --replace_regex /table_id=[0-9]+;/table_id=TABLE_ID;/
      SELECT NAME, ORDINAL_POSITION, TYPE, HAS_NO_DEFAULT, HIDDEN, SE_PRIVATE_DATA
         FROM mysql.columns
         WHERE TABLE_ID=@tid
         ORDER BY ORDINAL_POSITION;

      --echo # DD Metadata of partitions in table
      SELECT NAME, DESCRIPTION_UTF8, SE_PRIVATE_DATA
         FROM mysql.table_partitions
         WHERE TABLE_ID=@tid;
    }

    --echo # Metadata from INFORMATION_SCHEMA.TABLES
    eval SELECT NAME, N_COLS, INSTANT_COLS, TOTAL_ROW_VERSIONS,
      INITIAL_COLUMN_COUNTS, CURRENT_COLUMN_COUNTS, TOTAL_COLUMN_COUNTS
      FROM INFORMATION_SCHEMA.INNODB_TABLES
      WHERE TABLE_ID=$table_id;

    --echo # Metadata from INFORMATION_SCHEMA.COLUMNS
    SET SESSION debug= '+d,show_dropped_column';
    eval SELECT NAME, POS, MTYPE, PRTYPE, HAS_DEFAULT,
     VERSION_ADDED, VERSION_DROPPED
     FROM INFORMATION_SCHEMA.INNODB_COLUMNS
     WHERE TABLE_ID=$table_id;
  }

  if (!$is_debug)
  {
    --echo # Metadata from INFORMATION_SCHEMA.TABLES
    eval SELECT NAME, N_COLS, INSTANT_COLS, TOTAL_ROW_VERSIONS
      FROM INFORMATION_SCHEMA.INNODB_TABLES
      WHERE TABLE_ID=$table_id;

    --echo # Metadata from INFORMATION_SCHEMA.COLUMNS
    eval SELECT NAME, POS, MTYPE, PRTYPE, HAS_DEFAULT
     FROM INFORMATION_SCHEMA.INNODB_COLUMNS
     WHERE TABLE_ID=$table_id;
  }

  inc $part_no;
}
--enable_query_log
