#
# Bug#17604730 ASSERTION: *CURSOR->INDEX->NAME == TEMP_INDEX_PREFIX
#
create table t1 (f1 int primary key, f2 int, f3 int, unique key k1(f2),
key k2(f3)) engine=innodb;
insert into t1 values (14, 24, 34);
set debug = '+d,row_ins_sec_index_entry_timeout';
replace into t1 values (14, 25, 34);
drop table t1;
