create table t2 (a char(1));
insert into t2 values ('0'),('1'),('2'),('3'),('4'),('5'),('6'),('7');
insert into t2 values ('8'),('9'),('A'),('B'),('C'),('D'),('E'),('F');
create table t1 (
a varchar(2) character set cp932
) engine=myisam;
insert into t1
select unhex(concat(t24.a, t23.a, t22.a, t21.a))
from t2 t21, t2 t22, t2 t23, t2 t24;
delete from t1 where a='';
alter table t1 add key(a);
check table t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
drop table t1;
drop table t2;
