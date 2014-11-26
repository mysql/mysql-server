#
#
use test;
--disable_warnings
drop table if exists t1;
--enable_warnings
create table t1
        (keyc int, c1 char(100), c2 char(100),
         primary key(keyc)) engine = innodb;
begin;
call populate_t1();
delete from t1 where keyc < 20000;
update t1 set c1 = 'mysql' where  keyc > 20000;
update t1 set c2 = 'oracle' where  keyc > 20000;
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
commit;
drop table t1;
--sleep 40 
--source include/search_pattern.inc
--enable_reconnect
--source include/wait_until_connected_again.inc
--disable_reconnect
