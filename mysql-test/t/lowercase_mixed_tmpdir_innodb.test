--source include/have_lowercase2.inc
--source include/have_innodb.inc

--disable_warnings
drop table if exists t1;
--enable_warnings

create table t1 (id int) engine=InnoDB;
insert into t1 values (1);
create temporary table t2 engine=InnoDB select * from t1;
drop temporary table t2;
drop table t1;
