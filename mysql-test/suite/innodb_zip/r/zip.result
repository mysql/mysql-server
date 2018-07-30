SET @save_innodb_stats_on_metadata=@@global.innodb_stats_on_metadata;
set session innodb_strict_mode=0;
set global innodb_file_per_table=off;
SET @@global.innodb_stats_on_metadata=ON;
create table t0(a int primary key) engine=innodb row_format=compressed;
Warnings:
Warning	1478	InnoDB: ROW_FORMAT=COMPRESSED requires innodb_file_per_table.
Warning	1478	InnoDB: assuming ROW_FORMAT=DYNAMIC.
create table t00(a int primary key) engine=innodb
key_block_size=4 row_format=compressed;
Warnings:
Warning	1478	InnoDB: KEY_BLOCK_SIZE requires innodb_file_per_table.
Warning	1478	InnoDB: ignoring KEY_BLOCK_SIZE=4.
Warning	1478	InnoDB: ROW_FORMAT=COMPRESSED requires innodb_file_per_table.
Warning	1478	InnoDB: assuming ROW_FORMAT=DYNAMIC.
create table t1(a int primary key) engine=innodb row_format=dynamic;
create table t2(a int primary key) engine=innodb row_format=redundant;
create table t3(a int primary key) engine=innodb row_format=compact;
create table t4(a int primary key) engine=innodb key_block_size=9;
Warnings:
Warning	1478	InnoDB: KEY_BLOCK_SIZE requires innodb_file_per_table.
Warning	1478	InnoDB: ignoring KEY_BLOCK_SIZE=9.
create table t5(a int primary key) engine=innodb
key_block_size=1 row_format=redundant;
Warnings:
Warning	1478	InnoDB: KEY_BLOCK_SIZE requires innodb_file_per_table.
Warning	1478	InnoDB: ignoring KEY_BLOCK_SIZE=1.
set global innodb_file_per_table=on;
create table t6(a int primary key) engine=innodb
key_block_size=1 row_format=redundant;
Warnings:
Warning	1478	InnoDB: ignoring KEY_BLOCK_SIZE=1 as ROW_FORMAT is not COMPRESSED.
create table t7(a int primary key) engine=innodb
key_block_size=1 row_format=redundant;
Warnings:
Warning	1478	InnoDB: ignoring KEY_BLOCK_SIZE=1 as ROW_FORMAT is not COMPRESSED.
create table t8(a int primary key) engine=innodb
key_block_size=1 row_format=fixed;
Warnings:
Warning	1478	InnoDB: ignoring KEY_BLOCK_SIZE=1 as ROW_FORMAT is not COMPRESSED.
Warning	1478	InnoDB: assuming ROW_FORMAT=DYNAMIC.
create table t9(a int primary key) engine=innodb
key_block_size=1 row_format=compact;
Warnings:
Warning	1478	InnoDB: ignoring KEY_BLOCK_SIZE=1 as ROW_FORMAT is not COMPRESSED.
create table t10(a int primary key) engine=innodb
key_block_size=1 row_format=dynamic;
Warnings:
Warning	1478	InnoDB: ignoring KEY_BLOCK_SIZE=1 as ROW_FORMAT is not COMPRESSED.
create table t11(a int primary key) engine=innodb
key_block_size=1 row_format=compressed;
create table t12(a int primary key) engine=innodb
key_block_size=1;
create table t13(a int primary key) engine=innodb
row_format=compressed;
create table t14(a int primary key) engine=innodb key_block_size=9;
Warnings:
Warning	1478	InnoDB: ignoring KEY_BLOCK_SIZE=9.
analyze table t0 ,t00 ,t1 ,t2 ,t3 ,t4 ,t5 ,t6 ,t7 ,t8 ,t9 ,t10 ,t11 ,t12 ,t13 ,t14;
Table	Op	Msg_type	Msg_text
test.t0	analyze	status	OK
test.t00	analyze	status	OK
test.t1	analyze	status	OK
test.t2	analyze	status	OK
test.t3	analyze	status	OK
test.t4	analyze	status	OK
test.t5	analyze	status	OK
test.t6	analyze	status	OK
test.t7	analyze	status	OK
test.t8	analyze	status	OK
test.t9	analyze	status	OK
test.t10	analyze	status	OK
test.t11	analyze	status	OK
test.t12	analyze	status	OK
test.t13	analyze	status	OK
test.t14	analyze	status	OK
SELECT table_schema, table_name, row_format, data_length, index_length FROM information_schema.tables WHERE engine='innodb' AND table_schema != 'mysql' AND table_schema != 'sys' AND table_schema != 'PERFORMANCE_SCHEMA' AND table_schema != 'information_schema' ORDER BY table_schema, table_name;;
TABLE_SCHEMA	TABLE_NAME	ROW_FORMAT	DATA_LENGTH	INDEX_LENGTH
mtr	global_suppressions	Dynamic	{valid}	0
mtr	test_suppressions	Dynamic	{valid}	0
test	t0	Dynamic	{valid}	0
test	t00	Dynamic	{valid}	0
test	t1	Dynamic	{valid}	0
test	t10	Dynamic	{valid}	0
test	t11	Compressed	1024	0
test	t12	Compressed	1024	0
test	t13	Compressed	{valid}	0
test	t14	Dynamic	{valid}	0
test	t2	Redundant	{valid}	0
test	t3	Compact	{valid}	0
test	t4	Dynamic	{valid}	0
test	t5	Redundant	{valid}	0
test	t6	Redundant	{valid}	0
test	t7	Redundant	{valid}	0
test	t8	Dynamic	{valid}	0
test	t9	Compact	{valid}	0
drop table t0,t00,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12,t13,t14;
alter table t1 key_block_size=0;
alter table t1 row_format=dynamic;
analyze table t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SELECT table_schema, table_name, row_format, data_length, index_length FROM information_schema.tables WHERE engine='innodb' AND table_schema != 'mysql' AND table_schema != 'sys' AND table_schema != 'PERFORMANCE_SCHEMA' AND table_schema != 'information_schema' ORDER BY table_schema, table_name;;
TABLE_SCHEMA	TABLE_NAME	ROW_FORMAT	DATA_LENGTH	INDEX_LENGTH
mtr	global_suppressions	Dynamic	{valid}	0
mtr	test_suppressions	Dynamic	{valid}	0
test	t1	Dynamic	{valid}	0
alter table t1 row_format=compact;
analyze table t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SELECT table_schema, table_name, row_format, data_length, index_length FROM information_schema.tables WHERE engine='innodb' AND table_schema != 'mysql' AND table_schema != 'sys' AND table_schema != 'PERFORMANCE_SCHEMA' AND table_schema != 'information_schema' ORDER BY table_schema, table_name;;
TABLE_SCHEMA	TABLE_NAME	ROW_FORMAT	DATA_LENGTH	INDEX_LENGTH
mtr	global_suppressions	Dynamic	{valid}	0
mtr	test_suppressions	Dynamic	{valid}	0
test	t1	Compact	{valid}	0
alter table t1 row_format=redundant;
analyze table t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SELECT table_schema, table_name, row_format, data_length, index_length FROM information_schema.tables WHERE engine='innodb' AND table_schema != 'mysql' AND table_schema != 'sys' AND table_schema != 'PERFORMANCE_SCHEMA' AND table_schema != 'information_schema' ORDER BY table_schema, table_name;;
TABLE_SCHEMA	TABLE_NAME	ROW_FORMAT	DATA_LENGTH	INDEX_LENGTH
mtr	global_suppressions	Dynamic	{valid}	0
mtr	test_suppressions	Dynamic	{valid}	0
test	t1	Redundant	{valid}	0
drop table t1;
create table t1(a int not null, b text, index(b(10))) engine=innodb
key_block_size=1;
create table t2(b text)engine=innodb;
insert into t2 values(concat('1abcdefghijklmnopqrstuvwxyz', repeat('A',5000)));
insert into t1 select 1, b from t2;
commit;
begin;
update t1 set b=repeat('B',100);
select a,left(b,40) from t1 natural join t2;
a	left(b,40)
1	1abcdefghijklmnopqrstuvwxyzAAAAAAAAAAAAA
rollback;
select a,left(b,40) from t1 natural join t2;
a	left(b,40)
1	1abcdefghijklmnopqrstuvwxyzAAAAAAAAAAAAA
analyze table t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
analyze table t2;
Table	Op	Msg_type	Msg_text
test.t2	analyze	status	OK
SELECT table_schema, table_name, row_format, data_length, index_length FROM information_schema.tables WHERE engine='innodb' AND table_schema != 'mysql' AND table_schema != 'sys' AND table_schema != 'PERFORMANCE_SCHEMA' AND table_schema != 'information_schema' ORDER BY table_schema, table_name;;
TABLE_SCHEMA	TABLE_NAME	ROW_FORMAT	DATA_LENGTH	INDEX_LENGTH
mtr	global_suppressions	Dynamic	{valid}	0
mtr	test_suppressions	Dynamic	{valid}	0
test	t1	Compressed	2048	1024
test	t2	Dynamic	{valid}	0
drop table t1,t2;
create table t1( c1 int not null, c2 blob, c3 blob, c4 blob,
primary key(c1, c2(22), c3(22)))
engine = innodb row_format = dynamic;
begin;
insert into t1 values(1, repeat('A', 20000), repeat('B', 20000),
repeat('C', 20000));
update t1 set c3 = repeat('D', 20000) where c1 = 1;
commit;
select count(*) from t1 where c2 = repeat('A', 20000);
count(*)
1
select count(*) from t1 where c3 = repeat('D', 20000);
count(*)
1
select count(*) from t1 where c4 = repeat('C', 20000);
count(*)
1
update t1 set c3 = repeat('E', 20000) where c1 = 1;
drop table t1;
set global innodb_file_per_table = on;
set innodb_strict_mode = off;
create table t1 (id int primary key) engine = innodb key_block_size = 0;
drop table t1;
set innodb_strict_mode = on;
create table t1 (id int primary key) engine = innodb key_block_size = 0;
create table t2 (id int primary key) engine = innodb key_block_size = 9;
ERROR HY000: Table storage engine for 't2' doesn't have this option
show warnings;
Level	Code	Message
Warning	1478	InnoDB: invalid KEY_BLOCK_SIZE = 9. Valid values are [1, 2, 4, 8, 16]
Error	1031	Table storage engine for 't2' doesn't have this option
create table t3 (id int primary key) engine = innodb key_block_size = 1;
create table t4 (id int primary key) engine = innodb key_block_size = 2;
create table t5 (id int primary key) engine = innodb key_block_size = 4;
create table t8 (id int primary key) engine = innodb row_format = compressed;
create table t9 (id int primary key) engine = innodb row_format = dynamic;
create table t10(id int primary key) engine = innodb row_format = compact;
create table t11(id int primary key) engine = innodb row_format = redundant;
analyze table t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
analyze table t3;
Table	Op	Msg_type	Msg_text
test.t3	analyze	status	OK
analyze table t4;
Table	Op	Msg_type	Msg_text
test.t4	analyze	status	OK
analyze table t5;
Table	Op	Msg_type	Msg_text
test.t5	analyze	status	OK
analyze table t8;
Table	Op	Msg_type	Msg_text
test.t8	analyze	status	OK
analyze table t9;
Table	Op	Msg_type	Msg_text
test.t9	analyze	status	OK
analyze table t10;
Table	Op	Msg_type	Msg_text
test.t10	analyze	status	OK
analyze table t11;
Table	Op	Msg_type	Msg_text
test.t11	analyze	status	OK
SELECT table_schema, table_name, row_format, data_length, index_length FROM information_schema.tables WHERE engine='innodb' AND table_schema != 'mysql' AND table_schema != 'sys' AND table_schema != 'PERFORMANCE_SCHEMA' AND table_schema != 'information_schema' ORDER BY table_schema, table_name;;
TABLE_SCHEMA	TABLE_NAME	ROW_FORMAT	DATA_LENGTH	INDEX_LENGTH
mtr	global_suppressions	Dynamic	{valid}	0
mtr	test_suppressions	Dynamic	{valid}	0
test	t1	Dynamic	{valid}	0
test	t10	Compact	{valid}	0
test	t11	Redundant	{valid}	0
test	t3	Compressed	1024	0
test	t4	Compressed	{valid}	0
test	t5	Compressed	{valid}	0
test	t8	Compressed	{valid}	0
test	t9	Dynamic	{valid}	0
drop table t1, t3, t4, t5, t8, t9, t10, t11;
create table t1 (id int primary key) engine = innodb
key_block_size = 4 row_format = compressed;
create table t2 (id int primary key) engine = innodb
key_block_size = 4 row_format = redundant;
ERROR HY000: Table storage engine for 't2' doesn't have this option
show warnings;
Level	Code	Message
Warning	1478	InnoDB: cannot specify ROW_FORMAT=REDUNDANT with KEY_BLOCK_SIZE.
Error	1031	Table storage engine for 't2' doesn't have this option
create table t3 (id int primary key) engine = innodb
key_block_size = 4 row_format = compact;
ERROR HY000: Table storage engine for 't3' doesn't have this option
show warnings;
Level	Code	Message
Warning	1478	InnoDB: cannot specify ROW_FORMAT=COMPACT with KEY_BLOCK_SIZE.
Error	1031	Table storage engine for 't3' doesn't have this option
create table t4 (id int primary key) engine = innodb
key_block_size = 4 row_format = dynamic;
ERROR HY000: Table storage engine for 't4' doesn't have this option
show warnings;
Level	Code	Message
Warning	1478	InnoDB: cannot specify ROW_FORMAT=DYNAMIC with KEY_BLOCK_SIZE.
Error	1031	Table storage engine for 't4' doesn't have this option
create table t5 (id int primary key) engine = innodb
key_block_size = 4 row_format = default;
analyze table t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
analyze table t5;
Table	Op	Msg_type	Msg_text
test.t5	analyze	status	OK
SELECT table_schema, table_name, row_format, data_length, index_length FROM information_schema.tables WHERE engine='innodb' AND table_schema != 'mysql' AND table_schema != 'sys' AND table_schema != 'PERFORMANCE_SCHEMA' AND table_schema != 'information_schema' ORDER BY table_schema, table_name;;
TABLE_SCHEMA	TABLE_NAME	ROW_FORMAT	DATA_LENGTH	INDEX_LENGTH
mtr	global_suppressions	Dynamic	{valid}	0
mtr	test_suppressions	Dynamic	{valid}	0
test	t1	Compressed	{valid}	0
test	t5	Compressed	{valid}	0
drop table t1, t5;
create table t1 (id int primary key) engine = innodb
key_block_size = 9 row_format = redundant;
ERROR HY000: Table storage engine for 't1' doesn't have this option
show warnings;
Level	Code	Message
Warning	1478	InnoDB: invalid KEY_BLOCK_SIZE = 9. Valid values are [1, 2, 4, 8, 16]
Warning	1478	InnoDB: cannot specify ROW_FORMAT=REDUNDANT with KEY_BLOCK_SIZE.
Error	1031	Table storage engine for 't1' doesn't have this option
create table t2 (id int primary key) engine = innodb
key_block_size = 9 row_format = compact;
ERROR HY000: Table storage engine for 't2' doesn't have this option
show warnings;
Level	Code	Message
Warning	1478	InnoDB: invalid KEY_BLOCK_SIZE = 9. Valid values are [1, 2, 4, 8, 16]
Warning	1478	InnoDB: cannot specify ROW_FORMAT=COMPACT with KEY_BLOCK_SIZE.
Error	1031	Table storage engine for 't2' doesn't have this option
create table t2 (id int primary key) engine = innodb
key_block_size = 9 row_format = dynamic;
ERROR HY000: Table storage engine for 't2' doesn't have this option
show warnings;
Level	Code	Message
Warning	1478	InnoDB: invalid KEY_BLOCK_SIZE = 9. Valid values are [1, 2, 4, 8, 16]
Warning	1478	InnoDB: cannot specify ROW_FORMAT=DYNAMIC with KEY_BLOCK_SIZE.
Error	1031	Table storage engine for 't2' doesn't have this option
SELECT table_schema, table_name, row_format, data_length, index_length FROM information_schema.tables WHERE engine='innodb' AND table_schema != 'mysql' AND table_schema != 'sys' AND table_schema != 'PERFORMANCE_SCHEMA' AND table_schema != 'information_schema' ORDER BY table_schema, table_name;;
TABLE_SCHEMA	TABLE_NAME	ROW_FORMAT	DATA_LENGTH	INDEX_LENGTH
mtr	global_suppressions	Dynamic	{valid}	0
mtr	test_suppressions	Dynamic	{valid}	0
set global innodb_file_per_table = off;
create table t1 (id int primary key) engine = innodb key_block_size = 1;
ERROR HY000: Table storage engine for 't1' doesn't have this option
show warnings;
Level	Code	Message
Warning	1478	InnoDB: KEY_BLOCK_SIZE requires innodb_file_per_table.
Error	1031	Table storage engine for 't1' doesn't have this option
create table t2 (id int primary key) engine = innodb key_block_size = 2;
ERROR HY000: Table storage engine for 't2' doesn't have this option
show warnings;
Level	Code	Message
Warning	1478	InnoDB: KEY_BLOCK_SIZE requires innodb_file_per_table.
Error	1031	Table storage engine for 't2' doesn't have this option
create table t3 (id int primary key) engine = innodb key_block_size = 4;
ERROR HY000: Table storage engine for 't3' doesn't have this option
show warnings;
Level	Code	Message
Warning	1478	InnoDB: KEY_BLOCK_SIZE requires innodb_file_per_table.
Error	1031	Table storage engine for 't3' doesn't have this option
create table t6 (id int primary key) engine = innodb row_format = compressed;
ERROR HY000: Table storage engine for 't6' doesn't have this option
show warnings;
Level	Code	Message
Warning	1478	InnoDB: ROW_FORMAT=COMPRESSED requires innodb_file_per_table.
Error	1031	Table storage engine for 't6' doesn't have this option
create table t7 (id int primary key) engine = innodb row_format = dynamic;
create table t8 (id int primary key) engine = innodb row_format = compact;
create table t9 (id int primary key) engine = innodb row_format = redundant;
analyze table t7;
Table	Op	Msg_type	Msg_text
test.t7	analyze	status	OK
analyze table t8;
Table	Op	Msg_type	Msg_text
test.t8	analyze	status	OK
analyze table t9;
Table	Op	Msg_type	Msg_text
test.t9	analyze	status	OK
SELECT table_schema, table_name, row_format, data_length, index_length FROM information_schema.tables WHERE engine='innodb' AND table_schema != 'mysql' AND table_schema != 'sys' AND table_schema != 'PERFORMANCE_SCHEMA' AND table_schema != 'information_schema' ORDER BY table_schema, table_name;;
TABLE_SCHEMA	TABLE_NAME	ROW_FORMAT	DATA_LENGTH	INDEX_LENGTH
mtr	global_suppressions	Dynamic	{valid}	0
mtr	test_suppressions	Dynamic	{valid}	0
test	t7	Dynamic	{valid}	0
test	t8	Compact	{valid}	0
test	t9	Redundant	{valid}	0
drop table t7, t8, t9;
set global innodb_file_per_table = on;
create table t8 (id int primary key) engine = innodb row_format = compact;
create table t9 (id int primary key) engine = innodb row_format = redundant;
analyze table t8;
Table	Op	Msg_type	Msg_text
test.t8	analyze	status	OK
analyze table t9;
Table	Op	Msg_type	Msg_text
test.t9	analyze	status	OK
SELECT table_schema, table_name, row_format, data_length, index_length FROM information_schema.tables WHERE engine='innodb' AND table_schema != 'mysql' AND table_schema != 'sys' AND table_schema != 'PERFORMANCE_SCHEMA' AND table_schema != 'information_schema' ORDER BY table_schema, table_name;;
TABLE_SCHEMA	TABLE_NAME	ROW_FORMAT	DATA_LENGTH	INDEX_LENGTH
mtr	global_suppressions	Dynamic	{valid}	0
mtr	test_suppressions	Dynamic	{valid}	0
test	t8	Compact	{valid}	0
test	t9	Redundant	{valid}	0
drop table t8, t9;
set global innodb_file_per_table=1;
set global innodb_file_per_table=on;
create table normal_table (
c1 int
) engine = innodb;
create table zip_table (
c1 int
) engine = innodb key_block_size = 4;
drop table normal_table, zip_table;
