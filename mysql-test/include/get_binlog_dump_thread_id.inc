--exec $MYSQL test -e "show processlist" > $MYSQLTEST_VARDIR/tmp/bl_dump_thread_id
--disable_warnings
drop table if exists t999;
--enable_warnings
# Create a table to hold the process list
create temporary table t999(
  id int,
  user char(255),
  host char(255),
  db char(255),
  Command char(255),
  time int,
  State char(255),
  info char(255)
);
# Load processlist into table, headers will create seom warnings
--disable_warnings
--replace_result $MYSQLTEST_VARDIR "."
eval LOAD DATA INFILE "$MYSQLTEST_VARDIR/tmp/bl_dump_thread_id" into table t999;
--enable_warnings
let $id = `select Id from t999 where Command="Binlog Dump"`;
drop table t999;
