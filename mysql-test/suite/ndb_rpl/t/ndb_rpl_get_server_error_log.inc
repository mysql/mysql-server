# Puts error log for server $mysqld_name in table test.server_error_log

--disable_query_log
--disable_result_log
use test;
drop table if exists server_error_log;
create table server_error_log(
        row int auto_increment primary key,
        line varchar(1024) default null)
engine=myisam;

--let $my_log_error_path=$MYSQLTEST_VARDIR/log/$mysqld_name.err

eval load data local infile '$my_log_error_path' into table server_error_log
  fields terminated by 'xykls37' escaped by ''
  ignore 1 lines
  (line);

--enable_result_log
--enable_query_log


