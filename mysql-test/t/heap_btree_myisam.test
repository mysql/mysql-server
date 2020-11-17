--source include/force_myisam_default.inc
--source include/have_myisam.inc

#
# Test of heap tables.
#

create table t1 (a int not null,b int not null, primary key using BTREE (a)) engine=heap comment="testing heaps";
insert into t1 values(1,1),(2,2),(3,3),(4,4);
alter table t1 modify a int not null auto_increment, engine=myisam, comment="new myisam table";
#show table status like "t1";
select * from t1;
drop table t1;

create table t1 (a int not null) engine=heap;
insert into t1 values (869751),(736494),(226312),(802616),(728912);
--sorted_result
select * from t1 where a > 736494;
alter table t1 add unique uniq_id using BTREE (a);
--sorted_result
select * from t1 where a > 736494;
--sorted_result
select * from t1 where a = 736494;
--sorted_result
select * from t1 where a=869751 or a=736494;
--sorted_result
select * from t1 where a in (869751,736494,226312,802616);
alter table t1 engine=myisam;
explain select * from t1 where a in (869751,736494,226312,802616);
drop table t1;
