show variables like 'create_old_temporals';

reset master;

--echo Create table with mix of type versions
create table test.t1 (a int primary key, 
                      ts_old timestamp, 
                      ts_new_zero timestamp(0), 
                      ts_new_gtz timestamp (3),
                      dt_old datetime,
                      dt_new_zero datetime(0),
                      dt_new_gtz timestamp (1),
                      t_old time,
                      t_new_zero time(0),
                      t_new_gtz time(6)) engine=ndb;

--echo Show Ndb schema pre-alter : 
--source suite/ndb/t/show_attributes.inc

--echo Alter table
alter table test.t1 force;

--echo Show Ndb schema post-alter :
--source suite/ndb/t/show_attributes.inc

--echo Insert row.
insert into test.t1 values (1, '1973-07-29 12:13:14', 
                               '1973-07-29 12:13:14', 
                               '1973-07-29 12:13:14.156',
                               '1973-07-29 12:13:14', 
                               '1973-07-29 12:13:14', 
                               '1973-07-29 12:13:14.1',
                               '12:13:14',
                               '12:13:14',
                               '12:13:14.567891');

--disable_query_log
--disable_result_log
# Wait for Binlog flush
SHOW BINLOG EVENTS;


let $MYSQLD_DATADIR= `select @@datadir;`;
--exec $MYSQL_BINLOG --verbose --verbose $MYSQLD_DATADIR/mysqld-bin.000001 > $MYSQLTEST_VARDIR/tmp/ndb_binlog_mysqlbinlog.sql

create table raw_binlog_rows (txt varchar(1000));

--eval load data local infile '$MYSQLTEST_VARDIR/tmp/ndb_binlog_mysqlbinlog.sql' into table raw_binlog_rows columns terminated by '\n';

--enable_result_log
--enable_query_log


--echo Show metadata for events in Binlog
--echo Must have non '(X)' suffix for 'old' types
select replace(txt, '\r', '') as typeinfo
  from raw_binlog_rows 
 where txt like '%time%meta%' or 
       txt like '%date%meta%';

drop table raw_binlog_rows;
drop table test.t1;

