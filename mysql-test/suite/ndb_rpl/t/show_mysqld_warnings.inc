--disable_query_log
--echo MySQLD error output for server $server_num matching pattern $pattern
create table errlog (a int auto_increment primary key, txt text) engine=myisam;

--eval load data local infile "$MYSQLTEST_VARDIR/log/mysqld.$server_num.err" into table errlog columns terminated by '\n' (txt);

--eval select replace( lower( right(txt, length(txt) - locate('[',txt) + 1)), '\r', '') as relevant from errlog where txt like '$pattern' order by a desc limit $limit;

drop table errlog;
--enable_query_log
