--disable_query_log
let $dump_file = $MYSQLTEST_VARDIR/tmp/ndb_show_tables.txt;

# Build the string used for executing ndb_show_tables
let $show_cmd = $NDB_SHOW_TABLES --no-defaults;
let $show_cmd = $show_cmd -p;  # parsable format
let $show_cmd = $show_cmd --ndb-nodeid=255;  # fixed nodeid to avoid "steal"

# Run ndb_show_tables and dump output to file
--exec $show_cmd > $dump_file

TRUNCATE ndb_show_tables_results;

# needed for embedded
--chmod 0777 $dump_file
eval LOAD DATA INFILE '$dump_file'
  INTO TABLE ndb_show_tables_results;
--remove_file $dump_file

# Clean the last column "name" from any windows line endings
UPDATE ndb_show_tables_results SET name = REPLACE(name, '\r', '');

--enable_query_log

--echo
--echo ndb_show_tables completed.....
--echo
