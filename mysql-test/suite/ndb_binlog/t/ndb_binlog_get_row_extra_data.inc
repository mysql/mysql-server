#
# Get the mysqlbinlog tool --verbose mode to dump the Binlog contents with
# extra row data included, e.g.
#
#### Extra row ndb info: data_format: 0, len: 8 :0x0007200006000000
#
# Then process this input to get the number of distinct values, and hence
# distinct transaction ids in the binlog
#

--disable_query_log
let $MYSQLD_DATADIR= `select @@datadir;`;
--exec $MYSQL_BINLOG --verbose $MYSQLD_DATADIR/binlog.000001 > $MYSQLTEST_VARDIR/tmp/ndb_binlog_mysqlbinlog.sql

create table raw_binlog_rows (txt varchar(1000));
create table diff_extra_data (txt varchar(1000));

--eval load data local infile '$MYSQLTEST_VARDIR/tmp/ndb_binlog_mysqlbinlog.sql' into table raw_binlog_rows columns terminated by '\n';

insert into diff_extra_data select distinct(txt) from raw_binlog_rows where txt like '### Extra row ndb info: data_format:%';
--enable_query_log

--echo Number of different transaction ids in Binlog
select count(1) as number_of_transactions from diff_extra_data;

--disable_query_log
drop table diff_extra_data;
drop table raw_binlog_rows;
--enable_query_log
--remove_file $MYSQLTEST_VARDIR/tmp/ndb_binlog_mysqlbinlog.sql
