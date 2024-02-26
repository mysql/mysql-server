#
# Bug#18185930 UPD_NODE_INSERT_BLOB CAUSES BTR_EXTERN_OWNER_FLAG
# ASSERTION
#
create table t1 (f1 int primary key, f2 blob) engine = innodb;
insert into t1 values (1, repeat('*', 50000));
select f1, substring(f2, 1, 40) from t1;
f1	substring(f2, 1, 40)
1	****************************************
set debug = 'd,row_ins_index_entry_timeout';
update t1 set f1 = 3;
select f1, substring(f2, 1, 40) from t1;
f1	substring(f2, 1, 40)
3	****************************************
drop table t1;
