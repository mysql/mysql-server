# 
# Munch the mysqlbinlog tool output into deterministic form to look at server ids
#

--disable_query_log
let $MYSQLD_DATADIR= `select @@datadir;`;
--exec $MYSQL_BINLOG --verbose $MYSQLD_DATADIR/$BINLOG_FILENAME.000001 > $MYSQLTEST_VARDIR/tmp/rpl_ndb_mysqlbinlog.sql
set sql_log_bin=0;
create table raw_binlog_rows (txt varchar(1000));

--eval load data local infile '$MYSQLTEST_VARDIR/tmp/rpl_ndb_mysqlbinlog.sql' into table raw_binlog_rows columns terminated by '\n';

--enable_query_log
# Remove non-deterministic and uninteresting elements from the mysqlbinlog output
# Right trim of length-17 takes out the initial timestamp
# We are primarily interested in server id and event type
--replace_regex /thread_id=[0-9]*/ /     /exec_time=[0-9]*/ /   /end_log_pos [0-9]*/ /   /table id [0-9]*/ /   /error_code=[0-9]*/ /   /mapped to number [0-9]*/ /
select replace(right(txt, length(txt)-17), '\r', '') as binlog_entries from raw_binlog_rows where txt like '%server id%' and not (txt like '%Start%' or txt like '%Rotate%');

--disable_query_log
drop table raw_binlog_rows;
set sql_log_bin=1;
--enable_query_log