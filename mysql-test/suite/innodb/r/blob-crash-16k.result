#
# Bug #16963396 INNODB: USE OF LARGE EXTERNALLY-STORED FIELDS MAKES
# CRASH RECOVERY LOSE DATA
#
#
# Uncompressed Table - UPDATE Operation - Crash Test
# Update that moves non-updated column to blob
#
create table t2 (f1 bigint primary key, f2 longblob, f3 longblob,
index(f2(10), f3(10))) engine=innodb;
insert into t2 values (1, repeat('%', 7000), repeat('+', 30));
select f1, length(f2), length(f3) from t2;
f1	length(f2)	length(f3)
1	7000	30
# Connection con1:
set debug_sync='ib_mv_nonupdated_column_offpage SIGNAL s1 WAIT_FOR go_update';
begin;
update t2 set f3 = repeat('[', 3000);
# Connection default:
set debug_sync='now WAIT_FOR s1';
set debug_sync='now SIGNAL go_update';
# Connection con1:
# reap: update t2 set f3 = repeat('[', 3000);
# Connection default:
# Kill and restart
select f1, length(f2), length(f3) from t2;
f1	length(f2)	length(f3)
1	7000	30
select f1, right(f2, 40), right(f3, 40) from t2;
f1	right(f2, 40)	right(f3, 40)
1	%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%	++++++++++++++++++++++++++++++
check table t2;
Table	Op	Msg_type	Msg_text
test.t2	check	status	OK
#
# Compressed Table - Update Operation
# update that moves a non-updated column to BLOB
#
set global innodb_compression_level = 0;
create table t3 (f1 bigint primary key, f2 longblob, f3 longblob,
index(f2(10), f3(10))) engine=innodb row_format=compressed;
insert into t3 values (1, repeat('%', 7000), repeat('+', 30));
select f1, length(f2), length(f3) from t3;
f1	length(f2)	length(f3)
1	7000	30
# Connection con2:
set debug_sync='ib_mv_nonupdated_column_offpage SIGNAL s1 WAIT_FOR go_upd';
begin;
update t3 set f3 = repeat('[', 3000) where f1 = 1;;
# Connection default:
set debug_sync='now WAIT_FOR s1';
set debug_sync='now SIGNAL go_upd';
# Connection con2:
# reap: update t3 set f3 = repeat('[', 3000) where f1 = 1;
# Connection default:
# Kill and restart
select f1, length(f2), length(f3) from t3;
f1	length(f2)	length(f3)
1	7000	30
select f1, right(f2, 30), right(f3, 20) from t3;
f1	right(f2, 30)	right(f3, 20)
1	%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%	++++++++++++++++++++
check table t3;
Table	Op	Msg_type	Msg_text
test.t3	check	status	OK
DROP TABLE t2, t3;
