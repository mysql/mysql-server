# there is no neat way to find the backupid, this is a hack to find it...
--let $dump_file= $MYSQLTEST_VARDIR/tmp/select_all.txt
--exec $NDB_SELECT_ALL --no-defaults -d sys --delimiter=, SYSTAB_0 > $dump_file

CREATE TEMPORARY TABLE test.backup_info(
  a BIGINT,
  b BIGINT
) ENGINE = HEAP;

--disable_warnings
--replace_result $dump_file DUMP_FILE
eval LOAD DATA INFILE '$dump_file' IGNORE INTO TABLE test.backup_info
  FIELDS TERMINATED BY ',' IGNORE 1 LINES;
--remove_file $dump_file
--enable_warnings

# Load backup id into environment variable
let $_exists = `SELECT a from test.backup_info where a = 520093696`;
if (!$_exists)
{
  # Failed to read the value, dump table and report error
  select * from test.backup_info;
  die Could not read backup id from backup_info table;
}
let $the_backup_id = `SELECT b from test.backup_info where a = 520093696`;

DROP TABLE test.backup_info;


