--disable_query_log
let $dump_file = $MYSQLTEST_VARDIR/tmp/ndb_show_tables.txt;

# Dump the output of ndb_show_tables to file
--exec $NDB_SHOW_TABLES --no-defaults --p > $dump_file

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
