--disable_query_log
--echo MySQLD error output for server $server_num matching pattern $pattern
create table errlog (a int auto_increment primary key, txt text) engine=myisam;

# Avoid win path separators being interpreted as escapes
# for next char by having no escape char
--eval load data local infile "$MYSQLTEST_VARDIR/log/mysqld.$server_num.err" into table errlog columns terminated by '\n' escaped by '' (txt);

--eval delete from errlog where txt not like '$pattern';
# Trim time, win CRs and fix win path separators (or any other backslashes)
update errlog set txt= replace(replace( lower( right(txt, length(txt) - locate('[',txt) + 1)), '\r', ''), '\\', '/');
--eval select txt as relevant from errlog order by a desc limit $limit

drop table errlog;
--enable_query_log
