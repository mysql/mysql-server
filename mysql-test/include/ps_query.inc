####################### ps_query.inc #########################
#                                                            #
#       Tests for prepared statements: SELECTs               #
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
# Please do not modify (INSERT/UPDATE/DELETE) the content or the 
# structure (DROP/ALTER..) of the tables
#     't1' and 't_many_col_types'. 
# Such tests should be done in include/ps_modify.inc .
#
# But you are encouraged to use these two tables within your SELECT statements
# whenever possible. 
#     t1               - very simple table
#     t_many_col_types - table with nearly all available column types
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
select '------ simple select tests ------' as test_sequence ;
--enable_query_log

##### parameter used for keyword like SELECT (must fail)
set @arg00='SELECT' ;
# mysqltest gives no output for the next statement, Why ??
--error 1064
@arg00 a from t1 where a=1;
--error 1064
prepare stmt1 from ' ? a from t1 where a=1 ';

##### parameter in select column list
## parameter is not NULL
set @arg00=1 ;
select @arg00, b from t1 where a=1 ;
prepare stmt1 from ' select ?, b from t1 where a=1 ' ;
execute stmt1 using @arg00 ;
set @arg00='lion' ;
select @arg00, b from t1 where a=1 ;
prepare stmt1 from ' select ?, b from t1 where a=1 ' ;
execute stmt1 using @arg00 ;
## parameter is NULL
set @arg00=NULL ;
select @arg00, b from t1 where a=1 ;
prepare stmt1 from ' select ?, b from t1 where a=1 ' ;
execute stmt1 using @arg00 ;
## parameter within an expression
set @arg00=1 ;
select b, a - @arg00 from t1 where a=1 ;
prepare stmt1 from ' select b, a - ? from t1 where a=1 ' ;
execute stmt1 using @arg00 ;
## parameter is within a function
# variations on 'substr'
set @arg00='MySQL' ;
select substr(@arg00,1,2) from t1 where a=1 ;
prepare stmt1 from ' select substr(?,1,2) from t1 where a=1 ' ;
execute stmt1 using @arg00 ;
set @arg00=3 ;
select substr('MySQL',@arg00,5) from t1 where a=1 ;
prepare stmt1 from ' select substr(''MySQL'',?,5) from t1 where a=1 ' ;
execute stmt1 using @arg00 ;
select substr('MySQL',1,@arg00) from t1 where a=1 ;
prepare stmt1 from ' select substr(''MySQL'',1,?) from t1 where a=1 ' ;
execute stmt1 using @arg00 ;
# variations on 'concat'
set @arg00='MySQL' ;
select a , concat(@arg00,b) from t1 ;
# BUG#3796
prepare stmt1 from ' select a , concat(?,b) from t1 ' ;
execute stmt1 using @arg00;
#
select a , concat(b,@arg00) from t1 ;
prepare stmt1 from ' select a , concat(b,?) from t1 ' ;
execute stmt1 using @arg00;

# variations on 'group_concat'
set @arg00='MySQL' ;
select group_concat(@arg00,b) from t1 
group by 'a' ;
prepare stmt1 from ' select group_concat(?,b) from t1
group by ''a'' ' ;
execute stmt1 using @arg00;
#
select group_concat(b,@arg00) from t1 
group by 'a' ;
prepare stmt1 from ' select group_concat(b,?) from t1
group by ''a'' ' ;
execute stmt1 using @arg00;

## two parameters
set @arg00='first' ;
set @arg01='second' ;
set @arg02=NULL;
select @arg00, @arg01 from t1 where a=1 ;
prepare stmt1 from ' select ?, ? from t1 where a=1 ' ;
execute stmt1 using @arg00, @arg01 ;
# NULL as first and/or last parameter 
execute stmt1 using @arg02, @arg01 ;
execute stmt1 using @arg00, @arg02 ;
execute stmt1 using @arg02, @arg02 ;
# case derived from client_test.c: test_ps_conj_select()
#      for BUG#3420: select returned all rows of the table
--disable_warnings
drop table if exists new_tab ;
--enable_warnings
create table new_tab (id1 int(11) not null default '0',
       value2 varchar(100), value1 varchar(100)) ;
insert into new_tab values (1,'hh','hh'),(2,'hh','hh'),
                  (1,'ii','ii'),(2,'ii','ii') ;
prepare stmt1 from ' select id1,value1 from new_tab where id1=? or value1=? ' ;
set @arg00=1 ;
set @arg01='hh' ;
execute stmt1 using @arg00, @arg01 ;
drop table new_tab ;
# case derived from client_test.c: test_bug1180()
#      for BUG#1180 optimized away part of WHERE clause
--disable_warnings
drop table if exists new_tab ;
--enable_warnings
create table new_tab(session_id  char(9) not null) ;
insert into new_tab values ('abc') ;
prepare stmt1 from ' select * from new_tab
where ?=''1111'' and session_id = ''abc'' ' ;
set @arg00='abc' ;
execute stmt1 using @arg00 ;
set @arg00='1111' ;
execute stmt1 using @arg00 ;
set @arg00='abc' ;
execute stmt1 using @arg00 ;
drop table new_tab ;


##### parameter used for keyword FROM (must fail)
set @arg00='FROM' ;
--error 1064
select a @arg00 t1 where a=1 ;
--error 1064
prepare stmt1 from ' select a ? t1 where a=1 ' ;
##### parameter used for tablename (must fail)
set @arg00='t1' ;
--error 1064
select a from @arg00 where a=1 ;
--error 1064
prepare stmt1 from ' select a from ? where a=1 ' ;
##### parameter used for keyword WHERE tablename (must fail)
set @arg00='WHERE' ;
--error 1064
select a from t1 @arg00 a=1 ;
--error 1064
prepare stmt1 from ' select a from t1 ? a=1 ' ;

##### parameter used in where clause
# parameter is not NULL
set @arg00=1 ;
select a FROM t1 where a=@arg00 ;
prepare stmt1 from ' select a FROM t1 where a=? ' ;
execute stmt1 using @arg00 ;
set @arg00=1000 ;
# row not found
execute stmt1 using @arg00 ;
# parameter is NULL
set @arg00=NULL ;
select a FROM t1 where a=@arg00 ;
prepare stmt1 from ' select a FROM t1 where a=? ' ;
execute stmt1 using @arg00 ;
# parameter is not NULL within a function
set @arg00=4 ;
select a FROM t1 where a=sqrt(@arg00) ;
prepare stmt1 from ' select a FROM t1 where a=sqrt(?) ' ;
execute stmt1 using @arg00 ;
# parameter is NULL within a function
set @arg00=NULL ;
select a FROM t1 where a=sqrt(@arg00) ;
prepare stmt1 from ' select a FROM t1 where a=sqrt(?) ' ;
execute stmt1 using @arg00 ;
# parameter in IN
set @arg00=2 ;
set @arg01=3 ;
select a FROM t1 where a in (@arg00,@arg01);
prepare stmt1 from ' select a FROM t1 where a in (?,?) ';
execute stmt1 using @arg00, @arg01;
# parameter in LIKE
prepare stmt1 from ' select b FROM t1 where b like ? ';
set @arg00='two' ;
execute stmt1 using @arg00 ;
set @arg00='tw%' ;
execute stmt1 using @arg00 ;
set @arg00='%wo' ;
execute stmt1 using @arg00 ;

##### parameter used for operator in WHERE clause (must fail)
set @arg00='>' ;
--error 1064
select a FROM t1 where a @arg00 1 ;
--error 1064
prepare stmt1 from ' select a FROM t1 where a ? 1 ' ;

##### parameter used in group by clause
set @arg00=1 ;
select a,b FROM t1 where a is not NULL
AND b is not NULL group by a - @arg00 ;
prepare stmt1 from ' select a,b FROM t1 where a is not NULL
AND b is not NULL group by a - ? ' ;
execute stmt1 using @arg00 ;

##### parameter used in having clause
set @arg00='two' ;
select a,b FROM t1 where a is not NULL
AND b is not NULL having b <> @arg00 ;
prepare stmt1 from ' select a,b FROM t1 where a is not NULL
AND b is not NULL having b <> ? ' ;
execute stmt1 using @arg00 ;

##### parameter used in order clause
set @arg00=1 ;
select a,b FROM t1 where a is not NULL
AND b is not NULL order by a - @arg00 ;
prepare stmt1 from ' select a,b FROM t1 where a is not NULL
AND b is not NULL order by a - ? ' ;
execute stmt1 using @arg00 ;
## What is the semantic of a single parameter (integer >0)
#  after order by? column number or constant
set @arg00=2 ;
select a,b from t1 order by 2 ;
prepare stmt1 from ' select a,b from t1
order by ? ';
execute stmt1 using @arg00;
set @arg00=1 ;
execute stmt1 using @arg00;
set @arg00=0 ;
--error 1054
execute stmt1 using @arg00;

##### parameter used in limit clause
set @arg00=1;
prepare stmt1 from ' select a,b from t1
limit 1 ';
execute stmt1 ;
# currently (May 2004, Version 4.1) it is impossible
-- error 1064
prepare stmt1 from ' select a,b from t1
limit ? ';

##### parameter used in many places
set @arg00='b' ;
set @arg01=0 ;
set @arg02=2 ;
set @arg03=2 ;
select sum(a), @arg00 from t1 where a > @arg01
and b is not null group by substr(b,@arg02)
having sum(a) <> @arg03 ;
prepare stmt1 from ' select sum(a), ? from t1 where a > ?
and b is not null group by substr(b,?)
having sum(a) <> ? ';
execute stmt1 using @arg00, @arg01, @arg02, @arg03;


--disable_query_log
select '------ join tests ------' as test_sequence ;
--enable_query_log

# no parameter
select first.a as a1, second.a as a2 
from t1 first, t1 second
where first.a = second.a ;
prepare stmt1 from ' select first.a as a1, second.a as a2 
        from t1 first, t1 second
        where first.a = second.a ';
execute stmt1 ;

# some parameters
set @arg00='ABC';
set @arg01='two';
set @arg02='one';
select first.a, @arg00, second.a FROM t1 first, t1 second
where @arg01 = first.b or first.a = second.a or second.b = @arg02
order by second.a, first.a;
prepare stmt1 from ' select first.a, ?, second.a FROM t1 first, t1 second
                    where ? = first.b or first.a = second.a or second.b = ?
                    order by second.a, first.a';
execute stmt1 using @arg00, @arg01, @arg02;



--disable_query_log
select '------ subquery tests ------' as test_sequence ;
--enable_query_log

# no parameter
prepare stmt1 from ' select a, b FROM t1 outer_table where
   a = (select a from t1 where b = ''two'') ';
execute stmt1 ;

###### parameter in the outer part
set @arg00='two' ;
select a, b FROM t1 outer_table where
   a = (select a from t1 where b = 'two' ) and b=@arg00 ;
prepare stmt1 from ' select a, b FROM t1 outer_table where
   a = (select a from t1 where b = ''two'') and b=? ';
execute stmt1 using @arg00;
###### parameter in the inner part
set @arg00='two' ;
# Bug#4000 (only BDB tables)
select a, b FROM t1 outer_table where
   a = (select a from t1 where b = @arg00 ) and b='two' ;
prepare stmt1 from ' select a, b FROM t1 outer_table where
   a = (select a from t1 where b = ? ) and b=''two'' ' ;
execute stmt1 using @arg00;
set @arg00=3 ;
set @arg01='three' ;
select a,b FROM t1 where (a,b) in (select 3, 'three');
select a FROM t1 where (a,b) in (select @arg00,@arg01);
prepare stmt1 from ' select a FROM t1 where (a,b) in (select ?, ?) ';
execute stmt1 using @arg00, @arg01;

###### parameters in the both parts
set @arg00=1 ;
set @arg01='two' ;
set @arg02=2 ;
set @arg03='two' ;
# Bug#4000 (only BDB tables)
select a, @arg00, b FROM t1 outer_table where
   b=@arg01 and a = (select @arg02 from t1 where b = @arg03 ) ;
prepare stmt1 from ' select a, ?, b FROM t1 outer_table where
   b=? and a = (select ? from t1 where b = ? ) ' ;
execute stmt1 using @arg00, @arg01, @arg02, @arg03 ;

######## correlated subquery
# no parameter
prepare stmt1 from ' select a, b FROM t1 outer_table where
   a = (select a from t1 where b = outer_table.b ) ';
# also Bug#4000 (only BDB tables) ??
execute stmt1 ;

###### parameter in the outer part
set @arg00='two' ;
# Bug#4000 (only BDB tables)
select a, b FROM t1 outer_table where
   a = (select a from t1 where b = outer_table.b ) and b=@arg00 ;
prepare stmt1 from ' select a, b FROM t1 outer_table where
   a = (select a from t1 where b = outer_table.b) and b=? ';
# also Bug#4000 (only BDB tables) ??
execute stmt1 using @arg00;

###### parameter in the inner part
set @arg00=2 ;
select a, b FROM t1 outer_table where
   a = (select a from t1 where a = @arg00 and b = outer_table.b) and b='two' ;
prepare stmt1 from ' select a, b FROM t1 outer_table where
   a = (select a from t1 where a = ? and b = outer_table.b) and b=''two'' ' ;
execute stmt1 using @arg00;

set @arg00=2 ;
select a, b FROM t1 outer_table where
   a = (select a from t1 where outer_table.a = @arg00 and a=2) and b='two' ;
prepare stmt1 from ' select a, b FROM t1 outer_table where
   a = (select a from t1 where outer_table.a = ? and a=2) and b=''two'' ' ;
execute stmt1 using @arg00;

###### parameters in the both parts
set @arg00=1 ;
set @arg01='two' ;
set @arg02=2 ;
set @arg03='two' ;
# Bug#4000 (only BDB tables)
select a, @arg00, b FROM t1 outer_table where
   b=@arg01 and a = (select @arg02 from t1 where outer_table.b = @arg03
                        and outer_table.a=a ) ;
prepare stmt1 from ' select a, ?, b FROM t1 outer_table where
   b=? and a = (select ? from t1 where outer_table.b = ? 
                   and outer_table.a=a ) ' ;
# also Bug#4000 (only BDB tables) ??
execute stmt1 using @arg00, @arg01, @arg02, @arg03 ;

###### subquery after from
set @arg00=1 ;
set @arg01=0 ;
select a, @arg00 
from ( select a - @arg00 as a from t1 where a=@arg00 ) as t2
where a=@arg01;
prepare stmt1 from ' select a, ? 
                    from ( select a - ? as a from t1 where a=? ) as t2
                    where a=? ';
execute stmt1 using @arg00, @arg00, @arg00, @arg01 ;

###### heavy modified case derived from client_test.c: test_distinct()
## no parameters
--disable_warnings
drop table if exists t2 ;
--enable_warnings
create table t2 as select * from t_many_col_types;
#insert into t2 select * from t_many_col_types;
set @stmt= ' SELECT
   (SELECT SUM(c1 + c12 + 0.0) FROM t2 
    where (t_many_col_types.c2 - 0e-3) = t2.c2
    GROUP BY t_many_col_types.c15 LIMIT 1) as scalar_s,
   exists (select 1.0e+0 from t2 
           where t2.c3 * 9.0000000000 = t_many_col_types.c4) as exists_s,
   c5 * 4 in (select c6 + 0.3e+1 from t2) as in_s,
   (c7 - 4, c8 - 4) in (select c9 + 4.0, c10 + 40e-1 from t2) as in_row_s
FROM t_many_col_types,
(select c25 x, c32 y from t2) tt WHERE x = c25 ' ;
--enable_metadata
prepare stmt1 from @stmt ;
execute stmt1 ;
--disable_metadata
execute stmt1 ;
set @stmt= concat('explain ',@stmt);
--enable_metadata
prepare stmt1 from @stmt ;
execute stmt1 ;
--disable_metadata
execute stmt1 ;
## many parameters
set @stmt= ' SELECT
   (SELECT SUM(c1+c12+?) FROM t2 where (t_many_col_types.c2-?)=t2.c2
    GROUP BY t_many_col_types.c15 LIMIT 1) as scalar_s,
   exists (select ? from t2 
           where t2.c3*?=t_many_col_types.c4) as exists_s,
   c5*? in (select c6+? from t2) as in_s,
   (c7-?, c8-?) in (select c9+?, c10+? from t2) as in_row_s
FROM t_many_col_types,
(select c25 x, c32 y from t2) tt WHERE x =c25 ' ;
set @arg00= 0.0 ;
set @arg01= 0e-3 ;
set @arg02= 1.0e+0 ;
set @arg03= 9.0000000000 ;
set @arg04= 4 ;
set @arg05= 0.3e+1 ;
set @arg06= 4 ;
set @arg07= 4 ;
set @arg08= 4.0 ;
set @arg09= 40e-1 ;
--enable_metadata
prepare stmt1 from @stmt ;
execute stmt1 using @arg00, @arg01, @arg02, @arg03, @arg04, @arg05, @arg06,
                    @arg07, @arg08, @arg09 ;
--disable_metadata
execute stmt1 using @arg00, @arg01, @arg02, @arg03, @arg04, @arg05, @arg06,
                    @arg07, @arg08, @arg09 ;
set @stmt= concat('explain ',@stmt);
--enable_metadata
prepare stmt1 from @stmt ;
execute stmt1 using @arg00, @arg01, @arg02, @arg03, @arg04, @arg05, @arg06,
                    @arg07, @arg08, @arg09 ;
--disable_metadata
execute stmt1 using @arg00, @arg01, @arg02, @arg03, @arg04, @arg05, @arg06,
                    @arg07, @arg08, @arg09 ;
drop table t2 ;


--disable_query_log
select '------ union tests ------' as test_sequence ;
--enable_query_log

# no parameter
prepare stmt1 from ' select a FROM t1 where a=1
                     union distinct
                     select a FROM t1 where a=1 ';
execute stmt1 ;
# Bug#3577: the second execute crashes mysqld
execute stmt1 ;
prepare stmt1 from ' select a FROM t1 where a=1
                     union all
                     select a FROM t1 where a=1 ';
execute stmt1 ;

##### everything in the first table
# one parameter as constant in the first table
set @arg00=1 ;
select @arg00 FROM t1 where a=1
union distinct
select 1 FROM t1 where a=1;
prepare stmt1 from ' select ? FROM t1 where a=1
                     union distinct
                     select 1 FROM t1 where a=1 ' ;
execute stmt1 using @arg00;

##### everything in the second table
# one parameter as constant
set @arg00=1 ;
select 1 FROM t1 where a=1
union distinct
select @arg00 FROM t1 where a=1;
prepare stmt1 from ' select 1 FROM t1 where a=1
                     union distinct
                     select ? FROM t1 where a=1 ' ;
execute stmt1 using @arg00;

# one parameter in every table
set @arg00='a' ;
select @arg00 FROM t1 where a=1
union distinct
select @arg00 FROM t1 where a=1;
prepare stmt1 from ' select ? FROM t1 where a=1
                     union distinct
                     select ? FROM t1 where a=1 ';
# BUG#3811 wrong result, prepared statement, union, 
# parameter in result column list
execute stmt1 using @arg00, @arg00;
prepare stmt1 from ' select ? 
                     union distinct
                     select ? ';
execute stmt1 using @arg00, @arg00;

# many parameters
set @arg00='a' ;
set @arg01=1 ;
set @arg02='a' ;
set @arg03=2 ;
select @arg00 FROM t1 where a=@arg01
union distinct
select @arg02 FROM t1 where a=@arg03;
prepare stmt1 from ' select ? FROM t1 where a=?
                     union distinct
                     select ? FROM t1 where a=? ' ;
execute stmt1 using @arg00, @arg01, @arg02, @arg03;

## increased complexity

set @arg00=1 ;
# Bug#3686 the wrong server response was 1140 Mixing of GROUP columns ..
prepare stmt1 from ' select sum(a) + 200, ? from t1
union distinct
select sum(a) + 200, 1 from t1
group by b ' ;
execute stmt1 using @arg00;

set @Oporto='Oporto' ;
set @Lisboa='Lisboa' ;
set @0=0 ;
set @1=1 ;
set @2=2 ;
set @3=3 ;
set @4=4 ;
select @Oporto,@Lisboa,@0,@1,@2,@3,@4 ;

## union + group by
select sum(a) + 200 as the_sum, @Oporto as the_town from t1
group by b
union distinct
select sum(a) + 200, @Lisboa from t1
group by b ;

prepare stmt1 from ' select sum(a) + 200 as the_sum, ? as the_town from t1
                     group by b
                     union distinct
                     select sum(a) + 200, ? from t1
                     group by b ' ;
execute stmt1 using @Oporto, @Lisboa;


## union + where + group by
select sum(a) + 200 as the_sum, @Oporto as the_town from t1
where a > @1
group by b
union distinct
select sum(a) + 200, @Lisboa from t1
where a > @2
group by b ;

prepare stmt1 from ' select sum(a) + 200 as the_sum, ? as the_town from t1
                     where a > ?
                     group by b
                     union distinct
                     select sum(a) + 200, ? from t1
                     where a > ?
                     group by b ' ;
execute stmt1 using @Oporto, @1, @Lisboa, @2;

## union + where + group by + having
select sum(a) + 200 as the_sum, @Oporto as the_town from t1
where a > @1
group by b
having avg(a) > @2
union distinct
select sum(a) + 200, @Lisboa from t1
where a > @2
group by b 
having avg(a) > @3;

prepare stmt1 from ' select sum(a) + 200 as the_sum, ? as the_town from t1
                     where a > ?
                     group by b
                     having avg(a) > ?
                     union distinct
                     select sum(a) + 200, ? from t1
                     where a > ?
                     group by b
                     having avg(a) > ? ';
execute stmt1 using @Oporto, @1, @2, @Lisboa, @2, @3;


--disable_query_log
select '------ explain select tests ------' as test_sequence ;
--enable_query_log
prepare stmt1 from ' select * from t_many_col_types ' ;
--enable_metadata
execute stmt1;
--disable_metadata


