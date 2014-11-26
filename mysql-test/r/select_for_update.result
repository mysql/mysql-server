#
# Bug#19142753 ASSERT: MODE !=LOCK_X || LOCK_TABLE_HAS(THR_GET_TRX(THR),
#              INDEX->TABLE, LOCK_IX)
#
create table t1(a int, key(a)) engine=innodb;
insert into t1 values (0);
create procedure p1()
begin
declare counter integer default 0;
declare continue handler for sqlexception begin set counter = counter + 1;end;
repeat
if rand()>0.5 then start transaction; end if;
if rand()>0.5 then select count(*) from t1 for update; end if;
update t1 set a = 1 where a >= 0;
set counter = counter + 1;
until counter >= 50 end repeat;
end $
connect con1, localhost, root;
call p1();   # run this in two connections!;
connection default;
call p1();
connection con1;
reap
reap done
disconnect con1;
connection default;
drop procedure p1;
drop table t1;
