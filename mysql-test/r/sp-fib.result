drop table if exists t3;
create table t3 ( f bigint unsigned not null );
drop procedure if exists fib;
create procedure fib(n int unsigned)
begin
if n > 1 then
begin
declare x, y bigint unsigned;
declare c cursor for select f from t3 order by f desc limit 2;
open c;
fetch c into y;
fetch c into x;
insert into t3 values (x+y);
call fib(n-1);
## Close the cursor AFTER the recursion to ensure that the stack
## frame is somewhat intact.
close c;
end;
end if;
end|
set @@max_sp_recursion_depth= 20|
insert into t3 values (0), (1)|
call fib(4)|
select * from t3 order by f asc|
f
0
1
1
2
3
drop table t3|
drop procedure fib|
set @@max_sp_recursion_depth= 0|
