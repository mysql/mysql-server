reset master;
create database `drop-temp+table-test`;
use `drop-temp+table-test`;
create temporary table `table:name` (a int);
select get_lock("a",10);
get_lock("a",10)
1
select get_lock("a",10);
get_lock("a",10)
1
show binlog events;
drop database `drop-temp+table-test`;
