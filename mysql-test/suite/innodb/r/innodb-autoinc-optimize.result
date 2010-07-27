drop table if exists t1;
create table t1(a int not null auto_increment primary key) engine=innodb;
insert into t1 set a = -1;
optimize table t1;
Table	Op	Msg_type	Msg_text
test.t1	optimize	note	Table does not support optimize, doing recreate + analyze instead
test.t1	optimize	status	OK
==== clean up ====
DROP TABLE t1;
