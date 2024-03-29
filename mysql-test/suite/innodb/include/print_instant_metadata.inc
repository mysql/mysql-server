#-------------------------------------------------------
# Prints instant metadata of table and it's columns from
#	- DD Tables
#	- Information_schema tables
# NOTE : $table_name needs to be set.
#      : $is_debug to be set for debug build
#-------------------------------------------------------
let $table_id = `SELECT TABLE_ID FROM INFORMATION_SCHEMA.INNODB_TABLES WHERE NAME='test/$table_name'`;
--disable_query_log

if ($is_debug) {
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

  SET SESSION debug= '+d,skip_dd_table_access_check';
  eval SELECT ID INTO @tid FROM mysql.tables WHERE NAME='$table_name';
  --echo # DD Metadata of table
  SELECT NAME, SE_PRIVATE_DATA FROM mysql.tables WHERE ID=@tid;

  --echo # DD Metadata of columns in table
  --replace_regex /table_id=[0-9]+;/table_id=TABLE_ID;/
  SELECT NAME, ORDINAL_POSITION, TYPE, HAS_NO_DEFAULT, HIDDEN, SE_PRIVATE_DATA
     FROM mysql.columns
     WHERE TABLE_ID=@tid
     ORDER BY ORDINAL_POSITION;
}

if (!$is_debug) {
  --echo # Metadata from INFORMATION_SCHEMA.TABLES
  eval SELECT NAME, N_COLS, INSTANT_COLS, TOTAL_ROW_VERSIONS
    FROM INFORMATION_SCHEMA.INNODB_TABLES
    WHERE TABLE_ID=$table_id;

  --echo # Metadata from INFORMATION_SCHEMA.COLUMNS
  eval SELECT NAME, POS, MTYPE, PRTYPE, HAS_DEFAULT
    FROM INFORMATION_SCHEMA.INNODB_COLUMNS
    WHERE TABLE_ID=$table_id;
}

--enable_query_log
