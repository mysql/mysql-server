# Check that ndb_mgm has been compiled with support for debugging commands

disable_query_log;

# Build the standard command used for running "ndb_mgm"
let $mgm_cmd = $NDB_MGM;
let $mgm_cmd = $mgm_cmd --no-defaults;
let $mgm_cmd = $mgm_cmd --ndb-connectstring="$NDB_CONNECTSTRING";
let $mgm_cmd = $mgm_cmd --verbose=0;

# Save output of "ndb_mgm --help" in file
let $dump_file = $MYSQLTEST_VARDIR/tmp/help_debug.txt;
--exec $mgm_cmd -e "help" > $dump_file

# Load the output from ndb_mgm into table
create temporary table help_debug(help varchar(256));
--eval load data local infile '$dump_file' into table help_debug;

# Remove the output file
remove_file $dump_file;

if (!`SELECT COUNT(*) FROM help_debug WHERE help LIKE '%debug compiled%'`)
{
  --skip Test requires debug compiled NDB
}

# Cleanup
drop table help_debug;

enable_query_log;
