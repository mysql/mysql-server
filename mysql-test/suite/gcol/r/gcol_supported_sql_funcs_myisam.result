SET @@session.default_storage_engine = 'MyISAM';
#
# NUMERIC FUNCTIONS
#
# ABS()
set sql_warnings = 1;
create table t1 (a int, b int generated always as (abs(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (abs(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (-1, default);
select * from t1;
a	b
-1	1
drop table t1;
set sql_warnings = 0;
# ACOS()
set sql_warnings = 1;
create table t1 (a double, b double generated always as (format(acos(a),6)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` double GENERATED ALWAYS AS (format(acos(`a`),6)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (1, default);
insert into t1 values (1.0001,default);
insert into t1 values (0,default);
select * from t1;
a	b
0	1.570796
1	0
1.0001	NULL
drop table t1;
set sql_warnings = 0;
# ASIN()
set sql_warnings = 1;
create table t1 (a double, b double generated always as (format(asin(a),6)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` double GENERATED ALWAYS AS (format(asin(`a`),6)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (0.2, default);
insert into t1 values (1.0001,default);
select * from t1;
a	b
0.2	0.201358
1.0001	NULL
drop table t1;
set sql_warnings = 0;
#ATAN
set sql_warnings = 1;
create table t1 (a double, b double, c double generated always as (format(atan(a,b),6)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` double DEFAULT NULL,
  `c` double GENERATED ALWAYS AS (format(atan(`a`,`b`),6)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (-2,2,default);
insert into t1 values (format(PI(),6),0,default);
select * from t1;
a	b	c
-2	2	-0.785398
3.141593	0	1.570796
drop table t1;
set sql_warnings = 0;
set sql_warnings = 1;
create table t1 (a double, c double generated always as (format(atan(a),6)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `c` double GENERATED ALWAYS AS (format(atan(`a`),6)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (-2,default);
insert into t1 values (format(PI(),6),default);
select * from t1;
a	c
-2	-1.107149
3.141593	1.262627
drop table t1;
set sql_warnings = 0;
# ATAN2
set sql_warnings = 1;
create table t1 (a double, b double, c double generated always as (format(atan2(a,b),6)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` double DEFAULT NULL,
  `c` double GENERATED ALWAYS AS (format(atan(`a`,`b`),6)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (-2,2,default);
insert into t1 values (format(PI(),6),0,default);
select * from t1;
a	b	c
-2	2	-0.785398
3.141593	0	1.570796
drop table t1;
set sql_warnings = 0;
# CEIL()
set sql_warnings = 1;
create table t1 (a double, b int generated always as (ceil(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (ceiling(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (1.23,default);
insert into t1 values (-1.23,default);
select * from t1;
a	b
-1.23	-1
1.23	2
drop table t1;
set sql_warnings = 0;
# CONV()
set sql_warnings = 1;
create table t1 (a varchar(10), b int, c int, d varchar(10) generated always as (conv(a,b,c)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` int DEFAULT NULL,
  `c` int DEFAULT NULL,
  `d` varchar(10) GENERATED ALWAYS AS (conv(`a`,`b`,`c`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('a',16,2,default);
insert into t1 values ('6e',18,8,default);
insert into t1 values (-17,10,-18,default);
insert into t1 values (10+'10'+'10'+0xa,10,10,default);
select * from t1;
a	b	c	d
-17	10	-18	-H
40	10	10	40
6e	18	8	172
a	16	2	1010
drop table t1;
set sql_warnings = 0;
# COS()
set sql_warnings = 1;
create table t1 (a double, b double generated always as (format(cos(a),6)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` double GENERATED ALWAYS AS (format(cos(`a`),6)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (format(PI(),6),default);
select * from t1;
a	b
3.141593	-1
drop table t1;
set sql_warnings = 0;
# COT()
set sql_warnings = 1;
create table t1 (a double, b double generated always as (format(cot(a),6)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` double GENERATED ALWAYS AS (format(cot(`a`),6)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (0,default);
insert into t1 values (12,default);
select * from t1;
a	b
12	-1.572673
drop table t1;
set sql_warnings = 0;
# CRC32()
set sql_warnings = 1;
create table t1 (a varchar(10), b bigint generated always as (crc32(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` bigint GENERATED ALWAYS AS (crc32(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('MySQL',default);
insert into t1 values ('mysql',default);
select * from t1;
a	b
MySQL	3259397556
mysql	2501908538
drop table t1;
set sql_warnings = 0;
# DEGREES()
set sql_warnings = 1;
create table t1 (a double, b double generated always as (format(degrees(a),6)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` double GENERATED ALWAYS AS (format(degrees(`a`),6)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (format(PI(),6),default);
insert into t1 values (format(PI()/2,6),default);
select * from t1;
a	b
1.570796	89.999981
3.141593	180.00002
drop table t1;
set sql_warnings = 0;
# /
set sql_warnings = 1;
create table t1 (a double, b double generated always as (a/2) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` double GENERATED ALWAYS AS ((`a` / 2)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (2,default);
select * from t1;
a	b
2	1
drop table t1;
set sql_warnings = 0;
# EXP()
set sql_warnings = 1;
create table t1 (a double, b double generated always as (format(exp(a),6)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` double GENERATED ALWAYS AS (format(exp(`a`),6)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (2,default);
insert into t1 values (-2,default);
insert into t1 values (0,default);
select * from t1;
a	b
-2	0.135335
0	1
2	7.389056
drop table t1;
set sql_warnings = 0;
# FLOOR()
set sql_warnings = 1;
create table t1 (a double, b bigint generated always as (floor(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` bigint GENERATED ALWAYS AS (floor(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (1.23,default);
insert into t1 values (-1.23,default);
select * from t1;
a	b
-1.23	-2
1.23	1
drop table t1;
set sql_warnings = 0;
# LN()
set sql_warnings = 1;
create table t1 (a double, b double generated always as (format(ln(a),6)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` double GENERATED ALWAYS AS (format(ln(`a`),6)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (2,default);
insert into t1 values (-2,default);
select * from t1;
a	b
2	0.693147
drop table t1;
set sql_warnings = 0;
# LOG()
set sql_warnings = 1;
create table t1 (a double, b double, c double generated always as (format(log(a,b),6)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` double DEFAULT NULL,
  `c` double GENERATED ALWAYS AS (format(log(`a`,`b`),6)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (2,65536,default);
insert into t1 values (10,100,default);
insert into t1 values (1,100,default);
select * from t1;
a	b	c
10	100	2
2	65536	16
drop table t1;
set sql_warnings = 0;
set sql_warnings = 1;
create table t1 (a double, b double generated always as (format(log(a),6)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` double GENERATED ALWAYS AS (format(log(`a`),6)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (2,default);
insert into t1 values (-2,default);
select * from t1;
a	b
2	0.693147
drop table t1;
set sql_warnings = 0;
# LOG2()
set sql_warnings = 1;
create table t1 (a double, b double generated always as (format(log2(a),6)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` double GENERATED ALWAYS AS (format(log2(`a`),6)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (65536,default);
insert into t1 values (-100,default);
select * from t1;
a	b
65536	16
drop table t1;
set sql_warnings = 0;
# LOG10()
set sql_warnings = 1;
create table t1 (a double, b double generated always as (format(log10(a),6)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` double GENERATED ALWAYS AS (format(log10(`a`),6)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (2,default);
insert into t1 values (100,default);
insert into t1 values (-100,default);
select * from t1;
a	b
100	2
2	0.30103
drop table t1;
set sql_warnings = 0;
# -
set sql_warnings = 1;
create table t1 (a double, b double generated always as (a-1) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` double GENERATED ALWAYS AS ((`a` - 1)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (2,default);
select * from t1;
a	b
2	1
drop table t1;
set sql_warnings = 0;
# MOD()
set sql_warnings = 1;
create table t1 (a int, b int generated always as (mod(a,10)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` int GENERATED ALWAYS AS ((`a` % 10)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (1,default);
insert into t1 values (11,default);
select * from t1;
a	b
1	1
11	1
drop table t1;
set sql_warnings = 0;
# %
set sql_warnings = 1;
create table t1 (a int, b int generated always as (a % 10) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` int GENERATED ALWAYS AS ((`a` % 10)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (1,default);
insert into t1 values (11,default);
select * from t1;
a	b
1	1
11	1
drop table t1;
set sql_warnings = 0;
# OCT()
set sql_warnings = 1;
create table t1 (a double, b varchar(10) generated always as (oct(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (conv(`a`,10,8)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (12,default);
select * from t1;
a	b
12	14
drop table t1;
set sql_warnings = 0;
# PI()
set sql_warnings = 1;
create table t1 (a double, b double generated always as (format(PI()*a*a,6)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` double GENERATED ALWAYS AS (format(((pi() * `a`) * `a`),6)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (1,default);
select * from t1;
a	b
1	3.141593
drop table t1;
set sql_warnings = 0;
# +
set sql_warnings = 1;
create table t1 (a int, b int generated always as (a+1) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` int GENERATED ALWAYS AS ((`a` + 1)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (1,default);
select * from t1;
a	b
1	2
drop table t1;
set sql_warnings = 0;
# POW, POWER
set sql_warnings = 1;
create table t1 (a int, b int generated always as (pow(a,2)) virtual, c int generated always as (power(a,2)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (pow(`a`,2)) VIRTUAL,
  `c` int GENERATED ALWAYS AS (pow(`a`,2)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (1,default,default);
insert into t1 values (2,default,default);
select * from t1;
a	b	c
1	1	1
2	4	4
drop table t1;
set sql_warnings = 0;
# RADIANS()
set sql_warnings = 1;
create table t1 (a double, b double generated always as (format(radians(a),6)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` double GENERATED ALWAYS AS (format(radians(`a`),6)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (90,default);
select * from t1;
a	b
90	1.570796
drop table t1;
set sql_warnings = 0;
# ROUND()
set sql_warnings = 1;
create table t1 (a double, b int generated always as (round(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (round(`a`,0)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (-1.23,default);
insert into t1 values (-1.58,default);
insert into t1 values (1.58,default);
select * from t1;
a	b
-1.23	-1
-1.58	-2
1.58	2
drop table t1;
set sql_warnings = 0;
set sql_warnings = 1;
create table t1 (a double, b double, c int generated always as (round(a,b)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` double DEFAULT NULL,
  `c` int GENERATED ALWAYS AS (round(`a`,`b`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (1.298,1,default);
insert into t1 values (1.298,0,default);
insert into t1 values (23.298,-1,default);
select * from t1;
a	b	c
1.298	0	1
1.298	1	1
23.298	-1	20
drop table t1;
set sql_warnings = 0;
# SIGN()
set sql_warnings = 1;
create table t1 (a double, b int generated always as (sign(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (sign(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (-32,default);
insert into t1 values (0,default);
insert into t1 values (234,default);
select * from t1;
a	b
-32	-1
0	0
234	1
drop table t1;
set sql_warnings = 0;
# SIN()
set sql_warnings = 1;
create table t1 (a double, b double generated always as (format(sin(a),6)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` double GENERATED ALWAYS AS (format(sin(`a`),6)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (format(PI()/2,6),default);
select * from t1;
a	b
1.570796	1
drop table t1;
set sql_warnings = 0;
# SQRT()
set sql_warnings = 1;
create table t1 (a double, b double generated always as (format(sqrt(a),6)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` double GENERATED ALWAYS AS (format(sqrt(`a`),6)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (4,default);
insert into t1 values (20,default);
insert into t1 values (-16,default);
select * from t1;
a	b
-16	NULL
20	4.472136
4	2
drop table t1;
set sql_warnings = 0;
# TAN()
set sql_warnings = 1;
create table t1 (a double, b double generated always as (format(tan(a),6)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` double GENERATED ALWAYS AS (format(tan(`a`),6)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (format(PI(),6),default);
insert into t1 values (format(PI()+1,6),default);
select * from t1;
a	b
3.141593	0
4.141593	1.557409
drop table t1;
set sql_warnings = 0;
# *
set sql_warnings = 1;
create table t1 (a double, b double generated always as (a*3) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` double GENERATED ALWAYS AS ((`a` * 3)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (0,default);
insert into t1 values (1,default);
insert into t1 values (2,default);
select * from t1;
a	b
0	0
1	3
2	6
drop table t1;
set sql_warnings = 0;
# TRUNCATE()
set sql_warnings = 1;
create table t1 (a double, b double generated always as (truncate(a,4)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` double GENERATED ALWAYS AS (truncate(`a`,4)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (1.223,default);
insert into t1 values (1.999,default);
insert into t1 values (1.999,default);
insert into t1 values (122,default);
select * from t1;
a	b
1.223	1.223
1.999	1.999
1.999	1.999
122	122
drop table t1;
set sql_warnings = 0;
# Unary -
set sql_warnings = 1;
create table t1 (a double, b double generated always as (-a) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` double GENERATED ALWAYS AS (-(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (1,default);
insert into t1 values (-1,default);
select * from t1;
a	b
-1	1
1	-1
drop table t1;
set sql_warnings = 0;
#
# STRING FUNCTIONS
#
# ASCII()
set sql_warnings = 1;
create table t1 (a char(2), b int generated always as (ascii(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` char(2) DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (ascii(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2',default);
insert into t1 values (2,default);
insert into t1 values ('dx',default);
select * from t1;
a	b
2	50
2	50
dx	100
drop table t1;
set sql_warnings = 0;
# BIN()
set sql_warnings = 1;
create table t1 (a int, b varchar(10) generated always as (bin(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (conv(`a`,10,2)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (12,default);
select * from t1;
a	b
12	1100
drop table t1;
set sql_warnings = 0;
# BIT_LENGTH()
set sql_warnings = 1;
create table t1 (a varchar(10), b bigint generated always as (bit_length(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` bigint GENERATED ALWAYS AS (bit_length(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('text',default);
select * from t1;
a	b
text	32
drop table t1;
set sql_warnings = 0;
# CHAR_LENGTH()
set sql_warnings = 1;
create table t1 (a varchar(10), b bigint generated always as (char_length(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` bigint GENERATED ALWAYS AS (char_length(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('text',default);
select * from t1;
a	b
text	4
drop table t1;
set sql_warnings = 0;
# CHAR()
set sql_warnings = 1;
create table t1 (a int, b int, c varbinary(10) generated always as (char(a,b)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` int DEFAULT NULL,
  `c` varbinary(10) GENERATED ALWAYS AS (char(`a`,`b`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (77,121,default);
select * from t1;
a	b	c
77	121	My
drop table t1;
set sql_warnings = 0;
# CHARACTER_LENGTH()
set sql_warnings = 1;
create table t1 (a varchar(10), b bigint generated always as (character_length(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` bigint GENERATED ALWAYS AS (char_length(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('text',default);
select * from t1;
a	b
text	4
drop table t1;
set sql_warnings = 0;
# CONCAT_WS()
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10), c varchar(20) generated always as (concat_ws(',',a,b)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) DEFAULT NULL,
  `c` varchar(20) GENERATED ALWAYS AS (concat_ws(_latin1',',`a`,`b`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('value1','value2',default);
select * from t1;
a	b	c
value1	value2	value1,value2
drop table t1;
set sql_warnings = 0;
# CONCAT()
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10), c varchar(20) generated always as (concat(a,',',b)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) DEFAULT NULL,
  `c` varchar(20) GENERATED ALWAYS AS (concat(`a`,_latin1',',`b`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('value1','value2',default);
select * from t1;
a	b	c
value1	value2	value1,value2
drop table t1;
set sql_warnings = 0;
# ELT()
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10), c int, d varchar(10) generated always as (elt(c,a,b)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) DEFAULT NULL,
  `c` int DEFAULT NULL,
  `d` varchar(10) GENERATED ALWAYS AS (elt(`c`,`a`,`b`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('value1','value2',1,default);
insert into t1 values ('value1','value2',2,default);
select * from t1;
a	b	c	d
value1	value2	1	value1
value1	value2	2	value2
drop table t1;
set sql_warnings = 0;
# EXPORT_SET()
set sql_warnings = 1;
create table t1 (a int, b varchar(10) generated always as (export_set(a,'1','0','',10)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (export_set(`a`,_utf8mb4'1',_utf8mb4'0',_utf8mb4'',10)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (6,default);
select * from t1;
a	b
6	0110000000
drop table t1;
set sql_warnings = 0;
# FIELD()
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10), c int generated always as (field('aa',a,b)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) DEFAULT NULL,
  `c` int GENERATED ALWAYS AS (field(_utf8mb4'aa',`a`,`b`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('aa','bb',default);
insert into t1 values ('bb','aa',default);
select * from t1;
a	b	c
aa	bb	1
bb	aa	2
drop table t1;
set sql_warnings = 0;
# FIND_IN_SET()
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10), c int generated always as (find_in_set(a,b)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) DEFAULT NULL,
  `c` int GENERATED ALWAYS AS (find_in_set(`a`,`b`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('aa','aa,bb,cc',default);
insert into t1 values ('aa','bb,aa,cc',default);
select * from t1;
a	b	c
aa	aa,bb,cc	1
aa	bb,aa,cc	2
drop table t1;
set sql_warnings = 0;
# FORMAT()
set sql_warnings = 1;
create table t1 (a double, b varchar(20) generated always as (format(a,2)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` double DEFAULT NULL,
  `b` varchar(20) GENERATED ALWAYS AS (format(`a`,2)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (12332.123456,default);
select * from t1;
a	b
12332.123456	12,332.12
drop table t1;
set sql_warnings = 0;
# HEX()
set sql_warnings = 1;
create table t1 (a int, b varchar(10) generated always as (hex(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (hex(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (17,default);
select * from t1;
a	b
17	11
drop table t1;
set sql_warnings = 0;
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10) generated always as (hex(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (hex(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('abc',default);
select * from t1;
a	b
abc	616263
drop table t1;
set sql_warnings = 0;
# INSERT()
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10), c varchar(20) generated always as (insert(a,length(a),length(b),b)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) DEFAULT NULL,
  `c` varchar(20) GENERATED ALWAYS AS (insert(`a`,length(`a`),length(`b`),`b`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('start,','end',default);
select * from t1;
a	b	c
start,	end	startend
drop table t1;
set sql_warnings = 0;
# INSTR()
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10), c int generated always as (instr(a,b)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) DEFAULT NULL,
  `c` int GENERATED ALWAYS AS (locate(`b`,`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('foobarbar,','bar',default);
insert into t1 values ('xbar,','foobar',default);
select * from t1;
a	b	c
foobarbar,	bar	4
xbar,	foobar	0
drop table t1;
set sql_warnings = 0;
# LCASE()
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10) generated always as (lcase(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (lower(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('MySQL',default);
select * from t1;
a	b
MySQL	mysql
drop table t1;
set sql_warnings = 0;
# LEFT()
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(5) generated always as (left(a,5)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(5) GENERATED ALWAYS AS (left(`a`,5)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('foobarbar',default);
select * from t1;
a	b
foobarbar	fooba
drop table t1;
set sql_warnings = 0;
# LENGTH()
set sql_warnings = 1;
create table t1 (a varchar(10), b int generated always as (length(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (length(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('text',default);
select * from t1;
a	b
text	4
drop table t1;
set sql_warnings = 0;
# LIKE
set sql_warnings = 1;
create table t1 (a varchar(10), b bool generated always as (a like 'H%o') virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` tinyint(1) GENERATED ALWAYS AS ((`a` like _latin1'H%o')) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('Hello',default);
insert into t1 values ('MySQL',default);
select * from t1;
a	b
Hello	1
MySQL	0
drop table t1;
set sql_warnings = 0;
# LOCATE()
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10) generated always as (locate('bar',a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (locate(_latin1'bar',`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('foobarbar',default);
select * from t1;
a	b
foobarbar	4
drop table t1;
set sql_warnings = 0;
# LOWER()
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10) generated always as (lower(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (lower(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('MySQL',default);
select * from t1;
a	b
MySQL	mysql
drop table t1;
set sql_warnings = 0;
# LPAD()
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10) generated always as (lpad(a,4,' ')) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (lpad(`a`,4,_latin1' ')) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('MySQL',default);
insert into t1 values ('M',default);
select * from t1;
a	b
M	   M
MySQL	MySQ
drop table t1;
set sql_warnings = 0;
# LTRIM()
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10) generated always as (ltrim(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (ltrim(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('  MySQL',default);
insert into t1 values ('MySQL',default);
select * from t1;
a	b
  MySQL	MySQL
MySQL	MySQL
drop table t1;
set sql_warnings = 0;
# MAKE_SET()
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10), c int, d varchar(30) generated always as (make_set(c,a,b)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) DEFAULT NULL,
  `c` int DEFAULT NULL,
  `d` varchar(30) GENERATED ALWAYS AS (make_set(`c`,`a`,`b`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('a','b',1,default);
insert into t1 values ('a','b',3,default);
select * from t1;
a	b	c	d
a	b	1	a
a	b	3	a,b
drop table t1;
set sql_warnings = 0;
# MID()
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10) generated always as (mid(a,1,2)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (substr(`a`,1,2)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('foobarbar',default);
select * from t1;
a	b
foobarbar	fo
drop table t1;
set sql_warnings = 0;
# NOT LIKE
set sql_warnings = 1;
create table t1 (a varchar(10), b bool generated always as (a not like 'H%o') virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` tinyint(1) GENERATED ALWAYS AS ((not((`a` like _latin1'H%o')))) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('Hello',default);
insert into t1 values ('MySQL',default);
select * from t1;
a	b
Hello	0
MySQL	1
drop table t1;
set sql_warnings = 0;
# NOT REGEXP
set sql_warnings = 1;
create table t1 (a varchar(10), b bool generated always as (a not regexp 'H.+o') virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` tinyint(1) GENERATED ALWAYS AS ((not(regexp_like(`a`,_latin1'H.+o')))) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('Hello',default);
insert into t1 values ('hello',default);
select * from t1;
a	b
Hello	0
hello	0
drop table t1;
set sql_warnings = 0;
# OCTET_LENGTH()
set sql_warnings = 1;
create table t1 (a varchar(10), b int generated always as (octet_length(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (length(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('text',default);
select * from t1;
a	b
text	4
drop table t1;
set sql_warnings = 0;
# ORD()
set sql_warnings = 1;
create table t1 (a varchar(10), b bigint generated always as (ord(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` bigint GENERATED ALWAYS AS (ord(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2',default);
select * from t1;
a	b
2	50
drop table t1;
set sql_warnings = 0;
# POSITION()
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10) generated always as (position('bar' in a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (locate(_latin1'bar',`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('foobarbar',default);
select * from t1;
a	b
foobarbar	4
drop table t1;
set sql_warnings = 0;
# QUOTE()
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10) generated always as (quote(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (quote(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('Don''t',default);
select * from t1;
a	b
Don't	'Don\'t'
drop table t1;
set sql_warnings = 0;
# REGEXP()
set sql_warnings = 1;
create table t1 (a varchar(10), b bool generated always as (a regexp 'H.+o') virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` tinyint(1) GENERATED ALWAYS AS (regexp_like(`a`,_latin1'H.+o')) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('Hello',default);
insert into t1 values ('hello',default);
select * from t1;
a	b
Hello	1
hello	1
drop table t1;
set sql_warnings = 0;
# REPEAT()
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(30) generated always as (repeat(a,3)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(30) GENERATED ALWAYS AS (repeat(`a`,3)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('MySQL',default);
select * from t1;
a	b
MySQL	MySQLMySQLMySQL
drop table t1;
set sql_warnings = 0;
# REPLACE()
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(30) generated always as (replace(a,'aa','bb')) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(30) GENERATED ALWAYS AS (replace(`a`,_latin1'aa',_utf8mb4'bb')) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('maa',default);
select * from t1;
a	b
maa	mbb
drop table t1;
set sql_warnings = 0;
# REVERSE()
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(30) generated always as (reverse(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(30) GENERATED ALWAYS AS (reverse(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('maa',default);
select * from t1;
a	b
maa	aam
drop table t1;
set sql_warnings = 0;
# RIGHT()
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10) generated always as (right(a,4)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (right(`a`,4)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('foobarbar',default);
select * from t1;
a	b
foobarbar	rbar
drop table t1;
set sql_warnings = 0;
# RLIKE()
set sql_warnings = 1;
create table t1 (a varchar(10), b bool generated always as (a rlike 'H.+o') virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` tinyint(1) GENERATED ALWAYS AS (regexp_like(`a`,_latin1'H.+o')) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('Hello',default);
insert into t1 values ('MySQL',default);
select * from t1;
a	b
Hello	1
MySQL	0
drop table t1;
set sql_warnings = 0;
# RPAD()
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10) generated always as (rpad(a,4,'??')) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (rpad(`a`,4,_latin1'??')) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('He',default);
select * from t1;
a	b
He	He??
drop table t1;
set sql_warnings = 0;
# RTRIM();
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10) generated always as (rtrim(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (rtrim(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('Hello  ',default);
select * from t1;
a	b
Hello  	Hello
drop table t1;
set sql_warnings = 0;
# SOUNDEX()
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(20) generated always as (soundex(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(20) GENERATED ALWAYS AS (soundex(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('Hello',default);
select * from t1;
a	b
Hello	H400
drop table t1;
set sql_warnings = 0;
# SOUNDS LIKE
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10), c bool generated always as (a sounds like b) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) DEFAULT NULL,
  `c` tinyint(1) GENERATED ALWAYS AS ((soundex(`a`) = soundex(`b`))) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('Hello','Hello',default);
insert into t1 values ('Hello','MySQL',default);
insert into t1 values ('Hello','hello',default);
select * from t1;
a	b	c
Hello	Hello	1
Hello	MySQL	0
Hello	hello	1
drop table t1;
set sql_warnings = 0;
# SPACE()
set sql_warnings = 1;
create table t1 (a varchar(5), b varchar(10) generated always as (concat(a,space(5))) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(5) DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (concat(`a`,space(5))) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('Hello', default);
select * from t1;
a	b
Hello	Hello     
drop table t1;
set sql_warnings = 0;
# STRCMP()
set sql_warnings = 1;
create table t1 (a varchar(9), b varchar(9), c tinyint(1) generated always as (strcmp(a,b)) virtual) charset latin1;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(9) DEFAULT NULL,
  `b` varchar(9) DEFAULT NULL,
  `c` tinyint(1) GENERATED ALWAYS AS (strcmp(`a`,`b`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('Hello','Hello', default);
insert into t1 values ('Hello','Hello1', default);
select * from t1;
a	b	c
Hello	Hello	0
Hello	Hello1	-1
drop table t1;
set sql_warnings = 0;
# SUBSTR()
set sql_warnings = 1;
create table t1 (a varchar(5), b varchar(10) generated always as (substr(a,2)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(5) DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (substr(`a`,2)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('Hello',default);
select * from t1;
a	b
Hello	ello
drop table t1;
set sql_warnings = 0;
# SUBSTRING_INDEX()
set sql_warnings = 1;
create table t1 (a varchar(15), b varchar(10) generated always as (substring_index(a,'.',2)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(15) DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (substring_index(`a`,_utf8mb4'.',2)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('www.mysql.com',default);
select * from t1;
a	b
www.mysql.com	www.mysql
drop table t1;
set sql_warnings = 0;
# SUBSTRING()
set sql_warnings = 1;
create table t1 (a varchar(5), b varchar(10) generated always as (substring(a from 2 for 2)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(5) DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (substr(`a`,2,2)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('Hello',default);
select * from t1;
a	b
Hello	el
drop table t1;
set sql_warnings = 0;
# TRIM()
set sql_warnings = 1;
create table t1 (a varchar(15), b varchar(10) generated always as (trim(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(15) DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (trim(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (' aa ',default);
select * from t1;
a	b
 aa 	aa
drop table t1;
set sql_warnings = 0;
# UCASE()
set sql_warnings = 1;
create table t1 (a varchar(5), b varchar(10) generated always as (ucase(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(5) DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (upper(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('MySQL',default);
select * from t1;
a	b
MySQL	MYSQL
drop table t1;
set sql_warnings = 0;
# UNHEX()
set sql_warnings = 1;
create table t1 (a varchar(15), b varchar(10) generated always as (unhex(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(15) DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (unhex(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('4D7953514C',default);
select * from t1;
a	b
4D7953514C	MySQL
drop table t1;
set sql_warnings = 0;
# UPPER()
set sql_warnings = 1;
create table t1 (a varchar(5), b varchar(10) generated always as (upper(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(5) DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (upper(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('MySQL',default);
select * from t1;
a	b
MySQL	MYSQL
drop table t1;
set sql_warnings = 0;
# WEIGHT_STRING()
set sql_warnings = 1;
create table t1 (a varchar(5), b varchar(10) generated always as (weight_string(a as char(4))) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(5) DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (weight_string(`a` as char(4))) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('MySQL',default);
select * from t1;
a	b
MySQL	MYSQ
drop table t1;
set sql_warnings = 0;
#
# CONTROL FLOW FUNCTIONS
#
# CASE
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(16) generated always as (case a when NULL then 'asd' when 'b' then 'B' else a end) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(16) GENERATED ALWAYS AS ((case `a` when NULL then _latin1'asd' when _latin1'b' then _latin1'B' else `a` end)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (NULL,default);
insert into t1 values ('b',default);
insert into t1 values ('c',default);
select * from t1;
a	b
NULL	NULL
b	B
c	c
drop table t1;
set sql_warnings = 0;
# IF
set sql_warnings = 1;
create table t1 (a int, b int, c int generated always as (if(a=1,a,b)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` int DEFAULT NULL,
  `c` int GENERATED ALWAYS AS (if((`a` = 1),`a`,`b`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (1,2,default);
insert into t1 values (3,4,default);
select * from t1;
a	b	c
1	2	1
3	4	4
drop table t1;
set sql_warnings = 0;
# IFNULL
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10), c varchar(10) generated always as (ifnull(a,'DEFAULT')) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) DEFAULT NULL,
  `c` varchar(10) GENERATED ALWAYS AS (ifnull(`a`,_latin1'DEFAULT')) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (NULL,'adf',default);
insert into t1 values ('a','adf',default);
select * from t1;
a	b	c
NULL	adf	DEFAULT
a	adf	a
drop table t1;
set sql_warnings = 0;
# NULLIF
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10) generated always as (nullif(a,'DEFAULT')) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (nullif(`a`,_latin1'DEFAULT')) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('DEFAULT',default);
insert into t1 values ('a',default);
select * from t1;
a	b
DEFAULT	NULL
a	a
drop table t1;
set sql_warnings = 0;
#
# OPERATORS
#
# AND, && 
set sql_warnings = 1;
create table t1 (a int, b bool generated always as (a>0 && a<2) virtual) charset latin1;
Warnings:
Warning	1287	'&&' is deprecated and will be removed in a future release. Please use AND instead
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` tinyint(1) GENERATED ALWAYS AS (((`a` > 0) and (`a` < 2))) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (-1,default);
insert into t1 values (1,default);
select * from t1;
a	b
-1	0
1	1
drop table t1;
set sql_warnings = 0;
# BETWEEN ... AND ... 
set sql_warnings = 1;
create table t1 (a int, b bool generated always as (a between 0 and 2) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` tinyint(1) GENERATED ALWAYS AS ((`a` between 0 and 2)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (-1,default);
insert into t1 values (1,default);
select * from t1;
a	b
-1	0
1	1
drop table t1;
set sql_warnings = 0;
# BINARY
set sql_warnings = 1;
create table t1 (a varchar(10), b varbinary(10) generated always as (binary a) virtual) charset latin1;
Warnings:
Warning	1287	'BINARY expr' is deprecated and will be removed in a future release. Please use CAST instead
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varbinary(10) GENERATED ALWAYS AS (cast(`a` as char charset binary)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('11',default);
insert into t1 values (1,default);
select * from t1;
a	b
1	1
11	11
drop table t1;
set sql_warnings = 0;
# &
set sql_warnings = 1;
create table t1 (a int, b int generated always as (a & 5) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` int GENERATED ALWAYS AS ((`a` & 5)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (1,default);
insert into t1 values (0,default);
select * from t1;
a	b
0	0
1	1
drop table t1;
set sql_warnings = 0;
# ~
set sql_warnings = 1;
create table t1 (a int, b int generated always as (~a) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (~(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (1,default);
select * from t1;
a	b
drop table t1;
set sql_warnings = 0;
# |
set sql_warnings = 1;
create table t1 (a int, b int generated always as (a | 5) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` int GENERATED ALWAYS AS ((`a` | 5)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (1,default);
insert into t1 values (0,default);
insert into t1 values (2,default);
select * from t1;
a	b
0	5
1	5
2	7
drop table t1;
set sql_warnings = 0;
# ^
set sql_warnings = 1;
create table t1 (a int, b int generated always as (a ^ 5) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` int GENERATED ALWAYS AS ((`a` ^ 5)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (1,default);
insert into t1 values (0,default);
insert into t1 values (2,default);
select * from t1;
a	b
0	5
1	4
2	7
drop table t1;
set sql_warnings = 0;
# DIV
set sql_warnings = 1;
create table t1 (a int, b int generated always as (a div 5) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` int GENERATED ALWAYS AS ((`a` DIV 5)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (1,default);
insert into t1 values (7,default);
select * from t1;
a	b
1	0
7	1
drop table t1;
set sql_warnings = 0;
# <=>
set sql_warnings = 1;
create table t1 (a int, b int, c bool generated always as (a <=> b) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` int DEFAULT NULL,
  `c` tinyint(1) GENERATED ALWAYS AS ((`a` <=> `b`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (1,1,default);
insert into t1 values (NULL,NULL,default);
insert into t1 values (1,NULL,default);
select * from t1;
a	b	c
1	1	1
1	NULL	0
NULL	NULL	1
drop table t1;
set sql_warnings = 0;
# =
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10), c bool generated always as (a=b) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) DEFAULT NULL,
  `c` tinyint(1) GENERATED ALWAYS AS ((`a` = `b`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('a','b',default);
insert into t1 values ('a','a',default);
select * from t1;
a	b	c
a	a	1
a	b	0
drop table t1;
set sql_warnings = 0;
# >=
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10), c bool generated always as (a >= b) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) DEFAULT NULL,
  `c` tinyint(1) GENERATED ALWAYS AS ((`a` >= `b`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('a','b',default);
insert into t1 values ('a','a',default);
select * from t1;
a	b	c
a	a	1
a	b	0
drop table t1;
set sql_warnings = 0;
# >
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10), c bool generated always as (a > b) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) DEFAULT NULL,
  `c` tinyint(1) GENERATED ALWAYS AS ((`a` > `b`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('a','b',default);
insert into t1 values ('a','a',default);
select * from t1;
a	b	c
a	a	0
a	b	0
drop table t1;
set sql_warnings = 0;
# IS NOT NULL
set sql_warnings = 1;
create table t1 (a int, b bool generated always as (a is not null) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` tinyint(1) GENERATED ALWAYS AS ((`a` is not null)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (1,default);
insert into t1 values (NULL,default);
select * from t1;
a	b
1	1
NULL	0
drop table t1;
set sql_warnings = 0;
# IS NULL
set sql_warnings = 1;
create table t1 (a int, b bool generated always as (a is null) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` tinyint(1) GENERATED ALWAYS AS ((`a` is null)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (1,default);
insert into t1 values (NULL,default);
select * from t1;
a	b
1	0
NULL	1
drop table t1;
set sql_warnings = 0;
# <<
set sql_warnings = 1;
create table t1 (a int, b int generated always as (a << 2) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` int GENERATED ALWAYS AS ((`a` << 2)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (1,default);
insert into t1 values (3,default);
select * from t1;
a	b
1	4
3	12
drop table t1;
set sql_warnings = 0;
# <=
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10), c bool generated always as (a <= b) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) DEFAULT NULL,
  `c` tinyint(1) GENERATED ALWAYS AS ((`a` <= `b`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('b','a',default);
insert into t1 values ('b','b',default);
insert into t1 values ('b','c',default);
select * from t1;
a	b	c
b	a	0
b	b	1
b	c	1
drop table t1;
set sql_warnings = 0;
# <
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10), c bool generated always as (a < b) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) DEFAULT NULL,
  `c` tinyint(1) GENERATED ALWAYS AS ((`a` < `b`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('b','a',default);
insert into t1 values ('b','b',default);
insert into t1 values ('b','c',default);
select * from t1;
a	b	c
b	a	0
b	b	0
b	c	1
drop table t1;
set sql_warnings = 0;
# NOT BETWEEN ... AND ...
set sql_warnings = 1;
create table t1 (a int, b bool generated always as (a not between 0 and 2) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` tinyint(1) GENERATED ALWAYS AS ((`a` not between 0 and 2)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (-1,default);
insert into t1 values (1,default);
select * from t1;
a	b
-1	1
1	0
drop table t1;
set sql_warnings = 0;
# <>
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10), c bool generated always as (a <> b) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) DEFAULT NULL,
  `c` tinyint(1) GENERATED ALWAYS AS ((`a` <> `b`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('b','a',default);
insert into t1 values ('b','b',default);
insert into t1 values ('b','c',default);
select * from t1;
a	b	c
b	a	1
b	b	0
b	c	1
drop table t1;
set sql_warnings = 0;
# !=
set sql_warnings = 1;
create table t1 (a varchar(10), b varchar(10), c bool generated always as (a != b) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(10) DEFAULT NULL,
  `b` varchar(10) DEFAULT NULL,
  `c` tinyint(1) GENERATED ALWAYS AS ((`a` <> `b`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('b','a',default);
insert into t1 values ('b','b',default);
insert into t1 values ('b','c',default);
select * from t1;
a	b	c
b	a	1
b	b	0
b	c	1
drop table t1;
set sql_warnings = 0;
# ||, OR
set sql_warnings = 1;
create table t1 (a int, b int generated always as (a>5 || a<3) virtual) charset latin1;
Warnings:
Warning	1287	'|| as a synonym for OR' is deprecated and will be removed in a future release. Please use OR instead
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (((`a` > 5) or (`a` < 3))) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (1,default);
insert into t1 values (4,default);
select * from t1;
a	b
1	1
4	0
drop table t1;
set sql_warnings = 0;
# >>
set sql_warnings = 1;
create table t1 (a int, b int generated always as (a >> 2) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` int GENERATED ALWAYS AS ((`a` >> 2)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (8,default);
insert into t1 values (3,default);
select * from t1;
a	b
3	0
8	2
drop table t1;
set sql_warnings = 0;
# XOR
set sql_warnings = 1;
create table t1 (a int, b int generated always as (a xor 5) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (((0 <> `a`) xor (0 <> 5))) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (0,default);
insert into t1 values (1,default);
insert into t1 values (2,default);
select * from t1;
a	b
0	1
1	0
2	0
drop table t1;
set sql_warnings = 0;
#
# DATE AND TIME FUNCTIONS
#
# ADDDATE()
set sql_warnings = 1;
create table t1 (a datetime, b datetime generated always as (adddate(a,interval 1 month)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` datetime GENERATED ALWAYS AS ((`a` + interval 1 month)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2008-08-31',default);
select * from t1;
a	b
2008-08-31 00:00:00	2008-09-30 00:00:00
drop table t1;
set sql_warnings = 0;
# ADDTIME()
set sql_warnings = 1;
create table t1 (a datetime, b datetime generated always as (addtime(a,'02:00:00')) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` datetime GENERATED ALWAYS AS (addtime(`a`,_utf8mb4'02:00:00')) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2008-08-31',default);
select * from t1;
a	b
2008-08-31 00:00:00	2008-08-31 02:00:00
drop table t1;
set sql_warnings = 0;
# CONVERT_TZ()
set sql_warnings = 1;
create table t1 (a datetime, b datetime generated always as (convert_tz(a,'MET','UTC')) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` datetime GENERATED ALWAYS AS (convert_tz(`a`,_utf8mb4'MET',_utf8mb4'UTC')) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2008-08-31',default);
select * from t1;
a	b
2008-08-31 00:00:00	2008-08-30 22:00:00
drop table t1;
set sql_warnings = 0;
# DATE_ADD()
set sql_warnings = 1;
create table t1 (a datetime, b datetime generated always as (date_add(a,interval 1 month)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` datetime GENERATED ALWAYS AS ((`a` + interval 1 month)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2008-08-31',default);
select * from t1;
a	b
2008-08-31 00:00:00	2008-09-30 00:00:00
drop table t1;
set sql_warnings = 0;
# DATE_FORMAT()
set sql_warnings = 1;
create table t1 (a datetime, b varchar(64) generated always as (date_format(a,'%W %M %D')) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` varchar(64) GENERATED ALWAYS AS (date_format(`a`,_utf8mb4'%W %M %D')) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2008-08-31',default);
select * from t1;
a	b
2008-08-31 00:00:00	Sunday August 31st
drop table t1;
set sql_warnings = 0;
# DATE_SUB()
set sql_warnings = 1;
create table t1 (a datetime, b datetime generated always as (date_sub(a,interval 1 month)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` datetime GENERATED ALWAYS AS ((`a` - interval 1 month)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2008-08-31',default);
select * from t1;
a	b
2008-08-31 00:00:00	2008-07-31 00:00:00
drop table t1;
set sql_warnings = 0;
# DATE()
set sql_warnings = 1;
create table t1 (a datetime, b datetime generated always as (date(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` datetime GENERATED ALWAYS AS (cast(`a` as date)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2008-08-31 02:00:00',default);
select * from t1;
a	b
2008-08-31 02:00:00	2008-08-31 00:00:00
drop table t1;
set sql_warnings = 0;
# DATEDIFF()
set sql_warnings = 1;
create table t1 (a datetime, b bigint generated always as (datediff(a,'2000-01-01')) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` bigint GENERATED ALWAYS AS ((to_days(`a`) - to_days(_utf8mb4'2000-01-01'))) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2008-08-31',default);
select * from t1;
a	b
2008-08-31 00:00:00	3165
drop table t1;
set sql_warnings = 0;
# DAY()
set sql_warnings = 1;
create table t1 (a datetime, b int generated always as (day(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (dayofmonth(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2008-08-31',default);
select * from t1;
a	b
2008-08-31 00:00:00	31
drop table t1;
set sql_warnings = 0;
# DAYNAME()
set sql_warnings = 1;
create table t1 (a datetime, b varchar(10) generated always as (dayname(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (dayname(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2008-08-31',default);
select * from t1;
a	b
2008-08-31 00:00:00	Sunday
drop table t1;
set sql_warnings = 0;
# DAYOFMONTH()
set sql_warnings = 1;
create table t1 (a datetime, b int generated always as (dayofmonth(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (dayofmonth(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2008-08-31',default);
select * from t1;
a	b
2008-08-31 00:00:00	31
drop table t1;
set sql_warnings = 0;
# DAYOFWEEK()
set sql_warnings = 1;
create table t1 (a datetime, b int generated always as (dayofweek(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (dayofweek(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2008-08-31',default);
select * from t1;
a	b
2008-08-31 00:00:00	1
drop table t1;
set sql_warnings = 0;
# DAYOFYEAR()
set sql_warnings = 1;
create table t1 (a datetime, b int generated always as (dayofyear(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (dayofyear(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2008-08-31',default);
select * from t1;
a	b
2008-08-31 00:00:00	244
drop table t1;
set sql_warnings = 0;
# EXTRACT
set sql_warnings = 1;
create table t1 (a datetime, b int generated always as (extract(year from a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (extract(year from `a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2008-08-31',default);
select * from t1;
a	b
2008-08-31 00:00:00	2008
drop table t1;
set sql_warnings = 0;
# FROM_DAYS()
set sql_warnings = 1;
create table t1 (a bigint, b datetime generated always as (from_days(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` bigint DEFAULT NULL,
  `b` datetime GENERATED ALWAYS AS (from_days(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (730669,default);
select * from t1;
a	b
730669	2000-07-03 00:00:00
drop table t1;
set sql_warnings = 0;
# FROM_UNIXTIME()
set sql_warnings = 1;
create table t1 (a bigint, b datetime generated always as (from_unixtime(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` bigint DEFAULT NULL,
  `b` datetime GENERATED ALWAYS AS (from_unixtime(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (1196440219,default);
select * from t1;
a	b
1196440219	2007-11-30 19:30:19
drop table t1;
set sql_warnings = 0;
# GET_FORMAT()
set sql_warnings = 1;
create table t1 (a datetime, b varchar(32) generated always as (date_format(a,get_format(DATE,'EUR'))) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` varchar(32) GENERATED ALWAYS AS (date_format(`a`,get_format(DATE, _utf8mb4'EUR'))) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2008-08-31',default);
select * from t1;
a	b
2008-08-31 00:00:00	31.08.2008
drop table t1;
set sql_warnings = 0;
# HOUR()
set sql_warnings = 1;
create table t1 (a time, b bigint generated always as (hour(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` time DEFAULT NULL,
  `b` bigint GENERATED ALWAYS AS (hour(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('10:05:03',default);
select * from t1;
a	b
10:05:03	10
drop table t1;
set sql_warnings = 0;
# LAST_DAY()
set sql_warnings = 1;
create table t1 (a datetime, b datetime generated always as (last_day(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` datetime GENERATED ALWAYS AS (last_day(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2003-02-05',default);
insert into t1 values ('2003-02-32',default);
select * from t1;
a	b
2003-02-05 00:00:00	2003-02-28 00:00:00
drop table t1;
set sql_warnings = 0;
# MAKEDATE()
set sql_warnings = 1;
create table t1 (a int, b datetime generated always as (makedate(a,1)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` datetime GENERATED ALWAYS AS (makedate(`a`,1)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (2001,default);
select * from t1;
a	b
2001	2001-01-01 00:00:00
drop table t1;
set sql_warnings = 0;
# MAKETIME()
set sql_warnings = 1;
create table t1 (a int, b time generated always as (maketime(a,1,3)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` time GENERATED ALWAYS AS (maketime(`a`,1,3)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (12,default);
select * from t1;
a	b
12	12:01:03
drop table t1;
set sql_warnings = 0;
# MICROSECOND()
set sql_warnings = 1;
create table t1 (a datetime, b bigint generated always as (microsecond(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` bigint GENERATED ALWAYS AS (microsecond(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2009-12-31 12:00:00.123456',default);
insert into t1 values ('2009-12-31 23:59:59.000010',default);
select * from t1;
a	b
2009-12-31 12:00:00	0
2009-12-31 23:59:59	0
drop table t1;
set sql_warnings = 0;
# MINUTE()
set sql_warnings = 1;
create table t1 (a datetime, b int generated always as (minute(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (minute(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2009-12-31 23:59:59.000010',default);
select * from t1;
a	b
2009-12-31 23:59:59	59
drop table t1;
set sql_warnings = 0;
# MONTH()
set sql_warnings = 1;
create table t1 (a datetime, b int generated always as (month(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (month(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2009-12-31 23:59:59.000010',default);
select * from t1;
a	b
2009-12-31 23:59:59	12
drop table t1;
set sql_warnings = 0;
# MONTHNAME()
set sql_warnings = 1;
create table t1 (a datetime, b varchar(16) generated always as (monthname(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` varchar(16) GENERATED ALWAYS AS (monthname(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2009-12-31 23:59:59.000010',default);
select * from t1;
a	b
2009-12-31 23:59:59	December
drop table t1;
set sql_warnings = 0;
# PERIOD_ADD()
set sql_warnings = 1;
create table t1 (a int, b int generated always as (period_add(a,2)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (period_add(`a`,2)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (200801,default);
select * from t1;
a	b
200801	200803
drop table t1;
set sql_warnings = 0;
# PERIOD_DIFF()
set sql_warnings = 1;
create table t1 (a int, b int, c int generated always as (period_diff(a,b)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` int DEFAULT NULL,
  `c` int GENERATED ALWAYS AS (period_diff(`a`,`b`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (200802,200703,default);
select * from t1;
a	b	c
200802	200703	11
drop table t1;
set sql_warnings = 0;
# QUARTER()
set sql_warnings = 1;
create table t1 (a datetime, b int generated always as (quarter(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (quarter(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2008-08-31',default);
select * from t1;
a	b
2008-08-31 00:00:00	3
drop table t1;
set sql_warnings = 0;
# SEC_TO_TIME()
set sql_warnings = 1;
create table t1 (a bigint, b time generated always as (sec_to_time(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` bigint DEFAULT NULL,
  `b` time GENERATED ALWAYS AS (sec_to_time(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (2378,default);
select * from t1;
a	b
2378	00:39:38
drop table t1;
set sql_warnings = 0;
# SECOND()
set sql_warnings = 1;
create table t1 (a datetime, b int generated always as (second(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (second(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('10:05:03',default);
Warnings:
Warning	4095	Delimiter ':' in position 2 in datetime value '10:05:03' at row 1 is deprecated. Prefer the standard '-'.
select * from t1;
a	b
2010-05-03 00:00:00	0
drop table t1;
set sql_warnings = 0;
# STR_TO_DATE()
set sql_warnings = 1;
create table t1 (a varchar(64), b datetime generated always as (str_to_date(a,'%m/%d/%Y')) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(64) DEFAULT NULL,
  `b` datetime GENERATED ALWAYS AS (str_to_date(`a`,_utf8mb4'%m/%d/%Y')) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('04/30/2004',default);
select * from t1;
a	b
04/30/2004	2004-04-30 00:00:00
drop table t1;
set sql_warnings = 0;
# SUBDATE()
set sql_warnings = 1;
create table t1 (a datetime, b datetime generated always as (subdate(a,interval 1 month)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` datetime GENERATED ALWAYS AS ((`a` - interval 1 month)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2008-08-31',default);
select * from t1;
a	b
2008-08-31 00:00:00	2008-07-31 00:00:00
drop table t1;
set sql_warnings = 0;
# SUBTIME()
set sql_warnings = 1;
create table t1 (a datetime, b datetime generated always as (subtime(a,'02:00:00')) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` datetime GENERATED ALWAYS AS (subtime(`a`,_utf8mb4'02:00:00')) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2008-08-31',default);
select * from t1;
a	b
2008-08-31 00:00:00	2008-08-30 22:00:00
drop table t1;
set sql_warnings = 0;
# TIME_FORMAT()
set sql_warnings = 1;
create table t1 (a datetime, b varchar(32) generated always as (time_format(a,'%r')) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` varchar(32) GENERATED ALWAYS AS (time_format(`a`,_utf8mb4'%r')) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2008-08-31 02:03:04',default);
select * from t1;
a	b
2008-08-31 02:03:04	02:03:04 AM
drop table t1;
set sql_warnings = 0;
# TIME_TO_SEC()
set sql_warnings = 1;
create table t1 (a time, b bigint generated always as (time_to_sec(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` time DEFAULT NULL,
  `b` bigint GENERATED ALWAYS AS (time_to_sec(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('22:23:00',default);
select * from t1;
a	b
22:23:00	80580
drop table t1;
set sql_warnings = 0;
# TIME()
set sql_warnings = 1;
create table t1 (a datetime, b time generated always as (time(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` time GENERATED ALWAYS AS (cast(`a` as time)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2008-08-31 02:03:04',default);
select * from t1;
a	b
2008-08-31 02:03:04	02:03:04
drop table t1;
set sql_warnings = 0;
# TIMEDIFF()
set sql_warnings = 1;
create table t1 (a datetime, b datetime, c time generated always as (timediff(a,b)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` datetime DEFAULT NULL,
  `c` time GENERATED ALWAYS AS (timediff(`a`,`b`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2008-12-31 23:59:59.000001','2008-12-30 01:01:01.000002',default);
select * from t1;
a	b	c
2008-12-31 23:59:59	2008-12-30 01:01:01	46:58:58
drop table t1;
set sql_warnings = 0;
# TIMESTAMP()
set sql_warnings = 1;
create table t1 (a datetime, b timestamp generated always as (timestamp(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` timestamp GENERATED ALWAYS AS (cast(`a` as datetime)) VIRTUAL NULL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2008-12-31',default);
select * from t1;
a	b
2008-12-31 00:00:00	2008-12-31 00:00:00
drop table t1;
set sql_warnings = 0;
# TIMESTAMPADD()
set sql_warnings = 1;
create table t1 (a datetime, b timestamp generated always as (timestampadd(minute,1,a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` timestamp GENERATED ALWAYS AS ((`a` + interval 1 minute)) VIRTUAL NULL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2003-01-02',default);
select * from t1;
a	b
2003-01-02 00:00:00	2003-01-02 00:01:00
drop table t1;
set sql_warnings = 0;
# TIMESTAMPDIFF()
set sql_warnings = 1;
create table t1 (a timestamp, c bigint generated always as (timestampdiff(MONTH, a, a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` timestamp NULL DEFAULT NULL,
  `c` bigint GENERATED ALWAYS AS (timestampdiff(MONTH,`a`,`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2003-02-01',default);
select * from t1;
a	c
2003-02-01 00:00:00	0
drop table t1;
set sql_warnings = 0;
# TO_DAYS()
set sql_warnings = 1;
create table t1 (a datetime, b bigint generated always as (to_days(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` bigint GENERATED ALWAYS AS (to_days(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2007-10-07',default);
select * from t1;
a	b
2007-10-07 00:00:00	733321
drop table t1;
set sql_warnings = 0;
# WEEK()
set sql_warnings = 1;
create table t1 (a datetime, b int generated always as (week(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (week(`a`,0)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2008-09-01',default);
select * from t1;
a	b
2008-09-01 00:00:00	35
drop table t1;
set sql_warnings = 0;
# WEEKDAY()
set sql_warnings = 1;
create table t1 (a datetime, b int generated always as (weekday(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (weekday(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2008-09-01',default);
select * from t1;
a	b
2008-09-01 00:00:00	0
drop table t1;
set sql_warnings = 0;
# WEEKOFYEAR()
set sql_warnings = 1;
create table t1 (a datetime, b int generated always as (weekofyear(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (week(`a`,3)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2008-09-01',default);
select * from t1;
a	b
2008-09-01 00:00:00	36
drop table t1;
set sql_warnings = 0;
# YEAR()
set sql_warnings = 1;
create table t1 (a datetime, b int generated always as (year(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (year(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2008-09-01',default);
select * from t1;
a	b
2008-09-01 00:00:00	2008
drop table t1;
set sql_warnings = 0;
# YEARWEEK()
set sql_warnings = 1;
create table t1 (a datetime, b int generated always as (yearweek(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` datetime DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (yearweek(`a`,0)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('2008-09-01',default);
select * from t1;
a	b
2008-09-01 00:00:00	200835
drop table t1;
set sql_warnings = 0;
#
# FULL TEXT SEARCH FUNCTIONS
#
# None.
#
# CAST FUNCTIONS AND OPERATORS
#
# CAST()
set sql_warnings = 1;
create table t1 (a int, b bigint unsigned generated always as (cast(a as unsigned)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` bigint unsigned GENERATED ALWAYS AS (cast(`a` as unsigned)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (1,default);
insert into t1 values (-1,default);
select * from t1;
a	b
-1	18446744073709551615
1	1
drop table t1;
set sql_warnings = 0;
# Convert()
set sql_warnings = 1;
create table t1 (a int, b bigint unsigned generated always as (convert(a,unsigned)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` bigint unsigned GENERATED ALWAYS AS (cast(`a` as unsigned)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (1,default);
insert into t1 values (-1,default);
select * from t1;
a	b
-1	18446744073709551615
1	1
drop table t1;
set sql_warnings = 0;
#
# XML FUNCTIONS
#
# ExtractValue()
set sql_warnings = 1;
create table t1 (a varchar(1024), b varchar(1024) generated always as (ExtractValue(a,'/b')) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(1024) DEFAULT NULL,
  `b` varchar(1024) GENERATED ALWAYS AS (extractvalue(`a`,_latin1'/b')) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('<b>text</b>',default);
select * from t1;
a	b
<b>text</b>	text
drop table t1;
set sql_warnings = 0;
# None.
#
# JSON functions
#
# JSON_PRETTY()
set sql_warnings = 1;
create table t1 (a varchar(1024), b varchar(1024) generated always as (JSON_PRETTY(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(1024) DEFAULT NULL,
  `b` varchar(1024) GENERATED ALWAYS AS (json_pretty(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('{}', default);
select * from t1;
a	b
{}	{}
drop table t1;
set sql_warnings = 0;
# JSON_STORAGE_SIZE()
set sql_warnings = 1;
create table t1 (a varchar(1024), b varchar(1024) generated always as (JSON_STORAGE_SIZE(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(1024) DEFAULT NULL,
  `b` varchar(1024) GENERATED ALWAYS AS (json_storage_size(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('{}', default);
select * from t1;
a	b
{}	5
drop table t1;
set sql_warnings = 0;
# JSON_STORAGE_FREE()
set sql_warnings = 1;
create table t1 (a varchar(1024), b varchar(1024) generated always as (JSON_STORAGE_FREE(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(1024) DEFAULT NULL,
  `b` varchar(1024) GENERATED ALWAYS AS (json_storage_free(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('{}', default);
select * from t1;
a	b
{}	0
drop table t1;
set sql_warnings = 0;
# JSON_SCHEMA_VALID()
set sql_warnings = 1;
create table t1 (a varchar(1024), b varchar(1024) generated always as (JSON_SCHEMA_VALID('{"type": "object"}', a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(1024) DEFAULT NULL,
  `b` varchar(1024) GENERATED ALWAYS AS (json_schema_valid(_utf8mb4'{"type": "object"}',`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('{}', default);
insert into t1 values ('[]', default);
select * from t1;
a	b
[]	0
{}	1
drop table t1;
set sql_warnings = 0;
# JSON_SCHEMA_VALIDATION_REPORT()
set sql_warnings = 1;
create table t1 (a varchar(1024), b varchar(1024) generated always as
(JSON_SCHEMA_VALIDATION_REPORT('{"type": "object"}', a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(1024) DEFAULT NULL,
  `b` varchar(1024) GENERATED ALWAYS AS (json_schema_validation_report(_utf8mb4'{"type": "object"}',`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('{}', default);
insert into t1 values ('[]', default);
select * from t1;
a	b
[]	{"valid": false, "reason": "The JSON document location '#' failed requirement 'type' at JSON Schema location '#'", "schema-location": "#", "document-location": "#", "schema-failed-keyword": "type"}
{}	{"valid": true}
drop table t1;
set sql_warnings = 0;
#
# OTHER FUNCTIONS
#
# AES_DECRYPT(), AES_ENCRYPT()
set sql_warnings = 1;
create table t1 (a varchar(1024), b varchar(1024) generated always as (aes_encrypt(aes_decrypt(a,'adf'),'adf')) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(1024) DEFAULT NULL,
  `b` varchar(1024) GENERATED ALWAYS AS (aes_encrypt(aes_decrypt(`a`,_utf8mb4'adf'),_utf8mb4'adf')) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('MySQL',default);
select * from t1;
a	b
MySQL	NULL
drop table t1;
set sql_warnings = 0;
# BIT_COUNT()
set sql_warnings = 1;
create table t1 (a int, b int generated always as (bit_count(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (bit_count(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values (5,default);
select * from t1;
a	b
5	2
drop table t1;
set sql_warnings = 0;
# CHARSET()
set sql_warnings = 1;
create table t1 (a varchar(1024), b varchar(1024) generated always as (charset(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(1024) DEFAULT NULL,
  `b` varchar(1024) GENERATED ALWAYS AS (charset(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('abc',default);
select * from t1;
a	b
abc	latin1
drop table t1;
set sql_warnings = 0;
# COERCIBILITY()
set sql_warnings = 1;
create table t1 (a varchar(1024), b int generated always as (coercibility(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(1024) DEFAULT NULL,
  `b` int GENERATED ALWAYS AS (coercibility(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('abc',default);
select * from t1;
a	b
abc	2
drop table t1;
set sql_warnings = 0;
# COLLATION()
set sql_warnings = 1;
create table t1 (a varchar(1024), b varchar(1024) generated always as (collation(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(1024) DEFAULT NULL,
  `b` varchar(1024) GENERATED ALWAYS AS (collation(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('abc',default);
select * from t1;
a	b
abc	latin1_swedish_ci
drop table t1;
set sql_warnings = 0;
# COMPRESS(), UNCOMPRESS()
set sql_warnings = 1;
create table t1 (a varchar(1024), b varchar(1024) generated always as (uncompress(compress(a))) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(1024) DEFAULT NULL,
  `b` varchar(1024) GENERATED ALWAYS AS (uncompress(compress(`a`))) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('MySQL',default);
select * from t1;
a	b
MySQL	MySQL
drop table t1;
set sql_warnings = 0;
# DEFAULT()
set sql_warnings = 1;
create table t1 (a varchar(1024) default 'aaa', b varchar(1024) generated always as (ifnull(a,default(a))) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(1024) DEFAULT 'aaa',
  `b` varchar(1024) GENERATED ALWAYS AS (ifnull(`a`,default(`a`))) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('any value',default);
select * from t1;
a	b
any value	any value
drop table t1;
set sql_warnings = 0;
# INET_ATON(), INET_NTOA()
set sql_warnings = 1;
create table t1 (a varchar(1024), b varchar(1024) generated always as (inet_ntoa(inet_aton(a))) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(1024) DEFAULT NULL,
  `b` varchar(1024) GENERATED ALWAYS AS (inet_ntoa(inet_aton(`a`))) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('127.0.0.1',default);
select * from t1;
a	b
127.0.0.1	127.0.0.1
drop table t1;
set sql_warnings = 0;
# MD5()
set sql_warnings = 1;
create table t1 (a varchar(1024), b varbinary(32) generated always as (md5(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(1024) DEFAULT NULL,
  `b` varbinary(32) GENERATED ALWAYS AS (md5(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('testing',default);
select * from t1;
a	b
testing	ae2b1fca515949e5d54fb22b8ed95575
drop table t1;
set sql_warnings = 0;
# SHA1()
set sql_warnings = 1;
create table t1 (a varchar(1024), b varchar(1024) generated always as (sha1(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(1024) DEFAULT NULL,
  `b` varchar(1024) GENERATED ALWAYS AS (sha(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('abc',default);
select * from t1;
a	b
abc	a9993e364706816aba3e25717850c26c9cd0d89d
drop table t1;
set sql_warnings = 0;
# SHA()
set sql_warnings = 1;
create table t1 (a varchar(1024), b varchar(1024) generated always as (sha(a)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(1024) DEFAULT NULL,
  `b` varchar(1024) GENERATED ALWAYS AS (sha(`a`)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('abc',default);
select * from t1;
a	b
abc	a9993e364706816aba3e25717850c26c9cd0d89d
drop table t1;
set sql_warnings = 0;
# SHA2()
set sql_warnings = 1;
create table t1 (a varchar(1024), b varchar(1024) generated always as (sha2(a,224)) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(1024) DEFAULT NULL,
  `b` varchar(1024) GENERATED ALWAYS AS (sha2(`a`,224)) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('abc',default);
select * from t1;
a	b
abc	23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7
drop table t1;
set sql_warnings = 0;
# UNCOMPRESSED_LENGTH()
set sql_warnings = 1;
create table t1 (a char, b varchar(1024) generated always as (uncompressed_length(compress(repeat(a,30)))) virtual) charset latin1;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` char(1) DEFAULT NULL,
  `b` varchar(1024) GENERATED ALWAYS AS (uncompressed_length(compress(repeat(`a`,30)))) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
insert into t1 values ('a',default);
select * from t1;
a	b
a	30
drop table t1;
set sql_warnings = 0;
DROP VIEW  IF EXISTS v1,v2;
DROP TABLE IF EXISTS t1,t2,t3;
DROP PROCEDURE IF EXISTS p1;
DROP FUNCTION IF EXISTS f1;
DROP TRIGGER IF EXISTS trg1;
DROP TRIGGER IF EXISTS trg2;
set sql_warnings = 0;
