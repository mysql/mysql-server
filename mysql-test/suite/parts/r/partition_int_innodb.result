create table t1 (a tinyint unsigned not null, primary key(a)) engine='InnoDB' 
partition by key (a) (
partition pa1 max_rows=20 min_rows=2,
partition pa2 max_rows=30 min_rows=3,
partition pa3 max_rows=30 min_rows=4,
partition pa4 max_rows=40 min_rows=2);
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` tinyint unsigned NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY KEY (a)
(PARTITION pa1 MAX_ROWS = 20 MIN_ROWS = 2 ENGINE = InnoDB,
 PARTITION pa2 MAX_ROWS = 30 MIN_ROWS = 3 ENGINE = InnoDB,
 PARTITION pa3 MAX_ROWS = 30 MIN_ROWS = 4 ENGINE = InnoDB,
 PARTITION pa4 MAX_ROWS = 40 MIN_ROWS = 2 ENGINE = InnoDB) */
insert into t1 values (255), (254), (253), (252), (1), (2), (128);
select * from t1;
a
1
128
2
252
253
254
255
select * from t1 where a=253;
a
253
delete from t1 where a=253;
select * from t1;
a
1
128
2
252
254
255
drop table t1;
create table t2 (a tinyint unsigned not null, primary key(a)) engine='InnoDB' 
partition by key (a) partitions 8;
show create table t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `a` tinyint unsigned NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY KEY (a)
PARTITIONS 8 */
insert into t2 values (255), (254), (253), (252);
select * from t2;
a
252
253
254
255
select * from t2 where a=253;
a
253
delete from t2 where a=253;
select * from t2;
a
252
254
255
delete from t2;
255 inserts;
select count(*) from t2;
count(*)
255
drop table t2;
create table t3 (a tinyint not null, primary key(a)) engine='InnoDB' 
partition by key (a) partitions 7;
show create table t3;
Table	Create Table
t3	CREATE TABLE `t3` (
  `a` tinyint NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY KEY (a)
PARTITIONS 7 */
insert into t3 values (127), (126), (125), (124), (-128), (-127), (1), (-1), (0);
select * from t3;
a
-1
-127
-128
0
1
124
125
126
127
select * from t3 where a=125;
a
125
delete from t3 where a=125;
select * from t3;
a
-1
-127
-128
0
1
124
126
127
drop table t3;
create table t1 (a smallint unsigned not null, primary key(a)) engine='InnoDB' 
partition by key (a) (
partition pa1 max_rows=20 min_rows=2,
partition pa2 max_rows=30 min_rows=3,
partition pa3 max_rows=30 min_rows=4,
partition pa4 max_rows=40 min_rows=2);
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` smallint unsigned NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY KEY (a)
(PARTITION pa1 MAX_ROWS = 20 MIN_ROWS = 2 ENGINE = InnoDB,
 PARTITION pa2 MAX_ROWS = 30 MIN_ROWS = 3 ENGINE = InnoDB,
 PARTITION pa3 MAX_ROWS = 30 MIN_ROWS = 4 ENGINE = InnoDB,
 PARTITION pa4 MAX_ROWS = 40 MIN_ROWS = 2 ENGINE = InnoDB) */
insert into t1 values (65535), (65534), (65533), (65532), (1), (2), (256);
select * from t1;
a
1
2
256
65532
65533
65534
65535
select * from t1 where a=65533;
a
65533
delete from t1 where a=65533;
select * from t1;
a
1
2
256
65532
65534
65535
drop table t1;
create table t2 (a smallint unsigned not null, primary key(a)) engine='InnoDB' 
partition by key (a) partitions 8;
show create table t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `a` smallint unsigned NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY KEY (a)
PARTITIONS 8 */
insert into t2 values (65535), (65534), (65533), (65532);
select * from t2;
a
65532
65533
65534
65535
select * from t2 where a=65533;
a
65533
delete from t2 where a=65533;
select * from t2;
a
65532
65534
65535
delete from t2;
1024 inserts;
select count(*) from t2;
count(*)
1024
drop table t2;
create table t3 (a smallint not null, primary key(a)) engine='InnoDB' 
partition by key (a) partitions 7;
show create table t3;
Table	Create Table
t3	CREATE TABLE `t3` (
  `a` smallint NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY KEY (a)
PARTITIONS 7 */
insert into t3 values (32767), (32766), (32765), (32764), (-32768), (-32767), (1), (-1), (0);
select * from t3;
a
-1
-32767
-32768
0
1
32764
32765
32766
32767
select * from t3 where a=32765;
a
32765
delete from t3 where a=32765;
select * from t3;
a
-1
-32767
-32768
0
1
32764
32766
32767
drop table t3;
create table t1 (a int unsigned not null, primary key(a)) engine='InnoDB' 
partition by key (a) (
partition pa1 max_rows=20 min_rows=2,
partition pa2 max_rows=30 min_rows=3,
partition pa3 max_rows=30 min_rows=4,
partition pa4 max_rows=40 min_rows=2);
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int unsigned NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY KEY (a)
(PARTITION pa1 MAX_ROWS = 20 MIN_ROWS = 2 ENGINE = InnoDB,
 PARTITION pa2 MAX_ROWS = 30 MIN_ROWS = 3 ENGINE = InnoDB,
 PARTITION pa3 MAX_ROWS = 30 MIN_ROWS = 4 ENGINE = InnoDB,
 PARTITION pa4 MAX_ROWS = 40 MIN_ROWS = 2 ENGINE = InnoDB) */
insert into t1 values (4294967295), (4294967294), (4294967293), (4294967292), (1), (2), (65535);
select * from t1;
a
1
2
4294967292
4294967293
4294967294
4294967295
65535
select * from t1 where a=4294967293;
a
4294967293
delete from t1 where a=4294967293;
select * from t1;
a
1
2
4294967292
4294967294
4294967295
65535
drop table t1;
create table t2 (a int unsigned not null, primary key(a)) engine='InnoDB' 
partition by key (a) partitions 8;
show create table t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `a` int unsigned NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY KEY (a)
PARTITIONS 8 */
insert into t2 values (4294967295), (4294967294), (4294967293), (4294967292);
select * from t2;
a
4294967292
4294967293
4294967294
4294967295
select * from t2 where a=4294967293;
a
4294967293
delete from t2 where a=4294967293;
select * from t2;
a
4294967292
4294967294
4294967295
delete from t2;
1024 inserts;
select count(*) from t2;
count(*)
1024
drop table t2;
create table t3 (a int not null, primary key(a)) engine='InnoDB' 
partition by key (a) partitions 7;
show create table t3;
Table	Create Table
t3	CREATE TABLE `t3` (
  `a` int NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY KEY (a)
PARTITIONS 7 */
insert into t3 values (2147483647), (2147483646), (2147483645), (2147483644), (-2147483648), (-2147483647), (1), (-1), (0);
select * from t3;
a
-1
-2147483647
-2147483648
0
1
2147483644
2147483645
2147483646
2147483647
select * from t3 where a=2147483645;
a
2147483645
delete from t3 where a=2147483645;
select * from t3;
a
-1
-2147483647
-2147483648
0
1
2147483644
2147483646
2147483647
drop table t3;
create table t1 (a mediumint unsigned not null, primary key(a)) engine='InnoDB' 
partition by key (a) (
partition pa1 max_rows=20 min_rows=2,
partition pa2 max_rows=30 min_rows=3,
partition pa3 max_rows=30 min_rows=4,
partition pa4 max_rows=40 min_rows=2);
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` mediumint unsigned NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY KEY (a)
(PARTITION pa1 MAX_ROWS = 20 MIN_ROWS = 2 ENGINE = InnoDB,
 PARTITION pa2 MAX_ROWS = 30 MIN_ROWS = 3 ENGINE = InnoDB,
 PARTITION pa3 MAX_ROWS = 30 MIN_ROWS = 4 ENGINE = InnoDB,
 PARTITION pa4 MAX_ROWS = 40 MIN_ROWS = 2 ENGINE = InnoDB) */
insert into t1 values (16777215), (16777214), (16777213), (16777212), (1), (2), (65535);
select * from t1;
a
1
16777212
16777213
16777214
16777215
2
65535
select * from t1 where a=16777213;
a
16777213
delete from t1 where a=16777213;
select * from t1;
a
1
16777212
16777214
16777215
2
65535
drop table t1;
create table t2 (a mediumint unsigned not null, primary key(a)) engine='InnoDB' 
partition by key (a) partitions 8;
show create table t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `a` mediumint unsigned NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY KEY (a)
PARTITIONS 8 */
insert into t2 values (16777215), (16777214), (16777213), (16777212);
select * from t2;
a
16777212
16777213
16777214
16777215
select * from t2 where a=16777213;
a
16777213
delete from t2 where a=16777213;
select * from t2;
a
16777212
16777214
16777215
delete from t2;
1024 inserts;
select count(*) from t2;
count(*)
1024
drop table t2;
create table t3 (a mediumint not null, primary key(a)) engine='InnoDB' 
partition by key (a) partitions 7;
show create table t3;
Table	Create Table
t3	CREATE TABLE `t3` (
  `a` mediumint NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY KEY (a)
PARTITIONS 7 */
insert into t3 values (8388607), (8388606), (8388605), (8388604), (-8388608), (-8388607), (1), (-1), (0);
select * from t3;
a
-1
-8388607
-8388608
0
1
8388604
8388605
8388606
8388607
select * from t3 where a=8388605;
a
8388605
delete from t3 where a=8388605;
select * from t3;
a
-1
-8388607
-8388608
0
1
8388604
8388606
8388607
drop table t3;
create table t1 (a bigint unsigned not null, primary key(a)) engine='InnoDB' 
partition by key (a) (
partition pa1 max_rows=20 min_rows=2,
partition pa2 max_rows=30 min_rows=3,
partition pa3 max_rows=30 min_rows=4,
partition pa4 max_rows=40 min_rows=2);
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` bigint unsigned NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY KEY (a)
(PARTITION pa1 MAX_ROWS = 20 MIN_ROWS = 2 ENGINE = InnoDB,
 PARTITION pa2 MAX_ROWS = 30 MIN_ROWS = 3 ENGINE = InnoDB,
 PARTITION pa3 MAX_ROWS = 30 MIN_ROWS = 4 ENGINE = InnoDB,
 PARTITION pa4 MAX_ROWS = 40 MIN_ROWS = 2 ENGINE = InnoDB) */
insert into t1 values (18446744073709551615), (0xFFFFFFFFFFFFFFFE), (18446744073709551613), (18446744073709551612), (1), (2), (65535);
select * from t1;
a
1
18446744073709551612
18446744073709551613
18446744073709551614
18446744073709551615
2
65535
select * from t1 where a=-2;
a
delete from t1 where a=-2;
select * from t1;
a
1
18446744073709551612
18446744073709551613
18446744073709551614
18446744073709551615
2
65535
select * from t1 where a=18446744073709551615;
a
18446744073709551615
delete from t1 where a=18446744073709551615;
select * from t1;
a
1
18446744073709551612
18446744073709551613
18446744073709551614
2
65535
drop table t1;
create table t2 (a bigint unsigned not null, primary key(a)) engine='InnoDB' 
partition by key (a) partitions 8;
show create table t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `a` bigint unsigned NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY KEY (a)
PARTITIONS 8 */
insert into t2 values (18446744073709551615), (0xFFFFFFFFFFFFFFFE), (18446744073709551613), (18446744073709551612);
select * from t2;
a
18446744073709551612
18446744073709551613
18446744073709551614
18446744073709551615
select * from t2 where a=18446744073709551615;
a
18446744073709551615
delete from t2 where a=18446744073709551615;
select * from t2;
a
18446744073709551612
18446744073709551613
18446744073709551614
delete from t2;
1024 inserts;
select count(*) from t2;
count(*)
1024
drop table t2;
create table t3 (a bigint not null, primary key(a)) engine='InnoDB' 
partition by key (a) partitions 7;
show create table t3;
Table	Create Table
t3	CREATE TABLE `t3` (
  `a` bigint NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY KEY (a)
PARTITIONS 7 */
insert into t3 values (9223372036854775807), (9223372036854775806), (9223372036854775805), (9223372036854775804), (-9223372036854775808), (-9223372036854775807), (1), (-1), (0);
select * from t3;
a
-1
-9223372036854775807
-9223372036854775808
0
1
9223372036854775804
9223372036854775805
9223372036854775806
9223372036854775807
select * from t3 where a=9223372036854775806;
a
9223372036854775806
delete from t3 where a=9223372036854775806;
select * from t3;
a
-1
-9223372036854775807
-9223372036854775808
0
1
9223372036854775804
9223372036854775805
9223372036854775807
drop table t3;
