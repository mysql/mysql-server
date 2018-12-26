connection master;

--echo # CTE in Prepared statement with parameters

create table t1(c1 int,c2 int);
insert into t1 values(1,2),(3,4);
create table t2(a int, b int, c int, d int, e int);

--echo # Parameters in CTE which is referenced twice

prepare s from
'with qn as (select * from t1 where c1=?)
select * from qn, qn as qn1';

set @p_qn=1;
execute s using @p_qn;

--echo # Parameters in two CTEs which are referenced in FROM
--echo # in the opposite order of their WITH definitions.

prepare s from
'with qn  as (select * from t1 where c1=?),
      qn2 as (select * from t1 where c1=?)
select ? , qn.*, qn2.* from qn2 left join qn on 1';

--echo # Test binlogging, with INSERT SELECT, to see if the
--echo # parameter-substituted query is correct.

prepare si from
'insert into t2
with qn  as (select * from t1 where c1=?),
      qn2 as (select * from t1 where c1=?)
select ? , qn.*, qn2.* from qn2 left join qn on 1';

set @p_qn=1,@p_qn2=2,@p_select=10;
execute s using @p_qn,@p_qn2,@p_select;
--echo # We execute twice, as 2nd-exec bugs are common.
execute s using @p_qn,@p_qn2,@p_select;
execute si using @p_qn,@p_qn2,@p_select;
execute si using @p_qn,@p_qn2,@p_select;

select * from t2;
sync_slave_with_master;
connection slave;
--echo # On slave:
select * from t2;
--echo # Back to Master.
connection master;

set @p_qn=2,@p_qn2=1,@p_select=20;
execute s using @p_qn,@p_qn2,@p_select;
execute s using @p_qn,@p_qn2,@p_select;
execute si using @p_qn,@p_qn2,@p_select;
execute si using @p_qn,@p_qn2,@p_select;

select * from t2;
sync_slave_with_master;
--echo # On slave:
select * from t2;
--echo # Back to Master.
connection master;

drop table t1;

--echo # CTE in SP

delimiter |;

create table t1(a int)|
insert into t1 values(1),(2)|
create table t3(a char(16), b int)|

--echo # Materialized CTE

create procedure cur1()
begin
  declare a char(16);
  declare b int;
  declare done int default 0;
  declare c cursor for
   with qn(x,y) as (select * from t1, t1 t2 limit 4)
   select x,y from qn where y<3 order by x;
  declare continue handler for sqlstate '02000' set done = 1;

  open c;
  repeat
    fetch c into a, b;
    if not done then
       insert into test.t3 values (a,b);
    end if;
  until done end repeat;
  close c;
end|

call cur1()|
--sorted_result
select * from t3|
sync_slave_with_master|
connection slave|
--echo # On slave:
--sorted_result
select * from t3|
--echo # Back to Master.
connection master|

delete from t3|
# Test 2nd execution
call cur1()|
--sorted_result
select * from t3|
sync_slave_with_master|
connection slave|
--echo # On slave:
--sorted_result
select * from t3|
--echo # Back to Master.
connection master|
delete from t3|

drop procedure cur1|

--echo # Merged CTE

create procedure cur1()
begin
  declare a char(16);
  declare b int;
  declare done int default 0;
  declare c cursor for
   with qn(x,y) as (select * from t1, t1 t2)
   select x,y from qn where y<3 order by x;
  declare continue handler for sqlstate '02000' set done = 1;

  open c;
  repeat
    fetch c into a, b;
    if not done then
       insert into test.t3 values (a,b);
    end if;
  until done end repeat;
  close c;
end|

call cur1()|
--sorted_result
select * from t3|
sync_slave_with_master|
connection slave|
--echo # On slave:
--sorted_result
select * from t3|
--echo # Back to Master.
connection master|
delete from t3|

call cur1()|
--sorted_result
select * from t3|
sync_slave_with_master|
connection slave|
--echo # On slave:
--sorted_result
select * from t3|
--echo # Back to Master.
connection master|
delete from t3|
drop procedure cur1|

--echo # Recursive CTE
# Note the "select **t1.a**"; a mere "select a" would select the local
# variable!

create procedure cur1()
begin
  declare a char(16);
  declare b int;
  declare done int default 0;
  declare c cursor for
   with recursive qn(x) as (
   select t1.a from t1 union all select x*3 from qn where x<100)
   select "got ",x from qn;
  declare continue handler for sqlstate '02000' set done = 1;

  open c;
  repeat
    fetch c into a, b;
    if not done then
       insert into test.t3 values (a,b);
    end if;
  until done end repeat;
  close c;
end|

call cur1()|
select * from t3|
sync_slave_with_master|
connection slave|
--echo # On slave:
select * from t3|
--echo # Back to Master.
connection master|
delete from t3|

call cur1()|
select * from t3|
sync_slave_with_master|
connection slave|
--echo # On slave:
select * from t3|
--echo # Back to Master.
connection master|
delete from t3|
drop procedure cur1|

--echo # A stored function, returning the highest generated number
--echo # under a limit provided in input.
create function func1(M int) returns int
return (
  with recursive qn (n,x) as (
   select 1, t1.a from t1 union all
   select n+1, x*3 from qn where (x*3)<M)
  select max(x) from qn )|

select func1(100),func1(200)|
drop function func1|

--echo # A trigger, which when inserting "b" in t3,
--echo # sets "a" to the highest generated number under "b"; indeed a
--echo # WITH cannot be used in a generated column's expression (which
--echo # cannot contain a subquery), so a workaround is a trigger.

create trigger t3_bi before insert on t3 for each row
begin
  declare tmp int;
  set tmp = (
   with recursive qn (n,x) as (
     select 1, t1.a from t1 union all
     select n+1, x*3 from qn where (x*3)<new.b)
   select max(x) from qn );
  set new.a= tmp;
end|

delete from t3|
insert into t3 (b) values(300),(1000)|
select * from t3|
sync_slave_with_master|
connection slave|
--echo # On slave:
select * from t3|
--echo # Back to Master.
connection master|
delete from t3|

drop trigger t3_bi|

--echo # A local variable used in CTE which is referenced twice

insert into t1 values(3),(4)|

create procedure cur1()
begin
  declare a char(16);
  declare b int default 0;
  declare done int default 0;
  declare c cursor for
   with qn(x) as (select * from t1 where t1.a>b)
   select x*10 from qn union all
   select x*100 from qn;
  declare continue handler for sqlstate '02000' set done = 1;

  set b=2;
  open c;
  repeat
    fetch c into a;
    if not done then
       insert into test.t3 values (a,1);
    end if;
  until done end repeat;
  close c;
  set b=3;
  set done=0;
  open c;
  repeat
    fetch c into a;
    if not done then
       insert into test.t3 values (a,2);
    end if;
  until done end repeat;
  close c;
end|

call cur1()|
select * from t3|
sync_slave_with_master|
connection slave|
--echo # On slave:
select * from t3|
--echo # Back to Master.
connection master|
delete from t3|
drop procedure cur1|

--echo # Test binlogging, with INSERT SELECT, to see if the
--echo # parameter-substituted query is correct.

create procedure cur1()
begin
  declare a char(16);
  declare b int default 0;
  set b=2;
  insert into t3
   with qn(x) as (select * from t1 where t1.a>b)
   select x*10,1 from qn union all
   select x*100,2 from qn;
end|

call cur1()|
select * from t3|
sync_slave_with_master|
connection slave|
--echo # On slave:
select * from t3|
--echo # Back to Master.
connection master|
delete from t3|
drop procedure cur1|

--echo # Two local variables in two CTEs which are referenced in FROM
--echo # in the opposite order of their WITH definitions.

create procedure cur1()
begin
  declare a char(16);
  declare b int default 0;
  declare done int default 0;
  declare c cursor for
   with qn(x)  as (select t1.a from t1 where t1.a=b),
        qn1(x) as (select t1.a*10 from t1 where t1.a=a)
   select qn1.x-qn.x from qn1, qn;
  declare continue handler for sqlstate '02000' set done = 1;

  set a=2,b=3;
  open c;
  repeat
    fetch c into a;
    if not done then
       insert into test.t3 values (a,1);
    end if;
  until done end repeat;
  close c;
end|

call cur1()|
select * from t3|
sync_slave_with_master|
connection slave|
--echo # On slave:
select * from t3|
--echo # Back to Master.
connection master|
delete from t3|

call cur1()|
select * from t3|
sync_slave_with_master|
connection slave|
--echo # On slave:
select * from t3|
--echo # Back to Master.
connection master|
delete from t3|
drop procedure cur1|

--echo # Similar, with INSERT SELECT, to test binlogging

create procedure cur1()
begin
  declare a char(16);
  declare b int default 0;
  set b=3,a=2;
  insert into t3(a)
   with qn(x)  as (select t1.a from t1 where t1.a=b),
        qn1(x) as (select t1.a*10 from t1 where t1.a=a)
   select qn1.x-qn.x from qn1, qn;
end|

call cur1()|
select * from t3|
sync_slave_with_master|
connection slave|
--echo # On slave:
select * from t3|
--echo # Back to Master.
connection master|
delete from t3|

call cur1()|
select * from t3|
sync_slave_with_master|
connection slave|
--echo # On slave:
select * from t3|
--echo # Back to Master.
connection master|
delete from t3|
drop procedure cur1|

drop table t1,t2,t3|

delimiter ;|
