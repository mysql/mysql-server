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
select count(*) from t1;
# purge should kick-in after delete.
delete from t1 where a < 500;
select count(*) from t1;
update t1 set b = 'innodb' where b = 'a';
update t1 set c = 'mysql' where c = 'b' and a < 1000;
begin;
update t1 set c = 'sqldb' where c = 'mysql' and a < 1000;
select count(*) from t1 where c = 'sqldb';
rollback;
#
drop procedure populate_t1;

