############################ ps_conv.inc ##############################
#                                                                      #
#  Tests for prepared statements: conversion of parameters             #
#                                                                      #
#  Please don't                                                        #
#  - try to execute this script in ANSI mode, because many statements  #
#    will fail due to the strict type checking                         #
#  - reuse such ugly assignments like timestamp column = float value . #
#    I included them only for controlling purposes.                    #
########################################################################

#    
# NOTE: PLEASE SEE ps_1general.test (bottom) 
#       BEFORE ADDING NEW TEST CASES HERE !!!

#
# Please be aware, that this file will be sourced by several test case files
# stored within the subdirectory 't'. So every change here will affect 
# several test cases.


# The MySQL User Variables do not support the simulation of all 
# C-API field types.
#
# - There is no method to make an explicit assignment of a type to a variable.
# - The type of the variable can be only influenced by the writing style
#   of the value.
#
# The next tests should give an example for these properties.
--disable_warnings
drop table if exists t5 ;
--enable_warnings
set @arg01= 8;
set @arg02= 8.0;
set @arg03= 80.00000000000e-1;
set @arg04= 'abc' ;
set @arg05= CAST('abc' as binary) ;
set @arg06= '1991-08-05' ;
set @arg07= CAST('1991-08-05' as date);
set @arg08= '1991-08-05 01:01:01' ;
set @arg09= CAST('1991-08-05 01:01:01' as datetime) ;
set @arg10= unix_timestamp('1991-01-01 01:01:01');
set @arg11= YEAR('1991-01-01 01:01:01');
# This first assignment to @arg<n> fixes the type of the variable
# The second assignment sets the value to NULL, but it does not change
# the numeric types.
set @arg12= 8 ;
set @arg12= NULL ;
set @arg13= 8.0 ;
set @arg13= NULL ;
set @arg14= 'abc';
set @arg14= NULL ;
set @arg15= CAST('abc' as binary) ;
set @arg15= NULL ;
create table t5 as select
  8                           as const01, @arg01 as param01,
  8.0                         as const02, @arg02 as param02,
  80.00000000000e-1           as const03, @arg03 as param03,
  'abc'                       as const04, @arg04 as param04,
  CAST('abc' as binary)       as const05, @arg05 as param05,
  '1991-08-05'                as const06, @arg06 as param06,
  CAST('1991-08-05' as date)  as const07, @arg07 as param07,
  '1991-08-05 01:01:01'       as const08, @arg08 as param08,
  CAST('1991-08-05 01:01:01'  as datetime) as const09, @arg09 as param09,
  unix_timestamp('1991-01-01 01:01:01')    as const10, @arg10 as param10,
  YEAR('1991-01-01 01:01:01') as const11, @arg11 as param11, 
  NULL                        as const12, @arg12 as param12,
                                          @arg13 as param13,
                                          @arg14 as param14,
                                          @arg15 as param15;
  
# Bug#4788 show create table provides incorrect statement
show create table t5 ;
--vertical_results
--enable_metadata
--disable_ps_protocol
select * from t5 ;
--enable_ps_protocol
--disable_metadata
--horizontal_results
drop table t5 ;

# But there seems to be also an implicit conversion of C-API
# data types to a smaller number of base data types.
# 
# Example: C-API for prepared statements
#          CREATE TABLE abc as SELECT  ? as a, ? as b, ...
#
#    MYSQL_TYPE of parameter  column type
#    MYSQL_TYPE_TINY          bigint(4)
#    MYSQL_TYPE_SHORT         bigint(6)
#    MYSQL_TYPE_FLOAT         double
#    ...
#
# So we can hope that the functionality of mysqltest + user variables
# sufficient to simulate much of the behaviour of the C-API
# vis-a-vis the server.

# The main test object is the table t9, defined as follows:
#
# eval create table t9 
# (
#   c1  tinyint, c2  smallint, c3  mediumint, c4  int,
#   c5  integer, c6  bigint, c7  float, c8  double,
#   c9  double precision, c10 real, c11 decimal(7, 4), c12 numeric(8, 4),
#   c13 date, c14 datetime, c15 timestamp(14), c16 time,
#   c17 year, c18 bit, c19 bool, c20 char,
#   c21 char(10), c22 varchar(30), c23 tinyblob, c24 tinytext,
#   c25 blob, c26 text, c27 mediumblob, c28 mediumtext,
#   c29 longblob, c30 longtext, c31 enum('one', 'two', 'three'),
#   c32 set('monday', 'tuesday', 'wednesday'),
#   primary key(c1)
# ) engine = $type ;
# We test each statement in non-prepared mode and in prepared mode
# for comparison purposes.
#
# We test the following conversions:
# BIGINT -> the rest of numeric columns
# CHAR, LONGTEXT, LONGBLOB, NULL, FLOAT, REAL, DOUBLE -> numeric columns
# FLOAT, REAL, CHAR, LONGTEXT, BINARY, BIGINT -> string
# DATETIME, TIME -> text, and back


--disable_query_log
select '------ data type conversion tests ------' as test_sequence ;
--enable_query_log
--source include/ps_renew.inc

# insert a record with many NULLs
insert into t9 set c1= 0, c15= '1991-01-01 01:01:01' ;
select * from t9 order by c1 ;

############ select @parm:= .. / select .. into @parm tests ############
--disable_query_log
select '------ select @parameter:= column ------' as test_sequence ;
--enable_query_log
# PS query to retrieve the content of the @variables
prepare full_info from "select @arg01, @arg02, @arg03, @arg04,
       @arg05, @arg06, @arg07, @arg08,
       @arg09, @arg10, @arg11, @arg12,
       @arg13, @arg14, @arg15, @arg16,
       @arg17, @arg18, @arg19, @arg20,
       @arg21, @arg22, @arg23, @arg24,
       @arg25, @arg26, @arg27, @arg28,
       @arg29, @arg30, @arg31, @arg32" ;

# non PS statement for comparison purposes
select @arg01:=  c1, @arg02:=  c2, @arg03:=  c3, @arg04:=  c4,
       @arg05:=  c5, @arg06:=  c6, @arg07:=  c7, @arg08:=  c8,
       @arg09:=  c9, @arg10:= c10, @arg11:= c11, @arg12:= c12,
       @arg13:= c13, @arg14:= c14, @arg15:= c15, @arg16:= c16,
       @arg17:= c17, @arg18:= c18, @arg19:= c19, @arg20:= c20,
       @arg21:= c21, @arg22:= c22, @arg23:= c23, @arg24:= c24,
       @arg25:= c25, @arg26:= c26, @arg27:= c27, @arg28:= c28,
       @arg29:= c29, @arg30:= c30, @arg31:= c31, @arg32:= c32
from t9 where c1= 1 ;
# get as much informations about the parameters as possible
--enable_metadata
execute full_info ;
--disable_metadata
# now the same procedure with the record containing so many NULLs
select @arg01:=  c1, @arg02:=  c2, @arg03:=  c3, @arg04:=  c4,
       @arg05:=  c5, @arg06:=  c6, @arg07:=  c7, @arg08:=  c8,
       @arg09:=  c9, @arg10:= c10, @arg11:= c11, @arg12:= c12,
       @arg13:= c13, @arg14:= c14, @arg15:= c15, @arg16:= c16,
       @arg17:= c17, @arg18:= c18, @arg19:= c19, @arg20:= c20,
       @arg21:= c21, @arg22:= c22, @arg23:= c23, @arg24:= c24,
       @arg25:= c25, @arg26:= c26, @arg27:= c27, @arg28:= c28,
       @arg29:= c29, @arg30:= c30, @arg31:= c31, @arg32:= c32
from t9 where c1= 0 ;
# get as much informations about the parameters as possible
--enable_metadata
execute full_info ;
--disable_metadata

prepare stmt1 from "select 
       @arg01:=  c1, @arg02:=  c2, @arg03:=  c3, @arg04:=  c4,
       @arg05:=  c5, @arg06:=  c6, @arg07:=  c7, @arg08:=  c8,
       @arg09:=  c9, @arg10:= c10, @arg11:= c11, @arg12:= c12,
       @arg13:= c13, @arg14:= c14, @arg15:= c15, @arg16:= c16,
       @arg17:= c17, @arg18:= c18, @arg19:= c19, @arg20:= c20,
       @arg21:= c21, @arg22:= c22, @arg23:= c23, @arg24:= c24,
       @arg25:= c25, @arg26:= c26, @arg27:= c27, @arg28:= c28,
       @arg29:= c29, @arg30:= c30, @arg31:= c31, @arg32:= c32
from t9 where c1= ?" ;
set @my_key= 1 ;
execute stmt1 using @my_key ;
# get as much informations about the parameters as possible
--enable_metadata
execute full_info ;
--disable_metadata
# now the same procedure with the record containing so many NULLs
set @my_key= 0 ;
execute stmt1 using @my_key ;
# get as much informations about the parameters as possible
--enable_metadata
execute full_info ;
--disable_metadata

# the next statement must fail
--error 1064
prepare stmt1 from "select ? := c1 from t9 where c1= 1" ;

--disable_query_log
select '------ select column, .. into @parm,.. ------' as test_sequence ;
--enable_query_log
select c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12,
       c13, c14, c15, c16, c17, c18, c19, c20, c21, c22, c23, c24,
       c25, c26, c27, c28, c29, c30, c31, c32
into @arg01, @arg02, @arg03, @arg04, @arg05, @arg06, @arg07, @arg08,
     @arg09, @arg10, @arg11, @arg12, @arg13, @arg14, @arg15, @arg16,
     @arg17, @arg18, @arg19, @arg20, @arg21, @arg22, @arg23, @arg24,
     @arg25, @arg26, @arg27, @arg28, @arg29, @arg30, @arg31, @arg32
from t9 where c1= 1 ;
# get as much informations about the parameters as possible
--enable_metadata
execute full_info ;
--disable_metadata
# now the same procedure with the record containing so many NULLs
select c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12,
       c13, c14, c15, c16, c17, c18, c19, c20, c21, c22, c23, c24,
       c25, c26, c27, c28, c29, c30, c31, c32
into @arg01, @arg02, @arg03, @arg04, @arg05, @arg06, @arg07, @arg08,
     @arg09, @arg10, @arg11, @arg12, @arg13, @arg14, @arg15, @arg16,
     @arg17, @arg18, @arg19, @arg20, @arg21, @arg22, @arg23, @arg24,
     @arg25, @arg26, @arg27, @arg28, @arg29, @arg30, @arg31, @arg32
from t9 where c1= 0 ;
# get as much informations about the parameters as possible
--enable_metadata
execute full_info ;
--disable_metadata

prepare stmt1 from "select c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12,
       c13, c14, c15, c16, c17, c18, c19, c20, c21, c22, c23, c24,
       c25, c26, c27, c28, c29, c30, c31, c32
into @arg01, @arg02, @arg03, @arg04, @arg05, @arg06, @arg07, @arg08,
     @arg09, @arg10, @arg11, @arg12, @arg13, @arg14, @arg15, @arg16,
     @arg17, @arg18, @arg19, @arg20, @arg21, @arg22, @arg23, @arg24,
     @arg25, @arg26, @arg27, @arg28, @arg29, @arg30, @arg31, @arg32
from t9 where c1= ?" ;
set @my_key= 1 ;
execute stmt1 using @my_key ;
# get as much informations about the parameters as possible
--enable_metadata
execute full_info ;
--disable_metadata
# now the same procedure with the record containing so many NULLs
# Bug#5034: prepared "select 1 into @arg15", second execute crashes server
set @my_key= 0 ;
execute stmt1 using @my_key ;
# get as much informations about the parameters as possible
--enable_metadata
execute full_info ;
--disable_metadata

# the next statement must fail
--error 1064
prepare stmt1 from "select c1 into ? from t9 where c1= 1" ;



######################### test of numeric types ##########################
#                                                                        #
# c1  tinyint, c2  smallint, c3  mediumint, c4  int,                     # 
# c5  integer, c6  bigint, c7  float, c8  double,                        #
# c9  double precision, c10 real, c11 decimal(7, 4), c12 numeric(8, 4),  #
#                                                                        #
##########################################################################
--disable_query_log
select '-- insert into numeric columns --' as test_sequence ;
--enable_query_log

######## INSERT into .. numeric columns values(BIGINT(n),BIGINT) ########
insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20 ) ;
set @arg00= 21 ;
insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
                         @arg00, @arg00, @arg00, @arg00, @arg00 ) ;
prepare stmt1 from "insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22 )" ;
execute stmt1 ;
set @arg00= 23;
prepare stmt2 from "insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values 
  (  ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
                    @arg00, @arg00, @arg00, @arg00 ;


######## INSERT into .. numeric columns values(DOUBLE(m,n),DOUBLE) ########
insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( 30.0, 30.0, 30.0, 30.0, 30.0, 30.0, 30.0, 30.0,
    30.0, 30.0, 30.0 ) ;
set @arg00= 31.0 ;
insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
                         @arg00, @arg00, @arg00, @arg00, @arg00 ) ;
prepare stmt1 from "insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( 32.0, 32.0, 32.0, 32.0, 32.0, 32.0, 32.0, 32.0,
    32.0, 32.0, 32.0 )" ;
execute stmt1 ;
set @arg00= 33.0;
prepare stmt2 from "insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values 
  (  ?,  ?,  ?,  ?,  ?,  ?,  ?,  ?,  ?,   ?,   ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
                    @arg00, @arg00, @arg00, @arg00 ;


######## INSERT into .. numeric columns values(CHAR(n),LONGTEXT) #########
insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( '40', '40', '40', '40', '40', '40', '40', '40',
    '40', '40', '40' ) ;
set @arg00= '41' ;
insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
                             @arg00, @arg00, @arg00, @arg00, @arg00 ) ;
prepare stmt1 from "insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( '42', '42', '42', '42', '42', '42', '42', '42',
    '42', '42', '42' )" ;
execute stmt1 ;
set @arg00= '43';
prepare stmt2 from "insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values 
  ( ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
                    @arg00, @arg00, @arg00, @arg00 ;


######## INSERT into .. numeric columns values(BINARY(n),LONGBLOB) ########
insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( CAST('50' as binary), CAST('50' as binary), 
  CAST('50' as binary), CAST('50' as binary), CAST('50' as binary), 
  CAST('50' as binary), CAST('50' as binary), CAST('50' as binary),
  CAST('50' as binary), CAST('50' as binary), CAST('50' as binary) ) ;
set @arg00= CAST('51' as binary) ;
insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
                             @arg00, @arg00, @arg00, @arg00, @arg00 ) ;
prepare stmt1 from "insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( CAST('52' as binary), CAST('52' as binary),
  CAST('52' as binary), CAST('52' as binary), CAST('52' as binary), 
  CAST('52' as binary), CAST('52' as binary), CAST('52' as binary),
  CAST('52' as binary), CAST('52' as binary), CAST('52' as binary) )" ;
execute stmt1 ;
set @arg00= CAST('53' as binary) ;
prepare stmt2 from "insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values 
  ( ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
                    @arg00, @arg00, @arg00, @arg00 ;


######## INSERT into .. numeric columns values(BIGINT,NULL) ########
# we first assign number to arg00 to set it's datatype to numeric.
set @arg00= 2 ;
set @arg00= NULL ;
insert into t9
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( 60, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL ) ;
insert into t9
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( 61, @arg00, @arg00, @arg00, @arg00, @arg00,
                             @arg00, @arg00, @arg00, @arg00, @arg00 ) ;
prepare stmt1 from "insert into t9
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( 62, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL )" ;
execute stmt1 ;
prepare stmt2 from "insert into t9
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( 63, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
                    @arg00, @arg00, @arg00, @arg00 ;


######## INSERT into .. numeric columns values(DOUBLE,NULL) ########
set @arg00= 8.0 ;
set @arg00= NULL ;
insert into t9
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( 71, @arg00, @arg00, @arg00, @arg00, @arg00,
                             @arg00, @arg00, @arg00, @arg00, @arg00 ) ;
prepare stmt2 from "insert into t9
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( 73, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
                    @arg00, @arg00, @arg00, @arg00 ;


######## INSERT into .. numeric columns values(LONGBLOB,NULL) ########
set @arg00= 'abc' ;
set @arg00= NULL ;
insert into t9
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( 81, @arg00, @arg00, @arg00, @arg00, @arg00,
                             @arg00, @arg00, @arg00, @arg00, @arg00 ) ;
prepare stmt2 from "insert into t9
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( 83, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
                    @arg00, @arg00, @arg00, @arg00 ;



######## SELECT of all inserted records ########
select c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12
from t9 where c1 >= 20
order by c1 ;


--disable_query_log
select '-- select .. where numeric column = .. --' as test_sequence ;
--enable_query_log
######## SELECT .. WHERE column(numeric)=value(BIGINT(n)/BIGINT) ########
set @arg00= 20;
select 'true' as found from t9 
where c1= 20 and c2= 20 and c3= 20 and c4= 20 and c5= 20 and c6= 20 and c7= 20
  and c8= 20 and c9= 20 and c10= 20 and c12= 20;
select 'true' as found from t9 
where c1= @arg00 and c2= @arg00 and c3= @arg00 and c4= @arg00 and c5= @arg00 
  and c6= @arg00 and c7= @arg00 and c8= @arg00 and c9= @arg00 and c10= @arg00
  and c12= @arg00;
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and c2= 20 and c3= 20 and c4= 20 and c5= 20 and c6= 20 and c7= 20
  and c8= 20 and c9= 20 and c10= 20 and c12= 20 ";
execute stmt1 ;
prepare stmt1 from "select 'true' as found from t9 
where c1= ? and c2= ? and c3= ? and c4= ? and c5= ? 
  and c6= ? and c7= ? and c8= ? and c9= ? and c10= ?
  and c12= ? ";
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
                    @arg00, @arg00, @arg00, @arg00 ;


######## SELECT .. WHERE column(numeric)=value(DOUBLE(m,n)/DOUBLE) ########
set @arg00= 20.0;
select 'true' as found from t9 
where c1= 20.0 and c2= 20.0 and c3= 20.0 and c4= 20.0 and c5= 20.0 and c6= 20.0
  and c7= 20.0 and c8= 20.0 and c9= 20.0 and c10= 20.0 and c12= 20.0;
select 'true' as found from t9 
where c1= @arg00 and c2= @arg00 and c3= @arg00 and c4= @arg00 and c5= @arg00 
  and c6= @arg00 and c7= @arg00 and c8= @arg00 and c9= @arg00 and c10= @arg00
  and c12= @arg00;
prepare stmt1 from "select 'true' as found from t9 
where c1= 20.0 and c2= 20.0 and c3= 20.0 and c4= 20.0 and c5= 20.0 and c6= 20.0
  and c7= 20.0 and c8= 20.0 and c9= 20.0 and c10= 20.0 and c12= 20.0 ";
execute stmt1 ;
prepare stmt1 from "select 'true' as found from t9 
where c1= ? and c2= ? and c3= ? and c4= ? and c5= ? 
  and c6= ? and c7= ? and c8= ? and c9= ? and c10= ?
  and c12= ? ";
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
                    @arg00, @arg00, @arg00, @arg00 ;


######## SELECT .. WHERE column(numeric)=value(CHAR(n)/LONGTEXT) ########
select 'true' as found from t9 
where c1= '20' and c2= '20' and c3= '20' and c4= '20' and c5= '20' and c6= '20'
  and c7= '20' and c8= '20' and c9= '20' and c10= '20' and c12= '20';
prepare stmt1 from "select 'true' as found from t9
where c1= '20' and c2= '20' and c3= '20' and c4= '20' and c5= '20' and c6= '20'
  and c7= '20' and c8= '20' and c9= '20' and c10= '20' and c12= '20' ";
execute stmt1 ;
set @arg00= '20';
select 'true' as found from t9 
where c1= @arg00 and c2= @arg00 and c3= @arg00 and c4= @arg00 and c5= @arg00 
  and c6= @arg00 and c7= @arg00 and c8= @arg00 and c9= @arg00 and c10= @arg00
  and c12= @arg00;
prepare stmt1 from "select 'true' as found from t9 
where c1= ? and c2= ? and c3= ? and c4= ? and c5= ? 
  and c6= ? and c7= ? and c8= ? and c9= ? and c10= ?
  and c12= ? ";
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
                    @arg00, @arg00, @arg00, @arg00 ;


######## SELECT .. WHERE column(numeric)=value(BINARY(n)/LONGBLOB) ########
select 'true' as found from t9 
where c1= CAST('20' as binary) and c2= CAST('20' as binary) and 
      c3= CAST('20' as binary) and c4= CAST('20' as binary) and 
      c5= CAST('20' as binary) and c6= CAST('20' as binary) and 
      c7= CAST('20' as binary) and c8= CAST('20' as binary) and 
      c9= CAST('20' as binary) and c10= CAST('20' as binary) and 
      c12= CAST('20' as binary);
prepare stmt1 from "select 'true' as found from t9
where c1= CAST('20' as binary) and c2= CAST('20' as binary) and 
      c3= CAST('20' as binary) and c4= CAST('20' as binary) and 
      c5= CAST('20' as binary) and c6= CAST('20' as binary) and 
      c7= CAST('20' as binary) and c8= CAST('20' as binary) and 
      c9= CAST('20' as binary) and c10= CAST('20' as binary) and 
      c12= CAST('20' as binary) ";
execute stmt1 ;
set @arg00= CAST('20' as binary) ;
select 'true' as found from t9 
where c1= @arg00 and c2= @arg00 and c3= @arg00 and c4= @arg00 and c5= @arg00 
  and c6= @arg00 and c7= @arg00 and c8= @arg00 and c9= @arg00 and c10= @arg00
  and c12= @arg00;
prepare stmt1 from "select 'true' as found from t9 
where c1= ? and c2= ? and c3= ? and c4= ? and c5= ? 
  and c6= ? and c7= ? and c8= ? and c9= ? and c10= ?
  and c12= ? ";
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
                    @arg00, @arg00, @arg00, @arg00 ;

delete from t9 ;

#################### Some overflow experiments ################################
#                                                                             #
# MySQL Manual (July 2004)                                                    #
# - Setting a numeric column to a value that lies outside the column's range. #
#   The value is clipped to the closest endpoint of the range.                #
# ...                                                                         #
# - For example, inserting the string '1999.0e-2' into an INT, FLOAT,         #
#   DECIMAL(10,6), or YEAR column results in the values 1999, 19.9921,        #
#   19.992100, and 1999.                                                      #
# That means there is an anomaly if a float value is assigned via string to   #
# a column of type bigint. The string will be cut from the right side to      #
# a "usable" integer value.                                                   #
#                                                                             #
###############################################################################
--disable_query_log
select '-- some numeric overflow experiments --' as test_sequence ;
--enable_query_log
prepare my_insert from "insert into t9 
   ( c21, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values 
   ( 'O',  ?,  ?,  ?,  ?,  ?,  ?,  ?,  ?,  ?,   ?,   ? )" ;
prepare my_select from "select c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12
from t9 where c21 = 'O' ";
prepare my_delete from "delete from t9 where c21 = 'O' ";

# Numeric overflow of columns(c1, c2, c3, c4, c5, c12) with type not in 
# (BIGINT,FLOAT,REAL,DOUBLE) during insert
#
# Use the maximum BIGINT from the manual
set @arg00= 9223372036854775807 ;
execute my_insert using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
                        @arg00, @arg00, @arg00, @arg00, @arg00 ;
--vertical_results
execute my_select ;
--horizontal_results
execute my_delete ;
set @arg00= '9223372036854775807' ;
execute my_insert using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
                        @arg00, @arg00, @arg00, @arg00, @arg00 ;
--vertical_results
execute my_select ;
--horizontal_results
execute my_delete ;
# Use the minimum BIGINT from the manual
#
set @arg00= -9223372036854775808 ;
execute my_insert using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
                        @arg00, @arg00, @arg00, @arg00, @arg00 ;
--vertical_results
execute my_select ;
--horizontal_results
execute my_delete ;
set @arg00= '-9223372036854775808' ;
execute my_insert using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
                        @arg00, @arg00, @arg00, @arg00, @arg00 ;
--vertical_results
execute my_select ;
--horizontal_results
execute my_delete ;

# Numeric overflow of columns(c1, c2, c3, c4, c5, c12) with type not in 
# (FLOAT,REAL,DOUBLE) during insert
#
set @arg00= 1.11111111111111111111e+50 ;
execute my_insert using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
                        @arg00, @arg00, @arg00, @arg00, @arg00 ;
--vertical_results
execute my_select ;
--horizontal_results
execute my_delete ;
# Attention: The columns(c1,c2,c3,c4,c5,c6) do not get the overflow,
#             because the string is treated as written integer and
#             '.11111111111111111111e+50' is cut away.
set @arg00= '1.11111111111111111111e+50' ;
execute my_insert using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
                        @arg00, @arg00, @arg00, @arg00, @arg00 ;
--vertical_results
execute my_select ;
--horizontal_results
execute my_delete ;
set @arg00= -1.11111111111111111111e+50 ;
execute my_insert using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
                        @arg00, @arg00, @arg00, @arg00, @arg00 ;
--vertical_results
execute my_select ;
--horizontal_results
execute my_delete ;
# Attention: The columns(c1,c2,c3,c4,c5,c6) do not get the overflow,
#             because the string is treated as written integer and
#             '.11111111111111111111e+50' is cut away.
set @arg00= '-1.11111111111111111111e+50' ;
execute my_insert using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
                        @arg00, @arg00, @arg00, @arg00, @arg00 ;
--vertical_results
execute my_select ;
--horizontal_results
execute my_delete ;

########################## test of string types ##########################
#                                                                        # 
#   c20 char, c21 char(10), c22 varchar(30), c23 tinyblob, c24 tinytext, #
#   c25 blob, c26 text, c27 mediumblob, c28 mediumtext, c29 longblob,    #
#   c30 longtext, c31 enum('one', 'two', 'three')                        #
#                                                                        #
##########################################################################
--disable_query_log
select '-- insert into string columns --' as test_sequence ;
--enable_query_log

######## INSERT into .. string columns values(CHAR(n),LONGTEXT) ########
--disable_query_log
insert into t9 
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values
  ( 20, '20', '20', '20', '20', '20', '20', '20', '20', '20', '20', '20' ) ;
set @arg00= '21' ;
insert into t9 
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values
  ( 21, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
        @arg00, @arg00, @arg00 ) ;
prepare stmt1 from "insert into t9 
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values
  ( 22, '22', '22', '22', '22', '22', '22', '22', '22', '22', '22', '22' )" ;
execute stmt1 ;
set @arg00= '23';
prepare stmt2 from "insert into t9 
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values 
  ( 23, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
                    @arg00, @arg00, @arg00, @arg00 ;


######## INSERT into .. string columns values(BINARY(n),LONGBLOB) ########
insert into t9 
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values
  ( 30, CAST('30' as binary), CAST('30' as binary), CAST('30' as binary),
  CAST('30' as binary), CAST('30' as binary), CAST('30' as binary),
  CAST('30' as binary), CAST('30' as binary), CAST('30' as binary),
  CAST('30' as binary), CAST('30' as binary) ) ;
set @arg00= '31' ;
insert into t9 
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values
  ( 31, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
        @arg00, @arg00, @arg00 ) ;
prepare stmt1 from "insert into t9 
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values
  ( 32, CAST('32' as binary), CAST('32' as binary), CAST('32' as binary),
  CAST('32' as binary), CAST('32' as binary), CAST('32' as binary),
  CAST('32' as binary), CAST('32' as binary), CAST('32' as binary),
  CAST('32' as binary), CAST('32' as binary) )" ;
execute stmt1 ;
set @arg00= CAST('33' as binary);
prepare stmt2 from "insert into t9 
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values 
  ( 33, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
                    @arg00, @arg00, @arg00, @arg00 ;


######## INSERT into .. string columns values(BIGINT(n),BIGINT) ########
insert into t9 
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values
  ( 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40 ) ;
set @arg00= 41 ;
insert into t9 
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values
  ( 41, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
        @arg00, @arg00, @arg00 ) ;
prepare stmt1 from "insert into t9 
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values
  ( 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42 )" ;
execute stmt1 ;
set @arg00= 43;
prepare stmt2 from "insert into t9 
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values 
  ( 43, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
                    @arg00, @arg00, @arg00, @arg00 ;


######## INSERT into .. string columns values(DOUBLE(m,n),DOUBLE) ########
insert into t9 
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values
  ( 50, 50.0, 50.0, 50.0, 50.0, 50.0, 50.0, 50.0, 50.0, 50.0, 50.0, 50.0 ) ;
set @arg00= 51.0 ;
insert into t9 
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values
  ( 51, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
        @arg00, @arg00, @arg00 ) ;
prepare stmt1 from "insert into t9 
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values
  ( 52, 52.0, 52.0, 52.0, 52.0, 52.0, 52.0, 52.0, 52.0, 52.0, 52.0, 52.0 )" ;
execute stmt1 ;
set @arg00= 53.0;
prepare stmt2 from "insert into t9 
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values 
  ( 53, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
                    @arg00, @arg00, @arg00, @arg00 ;


######## INSERT into .. string columns values(DOUBLE(m,n),DOUBLE) ########
#  typical float writing style
insert into t9 
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values
  ( 54, 5.4e+1, 5.4e+1, 5.4e+1, 5.4e+1, 5.4e+1, 5.4e+1, 5.4e+1, 5.4e+1,
    5.4e+1, 5.4e+1, 5.4e+1 ) ;
set @arg00= 5.5e+1 ;
insert into t9 
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values
  ( 55, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
        @arg00, @arg00, @arg00 ) ;
prepare stmt1 from "insert into t9 
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values
  ( 56, 5.6e+1, 5.6e+1, 5.6e+1, 5.6e+1, 5.6e+1, 5.6e+1, 5.6e+1, 5.6e+1,
    5.6e+1, 5.6e+1, 5.6e+1 )" ;
execute stmt1 ;
set @arg00= 5.7e+1;
prepare stmt2 from "insert into t9 
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values 
  ( 57, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
                    @arg00, @arg00, @arg00, @arg00 ;


######## INSERT into .. string columns values(LONGBLOB,NULL) ########
set @arg00= 'abc' ;
set @arg00= NULL ;
insert into t9
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values
  ( 60, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL ) ;
insert into t9
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values
  ( 61, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
        @arg00, @arg00, @arg00 ) ;
prepare stmt1 from "insert into t9
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values
  ( 62, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL )" ;
execute stmt1 ;
prepare stmt2 from "insert into t9
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values
  ( 63, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
                    @arg00, @arg00, @arg00, @arg00 ;

######## INSERT into .. string columns values(BIGINT,NULL) ########
set @arg00= 2 ;
set @arg00= NULL ;
insert into t9
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values
  ( 71, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
        @arg00, @arg00, @arg00 ) ;
prepare stmt2 from "insert into t9
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values
  ( 73, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
                    @arg00, @arg00, @arg00, @arg00 ;

######## INSERT into .. string columns values(DOUBLE,NULL) ########
set @arg00= 8 ;
set @arg00= NULL ;
insert into t9
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values
  ( 81, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
        @arg00, @arg00, @arg00 ) ;
prepare stmt2 from "insert into t9
  ( c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30 )
values
  ( 83, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
                    @arg00, @arg00, @arg00, @arg00 ;

--enable_query_log

######## SELECT of all inserted records ########
select c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30
from t9 where c1 >= 20
order by c1 ;


--disable_query_log
select '-- select .. where string column = .. --' as test_sequence ;
--enable_query_log
######## SELECT .. WHERE column(string)=value(CHAR(n)/LONGTEXT) ########
set @arg00= '20';
# c20 (char) must be extended for the comparison
select 'true' as found from t9 
where c1= 20 and concat(c20,substr('20',1+length(c20)))= '20' and c21= '20' and
  c22= '20' and c23= '20' and c24= '20' and c25= '20' and c26= '20' and
  c27= '20' and c28= '20' and c29= '20' and c30= '20' ;
select 'true' as found from t9 
where c1= 20 and concat(c20,substr(@arg00,1+length(c20)))= @arg00 and
  c21= @arg00 and c22= @arg00 and c23= @arg00 and c25= @arg00 and
  c26= @arg00 and c27= @arg00 and c28= @arg00 and c29= @arg00 and c30= @arg00;
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and concat(c20,substr('20',1+length(c20)))= '20' and c21= '20' and
  c22= '20' and c23= '20' and c24= '20' and c25= '20' and c26= '20' and
  c27= '20' and c28= '20' and c29= '20' and c30= '20'" ;
execute stmt1 ;
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and concat(c20,substr(?,1+length(c20)))= ? and
  c21= ? and c22= ? and c23= ? and c25= ? and
  c26= ? and c27= ? and c28= ? and c29= ? and c30= ?" ;
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
                    @arg00, @arg00, @arg00, @arg00, @arg00 ;


######## SELECT .. WHERE column(string)=value(BINARY(n)/LONGBLOB) ########
set @arg00= CAST('20' as binary);
# c20 (char) must be extended for the comparison
select 'true' as found from t9 
where c1= 20 and concat(c20,substr(CAST('20' as binary),1+length(c20)))
                 = CAST('20' as binary) and c21= CAST('20' as binary)
  and c22= CAST('20' as binary) and c23= CAST('20' as binary) and
  c24= CAST('20' as binary) and c25= CAST('20' as binary) and
  c26= CAST('20' as binary) and c27= CAST('20' as binary) and
  c28= CAST('20' as binary) and c29= CAST('20' as binary) and
  c30= CAST('20' as binary) ;
select 'true' as found from t9 
where c1= 20 and concat(c20,substr(@arg00,1+length(c20))) = @arg00 and
  c21= @arg00 and c22= @arg00 and c23= @arg00 and c25= @arg00 and
  c26= @arg00 and c27= @arg00 and c28= @arg00 and c29= @arg00 and
  c30= @arg00;
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and concat(c20,substr(CAST('20' as binary),1+length(c20)))
                 = CAST('20' as binary) and c21= CAST('20' as binary)
  and c22= CAST('20' as binary) and c23= CAST('20' as binary) and
  c24= CAST('20' as binary) and c25= CAST('20' as binary) and
  c26= CAST('20' as binary) and c27= CAST('20' as binary) and
  c28= CAST('20' as binary) and c29= CAST('20' as binary) and
  c30= CAST('20' as binary)" ;
execute stmt1 ;
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and concat(c20,substr(?,1+length(c20))) = ? and c21= ? and
  c22= ? and c23= ? and c25= ? and c26= ? and c27= ? and c28= ? and
  c29= ? and c30= ?";
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
                    @arg00, @arg00, @arg00, @arg00, @arg00 ;


######## SELECT .. WHERE column(string)=value(BIGINT(m,n),BIGINT) ########
set @arg00= 20;
# c20 (char) must be extended for the comparison
select 'true' as found from t9 
where c1= 20 and concat(c20,substr(20,1+length(c20)))= 20 and c21= 20 and
  c22= 20 and c23= 20 and c24= 20 and c25= 20 and c26= 20 and
  c27= 20 and c28= 20 and c29= 20 and c30= 20 ;
select 'true' as found from t9 
where c1= 20 and concat(c20,substr(@arg00,1+length(c20)))= @arg00 and
  c21= @arg00 and c22= @arg00 and c23= @arg00 and c25= @arg00 and
  c26= @arg00 and c27= @arg00 and c28= @arg00 and c29= @arg00 and c30= @arg00;
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and concat(c20,substr(20,1+length(c20)))= 20 and c21= 20 and
  c22= 20 and c23= 20 and c24= 20 and c25= 20 and c26= 20 and
  c27= 20 and c28= 20 and c29= 20 and c30= 20" ;
execute stmt1 ;
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and concat(c20,substr(?,1+length(c20)))= ? and
  c21= ? and c22= ? and c23= ? and c25= ? and
  c26= ? and c27= ? and c28= ? and c29= ? and c30= ?" ;
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
                    @arg00, @arg00, @arg00, @arg00, @arg00 ;


######## SELECT .. WHERE column(string)=value(DOUBLE(m,n),DOUBLE) ########
set @arg00= 20.0;
# c20 (char) must be extended for the comparison
select 'true' as found from t9 
where c1= 20 and concat(c20,substr(20.0,1+length(c20)))= 20.0 and c21= 20.0 and
  c22= 20.0 and c23= 20.0 and c24= 20.0 and c25= 20.0 and c26= 20.0 and
  c27= 20.0 and c28= 20.0 and c29= 20.0 and c30= 20.0 ;
select 'true' as found from t9 
where c1= 20 and concat(c20,substr(@arg00,1+length(c20)))= @arg00 and
  c21= @arg00 and c22= @arg00 and c23= @arg00 and c25= @arg00 and
  c26= @arg00 and c27= @arg00 and c28= @arg00 and c29= @arg00 and c30= @arg00;
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and concat(c20,substr(20.0,1+length(c20)))= 20.0 and c21= 20.0 and
  c22= 20.0 and c23= 20.0 and c24= 20.0 and c25= 20.0 and c26= 20.0 and
  c27= 20.0 and c28= 20.0 and c29= 20.0 and c30= 20.0" ;
execute stmt1 ;
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and concat(c20,substr(?,1+length(c20)))= ? and
  c21= ? and c22= ? and c23= ? and c25= ? and
  c26= ? and c27= ? and c28= ? and c29= ? and c30= ?" ;
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
                    @arg00, @arg00, @arg00, @arg00, @arg00 ;

delete from t9 ;


######################### test of date/time columns ########################
#                                                                          #
#      c13 date, c14 datetime, c15 timestamp(14), c16 time, c17 year       #
#                                                                          #
############################################################################
--disable_query_log
select '-- insert into date/time columns --' as test_sequence ;
--enable_query_log
######## INSERT into .. date/time columns values(VARCHAR(19),LONGTEXT) ########
--disable_query_log
set @arg00= '1991-01-01 01:01:01' ;
insert into t9
  ( c1, c13, c14, c15, c16, c17 )
values
  ( 20, '1991-01-01 01:01:01', '1991-01-01 01:01:01', '1991-01-01 01:01:01',
        '1991-01-01 01:01:01', '1991-01-01 01:01:01') ;
insert into t9
  ( c1, c13, c14, c15, c16, c17 )
values
  ( 21, @arg00, @arg00, @arg00, @arg00, @arg00) ;
prepare stmt1 from "insert into t9
  ( c1, c13, c14, c15, c16, c17 )
values
  ( 22, '1991-01-01 01:01:01', '1991-01-01 01:01:01', '1991-01-01 01:01:01',
        '1991-01-01 01:01:01', '1991-01-01 01:01:01')" ;
execute stmt1 ;
prepare stmt2 from "insert into t9
  ( c1, c13, c14, c15, c16, c17 )
values
  ( 23, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00 ;


######## INSERT into .. date/time columns values(DATETIME,LONGBLOB) ########
set @arg00= CAST('1991-01-01 01:01:01' as datetime) ;
insert into t9
  ( c1, c13, c14, c15, c16, c17 )
values
  ( 30, CAST('1991-01-01 01:01:01' as datetime), 
        CAST('1991-01-01 01:01:01' as datetime),
        CAST('1991-01-01 01:01:01' as datetime),
        CAST('1991-01-01 01:01:01' as datetime),
        CAST('1991-01-01 01:01:01' as datetime)) ;
insert into t9
  ( c1, c13, c14, c15, c16, c17 )
values
  ( 31, @arg00, @arg00, @arg00, @arg00, @arg00) ;
prepare stmt1 from "insert into t9
  ( c1, c13, c14, c15, c16, c17 )
values
  ( 32, CAST('1991-01-01 01:01:01' as datetime),
        CAST('1991-01-01 01:01:01' as datetime),
        CAST('1991-01-01 01:01:01' as datetime),
        CAST('1991-01-01 01:01:01' as datetime),
        CAST('1991-01-01 01:01:01' as datetime))" ;
execute stmt1 ;
prepare stmt2 from "insert into t9
  ( c1, c13, c14, c15, c16, c17 )
values
  ( 33, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00 ;


######## INSERT into .. date/time columns values(BIGINT(n),BIGINT) ########
set @arg00= 2000000000 ;
insert into t9
  ( c1, c13, c14, c15, c16, c17 )
values
  ( 40, 2000000000, 2000000000, 2000000000, 2000000000, 2000000000 ) ;
insert into t9
  ( c1, c13, c14, c15, c16, c17 )
values
  ( 41, @arg00, @arg00, @arg00, @arg00, @arg00) ;
prepare stmt1 from "insert into t9
  ( c1, c13, c14, c15, c16, c17 )
values
  ( 42, 2000000000, 2000000000, 2000000000, 2000000000, 2000000000 )" ;
execute stmt1 ;
prepare stmt2 from "insert into t9
  ( c1, c13, c14, c15, c16, c17 )
values
  ( 43, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00 ;


######## INSERT into .. date/time columns values(DOUBLE(m,n),DOUBLE) ########
set @arg00= 1.0e+10 ;
insert into t9
  ( c1, c13, c14, c15, c16, c17 )
values
  ( 50, 1.0e+10, 1.0e+10, 1.0e+10, 1.0e+10, 1.0e+10 ) ;
insert into t9
  ( c1, c13, c14, c15, c16, c17 )
values
  ( 51, @arg00, @arg00, @arg00, @arg00, @arg00) ;
prepare stmt1 from "insert into t9
  ( c1, c13, c14, c15, c16, c17 )
values
  ( 52, 1.0e+10, 1.0e+10, 1.0e+10, 1.0e+10, 1.0e+10 )" ;
execute stmt1 ;
prepare stmt2 from "insert into t9
  ( c1, c13, c14, c15, c16, c17 )
values
  ( 53, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00 ;


######## INSERT into .. date/time columns values(LONGBLOB,NULL) ########
# Attention: c15 is timestamp and the manual says:
#    The first TIMESTAMP column in table row automatically is updated 
#    to the current timestamp when the value of any other column in the
#    row is changed, unless the TIMESTAMP column explicitly is assigned 
#    a value other than NULL.
# That's why a fixed NOT NULL value is inserted.
set @arg00= 'abc' ;
set @arg00= NULL ;
insert into t9
  ( c1, c13, c14, c15, c16, c17 )
values
  ( 60, NULL, NULL, '1991-01-01 01:01:01',
        NULL, NULL) ;
insert into t9
  ( c1, c13, c14, c15, c16, c17 )
values
  ( 61, @arg00, @arg00, '1991-01-01 01:01:01', @arg00, @arg00) ;
prepare stmt1 from "insert into t9
  ( c1, c13, c14, c15, c16, c17 )
values
  ( 62, NULL, NULL, '1991-01-01 01:01:01',
        NULL, NULL)" ;
execute stmt1 ;
prepare stmt2 from "insert into t9
  ( c1, c13, c14, c15, c16, c17 )
values
  ( 63, ?, ?, '1991-01-01 01:01:01', ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00 ;


######## INSERT into .. date/time columns values(BIGINT,NULL) ########
set @arg00= 8 ;
set @arg00= NULL ;
insert into t9
  ( c1, c13, c14, c15, c16, c17 )
values
  ( 71, @arg00, @arg00, '1991-01-01 01:01:01', @arg00, @arg00) ;
prepare stmt2 from "insert into t9
  ( c1, c13, c14, c15, c16, c17 )
values
  ( 73, ?, ?, '1991-01-01 01:01:01', ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00 ;


######## INSERT into .. date/time columns values(DOUBLE,NULL) ########
set @arg00= 8.0 ;
set @arg00= NULL ;
insert into t9
  ( c1, c13, c14, c15, c16, c17 )
values
  ( 81, @arg00, @arg00, '1991-01-01 01:01:01', @arg00, @arg00) ;
prepare stmt2 from "insert into t9
  ( c1, c13, c14, c15, c16, c17 )
values
  ( 83, ?, ?, '1991-01-01 01:01:01', ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00 ;

--enable_query_log

######## SELECT of all inserted records ########
select c1, c13, c14, c15, c16, c17 from t9 order by c1 ;


--disable_query_log
select '-- select .. where date/time column = .. --' as test_sequence ;
--enable_query_log
######## SELECT .. WHERE column(date/time/..)=value(CHAR(n)/LONGTEXT) ########
set @arg00= '1991-01-01 01:01:01' ;
select 'true' as found from t9 
where c1= 20 and c13= '1991-01-01 01:01:01' and c14= '1991-01-01 01:01:01' and
  c15= '1991-01-01 01:01:01' and c16= '1991-01-01 01:01:01' and
  c17= '1991-01-01 01:01:01' ;
select 'true' as found from t9 
where c1= 20 and c13= @arg00 and c14= @arg00 and c15= @arg00 and c16= @arg00
  and c17= @arg00 ;
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and c13= '1991-01-01 01:01:01' and c14= '1991-01-01 01:01:01' and
  c15= '1991-01-01 01:01:01' and c16= '1991-01-01 01:01:01' and
  c17= '1991-01-01 01:01:01'" ;
execute stmt1 ;
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and c13= ? and c14= ? and c15= ? and c16= ? and c17= ?" ;
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00 ;


######## SELECT .. WHERE column(date/time/..)=value(DATETIME/LONGBLOB) ########
set @arg00= CAST('1991-01-01 01:01:01' as datetime) ;
select 'true' as found from t9 
where c1= 20 and c13= CAST('1991-01-01 01:01:01' as datetime) and
  c14= CAST('1991-01-01 01:01:01' as datetime) and
  c15= CAST('1991-01-01 01:01:01' as datetime) and
  c16= CAST('1991-01-01 01:01:01' as datetime) and
  c17= CAST('1991-01-01 01:01:01' as datetime) ;
select 'true' as found from t9 
where c1= 20 and c13= @arg00 and c14= @arg00 and c15= @arg00 and c16= @arg00
  and c17= @arg00 ;
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and c13= CAST('1991-01-01 01:01:01' as datetime) and
  c14= CAST('1991-01-01 01:01:01' as datetime) and
  c15= CAST('1991-01-01 01:01:01' as datetime) and
  c16= CAST('1991-01-01 01:01:01' as datetime) and
  c17= CAST('1991-01-01 01:01:01' as datetime)" ;
execute stmt1 ;
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and c13= ? and c14= ? and c15= ? and c16= ? and c17= ?" ;
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00 ;


######## SELECT .. WHERE column(year)=value(INT(10)/BIGINT) ########
set @arg00= 1991 ;
select 'true' as found from t9 
where c1= 20 and c17= 1991 ;
select 'true' as found from t9 
where c1= 20 and c17= @arg00 ;
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and c17= 1991" ;
execute stmt1 ;
prepare stmt1 from "select 'true' as found from t9
where c1= 20 and c17= ?" ;
execute stmt1 using @arg00 ;


######## SELECT .. WHERE column(year)=value(DOUBLE(m,n)/DOUBLE) ########
set @arg00= 1.991e+3 ;
select 'true' as found from t9 
where c1= 20 and abs(c17 - 1.991e+3) < 0.01 ;
select 'true' as found from t9 
where c1= 20 and abs(c17 - @arg00) < 0.01 ;
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and abs(c17 - 1.991e+3) < 0.01" ;
execute stmt1 ;
prepare stmt1 from "select 'true' as found from t9
where c1= 20 and abs(c17 - ?) < 0.01" ;
execute stmt1 using @arg00 ;

