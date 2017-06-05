# Check that ndb is compiled for debugging
# to all for using debugging commands in ndb_mgm
disable_query_log;
disable_result_log;
--exec $NDB_MGM --no-defaults --ndb-connectstring="$NDB_CONNECTSTRING" --verbose=0 -e "help" > $MYSQLTEST_VARDIR/tmp/help_debug.txt
create table help_debug(help varchar(256));
-- eval load data local infile '$MYSQLTEST_VARDIR/tmp/help_debug.txt' into table help_debug;
select @help_count:=count(*) from help_debug where help like "%debug compiled%";
-- eval drop table help_debug;
enable_result_log;
-- require r/have_ndb_debug.require
select @help_count as have_ndb_debug;
enable_query_log;
