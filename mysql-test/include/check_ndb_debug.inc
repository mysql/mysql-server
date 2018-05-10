#
#  check_ndb_debug
#
#    Utility for checking whether the Ndb processes appear to have been
#    compiled with debug support
#
#    This utility uses the help text made available in the ndb_mgm binary
#    as an indicator of the presence of debug facilities being available
#    in ndb_mgm, ndb_mgmd, mgmapi, ndb[mtd] et al
#
#  Returns:
#    have_ndb_debug - wheter ndb* binaries are debug compiled or not
#    have_ndb_debug=1 : ndb* binaries are debug compiled
#    have_ndb_debug=0 : ndb* binaries are not debug compiled


disable_query_log;

let $have_ndb_debug = 0;

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

if (`SELECT COUNT(*) FROM help_debug WHERE help LIKE '%debug compiled%'`)
{
  let $have_ndb_debug = 1;
}

# Cleanup
drop table help_debug;

enable_query_log;
