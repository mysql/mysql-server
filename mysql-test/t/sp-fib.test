# Fibonacci, for recursion test. (Yet Another Numerical series :)
# Split from main.sp due to problems reported in Bug#15866

--disable_warnings
drop table if exists t3;
--enable_warnings
create table t3 ( f bigint unsigned not null );

# We deliberately do it the awkward way, fetching the last two
# values from the table, in order to exercise various statements
# and table accesses at each turn.
--disable_warnings
drop procedure if exists fib;
--enable_warnings

# Now for multiple statements...
delimiter |;

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

# Enable recursion
set @@max_sp_recursion_depth= 20|

insert into t3 values (0), (1)|

# The small number of recursion levels is intentional.
# We need to avoid
# Bug#15866 main.sp fails (thread stack limit
#           insufficient for recursive call "fib(20)")
# which affects some platforms.
call fib(4)|

select * from t3 order by f asc|

drop table t3|
drop procedure fib|
set @@max_sp_recursion_depth= 0|

