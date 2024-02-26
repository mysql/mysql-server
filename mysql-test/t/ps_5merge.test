--source include/have_myisam.inc 
#Get deafult engine value
--let $DEFAULT_ENGINE = `select @@global.default_storage_engine`

###############################################
#                                             #
#  Prepared Statements test on MERGE tables   #
#                                             #
###############################################

#    
# NOTE: PLEASE SEE ps_1general.test (bottom) 
#       BEFORE ADDING NEW TEST CASES HERE !!!

use test;

--disable_warnings
drop table if exists t1, t1_1, t1_2,
     t9, t9_1, t9_2;
--enable_warnings
let $type= 'MYISAM' ;
-- source include/ps_create.inc
rename table t1 to t1_1, t9 to t9_1 ;
-- source include/ps_create.inc
rename table t1 to t1_2, t9 to t9_2 ;

create table t1
(
  a int, b varchar(30),
  primary key(a)
) ENGINE = MERGE UNION=(t1_1,t1_2)
INSERT_METHOD=FIRST;
create table t9
(
  c1  tinyint, c2  smallint, c3  mediumint, c4  int,
  c5  integer, c6  bigint, c7  float, c8  double,
  c9  double precision, c10 real, c11 decimal(7, 4), c12 numeric(8, 4),
  c13 date, c14 datetime, c15 timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, c16 time,
  c17 year, c18 tinyint, c19 bool, c20 char,
  c21 char(10), c22 varchar(30), c23 tinyblob, c24 tinytext,
  c25 blob, c26 text, c27 mediumblob, c28 mediumtext,
  c29 longblob, c30 longtext, c31 enum('one', 'two', 'three'),
  c32 set('monday', 'tuesday', 'wednesday'),
  primary key(c1)
)  ENGINE = MERGE UNION=(t9_1,t9_2)
INSERT_METHOD=FIRST;
-- source include/ps_renew.inc

-- source include/ps_query.inc
-- source include/ps_modify.inc
# no test of ps_modify1, because insert .. select 
# is not allowed on MERGE tables
# -- source include/ps_modify1.inc
-- source include/ps_conv.inc

# Let us try the same tests with INSERT_METHOD=LAST
drop table t1, t9 ;
create table t1
(
  a int, b varchar(30),
  primary key(a)
) ENGINE = MERGE UNION=(t1_1,t1_2)
INSERT_METHOD=LAST;
create table t9
(
  c1  tinyint, c2  smallint, c3  mediumint, c4  int,
  c5  integer, c6  bigint, c7  float, c8  double,
  c9  double precision, c10 real, c11 decimal(7, 4), c12 numeric(8, 4),
  c13 date, c14 datetime, c15 timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, c16 time,
  c17 year, c18 tinyint, c19 bool, c20 char,
  c21 char(10), c22 varchar(30), c23 tinyblob, c24 tinytext,
  c25 blob, c26 text, c27 mediumblob, c28 mediumtext,
  c29 longblob, c30 longtext, c31 enum('one', 'two', 'three'),
  c32 set('monday', 'tuesday', 'wednesday'),
  primary key(c1)
)  ENGINE = MERGE UNION=(t9_1,t9_2)
INSERT_METHOD=LAST;
-- source include/ps_renew.inc

-- source include/ps_query.inc
-- source include/ps_modify.inc
# no test of ps_modify1, because insert .. select
# is not allowed on MERGE tables
# -- source include/ps_modify1.inc
-- source include/ps_conv.inc

drop table t1, t1_1, t1_2, 
           t9_1, t9_2, t9;

# End of 4.1 tests
