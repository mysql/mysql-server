###################### ps_modify1.inc ########################
#                                                            #
#  Tests for prepared statements: big INSERT .. SELECTs      #
#                                                            #
##############################################################

#
# NOTE: THESE TESTS CANNOT BE APPLIED TO TABLES OF TYPE MERGE.
#       Test which can be applied to MERGE tables should be stored in
#            include/ps_modify.inc .
#

#    
# NOTE: PLEASE SEE ps_1general.test (bottom) 
#       BEFORE ADDING NEW TEST CASES HERE !!!

#
# Please be aware, that this file will be sourced by several test case files
# stored within the subdirectory 't'. So every change here will affect 
# several test cases.
#
# Please do not modify the structure (DROP/ALTER..) of the tables
#     't1' and 't9'. 
#
# But you are encouraged to use these two tables within your statements
# (DELETE/UPDATE/...) whenever possible. 
#     t1   - very simple table
#     t9   - table with nearly all available column types
#
# The structure and the content of these tables can be found in
#     include/ps_create.inc  CREATE TABLE ...
#     include/ps_renew.inc   DELETE all rows and INSERT some rows
#
# Both tables are managed by the same storage engine.
# The type of the storage engine is stored in the variable '$type' . 



#------------------- Please insert your test cases here -------------------#


#-------- Please be very carefull when editing behind this line  ----------#

--source include/ps_renew.inc
#
# add a NULL row to t1: this row is used only in this test
insert into t1 values(0,NULL) ;

## big insert select statements
set @duplicate='duplicate ' ;
set @1000=1000 ;
set @5=5 ;
select a,b from t1 where a < 5 order by a ;
--enable_info
insert into t1 select a + @1000, concat(@duplicate,b) from t1
where a < @5 ;
--disable_info
select a,b from t1 where a >= 1000 order by a ;
delete from t1 where a >= 1000 ;
prepare stmt1 from ' insert into t1 select a + ?, concat(?,b) from t1
where a < ? ' ;
--enable_info
execute stmt1 using @1000, @duplicate, @5;
--disable_info
select a,b from t1 where a >= 1000 order by a ;
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
select a,b from t2 order by a ;
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
select a,b from t2 order by a ;
drop table t2;
