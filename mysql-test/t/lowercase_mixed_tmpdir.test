
--source include/have_case_sensitive_file_system.inc
--source include/have_lowercase1.inc

--disable_warnings
drop table if exists t1;
--enable_warnings

create table t1 (id int);
insert into t1 values (1);
create temporary table t2 select * from t1;
drop temporary table t2;
drop table t1;
