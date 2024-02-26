delimiter |;
create procedure populate_t1()
begin
	declare i int default 1;
	while (i <= 200) do
		insert into t1 values (i, 'a', 'b');
		set i = i + 1;
	end while;
end|
create procedure populate_t1_small()
begin
	declare i int default 1;
	while (i <= 20) do
		insert into t1 values (i, 'c', 'd');
		set i = i + 1;
	end while;
end|
create procedure populate_t1_small2()
begin
	declare i int default 30;
	while (i <= 50) do
		insert into t1 values (i, 'e', 'f');
		set i = i + 1;
	end while;
end|
delimiter ;|
#
begin;
select count(*) from t1;
call populate_t1();
select count(*) from t1;
select * from t1 limit 10;
rollback;
select count(*) from t1;
#
begin;
call populate_t1();
select count(*) from t1;
commit;
select count(*) from t1;
#
truncate table t1;
select count(*) from t1;
#
call populate_t1_small();
select count(*) from t1;
rollback;
select count(*) from t1;
truncate table t1;
#
call populate_t1();
select count(*) from t1;
delete from t1 where keyc <= 60;
select count(*) from t1;
call populate_t1_small();
select count(*) from t1;
select * from t1 limit 10;
begin;
call populate_t1_small2();
select count(*) from t1;
select * from t1 where keyc > 30 limit 10;
rollback;
select count(*) from t1;
select * from t1 where keyc > 30 limit 10;
#
update t1 set keyc = keyc + 2000;
select * from t1 limit 10;
rollback;
begin;
update t1 set keyc = keyc + 2000;
select * from t1 limit 10;
rollback;
select * from t1 limit 10;
commit;
select * from t1 limit 10;
#
insert into t2 select * from t1 where keyc < 2101;
select count(*) from t2;
#
drop procedure populate_t1;
drop procedure populate_t1_small;
drop procedure populate_t1_small2;
