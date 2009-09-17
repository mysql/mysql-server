drop table if exists t1;
create table t1 (id int) engine=InnoDB;
insert into t1 values (1);
create temporary table t2 engine=InnoDB select * from t1;
drop temporary table t2;
drop table t1;
