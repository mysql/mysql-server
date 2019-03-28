drop table if exists t1;
create table t1 (n int not null primary key) engine=myisam;
begin work;
insert into t1 values (4);
insert into t1 values (5);
rollback;
Warnings:
Warning	1196	Some non-transactional changed tables couldn't be rolled back
show warnings;
Level	Code	Message
Warning	1196	Some non-transactional changed tables couldn't be rolled back
show errors;
Level	Code	Message
select @@warning_count,@@error_count;
@@warning_count	@@error_count
1	0
select * from t1;
n
4
5
show warnings;
Level	Code	Message
select @@warning_count;
@@warning_count
0
drop table t1;
