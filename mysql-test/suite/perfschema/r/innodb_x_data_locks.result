#Test for Exclusive lock
drop table if exists t1;
create table t1 (id integer, x integer) engine=INNODB;
insert into t1 values(0, 0);
SET autocommit=0;
lock table t1 WRITE;
use performance_schema;
SELECT object_schema, object_name, lock_type, lock_mode, lock_status, lock_data FROM data_locks;
object_schema	object_name	lock_type	lock_mode	lock_status	lock_data
test	t1	TABLE	X	GRANTED	NULL
unlock tables;
drop table t1;
