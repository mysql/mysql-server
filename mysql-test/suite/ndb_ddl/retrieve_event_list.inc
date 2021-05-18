#
# Retrieve the list of events created in NDB. This is done using
# 'ndb_show_tables'to dump the events to a file and then load
# the file into a temporary table named `event_list`.
#
# This must be done before check_event_for_table.inc is used
#

let $dump_file = $MYSQLTEST_VARDIR/tmp/event_list.txt;

# Build the string used for executing ndb_show_tables
let $show_cmd = $NDB_SHOW_TABLES;
let $show_cmd = $show_cmd -p;  # parsable format
let $show_cmd = $show_cmd --type=11; # Only show events
#echo show_cmd: $show_cmd;

# Run ndb_show_tables and dump output to file
--exec $show_cmd > $dump_file

--disable_query_log
CREATE TEMPORARY TABLE test.event_list (
  id INT,
  type VARCHAR(20),
  state VARCHAR(20),
  logging VARCHAR(20),
  _database VARCHAR(255),
  _schema VARCHAR(20),
  event_name VARCHAR(255)
);

eval LOAD DATA INFILE '$dump_file' INTO TABLE test.event_list;
--remove_file $dump_file

# Clean the column "event_name" from any windows line endings
UPDATE test.event_list SET event_name = TRIM(TRAILING '\r' FROM event_name);
# Trim surrounding quotation marks from event_name
UPDATE test.event_list SET event_name = TRIM(BOTH '\'' FROM event_name);
--enable_query_log
