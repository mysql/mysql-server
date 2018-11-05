#
# Bug #24929748	REVERSE SCAN ON A PARTITIONED TABLE DOES ICP CHECK INCORRECTLY, CAUSING SLOWDOWN
#
create table t1 (
date_col datetime not null,
col1 int,
filler varchar(255),
key(date_col, col1)
) engine=innodb
partition by range columns(date_col)
(
partition p0 values less than ('2015-01-01 00:00:00'),
partition p1 values less than ('2016-01-01 00:00:00'),
partition p2 values less than ('2017-01-01 00:00:00'),
partition p3 values less than (MAXVALUE)
);
create table t2(a int);
insert into t2 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);
insert into t1 select date_add('2014-01-01', interval (A.a+10*B.a) day),123456, repeat('raw-data', 5)
from t2 A, t2 B, t2 C;
insert into t1 select date_add('2015-01-01', interval (A.a+10*B.a) day), 123456,repeat('raw-data', 5)
from t2 A, t2 B, t2 C;
set @cnt=1;
select * from t1 force index(date_col) where
col1 + 10000000 > (@cnt:=@cnt+1) and date_col >= date('2014-04-01') AND date_col < date('2014-04-01' + interval 1 day)
ORDER BY date_col DESC;
date_col	col1	filler
2014-04-01 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-01 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-01 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-01 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-01 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-01 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-01 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-01 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-01 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-01 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
Warnings:
Warning	1287	Setting user variables within expressions is deprecated and will be removed in a future release. Consider alternatives: 'SET variable=expression, ...', or 'SELECT expression(s) INTO variables(s)'.
select @cnt;
@cnt
11
delete from t1 where date_col>='2015-01-01' and date_col<='2016-01-06' limit 500;
set @cnt=1;
select * from t1 force index(date_col) where
col1 + 10000000 > (@cnt:=@cnt+1) and date_col >= date('2014-04-06') AND date_col < date('2015-02-10' + interval 1 day)
ORDER BY date_col DESC;
date_col	col1	filler
2014-04-10 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-10 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-10 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-10 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-10 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-10 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-10 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-10 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-10 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-10 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-09 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-09 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-09 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-09 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-09 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-09 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-09 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-09 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-09 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-09 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-08 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-08 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-08 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-08 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-08 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-08 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-08 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-08 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-08 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-08 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-07 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-07 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-07 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-07 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-07 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-07 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-07 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-07 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-07 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-07 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-06 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-06 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-06 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-06 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-06 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-06 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-06 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-06 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-06 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
2014-04-06 00:00:00	123456	raw-dataraw-dataraw-dataraw-dataraw-data
Warnings:
Warning	1287	Setting user variables within expressions is deprecated and will be removed in a future release. Consider alternatives: 'SET variable=expression, ...', or 'SELECT expression(s) INTO variables(s)'.
select @cnt;
@cnt
51
drop table t1,t2;
create table t1 ( f1 int ,f2 int, key(f2)) engine=innodb
partition by range columns (f2)
(
partition p0 values less than (10000),
partition p1 values less than (20000),
partition p2 values less than (30000),
PARTITION p3 VALUES LESS THAN (MAXVALUE)
);
select * from t1 where (f2>1000 and f2 <1002) or ( f2> 3000 and f2 < 3003) or (f2> 10001 and f2 < 10004) order by f2 desc;
f1	f2
123456	10003
123456	10002
123456	3002
123456	3001
123456	1001
drop table t1;
