drop table if exists t1;
SELECT 10,10.0,10.,.1e+2,100.0e-1;
10	10.0	10.	.1e+2	100.0e-1
10	10.0	10	10	10
SELECT 6e-05, -6e-05, --6e-05, -6e-05+1.000000;
6e-05	-6e-05	--6e-05	-6e-05+1.000000
6e-05	-6e-05	6e-05	0.99994
SELECT 1e1,1.e1,1.0e1,1e+1,1.e+1,1.0e+1,1e-1,1.e-1,1.0e-1;
1e1	1.e1	1.0e1	1e+1	1.e+1	1.0e+1	1e-1	1.e-1	1.0e-1
10	10	10	10	10	10	0.1	0.1	0.1
create table t1 (f1 float(24),f2 float(52));
show full columns from t1;
Field	Type	Collation	Null	Key	Default	Extra	Privileges	Comment
f1	float	NULL	YES		NULL			
f2	double	NULL	YES		NULL			
insert into t1 values(10,10),(1e+5,1e+5),(1234567890,1234567890),(1e+10,1e+10),(1e+15,1e+15),(1e+20,1e+20),(1e+50,1e+50),(1e+150,1e+150);
Warnings:
Warning	1264	Data truncated; out of range for column 'f1' at row 7
Warning	1264	Data truncated; out of range for column 'f1' at row 8
insert into t1 values(-10,-10),(1e-5,1e-5),(1e-10,1e-10),(1e-15,1e-15),(1e-20,1e-20),(1e-50,1e-50),(1e-150,1e-150);
select * from t1;
f1	f2
10	10
100000	100000
1.23457e+09	1234567890
1e+10	10000000000
1e+15	1e+15
1e+20	1e+20
3.40282e+38	1e+50
3.40282e+38	1e+150
-10	-10
1e-05	1e-05
1e-10	1e-10
1e-15	1e-15
1e-20	1e-20
0	1e-50
0	1e-150
drop table t1;
create table t1 (datum double);
insert into t1 values (0.5),(1.0),(1.5),(2.0),(2.5);
select * from t1;
datum
0.5
1
1.5
2
2.5
select * from t1 where datum < 1.5;
datum
0.5
1
select * from t1 where datum > 1.5;
datum
2
2.5
select * from t1 where datum = 1.5;
datum
1.5
drop table t1;
create table t1 (a  decimal(7,3) not null, key (a));
insert into t1 values ("0"),("-0.00"),("-0.01"),("-0.002"),("1");
select a from t1 order by a;
a
-0.010
-0.002
-0.000
0.000
1.000
select min(a) from t1;
min(a)
-0.010
drop table t1;
create table t1 (c1 double, c2 varchar(20));
insert t1 values (121,"16");
select c1 + c1 * (c2 / 100) as col from t1;
col
140.36
create table t2 select c1 + c1 * (c2 / 100) as col1, round(c1, 5) as col2, round(c1, 35) as col3, sqrt(c1*1e-15) col4 from t1;
select * from t2;
col1	col2	col3	col4
140.36	121.00000	121	3.47850542618522e-07
show create table t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `col1` double default NULL,
  `col2` double(22,5) default NULL,
  `col3` double default NULL,
  `col4` double default NULL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
drop table t1,t2;
create table t1 (a float);
insert into t1 values (1);
select max(a),min(a),avg(a) from t1;
max(a)	min(a)	avg(a)
1	1	1
drop table t1;
create table t1 (f float, f2 float(24), f3 float(6,2), d double, d2 float(53), d3 double(10,3), de decimal, de2 decimal(6), de3 decimal(5,2), n numeric, n2 numeric(8), n3 numeric(5,6));
show full columns from t1;
Field	Type	Collation	Null	Key	Default	Extra	Privileges	Comment
f	float	NULL	YES		NULL			
f2	float	NULL	YES		NULL			
f3	float(6,2)	NULL	YES		NULL			
d	double	NULL	YES		NULL			
d2	double	NULL	YES		NULL			
d3	double(10,3)	NULL	YES		NULL			
de	decimal(10,0)	NULL	YES		NULL			
de2	decimal(6,0)	NULL	YES		NULL			
de3	decimal(5,2)	NULL	YES		NULL			
n	decimal(10,0)	NULL	YES		NULL			
n2	decimal(8,0)	NULL	YES		NULL			
n3	decimal(7,6)	NULL	YES		NULL			
drop table t1;
create table t1 (a  decimal(7,3) not null, key (a));
insert into t1 values ("0"),("-0.00"),("-0.01"),("-0.002"),("1");
select a from t1 order by a;
a
-0.010
-0.002
-0.000
0.000
1.000
select min(a) from t1;
min(a)
-0.010
drop table t1;
create table t1 (a float(200,100), b double(200,100));
insert t1 values (1.0, 2.0);
select * from t1;
a	b
1.000000000000000000000000000000	2.000000000000000000000000000000
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` float(200,30) default NULL,
  `b` double(200,30) default NULL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
drop table t1;
create table t1 (c20 char);
insert into t1 values (5000.0);
drop table t1;
create table t1 (f float(54));
ERROR 42000: Incorrect column specifier for column 'f'
drop table if exists t1;
