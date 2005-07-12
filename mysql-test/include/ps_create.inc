############### include/ps_create.inc ##################
#                                                      #
#  drop + create the tables used in most PS test cases #
#         t/ps_*.test                                  #
#                                                      #
########################################################

#    
# NOTE: PLEASE SEE ps_1general.test (bottom) 
#       BEFORE ADDING NEW TABLES HERE !!!
#
# Please be aware, that this file will be sourced by several
# test case files stored within the subdirectory 't'.
# So every change here will affect several test cases.

#----------- Please insert your table definitions here ----------#


#---- Please do not alter the following table definitions -------#

--disable_warnings
drop table if exists t1, t9 ;
--enable_warnings

eval create table t1
(
  a int, b varchar(30),
  primary key(a)
) engine = $type ;

eval create table t9 
(
  c1  tinyint, c2  smallint, c3  mediumint, c4  int,
  c5  integer, c6  bigint, c7  float, c8  double,
  c9  double precision, c10 real, c11 decimal(7, 4), c12 numeric(8, 4),
  c13 date, c14 datetime, c15 timestamp, c16 time,
  c17 year, c18 tinyint, c19 bool, c20 char,
  c21 char(10), c22 varchar(30), c23 tinyblob, c24 tinytext,
  c25 blob, c26 text, c27 mediumblob, c28 mediumtext,
  c29 longblob, c30 longtext, c31 enum('one', 'two', 'three'),
  c32 set('monday', 'tuesday', 'wednesday'),
  primary key(c1)
) engine = $type ;
