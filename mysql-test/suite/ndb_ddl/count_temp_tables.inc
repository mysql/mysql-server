#
# Print number of tables with temp name present in NDB.
#
# This needs to be run after a COPY ALTER to verify that the
# tables with temp names don't exist in NDB.
#

let $dump_file = $MYSQLTEST_VARDIR/tmp/ndb_show_tables.txt;

# Build the string used for executing ndb_show_tables
let $show_cmd = $NDB_SHOW_TABLES;
let $show_cmd = $show_cmd -p;  # parsable format
let $show_cmd = $show_cmd --type=2; # Only show UserTables

# Run ndb_show_tables and dump output to file
--exec $show_cmd > $dump_file

--disable_query_log
--disable_warnings
CREATE TEMPORARY TABLE IF NOT EXISTS test.table_list (
  id INT,
  type VARCHAR(20),
  state VARCHAR(20),
  logging VARCHAR(20),
  _database VARCHAR(255),
  _schema VARCHAR(20),
  table_name VARCHAR(255)
);
--enable_warnings

# Truncate the table in case if the table was there already
TRUNCATE test.table_list;

eval LOAD DATA INFILE '$dump_file' INTO TABLE test.table_list;
--remove_file $dump_file

# Clean the column "table_name" from any windows line endings
UPDATE test.table_list SET table_name = TRIM(TRAILING '\r' FROM table_name);
# Trim surrounding quotation marks from table_name
UPDATE test.table_list SET table_name = TRIM(BOTH '\'' FROM table_name);

# Print the number of tables with temp name
SELECT COUNT(*) AS 'Number of tables with temp name in NDB'
  FROM test.table_list
  WHERE table_name LIKE "#sql%";
--enable_query_log
