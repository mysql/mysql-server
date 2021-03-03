--disable_query_log
--exec $NDB_SELECT_ALL -dsys NDB\$EVENTS_0 > $MYSQLTEST_VARDIR/tmp.dat
create temporary table raw_binlog_events (txt varchar(1000));
--eval load data local infile '$MYSQLTEST_VARDIR/tmp.dat' into table raw_binlog_events columns terminated by '\n';
--enable_query_log
select count(*) from raw_binlog_events where txt like '%t1\_%';
--disable_query_log
drop table raw_binlog_events;
--enable_query_log
