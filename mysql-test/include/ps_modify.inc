###################### ps_modify.inc #########################
#                                                            #
#  Tests for prepared statements: INSERT/DELETE/UPDATE...    #
#                                                            #
##############################################################

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
#     t1  - very simple table
#     t9  - table with nearly all available column types
#
# The structure and the content of these tables can be found in
#     include/ps_create.inc  CREATE TABLE ...
#     include/ps_renew.inc   DELETE all rows and INSERT some rows
#
# Both tables are managed by the same storage engine.
# The type of the storage engine is stored in the variable '$type' . 



#------------------- Please insert your test cases here -------------------#



#-------- Please be very carefull when editing behind this line  ----------#

--disable_query_log
select '------ delete tests ------' as test_sequence ;
--enable_query_log
--source include/ps_renew.inc

## delete without parameter
prepare stmt1 from 'delete from t1 where a=2' ;
execute stmt1;
select a,b from t1 where a=2;
# delete with row not found
execute stmt1;

## delete with one parameter in the where clause
insert into t1 values(0,NULL);
set @arg00=NULL;
prepare stmt1 from 'delete from t1 where b=?' ;
execute stmt1 using @arg00;
select a,b from t1 where b is NULL ;
set @arg00='one';
execute stmt1 using @arg00;
select a,b from t1 where b=@arg00;

## truncate a table
prepare stmt1 from 'truncate table t1' ;


--disable_query_log
select '------ update tests ------' as test_sequence ;
--enable_query_log
--source include/ps_renew.inc

## update without parameter
prepare stmt1 from 'update t1 set b=''a=two'' where a=2' ;
execute stmt1;
select a,b from t1 where a=2;
# dummy update
execute stmt1;
select a,b from t1 where a=2;

## update with one parameter in the set clause
set @arg00=NULL;
prepare stmt1 from 'update t1 set b=? where a=2' ;
execute stmt1 using @arg00;
select a,b from t1 where a=2;
set @arg00='two';
execute stmt1 using @arg00;
select a,b from t1 where a=2;

## update with one parameter in the where cause
set @arg00=2;
prepare stmt1 from 'update t1 set b=NULL where a=?' ;
execute stmt1 using @arg00;
select a,b from t1 where a=@arg00;
update t1 set b='two' where a=@arg00;
# row not found in update
set @arg00=2000;
execute stmt1 using @arg00;
select a,b from t1 where a=@arg00;

## update on primary key column (two parameters)
set @arg00=2;
set @arg01=22;
prepare stmt1 from 'update t1 set a=? where a=?' ;
# dummy update
execute stmt1 using @arg00, @arg00;
select a,b from t1 where a=@arg00;
execute stmt1 using @arg01, @arg00;
select a,b from t1 where a=@arg01;
execute stmt1 using @arg00, @arg01;
select a,b from t1 where a=@arg00;
set @arg00=NULL;
set @arg01=2;
execute stmt1 using @arg00, @arg01;
select a,b from t1 order by a;
set @arg00=0;
execute stmt1 using @arg01, @arg00;
select a,b from t1 order by a;

## update with subquery and several parameters
set @arg00=23;
set @arg01='two';
set @arg02=2;
set @arg03='two';
set @arg04=2;
--disable_warnings
drop table if exists t2;
--enable_warnings
# t2 will be of table type 'MYISAM'
create table t2 as select a,b from t1 ;
prepare stmt1 from 'update t1 set a=? where b=?
                    and a in (select ? from t2
                              where b = ? or a = ?)';
--enable_info
execute stmt1 using @arg00, @arg01, @arg02, @arg03, @arg04 ;
--disable_info
select a,b from t1 where a = @arg00 ;
prepare stmt1 from 'update t1 set a=? where b=?
                    and a not in (select ? from t2
                              where b = ? or a = ?)';
--enable_info
execute stmt1 using @arg04, @arg01, @arg02, @arg03, @arg00 ;
--disable_info
select a,b from t1 order by a ;
drop table t2 ;
# t2 is now of table type '$type'
# The test battery for table type 'MERGE' gets here only a 'MYISAM' table
#
# Test UPDATE with SUBQUERY in prepared mode
#
eval create table t2
(
  a int, b varchar(30),
  primary key(a)
) engine = $type ;
insert into t2(a,b) select a, b from t1 ;
prepare stmt1 from 'update t1 set a=? where b=?
                    and a in (select ? from t2
                              where b = ? or a = ?)';
--enable_info
execute stmt1 using @arg00, @arg01, @arg02, @arg03, @arg04 ;
--disable_info
select a,b from t1 where a = @arg00 ;
prepare stmt1 from 'update t1 set a=? where b=?
                    and a not in (select ? from t2
                              where b = ? or a = ?)';
--enable_info
execute stmt1 using @arg04, @arg01, @arg02, @arg03, @arg00 ;
--disable_info
select a,b from t1 order by a ;
drop table t2 ;

## update with parameters in limit
set @arg00=1;
prepare stmt1 from 'update t1 set b=''bla''
where a=2
limit 1';
execute stmt1 ;
select a,b from t1 where b = 'bla' ;
# currently (May 2004, Version 4.1) it is impossible
-- error 1064
prepare stmt1 from 'update t1 set b=''bla''
where a=2
limit ?';

--disable_query_log
select '------ insert tests ------' as test_sequence ;
--enable_query_log
--source include/ps_renew.inc

## insert without parameter
prepare stmt1 from 'insert into t1 values(5, ''five'' )';
execute stmt1;
select a,b from t1 where a = 5;

## insert with one parameter in values part
set @arg00='six' ;
prepare stmt1 from 'insert into t1 values(6, ? )';
execute stmt1 using @arg00;
select a,b from t1 where b = @arg00;
# the second insert fails, because the first column is primary key
--error 1062
execute stmt1 using @arg00;
set @arg00=NULL ;
prepare stmt1 from 'insert into t1 values(0, ? )';
execute stmt1 using @arg00;
select a,b from t1 where b is NULL;

## insert with two parameter in values part
set @arg00=8 ;
set @arg01='eight' ;
prepare stmt1 from 'insert into t1 values(?, ? )';
execute stmt1 using @arg00, @arg01 ;
select a,b from t1 where b = @arg01;
# cases derived from client_test.c: test_null()
set @NULL= null ;
set @arg00= 'abc' ;
# execute must fail, because first column is primary key (-> not null)
--error 1048
execute stmt1 using @NULL, @NULL ;
--error 1048
execute stmt1 using @NULL, @NULL ;
--error 1048
execute stmt1 using @NULL, @arg00 ;
--error 1048
execute stmt1 using @NULL, @arg00 ;
let $1 = 2;
while ($1)
{
  eval set @arg01= 10000 + $1 ;
  execute stmt1 using @arg01, @arg00 ;
  dec $1;
}
select * from t1 where a > 10000 order by a ;
delete from t1 where a > 10000 ;
let $1 = 2;
while ($1)
{
  eval set @arg01= 10000 + $1 ;
  execute stmt1 using @arg01, @NULL ;
  dec $1;
}
select * from t1 where a > 10000 order by a ;
delete from t1 where a > 10000 ;
let $1 = 10;
while ($1)
{
  eval set @arg01= 10000 + $1 ;
  execute stmt1 using @arg01, @arg01 ;
  dec $1;
}
select * from t1 where a > 10000 order by a ;
delete from t1 where a > 10000 ;


## insert with two rows in values part
set @arg00=81 ;
set @arg01='8-1' ;
set @arg02=82 ;
set @arg03='8-2' ;
prepare stmt1 from 'insert into t1 values(?,?),(?,?)';
execute stmt1 using @arg00, @arg01, @arg02, @arg03 ;
select a,b from t1 where a in (@arg00,@arg02) ;

## insert with two parameter in the set part
set @arg00=9 ;
set @arg01='nine' ;
prepare stmt1 from 'insert into t1 set a=?, b=? ';
execute stmt1 using @arg00, @arg01 ;
select a,b from t1 where a = @arg00 ;

## insert with parameters in the ON DUPLICATE KEY part 
set @arg00=6 ;
set @arg01=1 ;
prepare stmt1 from 'insert into t1 set a=?, b=''sechs''
                    on duplicate key update a=a + ?, b=concat(b,''modified'') ';
execute stmt1 using @arg00, @arg01;
select * from t1 order by a;
set @arg00=81 ;
set @arg01=1 ;
--error 1062
execute stmt1 using @arg00, @arg01;

## insert, autoincrement column and ' SELECT LAST_INSERT_ID() '
# cases derived from client_test.c: test_bug3117()
--disable_warnings
drop table if exists t2 ;
--enable_warnings
# The test battery for table type 'MERGE' gets here only a 'MYISAM' table
eval create table t2 (id int auto_increment primary key) 
ENGINE= $type ;
prepare stmt1 from ' select last_insert_id() ' ;
insert into t2 values (NULL) ;
execute stmt1 ;
insert into t2 values (NULL) ;
# bug#3117
execute stmt1 ;
drop table t2 ;

## many parameters
set @1000=1000 ;
set @x1000_2="x1000_2" ;
set @x1000_3="x1000_3" ;

set @x1000="x1000" ;
set @1100=1100 ;
set @x1100="x1100" ;
set @100=100 ;
set @updated="updated" ;
insert into t1 values(1000,'x1000_1') ;
insert into t1 values(@1000,@x1000_2),(@1000,@x1000_3)
               on duplicate key update a = a + @100, b = concat(b,@updated) ;
select a,b from t1 where a >= 1000 order by a ;
delete from t1 where a >= 1000 ;
insert into t1 values(1000,'x1000_1') ;
prepare stmt1 from ' insert into t1 values(?,?),(?,?)
               on duplicate key update a = a + ?, b = concat(b,?) ';
execute stmt1 using @1000, @x1000_2, @1000, @x1000_3, @100, @updated ;
select a,b from t1 where a >= 1000 order by a ;
delete from t1 where a >= 1000 ;
insert into t1 values(1000,'x1000_1') ;
execute stmt1 using @1000, @x1000_2, @1100, @x1000_3, @100, @updated ;
select a,b from t1 where a >= 1000 order by a ;
delete from t1 where a >= 1000 ;

## replace
--error 1295
prepare stmt1 from ' replace into t1 (a,b) select 100, ''hundred'' ';


## multi table statements
--disable_query_log
select '------ multi table tests ------' as test_sequence ;
--enable_query_log
# cases derived from client_test.c: test_multi
delete from t1 ;
delete from t9 ;
insert into t1(a,b) values (1, 'one'), (2, 'two'), (3, 'three') ;
insert into t9 (c1,c21)
  values (1, 'one'), (2, 'two'), (3, 'three') ;
prepare stmt_delete from " delete t1, t9 
  from t1, t9 where t1.a=t9.c1 and t1.b='updated' ";
prepare stmt_update from " update t1, t9 
  set t1.b='updated', t9.c21='updated'
  where t1.a=t9.c1 and t1.a=? ";
prepare stmt_select1 from " select a, b from t1 order by a" ;
prepare stmt_select2 from " select c1, c21 from t9 order by c1" ;
set @arg00= 1 ;
let $1= 3 ;
while ($1)
{
  execute stmt_update using @arg00 ;
  execute stmt_delete ;
  execute stmt_select1 ;
  execute stmt_select2 ;
  set @arg00= @arg00 + 1 ;
  dec $1 ;
}

