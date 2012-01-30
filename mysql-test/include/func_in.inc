# Initialise
--disable_warnings
drop table if exists t1, t2;
--enable_warnings
#
# test of IN (NULL)
#

select 1 in (1,2,3);
select 10 in (1,2,3);
select NULL in (1,2,3);
select 1 in (1,NULL,3);
select 3 in (1,NULL,3);
select 10 in (1,NULL,3);
select 1.5 in (1.5,2.5,3.5);
select 10.5 in (1.5,2.5,3.5);
select NULL in (1.5,2.5,3.5);
select 1.5 in (1.5,NULL,3.5);
select 3.5 in (1.5,NULL,3.5);
select 10.5 in (1.5,NULL,3.5);

CREATE TABLE t1 (a int, b int, c int);
insert into t1 values (1,2,3), (1,NULL,3);
select 1 in (a,b,c) from t1;
select 3 in (a,b,c) from t1;
select 10 in (a,b,c) from t1;
select NULL in (a,b,c) from t1;
drop table t1;
CREATE TABLE t1 (a float, b float, c float);
insert into t1 values (1.5,2.5,3.5), (1.5,NULL,3.5);
select 1.5 in (a,b,c) from t1;
select 3.5 in (a,b,c) from t1;
select 10.5 in (a,b,c) from t1;
drop table t1;
CREATE TABLE t1 (a varchar(10), b varchar(10), c varchar(10));
insert into t1 values ('A','BC','EFD'), ('A',NULL,'EFD');
select 'A' in (a,b,c) from t1;
select 'EFD' in (a,b,c) from t1;
select 'XSFGGHF' in (a,b,c) from t1;
drop table t1;

CREATE TABLE t1 (field char(1));
INSERT INTO t1 VALUES ('A'),(NULL);
SELECT * from t1 WHERE field IN (NULL);
SELECT * from t1 WHERE field NOT IN (NULL);
SELECT * from t1 where field = field;
SELECT * from t1 where field <=> field;
DELETE FROM t1 WHERE field NOT IN (NULL);
SELECT * FROM t1;
drop table t1;

create table t1 (id int(10) primary key);
insert into t1 values (1),(2),(3),(4),(5),(6),(7),(8),(9);
select * from t1 where id in (2,5,9);
drop table t1;

create table t1 (
a char(1) character set latin1 collate latin1_general_ci,
b char(1) character set latin1 collate latin1_swedish_ci,
c char(1) character set latin1 collate latin1_danish_ci
);
insert into t1 values ('A','B','C');
insert into t1 values ('a','c','c');
--error 1267
select * from t1 where a in (b);
--error 1270
select * from t1 where a in (b,c);
--error 1271
select * from t1 where 'a' in (a,b,c);
select * from t1 where 'a' in (a);
select * from t1 where a in ('a');
select * from t1 where 'a' collate latin1_general_ci in (a,b,c);
select * from t1 where 'a' collate latin1_bin in (a,b,c);
select * from t1 where 'a' in (a,b,c collate latin1_bin);
explain extended select * from t1 where 'a' in (a,b,c collate latin1_bin);
drop table t1;

set names utf8;
create table t1 (a char(10) character set utf8 not null);
insert into t1 values ('bbbb'),(_koi8r'√√√√'),(_latin1'ƒƒƒƒ');
select a from t1 where a in ('bbbb',_koi8r'√√√√',_latin1'ƒƒƒƒ') order by a;
drop table t1;
# Bug#7834 Illegal mix of collations in IN operator
create table t1 (a char(10) character set latin1 not null);
insert into t1 values ('a'),('b'),('c');
select a from t1 where a IN ('a','b','c') order by a;
drop table t1;
set names latin1;

select '1.0' in (1,2);
select 1 in ('1.0',2);
select 1 in (1,'2.0');
select 1 in ('1.0',2.0);
select 1 in (1.0,'2.0');
select 1 in ('1.1',2);
select 1 in ('1.1',2.0);

# Test case for bug #6365

create table t1 (a char(2) character set binary);
insert into t1 values ('aa'), ('bb');
select * from t1 where a in (NULL, 'aa');
drop table t1;

# BUG#13419
create table t1 (id int, key(id));
insert into t1 values (1),(2),(3);
select count(*) from t1 where id not in (1);
select count(*) from t1 where id not in (1,2);
drop table t1;


#
# BUG#17047: CHAR() and IN() can return NULL without signaling NULL
# result
#
# The problem was in the IN() function that ignored maybe_null flags
# of all arguments except the first (the one _before_ the IN
# keyword, '1' in the test case below).
#
--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

CREATE TABLE t1 SELECT 1 IN (2, NULL);
--echo SELECT should return NULL.
SELECT * FROM t1;

DROP TABLE t1;


--echo End of 4.1 tests


#
# Bug #11885: WHERE condition with NOT IN (one element)          
#             

CREATE TABLE t1 (a int PRIMARY KEY);
INSERT INTO t1 VALUES (44), (45), (46);

SELECT * FROM t1 WHERE a IN (45);
SELECT * FROM t1 WHERE a NOT IN (0, 45);
SELECT * FROM t1 WHERE a NOT IN (45);

CREATE VIEW v1 AS SELECT * FROM t1 WHERE a NOT IN (45);
SHOW CREATE VIEW v1;
SELECT * FROM v1; 

DROP VIEW v1;
DROP TABLE t1;

# BUG#15872: Excessive memory consumption of range analysis of NOT IN
create table t1 (a int);
insert into t1 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9); 
create table t2 (a int, filler char(200), key(a));

insert into t2 select C.a*2,   'no'  from t1 A, t1 B, t1 C;
insert into t2 select C.a*2+1, 'yes' from t1 C;

explain 
select * from t2 where a NOT IN (0, 2,4,6,8,10,12,14,16,18);
select * from t2 where a NOT IN (0, 2,4,6,8,10,12,14,16,18);

explain select * from t2 force index(a) where a NOT IN (2,2,2,2,2,2);
explain select * from t2 force index(a) where a <> 2;

drop table t2;

#
# Repeat the test for DATETIME
#
create table t2 (a datetime, filler char(200), key(a));

insert into t2 select '2006-04-25 10:00:00' + interval C.a minute,
               'no'  from t1 A, t1 B, t1 C where C.a % 2 = 0;

insert into t2 select '2006-04-25 10:00:00' + interval C.a*2+1 minute,
               'yes' from t1 C;

explain 
select * from t2 where a NOT IN (
  '2006-04-25 10:00:00','2006-04-25 10:02:00','2006-04-25 10:04:00', 
  '2006-04-25 10:06:00', '2006-04-25 10:08:00');
select * from t2 where a NOT IN (
  '2006-04-25 10:00:00','2006-04-25 10:02:00','2006-04-25 10:04:00', 
  '2006-04-25 10:06:00', '2006-04-25 10:08:00');
drop table t2;

#
# Repeat the test for CHAR(N)
#
create table t2 (a varchar(10), filler char(200), key(a));

insert into t2 select 'foo', 'no' from t1 A, t1 B;
insert into t2 select 'barbar', 'no' from t1 A, t1 B;
insert into t2 select 'bazbazbaz', 'no' from t1 A, t1 B;

insert into t2 values ('fon', '1'), ('fop','1'), ('barbaq','1'), 
  ('barbas','1'), ('bazbazbay', '1'),('zz','1');

explain select * from t2 where a not in('foo','barbar', 'bazbazbaz');

drop table t2;

#
# Repeat for DECIMAL
#
create table t2 (a decimal(10,5), filler char(200), key(a));

insert into t2 select 345.67890, 'no' from t1 A, t1 B;
insert into t2 select 43245.34, 'no' from t1 A, t1 B;
insert into t2 select 64224.56344, 'no' from t1 A, t1 B;

insert into t2 values (0, '1'), (22334.123,'1'), (33333,'1'), 
  (55555,'1'), (77777, '1');

explain
select * from t2 where a not in (345.67890, 43245.34, 64224.56344);
select * from t2 where a not in (345.67890, 43245.34, 64224.56344);

drop table t2;

# Try a very big IN-list
create table t2 (a int, key(a), b int);
insert into t2 values (1,1),(2,2);

set @cnt= 1; 
set @str="update t2 set b=1 where a not in (";
select count(*) from (
  select @str:=concat(@str, @cnt:=@cnt+1, ",") 
  from t1 A, t1 B, t1 C, t1 D) Z;

set @str:=concat(@str, "10000)");
select substr(@str, 1, 50);
prepare s from @str;
execute s;
deallocate prepare s;
set @str=NULL;

drop table t2;
drop table t1;

# BUG#19618: Crash in range optimizer for 
#   "unsigned_keypart NOT IN(negative_number,...)" 
#   (introduced in fix BUG#15872) 
create table t1 (
  some_id smallint(5) unsigned,
  key (some_id)
);
insert into t1 values (1),(2);
select some_id from t1 where some_id not in(2,-1);
select some_id from t1 where some_id not in(-4,-1,-4);
select some_id from t1 where some_id not in(-4,-1,3423534,2342342);

#
# BUG#24261: crash when WHERE contains NOT IN ('<negative value>') for unsigned column type
#

select some_id from t1 where some_id not in('-1', '0');

drop table t1;

#
# BUG#20420: optimizer reports wrong keys on left join with IN
#
CREATE TABLE t1 (a int, b int, PRIMARY KEY (a));
INSERT INTO t1 VALUES (1,1),(2,1),(3,1),(4,1),(5,1),(6,1);

CREATE TABLE t2 (a int, b int, PRIMARY KEY (a));
INSERT INTO t2 VALUES (3,2),(4,2),(100,100),(101,201),(102,102);

CREATE TABLE t3 (a int PRIMARY KEY);
INSERT INTO t3 VALUES (1),(2),(3),(4);

CREATE TABLE t4 (a int PRIMARY KEY,b int);
INSERT INTO t4 VALUES (1,1),(2,2),(1000,1000),(1001,1001),(1002,1002),
       (1003,1003),(1004,1004);

EXPLAIN SELECT STRAIGHT_JOIN * FROM t3 
  JOIN t1 ON t3.a=t1.a 
  JOIN t2 ON t3.a=t2.a
  JOIN t4 WHERE t4.a IN (t1.b, t2.b);

SELECT STRAIGHT_JOIN * FROM t3 
  JOIN t1 ON t3.a=t1.a 
  JOIN t2 ON t3.a=t2.a
  JOIN t4 WHERE t4.a IN (t1.b, t2.b);

EXPLAIN SELECT STRAIGHT_JOIN 
   (SELECT SUM(t4.a) FROM t4 WHERE t4.a IN (t1.b, t2.b)) 
  FROM t3, t1, t2
  WHERE t3.a=t1.a AND t3.a=t2.a;

SELECT STRAIGHT_JOIN 
   (SELECT SUM(t4.a) FROM t4 WHERE t4.a IN (t1.b, t2.b)) 
  FROM t3, t1, t2
  WHERE t3.a=t1.a AND t3.a=t2.a;

DROP TABLE t1,t2,t3,t4;  

#
# BUG#19342: IN works incorrectly for BIGINT UNSIGNED values
#
CREATE TABLE t1(a BIGINT UNSIGNED);
INSERT INTO t1 VALUES (0xFFFFFFFFFFFFFFFF);

SELECT * FROM t1 WHERE a=-1 OR a=-2 ;
SELECT * FROM t1 WHERE a IN (-1, -2);

CREATE TABLE t2 (a BIGINT UNSIGNED);
insert into t2 values(13491727406643098568),
       (0x7fffffefffffffff),
       (0x7ffffffeffffffff),
       (0x7fffffffefffffff),
       (0x7ffffffffeffffff),
       (0x7fffffffffefffff),
       (0x7ffffffffffeffff),
       (0x7fffffffffffefff),
       (0x7ffffffffffffeff),
       (0x7fffffffffffffef),
       (0x7ffffffffffffffe),
       (0x7fffffffffffffff),
       (0x8000000000000000),
       (0x8000000000000001),
       (0x8000000000000002),
       (0x8000000000000300),
       (0x8000000000000400),
       (0x8000000000000401),
       (0x8000000000004001),
       (0x8000000000040001),
       (0x8000000000400001),
       (0x8000000004000001),
       (0x8000000040000001),
       (0x8000000400000001),
       (0x8000004000000001),
       (0x8000040000000001);

SELECT HEX(a) FROM t2 WHERE a IN 
  (CAST(0xBB3C3E98175D33C8 AS UNSIGNED),
   42);

SELECT HEX(a) FROM t2 WHERE a IN
  (CAST(0xBB3C3E98175D33C8 AS UNSIGNED),
   CAST(0x7fffffffffffffff AS UNSIGNED),
   CAST(0x8000000000000000 AS UNSIGNED),
   CAST(0x8000000000000400 AS UNSIGNED),
   CAST(0x8000000000000401 AS UNSIGNED),
   42);

SELECT HEX(a) FROM t2 WHERE a IN 
  (CAST(0x7fffffffffffffff AS UNSIGNED), 
   CAST(0x8000000000000001 AS UNSIGNED));
SELECT HEX(a) FROM t2 WHERE a IN 
  (CAST(0x7ffffffffffffffe AS UNSIGNED), 
   CAST(0x7fffffffffffffff AS UNSIGNED));
SELECT HEX(a) FROM t2 WHERE a IN 
  (0x7ffffffffffffffe, 
   0x7fffffffffffffff,
   'abc');

CREATE TABLE t3 (a BIGINT UNSIGNED);
INSERT INTO t3 VALUES (9223372036854775551);

SELECT HEX(a) FROM t3 WHERE a IN (9223372036854775807, 42);

CREATE TABLE t4 (a DATE);
INSERT INTO t4 VALUES ('1972-02-06'), ('1972-07-29');
SELECT * FROM t4 WHERE a IN ('1972-02-06','19772-07-29');

DROP TABLE t1,t2,t3,t4;

#
# BUG#27362: IN with a decimal expression that may return NULL
#

CREATE TABLE t1 (id int not null);
INSERT INTO t1 VALUES (1),(2);

SELECT id FROM t1 WHERE id IN(4564, (SELECT IF(1=0,1,1/0)) );

DROP TABLE t1;

--echo End of 5.0 tests


#
# Bug#18360: Type aggregation for IN and CASE may lead to a wrong result 
#
create table t1(f1 char(1));
insert into t1 values ('a'),('b'),('1');
select f1 from t1 where f1 in ('a',1);
select f1, case f1 when 'a' then '+' when 1 then '-' end from t1;
create index t1f1_idx on t1(f1);
select f1 from t1 where f1 in ('a',1);
explain select f1 from t1 where f1 in ('a',1);
select f1 from t1 where f1 in ('a','b');
explain select f1 from t1 where f1 in ('a','b');
select f1 from t1 where f1 in (2,1);
explain select f1 from t1 where f1 in (2,1);
create table t2(f2 int, index t2f2(f2));
insert into t2 values(0),(1),(2);
select f2 from t2 where f2 in ('a',2);
explain select f2 from t2 where f2 in ('a',2);
select f2 from t2 where f2 in ('a','b');
explain select f2 from t2 where f2 in ('a','b');
select f2 from t2 where f2 in (1,'b');
explain select f2 from t2 where f2 in (1,'b');
drop table t1, t2;

#
# Bug #31075: crash in get_func_mm_tree
#

create table t1 (a time, key(a));
insert into t1 values (),(),(),(),(),(),(),(),(),();
select a from t1 where a not in (a,a,a) group by a;
drop table t1;

#
# Bug #37761: IN handles NULL differently for table-subquery and value-list
#

create table t1 (id int);
select * from t1 where NOT id in (select null union all select 1);
select * from t1 where NOT id in (null, 1);
drop table t1;

#
# Bug #41363: crash of mysqld on windows with aggregate in case
#

CREATE TABLE t1(c0 INTEGER, c1 INTEGER, c2 INTEGER);
INSERT INTO t1 VALUES(1, 1, 1), (1, 1, 1);

SELECT CASE AVG (c0) WHEN c1 * c2 THEN 1 END FROM t1;
SELECT CASE c1 * c2 WHEN SUM(c0) THEN 1 WHEN AVG(c0) THEN 2 END FROM t1;
SELECT CASE c1 WHEN c1 + 1 THEN 1 END, ABS(AVG(c0)) FROM t1;

DROP TABLE t1;

#
# Bug #44399: crash with statement using TEXT columns, aggregates, GROUP BY, 
# and HAVING
#

CREATE TABLE t1(a TEXT, b INT, c INT UNSIGNED, d DECIMAL(12,2), e REAL);
INSERT INTO t1 VALUES('iynfj', 1, 1, 1, 1);
INSERT INTO t1 VALUES('innfj', 2, 2, 2, 2);
SELECT SUM( DISTINCT a ) FROM t1 GROUP BY a HAVING a IN ( AVG( 1 ), 1 + a);
SELECT SUM( DISTINCT b ) FROM t1 GROUP BY b HAVING b IN ( AVG( 1 ), 1 + b);
SELECT SUM( DISTINCT c ) FROM t1 GROUP BY c HAVING c IN ( AVG( 1 ), 1 + c);
SELECT SUM( DISTINCT d ) FROM t1 GROUP BY d HAVING d IN ( AVG( 1 ), 1 + d);
SELECT SUM( DISTINCT e ) FROM t1 GROUP BY e HAVING e IN ( AVG( 1 ), 1 + e);
SELECT SUM( DISTINCT e ) FROM t1 GROUP BY b,c,d HAVING (b,c,d) IN 
  ((AVG( 1 ), 1 + c, 1 + d), (AVG( 1 ), 2 + c, 2 + d));
DROP TABLE t1;

--echo #
--echo # Bug #44139: Table scan when NULL appears in IN clause
--echo #

--disable_warnings

CREATE TABLE t1 (
  c_int INT NOT NULL,
  c_decimal DECIMAL(5,2) NOT NULL,
  c_float FLOAT(5, 2) NOT NULL,
  c_bit BIT(10) NOT NULL,
  c_date DATE NOT NULL, 
  c_datetime DATETIME NOT NULL,
  c_timestamp TIMESTAMP NOT NULL,
  c_time TIME NOT NULL,
  c_year YEAR NOT NULL,
  c_char CHAR(10) NOT NULL,
  INDEX(c_int), INDEX(c_decimal), INDEX(c_float), INDEX(c_bit), INDEX(c_date),
  INDEX(c_datetime), INDEX(c_timestamp), INDEX(c_time), INDEX(c_year),
  INDEX(c_char));

INSERT INTO t1 (c_int) VALUES (1), (2), (3), (4), (5);
INSERT INTO t1 (c_int) SELECT 0 FROM t1;
INSERT INTO t1 (c_int) SELECT 0 FROM t1;

--enable_warnings

EXPLAIN SELECT * FROM t1 WHERE c_int IN (1, 2, 3);
EXPLAIN SELECT * FROM t1 WHERE c_int IN (NULL, 1, 2, 3);

EXPLAIN SELECT * FROM t1 WHERE c_int IN (1, 2, 3);
EXPLAIN SELECT * FROM t1 WHERE c_int IN (1, NULL, 2, NULL, 3, NULL);
EXPLAIN SELECT * FROM t1 WHERE c_int IN (NULL);
EXPLAIN SELECT * FROM t1 WHERE c_int IN (NULL, NULL);

EXPLAIN SELECT * FROM t1 WHERE c_decimal IN (1, 2, 3);
EXPLAIN SELECT * FROM t1 WHERE c_decimal IN (NULL, 1, 2, 3);
EXPLAIN SELECT * FROM t1 WHERE c_decimal IN (NULL);
EXPLAIN SELECT * FROM t1 WHERE c_decimal IN (NULL, NULL);

EXPLAIN SELECT * FROM t1 WHERE c_float IN (1, 2, 3);
EXPLAIN SELECT * FROM t1 WHERE c_float IN (NULL, 1, 2, 3);
EXPLAIN SELECT * FROM t1 WHERE c_float IN (NULL);
EXPLAIN SELECT * FROM t1 WHERE c_float IN (NULL, NULL);

EXPLAIN SELECT * FROM t1 WHERE c_bit IN (1, 2, 3);
EXPLAIN SELECT * FROM t1 WHERE c_bit IN (NULL, 1, 2, 3);
EXPLAIN SELECT * FROM t1 WHERE c_bit IN (NULL);
EXPLAIN SELECT * FROM t1 WHERE c_bit IN (NULL, NULL);

EXPLAIN SELECT * FROM t1 WHERE c_date 
  IN ('2009-09-01', '2009-09-02', '2009-09-03');
EXPLAIN SELECT * FROM t1 WHERE c_date
  IN (NULL, '2009-09-01', '2009-09-02', '2009-09-03');
EXPLAIN SELECT * FROM t1 WHERE c_date IN (NULL);
EXPLAIN SELECT * FROM t1 WHERE c_date IN (NULL, NULL);

EXPLAIN SELECT * FROM t1 WHERE c_datetime
  IN ('2009-09-01 00:00:01', '2009-09-02 00:00:01', '2009-09-03 00:00:01');
EXPLAIN SELECT * FROM t1 WHERE c_datetime
  IN (NULL, '2009-09-01 00:00:01', '2009-09-02 00:00:01', '2009-09-03 00:00:01');
EXPLAIN SELECT * FROM t1 WHERE c_datetime IN (NULL);
EXPLAIN SELECT * FROM t1 WHERE c_datetime IN (NULL, NULL);

EXPLAIN SELECT * FROM t1 WHERE c_timestamp
  IN ('2009-09-01 00:00:01', '2009-09-01 00:00:02', '2009-09-01 00:00:03');
EXPLAIN SELECT * FROM t1 WHERE c_timestamp
  IN (NULL, '2009-09-01 00:00:01', '2009-09-01 00:00:02', '2009-09-01 00:00:03');
EXPLAIN SELECT * FROM t1 WHERE c_timestamp IN (NULL);
EXPLAIN SELECT * FROM t1 WHERE c_timestamp IN (NULL, NULL);

EXPLAIN SELECT * FROM t1 WHERE c_year IN (1, 2, 3);
EXPLAIN SELECT * FROM t1 WHERE c_year IN (NULL, 1, 2, 3);
EXPLAIN SELECT * FROM t1 WHERE c_year IN (NULL);
EXPLAIN SELECT * FROM t1 WHERE c_year IN (NULL, NULL);

EXPLAIN SELECT * FROM t1 WHERE c_char IN ('1', '2', '3');
EXPLAIN SELECT * FROM t1 WHERE c_char IN (NULL, '1', '2', '3');
EXPLAIN SELECT * FROM t1 WHERE c_char IN (NULL);
EXPLAIN SELECT * FROM t1 WHERE c_char IN (NULL, NULL);

DROP TABLE t1;

--echo #
--echo # Bug#54477: Crash on IN / CASE with NULL arguments
--echo #

CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1), (2);

SELECT 1 IN (NULL, a) FROM t1;

SELECT a IN (a, a) FROM t1 GROUP BY a WITH ROLLUP;

SELECT CASE a WHEN a THEN a END FROM t1 GROUP BY a WITH ROLLUP;

DROP TABLE t1;

--echo #
--echo # Bug#58628: Incorrect result for 'WHERE NULL NOT IN (<subquery>)
--echo #

CREATE TABLE t1 (pk INT NOT NULL, i INT);
INSERT INTO t1 VALUES (0,NULL), (1,NULL), (2,NULL), (3,NULL);

CREATE TABLE subq (pk INT NOT NULL, i INT NOT NULL, PRIMARY KEY(i,pk));
INSERT INTO subq VALUES (0,0), (1,1), (2,2), (3,3);

## Baseline queries: t1.i contains only NULL and should effectively
## be evaluated as 'WHERE NULL IN'
## .. These return correct resultset !

--sorted_result
SELECT * FROM t1
  WHERE t1.i NOT IN
    (SELECT i FROM subq WHERE subq.pk = t1.pk);

--sorted_result
SELECT * FROM t1
  WHERE t1.i IN
    (SELECT i FROM subq WHERE subq.pk = t1.pk) IS UNKNOWN;

## Replaced 't1.i' with some constant expression which 
## also evaluates to NULL. Expected to return same result as above:

--sorted_result
SELECT * FROM t1
  WHERE NULL NOT IN
    (SELECT i FROM subq WHERE subq.pk = t1.pk);

--sorted_result
SELECT * FROM t1
  WHERE NULL IN
    (SELECT i FROM subq WHERE subq.pk = t1.pk) IS UNKNOWN;

--sorted_result
SELECT * FROM t1
  WHERE 1+NULL NOT IN
    (SELECT i FROM subq WHERE subq.pk = t1.pk);

DROP TABLE t1,subq;

--echo #
--echo # Bug #11766270  59343: YEAR(4): INCORRECT RESULT AND VALGRIND WARNINGS WITH MIN/MAX, UNION
--echo #

CREATE TABLE t1(f1 YEAR(4));
INSERT INTO t1 VALUES (0000),(2001);
--enable_metadata
(SELECT MAX(f1) FROM t1) UNION (SELECT MAX(f1) FROM t1);
--disable_metadata
DROP TABLE t1;

--echo #
--echo # Bug #11764651-57510: IN(string,real,string) causes invalid read in sort function
--echo #

SELECT LEFT(GEOMFROMTEXT("POINT(0 0)"),1) IN (@@global.query_cache_type,1,"");

--echo # End of test BUG#11764651-57510

--echo End of 5.1 tests
