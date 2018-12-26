--eval create table t1(a int not null, b int, c char(10), d varchar(20), primary key (a)) engine = innodb default charset=$charset
insert into t1 values (1,1,'ab','ab'),(2,2,'ac','ac'),(3,2,'ad','ad'),(4,4,'afe','afe');
commit;
--error ER_DUP_ENTRY
alter table t1 add unique index (b);
insert into t1 values(8,9,'fff','fff');
select * from t1;
show create table t1;
alter table t1 add index (b);
insert into t1 values(10,10,'kkk','iii');
-- disable_query_log
-- disable_result_log
analyze table t1;
-- enable_result_log
-- enable_query_log
select * from t1;
select * from t1 force index(b) order by b;
explain select * from t1 force index(b) order by b;
show create table t1;
alter table t1 add unique index (c), add index (d);
insert into t1 values(11,11,'aaa','mmm');
-- disable_query_log
-- disable_result_log
analyze table t1;
-- enable_result_log
-- enable_query_log
select * from t1;
select * from t1 force index(b) order by b;
select * from t1 force index(c) order by c;
select * from t1 force index(d) order by d;
explain select * from t1 force index(b) order by b;
explain select * from t1 force index(c) order by c;
explain select * from t1 force index(d) order by d;
show create table t1;
check table t1;
drop table t1;
