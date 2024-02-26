#Test for Shared, GAP locks
drop table if exists t1_s_gap;
drop table if exists t2_s_gap;
use test;
create table t1_s_gap (col1 int primary key);
create table t2_s_gap (col1 int primary key, col2 int not null, foreign key(col2) references t1_s_gap(col1));
insert into t1_s_gap values (10),(20),(30),(40);
start transaction;
insert into t2_s_gap values (1, 25);
ERROR 23000: Cannot add or update a child row: a foreign key constraint fails (`test`.`t2_s_gap`, CONSTRAINT `t2_s_gap_ibfk_1` FOREIGN KEY (`col2`) REFERENCES `t1_s_gap` (`col1`))
use performance_schema;
select object_schema, object_name, lock_type, lock_mode, lock_status, lock_data
from data_locks
where object_name in ('t1_s_gap', 't2_s_gap')
and (lock_type = 'TABLE')
order by object_name, lock_type, lock_mode, lock_status, lock_data;
object_schema	object_name	lock_type	lock_mode	lock_status	lock_data
test	t1_s_gap	TABLE	IS	GRANTED	NULL
test	t2_s_gap	TABLE	IX	GRANTED	NULL
select object_schema, object_name, lock_type, lock_mode, lock_status,
if(lock_data is null, '30-or-null',
if (lock_data = '30', '30-or-null', lock_data))
as lock_data
from data_locks
where object_name in ('t1_s_gap', 't2_s_gap')
and (lock_type != 'TABLE')
order by object_name, lock_type, lock_mode, lock_status, lock_data;
object_schema	object_name	lock_type	lock_mode	lock_status	lock_data
test	t1_s_gap	RECORD	S,GAP	GRANTED	30-or-null
test	t2_s_gap	RECORD	X	GRANTED	supremum pseudo-record
start transaction;
insert into t1_s_gap values (25);
use performance_schema;
select object_schema, object_name, lock_type, lock_mode, lock_status, lock_data
from data_locks
where object_name in ('t1_s_gap', 't2_s_gap')
and (lock_type = 'TABLE')
order by object_name, lock_type, lock_mode, lock_status, lock_data;
object_schema	object_name	lock_type	lock_mode	lock_status	lock_data
test	t1_s_gap	TABLE	IS	GRANTED	NULL
test	t1_s_gap	TABLE	IX	GRANTED	NULL
test	t2_s_gap	TABLE	IX	GRANTED	NULL
select object_schema, object_name, lock_type, lock_mode, lock_status,
if(lock_data is null, '30-or-null',
if (lock_data = '30', '30-or-null', lock_data))
as lock_data
from data_locks
where object_name in ('t1_s_gap', 't2_s_gap')
and (lock_type != 'TABLE')
order by object_name, lock_type, lock_mode, lock_status, lock_data;
object_schema	object_name	lock_type	lock_mode	lock_status	lock_data
test	t1_s_gap	RECORD	S,GAP	GRANTED	30-or-null
test	t1_s_gap	RECORD	X,GAP,INSERT_INTENTION	WAITING	30-or-null
test	t2_s_gap	RECORD	X	GRANTED	supremum pseudo-record
commit;
commit;
drop table t2_s_gap;
drop table t1_s_gap;
