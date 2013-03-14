#
# test loading of table with different action that
# cause commit/rollback/purge (mainly involving undo-logs)
#
delimiter |;
create procedure populate_t1()
begin
	declare i int default 1;
	while (i <= 2000) DO
		insert into t1 values (i, 'a', 'b');
		set i = i + 1;
	end while;
end|
delimiter ;|
begin;
call populate_t1();
commit;
select count(*) from t1;
begin;
call populate_t1();
rollback;
select count(*) from t1;
truncate table t1;
select count(*) from t1;
begin;
call populate_t1();
commit;
# post this stmt purge should kick in.
delete from t1 where i < 500;
update t1 set b = 'innodb' where b = 'a'; 
update t1 set c = 'mysql' where c = 'b' and i < 1000;
update t1 set c = 'sqldb' where c = 'b' and i < 1000; 
select count(*) from t1;
select count(*) from t1 where c = 'sqldb';
#
drop procedure populate_t1;

