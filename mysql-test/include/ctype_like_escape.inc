#
# Bugs: #13046:
# LIKE pattern matching using prefix index doesn't return correct result
#
select @@collation_connection;
create table t1 ROW_FORMAT=DYNAMIC select repeat('a',50) as c1 ;
insert into t1 values('abcdef');
insert into t1 values('_bcdef');
insert into t1 values('a_cdef');
insert into t1 values('ab_def');
insert into t1 values('abc_ef');
insert into t1 values('abcd_f');
insert into t1 values('abcde_');
# should return ab_def
select c1 as c1u from t1 where c1 like 'ab\_def';
# should return ab_def
select c1 as c2h from t1 where c1 like 'ab#_def' escape '#';
drop table t1;
