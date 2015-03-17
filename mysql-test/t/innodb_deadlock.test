--echo #
--echo # Bug#20262654 INNODB: MYSQL IS TRYING TO PERFORM A CONSISTENT READ
--echo #              BUT THE READ VIEW IS NOT AS
--echo #

set sql_mode="";
create table t1(a int)engine=innodb;
create table t2(b int)engine=innodb;

delimiter $;
create procedure p1()
begin
  declare counter integer default 0;
  declare continue handler for sqlexception begin set counter = counter + 10;end;
  repeat
    if rand()>0.5 then start transaction; end if;
    if rand()>0.5 then 
      select var_samp(1), exists(select 1 from t1 lock in share mode) 
      from t1 into @a,@b; 
    end if;
    if rand()>0.5 then 
      select var_samp(1), exists(select 1 from t1 for update)
      from t1 into @a,@b; 
    end if;
    if rand()>0.5 then insert ignore into t1 values (); end if;
    if rand()>0.5 then insert ignore into t2 values (); end if;
    if rand()>0.5 then delete from t1; end if;
    if rand()>0.5 then delete from t2; end if;
    if rand()>0.5 then commit; end if;
    set counter = counter + 1;
  until counter >= 100 end repeat;
end $
delimiter ;$

--enable_connect_log
--connect(con1, localhost, root)
--disable_result_log
--send call p1();   # run this in two connections!
--connection default
call p1();
--connection con1
--echo reap
--reap
--echo reap done
--disconnect con1
--source include/wait_until_disconnected.inc 
--enable_result_log
--connection default
--disable_connect_log

drop procedure p1;
drop table t1,t2;

set sql_mode=default;
