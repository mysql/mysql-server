###################### ps_modify1.inc ########################
#                                                            #
#  Tests for prepared statements: big INSERT .. SELECTs      #
#                                                            #
##############################################################

## big insert select statements
set @duplicate='duplicate ' ;
set @1000=1000 ;
set @5=5 ;
select a,b from t1 where a < 5 ;
--enable_info
insert into t1 select a + @1000, concat(@duplicate,b) from t1
where a < @5 ;
--disable_info
select a,b from t1 where a >= 1000 ;
delete from t1 where a >= 1000 ;
prepare stmt1 from ' insert into t1 select a + ?, concat(?,b) from t1
where a < ? ' ;
--enable_info
execute stmt1 using @1000, @duplicate, @5;
--disable_info
select a,b from t1 where a >= 1000 ;
delete from t1 where a >= 1000 ;

set @float=1.00;
set @five='five' ;
--disable_warnings
drop table if exists t2;
--enable_warnings
create table t2 like t1 ;
--enable_info
insert into t2 (b,a) 
select @duplicate, sum(first.a) from t1 first, t1 second
  where first.a <> @5 and second.b = first.b
     and second.b <> @five
  group by second.b
  having sum(second.a) > @2
union
select b, a + @100 from t1
  where (a,b) in ( select sqrt(a+@1)+CAST(@float AS signed),b 
                 from t1);
--disable_info
select a,b from t2;
delete from t2 ;
prepare stmt1 from ' insert into t2 (b,a) 
select ?, sum(first.a)
  from t1 first, t1 second 
  where first.a <> ? and second.b = first.b and second.b <> ?
  group by second.b
  having sum(second.a) > ?
union
select b, a + ? from t1
  where (a,b) in ( select sqrt(a+?)+CAST(? AS signed),b 
                 from t1 ) ' ;
--enable_info
execute stmt1 using @duplicate, @5, @five, @2, @100, @1, @float ;
--disable_info
select a,b from t2;
drop table t2;
