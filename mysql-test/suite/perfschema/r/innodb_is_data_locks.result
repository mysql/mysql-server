#Test for Intention Shared lock
drop table if exists t1_is;
use test;
create table t1_is (id integer, x integer, PRIMARY KEY (id)) engine=INNODB;
insert into t1_is values (0,0),(1,1);
SET autocommit=0;
begin;
select * from t1_is where id=0 FOR SHARE;
id	x
0	0
begin;
select * from t1_is;
id	x
0	0
1	1
update t1_is set x = 2 where id = 0;
begin;
select object_schema, object_name, lock_type, lock_mode, lock_status, lock_data
from performance_schema.data_locks
where object_name="t1_is"
  order by lock_type, lock_mode, lock_status, lock_data;
object_schema	object_name	lock_type	lock_mode	lock_status	lock_data
test	t1_is	RECORD	S,REC_NOT_GAP	GRANTED	0
test	t1_is	RECORD	X,REC_NOT_GAP	WAITING	0
test	t1_is	TABLE	IS	GRANTED	NULL
test	t1_is	TABLE	IX	GRANTED	NULL
select object_schema, object_name, lock_type, lock_mode, lock_status, lock_data
from performance_schema.data_locks
where object_name="t1_is"
  order by lock_type, lock_mode, lock_status, lock_data;
object_schema	object_name	lock_type	lock_mode	lock_status	lock_data
test	t1_is	RECORD	S,REC_NOT_GAP	GRANTED	0
test	t1_is	RECORD	X,REC_NOT_GAP	WAITING	0
test	t1_is	TABLE	IS	GRANTED	NULL
test	t1_is	TABLE	IX	GRANTED	NULL
select count(*) into @health_check
from performance_schema.data_locks where object_name="t1_is";
commit;
commit;
commit;
drop table t1_is;
