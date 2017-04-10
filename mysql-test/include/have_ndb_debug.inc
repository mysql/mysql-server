# Check that ndb is compiled for debugging
# to all for using debugging commands in ndb_mgm
disable_query_log;
--exec $NDB_MGM --no-defaults --ndb-connectstring="$NDB_CONNECTSTRING" --verbose=0 -e "help" > $MYSQLTEST_VARDIR/tmp/help_debug.txt
create table help_debug(help varchar(256));
--eval load data local infile '$MYSQLTEST_VARDIR/tmp/help_debug.txt' into table help_debug;

let $have_ndb_debug = `SELECT COUNT(*) FROM help_debug WHERE help LIKE '%debug compiled%'`;
if (!$have_ndb_debug)
{
  --skip Test requires debug compiled NDB
}
