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
--source include/wait_until_disconnected.inc
--source include/search_pattern.inc
--source include/start_mysqld.inc
drop table t1;
