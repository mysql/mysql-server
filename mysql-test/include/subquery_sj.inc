#
# Nested Loops semi-join subquery evaluation tests
#

# This portion of the file vas developed when subquery materialization
# was rule-based; to preserve the intended test scenarios, we switch
# off cost-based choice for them.
set @old_opt_switch=@@optimizer_switch;
set optimizer_switch='subquery_materialization_cost_based=off';

--disable_warnings
drop table if exists t0, t1, t2, t10, t11, t12;
--enable_warnings

#
# IN subquery optimization test
#
create table t1 (a int not null, b int, primary key (a));
create table t2 (a int not null, primary key (a));
create table t3 (a int not null, b int, primary key (a));
insert into t1 values (1,10), (2,20), (3,30),  (4,40);
insert into t2 values (2), (3), (4), (5);
insert into t3 values (10,3), (20,4), (30,5);
select * from t2 where t2.a in (select a from t1);
explain extended select * from t2 where t2.a in (select a from t1);
select * from t2 where t2.a in (select a from t1 where t1.b <> 30);
explain extended select * from t2 where t2.a in (select a from t1 where t1.b <> 30);
select * from t2 where t2.a in (select t1.a from t1,t3 where t1.b=t3.a);
explain extended select * from t2 where t2.a in (select t1.a from t1,t3 where t1.b=t3.a);
drop table t1, t2, t3;
create table t1 (a int, b int, index a (a,b));
create table t2 (a int, index a (a));
create table t3 (a int, b int, index a (a));
insert into t1 values (1,10), (2,20), (3,30), (4,40);
# making table large enough
create table t0(a int);
insert into t0 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);
insert into t1
select rand()*100000+200,rand()*100000 from t0 A, t0 B, t0 C, t0 D;

insert into t2 values (2), (3), (4), (5);
insert into t3 values (10,3), (20,4), (30,5);
select * from t2 where t2.a in (select a from t1);
explain extended select * from t2 where t2.a in (select a from t1);
select * from t2 where t2.a in (select a from t1 where t1.b <> 30);
explain extended select * from t2 where t2.a in (select a from t1 where t1.b <> 30);
select * from t2 where t2.a in (select t1.a from t1,t3 where t1.b=t3.a);
explain extended select * from t2 where t2.a in (select t1.a from t1,t3 where t1.b=t3.a);
insert into t1 values (3,31);
select * from t2 where t2.a in (select a from t1 where t1.b <> 30);
select * from t2 where t2.a in (select a from t1 where t1.b <> 30 and t1.b <> 31);
explain extended select * from t2 where t2.a in (select a from t1 where t1.b <> 30);
drop table t0, t1, t2, t3;


#
# 1. Subqueries that are converted into semi-joins
#
create table t0 (a int);
insert into t0 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);

create table t1(a int, b int);
insert into t1 values (0,0),(1,1),(2,2);
create table t2 as select * from t1;

create table t11(a int, b int);

create table t10 (pk int, a int, primary key(pk));
insert into t10 select a,a from t0;
create table t12 like t10;
insert into t12 select * from t10;


--echo Flattened because of dependency, t10=func(t1)
explain select * from t1 where a in (select pk from t10);
select * from t1 where a in (select pk from t10);

--echo A confluent case of dependency
explain select * from t1 where a in (select a from t10 where pk=12);
select * from t1 where a in (select a from t10 where pk=12);

explain select * from t1 where a in (select a from t10 where pk=9);
select * from t1 where a in (select a from t10 where pk=9);

--echo An empty table inside
explain select * from t1 where a in (select a from t11);
select * from t1 where a in (select a from t11);

explain select * from t1 where a in (select pk from t10) and b in (select pk from t10);
select * from t1 where a in (select pk from t10) and b in (select pk from t10);

--echo flattening a nested subquery
explain select * from t1 where a in (select pk from t10 where t10.a in (select pk from t12));
select * from t1 where a in (select pk from t10 where t10.a in (select pk from t12));

--echo flattening subquery w/ several tables
explain extended select * from t1 where a in (select t10.pk from t10, t12 where t12.pk=t10.a);

--echo subqueries within outer joins go into ON expr.
# TODO: psergey: check if case conversions like those are ok (it broke on  windows)
--replace_result a A b B
explain extended
select * from t1 left join (t2 A, t2 B) on ( A.a= t1.a and B.a in (select pk from t10));

# TODO: psergey: check if case conversions like those are ok (it broke on  windows)
--echo t2 should be wrapped into OJ-nest, so we have "t1 LJ (t2 J t10)"
--replace_result a A b B
explain extended
select * from t1 left join t2 on (t2.a= t1.a and t2.a in (select pk from t10));

--echo we shouldn't flatten if we're going to get a join of > MAX_TABLES.
explain select * from 
  t1 s00, t1 s01,  t1 s02, t1 s03, t1 s04,t1 s05,t1 s06,t1 s07,t1 s08,t1 s09,
  t1 s10, t1 s11,  t1 s12, t1 s13, t1 s14,t1 s15,t1 s16,t1 s17,t1 s18,t1 s19,
  t1 s20, t1 s21,  t1 s22, t1 s23, t1 s24,t1 s25,t1 s26,t1 s27,t1 s28,t1 s29,
  t1 s30, t1 s31,  t1 s32, t1 s33, t1 s34,t1 s35,t1 s36,t1 s37,t1 s38,t1 s39,
  t1 s40, t1 s41,  t1 s42, t1 s43, t1 s44,t1 s45,t1 s46,t1 s47,t1 s48,t1 s49
where
  s00.a in (
  select m00.a from
    t1 m00, t1 m01,  t1 m02, t1 m03, t1 m04,t1 m05,t1 m06,t1 m07,t1 m08,t1 m09,
    t1 m10, t1 m11,  t1 m12, t1 m13, t1 m14,t1 m15,t1 m16,t1 m17,t1 m18,t1 m19
  );

select * from
  t1 left join t2 on (t2.a= t1.a and t2.a in (select pk from t10)) 
where t1.a < 5;

# 
# Prepared statements
#
prepare s1 from
  ' select * from
    t1 left join t2 on (t2.a= t1.a and t2.a in (select pk from t10))
  where t1.a < 5';
execute s1;
execute s1;

# Try I2O orders
insert into t1 select (A.a + 10 * B.a),1 from t0 A, t0 B;
explain extended select * from t1 where a in (select pk from t10 where pk<3);

drop table t0, t1, t2;
drop table t10, t11, t12;

--echo #
--echo # Check that subqueries with outer joins or straight_join work for 
--echo # different permutations of const and non-const tables.  (Ref. Bug#46692)
--echo #
CREATE TABLE t1 (i INTEGER);
CREATE TABLE t2 (i INTEGER);
CREATE TABLE t3 (i INTEGER);
let $i=3;
while ($i)
{
   let $j=3;
   while ($j)
   {
       let $k=3;
       while ($k)
       {
         SELECT (SELECT COUNT(*) from t1) AS c1,
                (SELECT COUNT(*) from t2) AS c2,
                (SELECT COUNT(*) from t3) AS c3;

         let $query=
         SELECT * FROM t1 WHERE (t1.i) IN 
           (SELECT t3.i FROM t2 INNER JOIN t3 ON t2.i=t3.i);
         eval EXPLAIN $query;
         eval $query;

         eval PREPARE stmt FROM "$query";
         EXECUTE stmt;
         EXECUTE stmt;
         DEALLOCATE PREPARE stmt;

         let $query=
         SELECT * FROM t1 WHERE (t1.i) IN 
           (SELECT t3.i FROM t2 LEFT JOIN t3 ON t2.i=t3.i);
         eval EXPLAIN $query;
         eval $query;

         eval PREPARE stmt FROM "$query";
         EXECUTE stmt;
         EXECUTE stmt;
         DEALLOCATE PREPARE stmt;

         let $query=
         SELECT * FROM t1 WHERE (t1.i) IN 
           (SELECT t3.i FROM t2 RIGHT JOIN t3 ON t2.i=t3.i);
         eval EXPLAIN $query;
         eval $query;

         let $query=
         SELECT * FROM t1 WHERE (t1.i) IN 
           (SELECT t3.i FROM t2 STRAIGHT_JOIN t3);
         eval EXPLAIN $query;
         eval $query;

         let $query=
         SELECT * FROM t1 WHERE (11) IN 
           (SELECT t3.i FROM t2 LEFT JOIN t3 ON t2.i=t3.i);
         eval EXPLAIN $query;
         eval $query;

         let $query=
         SELECT * FROM t1 WHERE (11) IN 
           (SELECT t3.i FROM t2 LEFT JOIN t3 ON t2.i=t3.i WHERE t1.i);
         eval EXPLAIN $query;
         eval $query;

         let $query=
         SELECT * FROM t1 WHERE (11) IN 
           (SELECT t3.i FROM t2 RIGHT JOIN t3 ON t2.i=t3.i);
         eval EXPLAIN $query;
         eval $query;

         let $query=
         SELECT * FROM t1 WHERE (11) IN 
           (SELECT t3.i FROM t2 STRAIGHT_JOIN t3);
         eval EXPLAIN $query;
         eval $query;

	 dec $k;
	 eval INSERT INTO t3 VALUES ($k);
       }
       DELETE FROM t3;
       dec $j;
       eval INSERT INTO t2 VALUES ($j);	
   }
   DELETE FROM t2;
   dec $i;
   eval INSERT INTO t1 VALUES ($i);
}
DROP TABLE t1, t2, t3;

# Test various IN and EXISTS queries with NULL values and UNKNOWN

create table x1(k int primary key, d1 int, d2 int);
create table x2(k int primary key, d1 int, d2 int);

insert into x1 values
    (10,   10,   10),
    (20,   20,   20),
    (21,   20,   null),
    (30,   null, 30),
    (40,   40,   40);
insert into x2 values
    (10,   10,   10),
    (20,   20,   20),
    (21,   20,   null),
    (30,   null, 30);

# Q1 T=(10, 20) U=(21,30) F=(40)
select *
from x1
where (d1, d2) in (select d1, d2
                   from x2);
select *
from x1
where (d1, d2) in (select d1, d2
                   from x2) is true;
select *
from x1
where (d1, d2) in (select d1, d2
                   from x2) is false;
select *
from x1
where (d1, d2) in (select d1, d2
                   from x2) is unknown;

# Q2 T=(10, 20) U=(30) F=(21, 40)
select *
from x1
where d1 in (select d1
             from x2
             where x1.d2=x2.d2);
select *
from x1
where d1 in (select d1
             from x2
             where x1.d2=x2.d2) is true;
select *
from x1
where d1 in (select d1
             from x2
             where x1.d2=x2.d2) is false;
select *
from x1
where d1 in (select d1
             from x2
             where x1.d2=x2.d2) is unknown;

# Q3 T=(10, 20) U=() F=(21, 30, 40)
select *
from x1
where 1 in (select 1
            from x2
            where x1.d1=x2.d1 and x1.d2=x2.d2);
select *
from x1
where 1 in (select 1
            from x2
            where x1.d1=x2.d1 and x1.d2=x2.d2) is true;
select *
from x1
where 1 in (select 1
            from x2
            where x1.d1=x2.d1 and x1.d2=x2.d2) is false;
select *
from x1
where 1 in (select 1
            from x2
            where x1.d1=x2.d1 and x1.d2=x2.d2) is unknown;

# Q4 T=(10, 20) F=(21, 30, 40)
select *
from x1
where exists (select *
              from x2
              where x1.d1=x2.d1 and x1.d2=x2.d2);

drop table x1;
drop table x2;


#
# Test for the problem with using sj-materialization when subquery's select 
# list element SCOL is covered by equality propagation and has preceding equal
# column PCOL which belongs to a table within the the semi-join nest: SJM-Scan
# process should unpack column value not to SCOL but rather to PCOL, as 
# substitute_best_equal has made all conditions to refer to PCOL.
#
CREATE TABLE t1 (
  a int(11) NOT NULL,
  b int(11) NOT NULL,
  c datetime default NULL,
  PRIMARY KEY  (a),
  KEY idx_bc (b,c)
);

INSERT INTO t1 VALUES 
(406989,67,'2006-02-23 17:08:46'), (150078,67,'2005-10-26 11:17:45'),
(406993,67,'2006-02-27 11:20:57'), (245655,67,'2005-12-08 15:59:08'),
(406994,67,'2006-02-27 11:26:46'), (256,67,NULL),
(398341,67,'2006-02-20 04:48:44'), (254,67,NULL),(1120,67,NULL),
(406988,67,'2006-02-23 17:07:22'), (255,67,NULL),
(398340,67,'2006-02-20 04:38:53'),(406631,67,'2006-02-23 10:49:42'),
(245653,67,'2005-12-08 15:59:07'),(406992,67,'2006-02-24 16:47:18'),
(245654,67,'2005-12-08 15:59:08'),(406995,67,'2006-02-28 11:55:00'),
(127261,67,'2005-10-13 12:17:58'),(406991,67,'2006-02-24 16:42:32'),
(245652,67,'2005-12-08 15:58:27'),(398545,67,'2006-02-20 04:53:13'),
(154504,67,'2005-10-28 11:53:01'),(9199,67,NULL),(1,67,'2006-02-23 15:01:35'),
(223456,67,NULL),(4101,67,NULL),(1133,67,NULL),
(406990,67,'2006-02-23 18:01:45'),(148815,67,'2005-10-25 15:34:17'),
(148812,67,'2005-10-25 15:30:01'),(245651,67,'2005-12-08 15:58:27'),
(154503,67,'2005-10-28 11:52:38');

create table t11 select * from t1 where b = 67 AND (c IS NULL OR c > NOW()) order by 3 asc;
create table t12 select * from t1 where b = 67 AND (c IS NULL OR c > NOW()) order by 3 desc;
create table t21 select * from t1 where b = 67 AND (c IS NULL OR c > '2005-12-08') order by 3 asc;
create table t22 select * from t1 where b = 67 AND (c IS NULL OR c > '2005-12-08') order by 3 desc;

update t22 set c = '2005-12-08 15:58:27' where a = 255;
explain select t21.* from t21,t22 where t21.a = t22.a and 
t22.a in (select t12.a from t11, t12 where t11.a in(255,256) and t11.a = t12.a and t11.c is null) and t22.c is null order by t21.a;
explain format=json select * from t1 where a in (select a from t11);
select t21.* from t21,t22 where t21.a = t22.a and 
t22.a in (select t12.a from t11, t12 where t11.a in(255,256) and t11.a = t12.a and t11.c is null) and t22.c is null order by t21.a;

drop table t1, t11, t12, t21, t22;

#
# Test sj-materialization re-execution. The test isn't meaningful (materialized
# table stays the same across all executions) because it's hard to create a
# dataset that would verify correct re-execution without hitting BUG#31480
# 
create table t1(a int);
insert into t1 values (0),(1);

explain 
select (select max(Y.a) from t1 Y where a in (select a from t1 Z) and a < X.a) as subq from t1 X;
select (select max(Y.a) from t1 Y where a in (select a from t1 Z) and a < X.a) as subq from t1 X;

drop table t1;

#
# Test confluent duplicate weedout
#
create table t0 (a int);
insert into t0 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);
create table t1 as select * from t0;
insert into t1 select a+10 from t0;
insert into t0 values(2);
explain select * from t1 where 2 in (select a from t0);
select * from t1 where 2 in (select a from t0);

# 
# FirstMatch referring to a derived table
#
let $query=select * from (select a from t0) x where a in (select a from t1);
--eval explain $query
--eval explain format=json $query
drop table t0, t1;

#
# LooseScan: Check if we can pick it together with range access
#
create table t0 (a int);
insert into t0 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);

create table t1 (kp1 int, kp2 int, c int, filler char(100), key(kp1, kp2));
insert into t1 select A.a+10*(B.a+10*C.a), 0, 0, 'filler' from t0 A, t0 B, t0 C;
insert into t1 select * from t1 where kp1 < 20;

create table t3 (a int);
insert into t3 select A.a + 10*B.a from t0 A, t0 B;

explain select * from t3 where a in (select kp1 from t1 where kp1<20);
select * from t3 where a in (select kp1 from t1 where kp1<20);

explain select * from t3 where a in (select kp1 from t1 where kp1<20) and a<20;
select * from t3 where a in (select kp1 from t1 where kp1<20) and a<20;

create table t4 (pk int primary key);
insert into t4 select a from t3;

explain select * from t3 where a in 
          (select t1.kp1 from t1,t4 where kp1<20 and t4.pk=t1.c);
select * from t3 where a in 
  (select t1.kp1 from t1,t4 where kp1<20 and t4.pk=t1.c);

drop table t1, t3, t4;

#
# Test if we handle duplicate elimination temptable overflowing to disk
#
create table t1 (a int);
insert into t1 values (0),(0),(0),(1),(1),(1),(2),(2),(2),(3),(3),(3);

set @save_max_heap_table_size=@@max_heap_table_size;
set @@max_heap_table_size= 16384;

--echo # Attempt to make one test that overflows the heap table when a
--echo # non-duplicate row is inserted and one test that overflows the
--echo # heap table when a duplicate record is inserted. Debugging showed
--echo # that these situations occurred with max_heap_table_size=16384
--echo # and optimizer_join_cache_level equals 1 and 0, respectively.
--echo # Finally execute a test that does not overflow the heap table.
explain
select count(*) from t0 A, t0 B, t0 C
where C.a in (select a from t1 D);
flush status;
select count(*) from t0 A, t0 B, t0 C
where C.a in (select a from t1 D);
show status like 'Created_tmp_disk_tables';

set @@max_heap_table_size= @save_max_heap_table_size;
flush status;
select count(*) from t0 A, t0 B, t0 C
where C.a in (select a from t1 D);
show status like 'Created_tmp_disk_tables';

drop table t0, t1;
#
# Materialize + Scan + ref access to the subsequent table based on scanned
# value
# 
create table t0 (a int);
insert into t0 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);
create table t2(a int);
insert into t2 values (1),(2);
create table t3 ( a int , filler char(100), key(a)); 
insert into t3 select A.a + 10*B.a, 'filler' from t0 A, t0 B;
explain select * from t3 where a in (select a from t2) and (a > 5 or a < 10); 
select * from t3 where a in (select a from t2); 

drop table t0, t2, t3;

#
# DATETIME type checks
#
create table t1 (a date);
insert into t1 values ('2008-01-01'),('2008-01-01'),('2008-02-01'),('2008-02-01');
create table t2 (a int);
insert into t2 values (1),(2);
create table t3 (a char(10));
insert into t3 select * from t1;
insert into t3 values (1),(2);
explain select * from t2 where a in (select a from t1);
explain select * from t2 where a in (select a from t2);
explain select * from t2 where a in (select a from t3);
explain select * from t1 where a in (select a from t3);
drop table t1, t2, t3;
create table t1 (a decimal);
insert into t1 values (1),(2);
explain select * from t1 where a in (select a from t1);
drop table t1;

#
# SJ-Materialization-scan for non-first table
#
create table t1 (a int);
insert into t1 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);
create table t2 as select * from t1;
create table t3 (a int, b int, filler char(100), key(a));
insert into t3 select A.a + 10*B.a, A.a + 10*B.a, 'filler' from t1 A, t1 B, t1 C;
explain select * from t1, t3 where t3.a in (select a from t2) and (t3.a < 10 or t3.a >30) and t1.a =3;
explain format=json select * from t1, t3 where t3.a in (select a from t2) and (t3.a < 10 or t3.a >30) and t1.a =3;

#
# Verify that straight_join modifier in parent or child prevents flattening
#
explain select straight_join * from t1 A, t1 B where A.a in (select a from t2);
explain select * from t2 where a in (select straight_join A.a from t1 A, t1 B);
explain select * from t2 where a in (select straight_join A.a from t1 A, t1 B);
explain select straight_join * from t2 X, t2 Y 
where X.a in (select straight_join A.a from t1 A, t1 B);

#
# SJ-Materialization scan + first table being system const table
#
create table t0 (a int, b int);
insert into t0 values(1,1);
explain select * from t0, t3 where t3.a in (select a from t2) and (t3.a < 10 or t3.a >30);
create table t4 as select a as x, a as y from t1;
explain select * from t0, t3 where (t3.a, t3.b) in (select x,y from t4) and (t3.a < 10 or t3.a >30);
drop table t0,t1,t2,t3,t4;

#
# LooseScan with ref access
#
create table t0 (a int);
insert into t0 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);
create table t1 (a int, b int, filler char(100), key(a,b));
insert into t1 select A.a, B.a, 'filler' from t0 A, t0 B;
create table t2 as select * from t1;

explain select * from t2 where a in (select b from t1 where a=3);
explain select * from t2 where (b,a) in (select a,b from t1 where a=3);

drop table t1,t2;

#
# Multi-column sj-materialization with lookups
#
create table t1 (a int, b int);
insert into t1 select a,a from t0;
create table t2 (a int, b int); 
insert into t2 select A.a + 10*B.a, A.a + 10*B.a from t0 A, t0 B;

explain select * from t1 where (a,b) in (select a,b from t2);

drop table t0, t1, t2;


#
# Primitive SJ-Materialization tests for DECIMAL and DATE
#
create table t0 (a decimal(4,2));
insert into t0 values (10.24), (22.11);
create table t1 as select * from t0;
insert into t1 select * from t0;
explain select * from t0 where a in (select a from t1);
select * from t0 where a in (select a from t1);
drop table t0, t1;

create table t0(a date);
insert into t0 values ('2008-01-01'),('2008-02-02');
create table t1 as select * from t0;
insert into t1 select * from t0;
explain select * from t0 where a in (select a from t1);
select * from t0 where a in (select a from t1);
drop table t0, t1;

#
# Fix a trivial crash with SJ-Materialization lookup, multiple tables in the
# subquery, and a condition on some of inner tables but not others
#
create table t0(a int);
insert into t0 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);
create table t1 as select a as a, a as b, a as c from t0 where a < 3;
create table t2 as select a as a, a as b from t0 where a < 3;
insert into t2 select * from t2;

explain select * from t1 where (a,b,c) in (select X.a, Y.a, Z.a from t2 X, t2 Y, t2 Z where X.b=33);

drop table t0,t1,t2;


#
# Test join buffering
#
set @save_join_buffer_size = @@join_buffer_size;
set join_buffer_size= 8192;

create table t0 (a int);
insert into t0 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);

create table t1 (a int, filler1 binary(200), filler2 binary(200));
insert into t1 select a, 'filler123456', 'filler123456' from t0;
insert into t1 select a+10, 'filler123456', 'filler123456' from t0;

create table t2 as select * from t1;
insert into t1 select a+20, 'filler123456', 'filler123456' from t0;

insert into t1 values (2, 'duplicate ok', 'duplicate ok');
insert into t1 values (18, 'duplicate ok', 'duplicate ok');

insert into t2 values (3, 'duplicate ok', 'duplicate ok');
insert into t2 values (19, 'duplicate ok', 'duplicate ok');

explain select 
 a, mid(filler1, 1,10), length(filler1)=length(filler2) as Z 
from t1 ot where a in (select a from t2 it);
--sorted_result
select 
 a, mid(filler1, 1,10), length(filler1)=length(filler2) as Z 
from t1 ot where a in (select a from t2 it);

explain select 
  a, mid(filler1, 1,10), length(filler1)=length(filler2) 
from t2 ot where a in (select a from t1 it);
--sorted_result
select 
  a, mid(filler1, 1,10), length(filler1)=length(filler2) 
from t2 ot where a in (select a from t1 it);

# Now let the buffer overfill:
insert into t1 select a+20, 'filler123456', 'filler123456' from t0;
insert into t1 select a+20, 'filler123456', 'filler123456' from t0;

explain select 
 a, mid(filler1, 1,10), length(filler1)=length(filler2) as Z 
from t1 ot where a in (select a from t2 it);
--sorted_result
select 
 a, mid(filler1, 1,10), length(filler1)=length(filler2) as Z 
from t1 ot where a in (select a from t2 it);

explain select 
  a, mid(filler1, 1,10), length(filler1)=length(filler2) 
from t2 ot where a in (select a from t1 it);
--sorted_result
select 
  a, mid(filler1, 1,10), length(filler1)=length(filler2) 
from t2 ot where a in (select a from t1 it);

set @@join_buffer_size = @save_join_buffer_size;
drop table t1, t2;

# Check ref access to tables inside the OJ nest inside the SJ nest
create table t1 (a int, b int, key(a));
create table t2 (a int, b int, key(a));
create table t3 (a int, b int, key(a));

insert into t1 select a,a from t0;
insert into t2 select a,a from t0;
insert into t3 select a,a from t0;

--echo t2 and t3 must be use 'ref', not 'ALL':
explain select * 
from t0 where a in
  (select t2.a+t3.a from t1 left join (t2 join t3) on t2.a=t1.a and t3.a=t1.a);

drop table t0, t1,t2,t3;


--echo
--echo Test that neither MaterializeLookup strategy for semijoin,
--echo nor subquery materialization is used when BLOBs are involved 
--echo (except when arguments of some functions).
--echo
set @prefix_len = 6;

# BLOB == 16 (small blobs that could be stored in HEAP tables)
set @blob_len = 16;
set @suffix_len = @blob_len - @prefix_len;

create table t1_16 (a1 blob(16), a2 blob(16));
create table t2_16 (b1 blob(16), b2 blob(16));
create table t3_16 (c1 blob(16), c2 blob(16));

insert into t1_16 values
 (concat('1 - 00', repeat('x', @suffix_len)), concat('2 - 00', repeat('x', @suffix_len)));
insert into t1_16 values
 (concat('1 - 01', repeat('x', @suffix_len)), concat('2 - 01', repeat('x', @suffix_len)));
insert into t1_16 values
 (concat('1 - 02', repeat('x', @suffix_len)), concat('2 - 02', repeat('x', @suffix_len)));

insert into t2_16 values
 (concat('1 - 01', repeat('x', @suffix_len)), concat('2 - 01', repeat('x', @suffix_len)));
insert into t2_16 values
 (concat('1 - 02', repeat('x', @suffix_len)), concat('2 - 02', repeat('x', @suffix_len)));
insert into t2_16 values
 (concat('1 - 03', repeat('x', @suffix_len)), concat('2 - 03', repeat('x', @suffix_len)));

insert into t3_16 values
 (concat('1 - 01', repeat('x', @suffix_len)), concat('2 - 01', repeat('x', @suffix_len)));
insert into t3_16 values
 (concat('1 - 02', repeat('x', @suffix_len)), concat('2 - 02', repeat('x', @suffix_len)));
insert into t3_16 values
 (concat('1 - 03', repeat('x', @suffix_len)), concat('2 - 03', repeat('x', @suffix_len)));
insert into t3_16 values
 (concat('1 - 04', repeat('x', @suffix_len)), concat('2 - 04', repeat('x', @suffix_len)));

# single value transformer
explain extended select left(a1,7), left(a2,7)
from t1_16
where a1 in (select b1 from t2_16 where b1 > '0');

select left(a1,7), left(a2,7)
from t1_16
where a1 in (select b1 from t2_16 where b1 > '0');

# row value transformer
explain extended select left(a1,7), left(a2,7)
from t1_16
where (a1,a2) in (select b1, b2 from t2_16 where b1 > '0');

select left(a1,7), left(a2,7)
from t1_16
where (a1,a2) in (select b1, b2 from t2_16 where b1 > '0');

# string function with a blob argument, the return type may be != blob
explain extended select left(a1,7), left(a2,7)
from t1_16
where a1 in (select substring(b1,1,16) from t2_16 where b1 > '0');

select left(a1,7), left(a2,7)
from t1_16
where a1 in (select substring(b1,1,16) from t2_16 where b1 > '0');

# group_concat with a blob argument - depends on
# the variable group_concat_max_len, and
# convert_blob_length == max_len*collation->mbmaxlen > CONVERT_IF_BIGGER_TO_BLOB
explain extended select left(a1,7), left(a2,7)
from t1_16
where a1 in (select group_concat(b1) from t2_16 group by b2);

select left(a1,7), left(a2,7)
from t1_16
where a1 in (select group_concat(b1) from t2_16 group by b2);

set @@group_concat_max_len = 256; # anything < (CONVERT_IF_BIGGER_TO_BLOB = 512)

explain extended select left(a1,7), left(a2,7)
from t1_16
where a1 in (select group_concat(b1) from t2_16 group by b2);

select left(a1,7), left(a2,7)
from t1_16
where a1 in (select group_concat(b1) from t2_16 group by b2);

# BLOB column at the second (intermediate) level of nesting
create table t1 (a1 char(8), a2 char(8));
create table t2 (b1 char(8), b2 char(8));
create table t3 (c1 char(8), c2 char(8));
insert into t1 values ('1 - 00', '2 - 00');
insert into t1 values ('1 - 01', '2 - 01');
insert into t1 values ('1 - 02', '2 - 02');
insert into t2 values ('1 - 01', '2 - 01');
insert into t2 values ('1 - 01', '2 - 01');
insert into t2 values ('1 - 02', '2 - 02');
insert into t2 values ('1 - 02', '2 - 02');
insert into t2 values ('1 - 03', '2 - 03');
insert into t3 values ('1 - 01', '2 - 01');
insert into t3 values ('1 - 02', '2 - 02');
insert into t3 values ('1 - 03', '2 - 03');
insert into t3 values ('1 - 04', '2 - 04');

explain extended
select * from t1
where concat(a1,'x') IN
      (select left(a1,8) from t1_16
       where (a1, a2) IN
             (select t2_16.b1, t2_16.b2 from t2_16, t2
              where t2.b2 = substring(t2_16.b2,1,6) and
                    t2.b1 IN (select c1 from t3 where c2 > '0')));


drop table t1_16, t2_16, t3_16, t1, t2, t3;


# BLOB == 512 (CONVERT_IF_BIGGER_TO_BLOB == 512)
set @blob_len = 512;
set @suffix_len = @blob_len - @prefix_len;

create table t1_512 (a1 blob(512), a2 blob(512));
create table t2_512 (b1 blob(512), b2 blob(512));
create table t3_512 (c1 blob(512), c2 blob(512));

insert into t1_512 values
 (concat('1 - 00', repeat('x', @suffix_len)), concat('2 - 00', repeat('x', @suffix_len)));
insert into t1_512 values
 (concat('1 - 01', repeat('x', @suffix_len)), concat('2 - 01', repeat('x', @suffix_len)));
insert into t1_512 values
 (concat('1 - 02', repeat('x', @suffix_len)), concat('2 - 02', repeat('x', @suffix_len)));

insert into t2_512 values
 (concat('1 - 01', repeat('x', @suffix_len)), concat('2 - 01', repeat('x', @suffix_len)));
insert into t2_512 values
 (concat('1 - 02', repeat('x', @suffix_len)), concat('2 - 02', repeat('x', @suffix_len)));
insert into t2_512 values
 (concat('1 - 03', repeat('x', @suffix_len)), concat('2 - 03', repeat('x', @suffix_len)));

insert into t3_512 values
 (concat('1 - 01', repeat('x', @suffix_len)), concat('2 - 01', repeat('x', @suffix_len)));
insert into t3_512 values
 (concat('1 - 02', repeat('x', @suffix_len)), concat('2 - 02', repeat('x', @suffix_len)));
insert into t3_512 values
 (concat('1 - 03', repeat('x', @suffix_len)), concat('2 - 03', repeat('x', @suffix_len)));
insert into t3_512 values
 (concat('1 - 04', repeat('x', @suffix_len)), concat('2 - 04', repeat('x', @suffix_len)));

# single value transformer
explain extended select left(a1,7), left(a2,7)
from t1_512
where a1 in (select b1 from t2_512 where b1 > '0');

select left(a1,7), left(a2,7)
from t1_512
where a1 in (select b1 from t2_512 where b1 > '0');

# row value transformer
explain extended select left(a1,7), left(a2,7)
from t1_512
where (a1,a2) in (select b1, b2 from t2_512 where b1 > '0');

select left(a1,7), left(a2,7)
from t1_512
where (a1,a2) in (select b1, b2 from t2_512 where b1 > '0');

# string function with a blob argument, the return type may be != blob
explain extended select left(a1,7), left(a2,7)
from t1_512
where a1 in (select substring(b1,1,512) from t2_512 where b1 > '0');

select left(a1,7), left(a2,7)
from t1_512
where a1 in (select substring(b1,1,512) from t2_512 where b1 > '0');

# group_concat with a blob argument - depends on
# the variable group_concat_max_len, and
# convert_blob_length == max_len*collation->mbmaxlen > CONVERT_IF_BIGGER_TO_BLOB
explain extended select left(a1,7), left(a2,7)
from t1_512
where a1 in (select group_concat(b1) from t2_512 group by b2);

select left(a1,7), left(a2,7)
from t1_512
where a1 in (select group_concat(b1) from t2_512 group by b2);

set @@group_concat_max_len = 256; # anything < (CONVERT_IF_BIGGER_TO_BLOB = 512)

explain extended select left(a1,7), left(a2,7)
from t1_512
where a1 in (select group_concat(b1) from t2_512 group by b2);

select left(a1,7), left(a2,7)
from t1_512
where a1 in (select group_concat(b1) from t2_512 group by b2);

drop table t1_512, t2_512, t3_512;


# BLOB == 513 (CONVERT_IF_BIGGER_TO_BLOB < 513)
set @blob_len = 513;
set @suffix_len = @blob_len - @prefix_len;

create table t1_513 (a1 blob(513), a2 blob(513));
create table t2_513 (b1 blob(513), b2 blob(513));
create table t3_513 (c1 blob(513), c2 blob(513));

insert into t1_513 values
 (concat('1 - 00', repeat('x', @suffix_len)), concat('2 - 00', repeat('x', @suffix_len)));
insert into t1_513 values
 (concat('1 - 01', repeat('x', @suffix_len)), concat('2 - 01', repeat('x', @suffix_len)));
insert into t1_513 values
 (concat('1 - 02', repeat('x', @suffix_len)), concat('2 - 02', repeat('x', @suffix_len)));

insert into t2_513 values
 (concat('1 - 01', repeat('x', @suffix_len)), concat('2 - 01', repeat('x', @suffix_len)));
insert into t2_513 values
 (concat('1 - 02', repeat('x', @suffix_len)), concat('2 - 02', repeat('x', @suffix_len)));
insert into t2_513 values
 (concat('1 - 03', repeat('x', @suffix_len)), concat('2 - 03', repeat('x', @suffix_len)));

insert into t3_513 values
 (concat('1 - 01', repeat('x', @suffix_len)), concat('2 - 01', repeat('x', @suffix_len)));
insert into t3_513 values
 (concat('1 - 02', repeat('x', @suffix_len)), concat('2 - 02', repeat('x', @suffix_len)));
insert into t3_513 values
 (concat('1 - 03', repeat('x', @suffix_len)), concat('2 - 03', repeat('x', @suffix_len)));
insert into t3_513 values
 (concat('1 - 04', repeat('x', @suffix_len)), concat('2 - 04', repeat('x', @suffix_len)));

# single value transformer
explain extended select left(a1,7), left(a2,7)
from t1_513
where a1 in (select b1 from t2_513 where b1 > '0');

select left(a1,7), left(a2,7)
from t1_513
where a1 in (select b1 from t2_513 where b1 > '0');

# row value transformer
explain extended select left(a1,7), left(a2,7)
from t1_513
where (a1,a2) in (select b1, b2 from t2_513 where b1 > '0');

select left(a1,7), left(a2,7)
from t1_513
where (a1,a2) in (select b1, b2 from t2_513 where b1 > '0');

# string function with a blob argument, the return type may be != blob
explain extended select left(a1,7), left(a2,7)
from t1_513
where a1 in (select substring(b1,1,513) from t2_513 where b1 > '0');

select left(a1,7), left(a2,7)
from t1_513
where a1 in (select substring(b1,1,513) from t2_513 where b1 > '0');

# group_concat with a blob argument - depends on
# the variable group_concat_max_len, and
# convert_blob_length == max_len*collation->mbmaxlen > CONVERT_IF_BIGGER_TO_BLOB
explain extended select left(a1,7), left(a2,7)
from t1_513
where a1 in (select group_concat(b1) from t2_513 group by b2);

select left(a1,7), left(a2,7)
from t1_513
where a1 in (select group_concat(b1) from t2_513 group by b2);

drop table t1_513, t2_513, t3_513;


# BLOB == 1024 (group_concat_max_len == 1024)
set @blob_len = 1024;
set @suffix_len = @blob_len - @prefix_len;

create table t1_1024 (a1 blob(1024), a2 blob(1024));
create table t2_1024 (b1 blob(1024), b2 blob(1024));
create table t3_1024 (c1 blob(1024), c2 blob(1024));

insert into t1_1024 values
 (concat('1 - 00', repeat('x', @suffix_len)), concat('2 - 00', repeat('x', @suffix_len)));
insert into t1_1024 values
 (concat('1 - 01', repeat('x', @suffix_len)), concat('2 - 01', repeat('x', @suffix_len)));
insert into t1_1024 values
 (concat('1 - 02', repeat('x', @suffix_len)), concat('2 - 02', repeat('x', @suffix_len)));

insert into t2_1024 values
 (concat('1 - 01', repeat('x', @suffix_len)), concat('2 - 01', repeat('x', @suffix_len)));
insert into t2_1024 values
 (concat('1 - 02', repeat('x', @suffix_len)), concat('2 - 02', repeat('x', @suffix_len)));
insert into t2_1024 values
 (concat('1 - 03', repeat('x', @suffix_len)), concat('2 - 03', repeat('x', @suffix_len)));

insert into t3_1024 values
 (concat('1 - 01', repeat('x', @suffix_len)), concat('2 - 01', repeat('x', @suffix_len)));
insert into t3_1024 values
 (concat('1 - 02', repeat('x', @suffix_len)), concat('2 - 02', repeat('x', @suffix_len)));
insert into t3_1024 values
 (concat('1 - 03', repeat('x', @suffix_len)), concat('2 - 03', repeat('x', @suffix_len)));
insert into t3_1024 values
 (concat('1 - 04', repeat('x', @suffix_len)), concat('2 - 04', repeat('x', @suffix_len)));

# single value transformer
explain extended select left(a1,7), left(a2,7)
from t1_1024
where a1 in (select b1 from t2_1024 where b1 > '0');

select left(a1,7), left(a2,7)
from t1_1024
where a1 in (select b1 from t2_1024 where b1 > '0');

# row value transformer
explain extended select left(a1,7), left(a2,7)
from t1_1024
where (a1,a2) in (select b1, b2 from t2_1024 where b1 > '0');

select left(a1,7), left(a2,7)
from t1_1024
where (a1,a2) in (select b1, b2 from t2_1024 where b1 > '0');

# string function with a blob argument, the return type may be != blob
explain extended select left(a1,7), left(a2,7)
from t1_1024
where a1 in (select substring(b1,1,1024) from t2_1024 where b1 > '0');

select left(a1,7), left(a2,7)
from t1_1024
where a1 in (select substring(b1,1,1024) from t2_1024 where b1 > '0');

# group_concat with a blob argument - depends on
# the variable group_concat_max_len, and
# convert_blob_length == max_len*collation->mbmaxlen > CONVERT_IF_BIGGER_TO_BLOB
explain extended select left(a1,7), left(a2,7)
from t1_1024
where a1 in (select group_concat(b1) from t2_1024 group by b2);

select left(a1,7), left(a2,7)
from t1_1024
where a1 in (select group_concat(b1) from t2_1024 group by b2);

set @@group_concat_max_len = 256; # anything < (CONVERT_IF_BIGGER_TO_BLOB = 1024)

explain extended select left(a1,7), left(a2,7)
from t1_1024
where a1 in (select group_concat(b1) from t2_1024 group by b2);

select left(a1,7), left(a2,7)
from t1_1024
where a1 in (select group_concat(b1) from t2_1024 group by b2);

drop table t1_1024, t2_1024, t3_1024;


# BLOB == 1025
set @blob_len = 1025;
set @suffix_len = @blob_len - @prefix_len;

create table t1_1025 (a1 blob(1025), a2 blob(1025));
create table t2_1025 (b1 blob(1025), b2 blob(1025));
create table t3_1025 (c1 blob(1025), c2 blob(1025));

insert into t1_1025 values
 (concat('1 - 00', repeat('x', @suffix_len)), concat('2 - 00', repeat('x', @suffix_len)));
insert into t1_1025 values
 (concat('1 - 01', repeat('x', @suffix_len)), concat('2 - 01', repeat('x', @suffix_len)));
insert into t1_1025 values
 (concat('1 - 02', repeat('x', @suffix_len)), concat('2 - 02', repeat('x', @suffix_len)));

insert into t2_1025 values
 (concat('1 - 01', repeat('x', @suffix_len)), concat('2 - 01', repeat('x', @suffix_len)));
insert into t2_1025 values
 (concat('1 - 02', repeat('x', @suffix_len)), concat('2 - 02', repeat('x', @suffix_len)));
insert into t2_1025 values
 (concat('1 - 03', repeat('x', @suffix_len)), concat('2 - 03', repeat('x', @suffix_len)));

insert into t3_1025 values
 (concat('1 - 01', repeat('x', @suffix_len)), concat('2 - 01', repeat('x', @suffix_len)));
insert into t3_1025 values
 (concat('1 - 02', repeat('x', @suffix_len)), concat('2 - 02', repeat('x', @suffix_len)));
insert into t3_1025 values
 (concat('1 - 03', repeat('x', @suffix_len)), concat('2 - 03', repeat('x', @suffix_len)));
insert into t3_1025 values
 (concat('1 - 04', repeat('x', @suffix_len)), concat('2 - 04', repeat('x', @suffix_len)));

# single value transformer
explain extended select left(a1,7), left(a2,7)
from t1_1025
where a1 in (select b1 from t2_1025 where b1 > '0');

select left(a1,7), left(a2,7)
from t1_1025
where a1 in (select b1 from t2_1025 where b1 > '0');

# row value transformer
explain extended select left(a1,7), left(a2,7)
from t1_1025
where (a1,a2) in (select b1, b2 from t2_1025 where b1 > '0');

select left(a1,7), left(a2,7)
from t1_1025
where (a1,a2) in (select b1, b2 from t2_1025 where b1 > '0');

# string function with a blob argument, the return type may be != blob
explain extended select left(a1,7), left(a2,7)
from t1_1025
where a1 in (select substring(b1,1,1025) from t2_1025 where b1 > '0');

select left(a1,7), left(a2,7)
from t1_1025
where a1 in (select substring(b1,1,1025) from t2_1025 where b1 > '0');

# group_concat with a blob argument - depends on
# the variable group_concat_max_len, and
# convert_blob_length == max_len*collation->mbmaxlen > CONVERT_IF_BIGGER_TO_BLOB
explain extended select left(a1,7), left(a2,7)
from t1_1025
where a1 in (select group_concat(b1) from t2_1025 group by b2);

select left(a1,7), left(a2,7)
from t1_1025
where a1 in (select group_concat(b1) from t2_1025 group by b2);

set @@group_concat_max_len = 256; # anything < (CONVERT_IF_BIGGER_TO_BLOB = 1025)

explain extended select left(a1,7), left(a2,7)
from t1_1025
where a1 in (select group_concat(b1) from t2_1025 group by b2);

select left(a1,7), left(a2,7)
from t1_1025
where a1 in (select group_concat(b1) from t2_1025 group by b2);

drop table t1_1025, t2_1025, t3_1025;

--echo #
--echo # WL#5561: Enable semi join transformation with outer join.
--echo #

CREATE TABLE ot1(a INT);
CREATE TABLE ot2(a INT);
CREATE TABLE ot3(a INT);
CREATE TABLE it1(a INT);
CREATE TABLE it2(a INT);
CREATE TABLE it3(a INT);

INSERT INTO ot1 VALUES(0),(1),(2),(3),(4),(5),(6),(7);
INSERT INTO ot2 VALUES(0),(2),(4),(6);
INSERT INTO ot3 VALUES(0),(3),(6);
INSERT INTO it1 VALUES(0),(1),(2),(3),(4),(5),(6),(7);
INSERT INTO it2 VALUES(0),(2),(4),(6);
INSERT INTO it3 VALUES(0),(3),(6);

--echo # Test cases, Subquery Pattern 1

--echo # Example SQ1.1:

let $query=
SELECT *
FROM ot1 LEFT JOIN ot2 ON ot1.a=ot2.a
WHERE ot1.a IN (SELECT a FROM it3);
eval explain $query;
--sorted_result
eval $query;

--echo # Example SQ1.2:

let $query=
SELECT *
FROM ot1 LEFT JOIN ot2 ON ot1.a=ot2.a
WHERE COALESCE(ot2.a,0) IN (SELECT a FROM it3);
eval explain $query;
--sorted_result
eval $query;

--echo # Example SQ1.3:

let $query=
SELECT *
FROM ot1 LEFT JOIN ot2 ON ot1.a=ot2.a
WHERE (ot1.a,ot2.a) IN (SELECT a, a FROM it3);
eval explain $query;
--sorted_result
eval $query;

--echo # More test cases

--sorted_result
SELECT *
FROM ot1 LEFT JOIN ot2 ON ot1.a=ot2.a+0
WHERE ot1.a IN (SELECT a FROM it3);

--sorted_result
SELECT *
FROM ot1 LEFT JOIN ot2 ON ot1.a=ot2.a+0
WHERE ot1.a IN (SELECT a+0 FROM it3);

--sorted_result
SELECT *
FROM ot1 LEFT JOIN ot2 ON ot1.a=ot2.a+0
WHERE COALESCE(ot2.a,0) IN (SELECT a FROM it3);

--sorted_result
SELECT *
FROM ot1 LEFT JOIN ot2 ON ot1.a=ot2.a
WHERE COALESCE(ot2.a,0) IN (SELECT a+0 FROM it3);

--sorted_result
SELECT *
FROM ot1 LEFT JOIN ot2 ON ot1.a=ot2.a+0
WHERE (ot1.a,ot2.a) IN (SELECT a, a FROM it3);

--sorted_result
SELECT *
FROM ot1 LEFT JOIN ot2 ON ot1.a=ot2.a
         LEFT JOIN ot3 ON ot1.a=ot3.a
WHERE ot1.a IN (SELECT a FROM it3);

--sorted_result
SELECT *
FROM ot1 LEFT JOIN ot2 ON ot1.a=ot2.a
         LEFT JOIN ot3 ON ot1.a=ot3.a
WHERE COALESCE(ot2.a,0) IN (SELECT a FROM it3);

--sorted_result
SELECT *
FROM ot1 LEFT JOIN ot2 ON ot1.a=ot2.a
         LEFT JOIN ot3 ON ot1.a=ot3.a
WHERE COALESCE(ot3.a,0) IN (SELECT a FROM it3);

--sorted_result
SELECT *
FROM ot1 LEFT JOIN ot2 ON ot1.a=ot2.a
         LEFT JOIN ot3 ON ot2.a=ot3.a
WHERE ot1.a IN (SELECT a FROM it3);

--sorted_result
SELECT *
FROM ot1 LEFT JOIN ot2 ON ot1.a=ot2.a
         LEFT JOIN ot3 ON ot2.a=ot3.a
WHERE COALESCE(ot2.a,0) IN (SELECT a FROM it3);

--sorted_result
SELECT *
FROM ot1 LEFT JOIN ot2 ON ot1.a=ot2.a
         LEFT JOIN ot3 ON ot2.a=ot3.a
WHERE COALESCE(ot3.a,0) IN (SELECT a FROM it3);

--echo # Test cases, Subquery Pattern 2

--echo # Example SQ2.1:

let $query=
SELECT *
FROM ot1 JOIN ot2 ON ot1.a=ot2.a AND ot1.a IN (SELECT a FROM it3);
eval explain $query;
--sorted_result
eval $query;

--echo # Example SQ2.2:

let $query=
SELECT *
FROM ot1 JOIN ot2 ON ot1.a=ot2.a AND ot1.a IN (SELECT a FROM it2)
                                 AND ot2.a IN (SELECT a FROM it3);
eval explain $query;
--sorted_result
eval $query;

--echo # More test cases

--sorted_result
SELECT *
FROM ot1 JOIN ot2 ON ot1.a=ot2.a+0 AND ot1.a IN (SELECT a FROM it3);

--sorted_result
SELECT *
FROM ot1 JOIN ot2 ON ot1.a=ot2.a AND ot1.a IN (SELECT a+0 FROM it3);

--sorted_result
SELECT *
FROM ot1 JOIN ot2 ON ot1.a=ot2.a+0 AND ot2.a IN (SELECT a FROM it3);

--sorted_result
SELECT *
FROM ot1 JOIN ot2 ON ot1.a=ot2.a AND ot2.a IN (SELECT a+0 FROM it3);

--sorted_result
SELECT *
FROM ot1 JOIN ot2 ON ot1.a=ot2.a AND ot1.a IN (SELECT a+0 FROM it2)
                                 AND ot2.a IN (SELECT a+0 FROM it3);

--sorted_result
SELECT *
FROM ot1 JOIN ot2 ON ot1.a=ot2.a AND ot1.a IN (SELECT a FROM it3)
         JOIN ot3 ON ot2.a=ot3.a AND ot3.a IN (SELECT a FROM it3);

--echo # Test cases, Subquery Pattern 3

--echo # Example SQ3.1:

let $query=
SELECT *
FROM ot1 LEFT JOIN ot2 ON ot1.a=ot2.a AND ot1.a IN (SELECT a FROM it3);
eval explain $query;
--sorted_result
eval $query;

--echo # Example SQ3.2:

let $query=
SELECT *
FROM ot1 LEFT JOIN ot2 ON ot1.a=ot2.a AND ot2.a IN (SELECT a FROM it2);
eval explain $query;
--sorted_result
eval $query;

--echo # Example SQ3.3

let $query=
SELECT *
FROM ot1 LEFT JOIN ot2 ON ot1.a=ot2.a AND ot1.a IN (SELECT a FROM it1)
                                      AND ot2.a IN (SELECT a FROM it2);
eval explain $query;
--sorted_result
eval $query;

--echo # Example SQ3.4

let $query=
SELECT *
FROM ot1 LEFT JOIN ot2 ON ot1.a=ot2.a AND
                         (ot1.a, ot2.a) IN (SELECT it1.a, it2.a
                                            FROM it1 JOIN it2 ON it1.a=it2.a);
eval explain $query;
--sorted_result
eval $query;

--echo # More test cases

--sorted_result
SELECT *
FROM ot1 LEFT JOIN ot2 ON ot1.a=ot2.a+0 AND ot1.a IN (SELECT a FROM it3);

--sorted_result
SELECT *
FROM ot1 LEFT JOIN ot2 ON ot1.a=ot2.a AND ot1.a IN (SELECT a+0 FROM it3);

--sorted_result
SELECT *
FROM ot1 LEFT JOIN ot2 ON ot1.a=ot2.a+0 AND ot2.a IN (SELECT a FROM it2);

--sorted_result
SELECT *
FROM ot1 LEFT JOIN ot2 ON ot1.a=ot2.a AND ot2.a IN (SELECT a+0 FROM it2);

--sorted_result
SELECT *
FROM ot1 LEFT JOIN ot2 ON ot1.a=ot2.a+0 AND ot1.a IN (SELECT a+0 FROM it1)
                                        AND ot2.a IN (SELECT a+0 FROM it2);

--sorted_result
SELECT *
FROM ot1 LEFT JOIN ot2 ON ot1.a=ot2.a+0 AND
                         (ot1.a, ot2.a) IN (SELECT it1.a+0, it2.a+0
                                            FROM it1 JOIN it2 ON it1.a=it2.a);

--sorted_result
SELECT *
FROM ot1 LEFT JOIN ot2 ON ot1.a=ot2.a AND ot1.a IN (SELECT a FROM it3)
         LEFT JOIN ot3 ON ot2.a=ot3.a AND ot3.a IN (SELECT a FROM it3);

--sorted_result
SELECT *
FROM ot1 LEFT JOIN ot2 ON ot1.a=ot2.a+0 AND ot1.a IN (SELECT a FROM it3)
         LEFT JOIN ot3 ON ot2.a=ot3.a+0 AND ot3.a IN (SELECT a FROM it3);

--echo # Test cases, Subquery Pattern 4

--echo # Example SQ4.1:

let $query=
SELECT *
FROM   ot1
     LEFT JOIN
       (ot2 JOIN ot3 ON ot2.a=ot3.a)
     ON ot1.a=ot2.a AND ot1.a IN (SELECT a FROM it1);
eval explain $query;
--sorted_result
eval $query;

--echo # Example SQ4.2:

let $query=
SELECT *
FROM   ot1
     JOIN
       (ot2 JOIN ot3 ON ot2.a=ot3.a)
     ON ot1.a=ot2.a AND ot1.a IN (SELECT a FROM it1);
eval explain $query;
--sorted_result
eval $query;

--echo # Example SQ4.3:

let $query=
SELECT *
FROM   ot1
     JOIN
       (ot2 LEFT JOIN ot3 ON ot2.a=ot3.a)
     ON ot1.a=ot2.a AND ot1.a IN (SELECT a FROM it1);
eval explain $query;
--sorted_result
eval $query;

--echo # Example SQ4.4:

let $query=
SELECT *
FROM   ot1
     LEFT JOIN
       (ot2 LEFT JOIN ot3 ON ot2.a=ot3.a)
     ON ot1.a=ot2.a AND ot1.a IN (SELECT a FROM it1);
eval explain $query;
--sorted_result
eval $query;

--echo # More test cases

--sorted_result
SELECT *
FROM   ot1
     LEFT JOIN
       (ot2 JOIN ot3 ON ot2.a=ot3.a+0)
     ON ot1.a=ot2.a AND ot1.a IN (SELECT a FROM it1);

--sorted_result
SELECT *
FROM   ot1
     LEFT JOIN
       (ot2 JOIN ot3 ON ot2.a=ot3.a)
     ON ot1.a=ot2.a+0 AND ot1.a IN (SELECT a FROM it1);

--sorted_result
SELECT *
FROM   ot1
     LEFT JOIN
       (ot2 JOIN ot3 ON ot2.a=ot3.a)
     ON ot1.a=ot2.a AND ot1.a IN (SELECT a+0 FROM it1);

--sorted_result
SELECT *
FROM   ot1
     JOIN
       (ot2 JOIN ot3 ON ot2.a=ot3.a+0)
     ON ot1.a=ot2.a AND ot1.a IN (SELECT a FROM it1);

--sorted_result
SELECT *
FROM   ot1
     JOIN
       (ot2 JOIN ot3 ON ot2.a=ot3.a)
     ON ot1.a=ot2.a+0 AND ot1.a IN (SELECT a FROM it1);

--sorted_result
SELECT *
FROM   ot1
     JOIN
       (ot2 JOIN ot3 ON ot2.a=ot3.a)
     ON ot1.a=ot2.a AND ot1.a IN (SELECT a+0 FROM it1);

--sorted_result
SELECT *
FROM   ot1
     JOIN
       (ot2 LEFT JOIN ot3 ON ot2.a=ot3.a+0)
     ON ot1.a=ot2.a AND ot1.a IN (SELECT a FROM it1);

--sorted_result
SELECT *
FROM   ot1
     JOIN
       (ot2 LEFT JOIN ot3 ON ot2.a=ot3.a)
     ON ot1.a=ot2.a+0 AND ot1.a IN (SELECT a FROM it1);

--sorted_result
SELECT *
FROM   ot1
     JOIN
       (ot2 LEFT JOIN ot3 ON ot2.a=ot3.a)
     ON ot1.a=ot2.a AND ot1.a IN (SELECT a+0 FROM it1);

--sorted_result
SELECT *
FROM   ot1
     LEFT JOIN
       (ot2 LEFT JOIN ot3 ON ot2.a=ot3.a+0)
     ON ot1.a=ot2.a AND ot1.a IN (SELECT a FROM it1);

--sorted_result
SELECT *
FROM   ot1
     LEFT JOIN
       (ot2 LEFT JOIN ot3 ON ot2.a=ot3.a)
     ON ot1.a=ot2.a+0 AND ot1.a IN (SELECT a FROM it1);

--sorted_result
SELECT *
FROM   ot1
     LEFT JOIN
       (ot2 LEFT JOIN ot3 ON ot2.a=ot3.a)
     ON ot1.a=ot2.a AND ot1.a IN (SELECT a+0 FROM it1);

--sorted_result
SELECT *
FROM   ot1
     LEFT JOIN
       (ot2 LEFT JOIN ot3 ON ot2.a=ot3.a)
     ON ot1.a=ot2.a AND ot1.a IN (SELECT a FROM it1)
     LEFT JOIN
       ot1 AS ot4
     ON ot2.a=ot4.a;

--sorted_result
SELECT *
FROM   ot1
     LEFT JOIN
       (ot2 LEFT JOIN ot3 ON ot2.a=ot3.a
            LEFT JOIN ot1 AS ot4 ON ot3.a=ot4.a)
     ON ot1.a=ot2.a AND ot1.a IN (SELECT a FROM it1);

DROP TABLE ot1,ot2,ot3,it1,it2,it3;

CREATE TABLE t (
  a INTEGER DEFAULT NULL
) ENGINE=InnoDB;
INSERT INTO t VALUES (1);

CREATE TABLE t2 (
  a INTEGER DEFAULT NULL
) ENGINE=InnoDB;
INSERT INTO t2 VALUES (1),(1);

CREATE TABLE t4 (
  a INTEGER DEFAULT NULL
) ENGINE=InnoDB;
INSERT INTO t4 VALUES (1),(1);

CREATE TABLE v (
  a INTEGER DEFAULT NULL
) ENGINE=InnoDB;
INSERT INTO v VALUES (1),(1);

let $query=
SELECT *
FROM t AS t1
     LEFT JOIN
      (t2
       LEFT JOIN t AS t3
       ON t3.a IN (SELECT a FROM t AS it)
       JOIN t4
       ON t4.a=100
      )
     ON TRUE
WHERE t1.a IN (SELECT * FROM v AS it2);
eval explain $query;
eval $query;

DROP TABLE t,t2,t4,v;

--echo # End of WL#5561

--echo #
--echo # Bug#48868: Left outer join in subquery causes segmentation fault in
--echo #            make_join_select.
--echo #
CREATE TABLE t1 (i INTEGER);
INSERT INTO t1 VALUES (1);	
INSERT INTO t1 VALUES (2);	
CREATE TABLE t2 (i INTEGER);
INSERT INTO t2 VALUES(1);
CREATE TABLE t3 (i INTEGER);
INSERT INTO t3 VALUES (1);	
INSERT INTO t3 VALUES (2);	

SELECT * FROM t1 WHERE (t1.i) IN 
  (SELECT t2.i FROM t2 LEFT JOIN t3 ON t2.i=t3.i);

DROP TABLE t1, t2, t3;

--echo
--echo Bug#37899: Wrongly checked optimization prerequisite caused failed
--echo            assertion.
--echo
CREATE TABLE t1 (
  `pk` int(11),
  `varchar_nokey` varchar(5)
);

INSERT INTO t1 VALUES
(1,'qk'),(2,'j'),(3,'aew');

SELECT *
FROM t1
WHERE varchar_nokey IN (
 SELECT
 varchar_nokey
 FROM
 t1
) XOR pk = 30;
drop table t1;

--echo #
--echo # BUG#41842: Semi-join materialization strategy crashes when the upper query has HAVING
--echo #

CREATE TABLE t1 (
  pk int(11) NOT NULL AUTO_INCREMENT,
  int_nokey int(11) NOT NULL,
  time_key time NOT NULL,
  datetime_key datetime NOT NULL,
  datetime_nokey datetime NOT NULL,
  varchar_key varchar(1) NOT NULL,
  varchar_nokey varchar(1) NOT NULL,
  PRIMARY KEY (pk),
  KEY time_key (time_key),
  KEY datetime_key (datetime_key),
  KEY varchar_key (varchar_key)
);
INSERT INTO t1 VALUES 
(1,0, '00:16:10','2008-09-03 14:25:40','2008-09-03 14:25:40','h','h'),
(2,7, '00:00:00','2001-01-13 00:00:00','2001-01-13 00:00:00','',''),
(3,0, '00:00:00','0000-00-00 00:00:00','0000-00-00 00:00:00','x','x'),
(4,2, '16:29:24','2000-10-16 01:39:08','2000-10-16 01:39:08','w','w'),
(5,1, '09:23:32','0000-00-00 00:00:00','0000-00-00 00:00:00','p','p'),
(6,3, '00:00:00','2007-12-02 00:00:00','2007-12-02 00:00:00','o','o'),
(7,3, '00:00:00','2008-09-11 00:00:00','2008-09-11 00:00:00','',''),
(8,0, '13:59:04','0000-00-00 00:00:00','0000-00-00 00:00:00','s','s'),
(9,7, '09:01:06','0000-00-00 00:00:00','0000-00-00 00:00:00','d','d'),
(10,5,'00:00:00','0000-00-00 00:00:00','0000-00-00 00:00:00','n','n'),
(11,0,'21:06:46','0000-00-00 00:00:00','0000-00-00 00:00:00','o','o'),
(12,2,'00:00:00','0000-00-00 00:00:00','0000-00-00 00:00:00','',''),
(13,6,'14:45:34','2003-07-28 02:34:08','2003-07-28 02:34:08','w','w'),
(14,1,'15:04:12','0000-00-00 00:00:00','0000-00-00 00:00:00','o','o'),
(15,0,'00:00:00','0000-00-00 00:00:00','0000-00-00 00:00:00','x','x'),
(16,0,'15:55:23','2004-03-17 00:32:27','2004-03-17 00:32:27','p','p'),
(17,1,'16:30:00','2004-12-27 19:20:00','2004-12-27 19:20:00','d','d'),
(18,0,'00:00:00','0000-00-00 00:00:00','0000-00-00 00:00:00','h','h'),
(19,0,'14:13:26','2008-11-09 05:53:48','2008-11-09 05:53:48','o','o'),
(20,0,'00:00:00','2009-10-11 06:58:04','2009-10-11 06:58:04','k','k');

CREATE TABLE t2 (
  pk int(11) NOT NULL AUTO_INCREMENT,
  int_nokey int(11) NOT NULL,
  time_key time NOT NULL,
  datetime_key datetime NOT NULL,
  datetime_nokey datetime NOT NULL,
  varchar_key varchar(1) NOT NULL,
  varchar_nokey varchar(1) NOT NULL,
  PRIMARY KEY (pk),
  KEY time_key (time_key),
  KEY datetime_key (datetime_key),
  KEY varchar_key (varchar_key)
);
INSERT INTO t2 VALUES 
(10,0,'19:39:13','0000-00-00 00:00:00','0000-00-00 00:00:00','g','g'),
(11,8,'03:43:53','0000-00-00 00:00:00','0000-00-00 00:00:00','b','b');
SELECT OUTR.datetime_nokey AS X FROM t1 AS OUTR 
WHERE 
  OUTR.varchar_nokey IN (SELECT 
                             INNR . varchar_nokey AS Y 
                             FROM t2 AS INNR 
                             WHERE
                               INNR . datetime_key >= INNR . time_key OR 
                               INNR . pk = INNR . int_nokey  
                             ) 
  AND OUTR . varchar_nokey <= 'w' 
HAVING X > '2012-12-12';
drop table t1, t2;

--echo
--echo Bug#46797 "Crash in fix_semijoin_strategies_for_picked_join_order 
--echo with semijoin=on"
--echo
CREATE TABLE t1 (
  varchar_key varchar(1) DEFAULT NULL,
  KEY varchar_key (varchar_key)
);

CREATE TABLE t2 (
  varchar_key varchar(1) DEFAULT NULL,
  KEY varchar_key (varchar_key)
);
INSERT INTO t2 VALUES
  (NULL),(NULL),(NULL),(NULL),('a'),('a'),('a'),('b'),('b'),('b'),('b'),('c'),
  ('c'),('c'),('c'),('c'),('c'),('c'),('d'),('d'),('d'),('d'),('d'),('d'),('e'),
  ('e'),('e'),('e'),('e'),('e'),('f'),('f'),('f'),('g'),('g'),('h'),('h'),('h'),
  ('h'),('i'),('j'),('j'),('j'),('k'),('k'),('l'),('l'),('m'),('m'),('m'),('m'),
  ('n'),('n'),('n'),('o'),('o'),('o'),('p'),('p'),('p'),('q'),('q'),('q'),('r'),
  ('r'),('r'),('r'),('s'),('s'),('s'),('s'),('t'),('t'),('t'),('t'),('u'),('u'),
  ('u'),('u'),('v'),('v'),('v'),('v'),('w'),('w'),('w'),('w'),('w'),('w'),('x'),
  ('x'),('x'),('y'),('y'),('y'),('y'),('z'),('z'),('z'),('z');

CREATE TABLE t3 (
  varchar_key varchar(1) DEFAULT NULL,
  KEY varchar_key (varchar_key)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
INSERT INTO t3 VALUES
  (NULL),('c'),('d'),('e'),('f'),('h'),('j'),('k'),('k'),('m'),('m'),('m'),
  ('n'),('o'),('r'),('t'),('t'),('u'),('w'),('y');
 
SELECT varchar_key FROM t3 
WHERE (SELECT varchar_key FROM t3 
       WHERE (varchar_key,varchar_key) 
         IN (SELECT t1.varchar_key, t2 .varchar_key 
             FROM t1 RIGHT JOIN t2 ON t1.varchar_key  
            )  
      );

DROP TABLE t1, t2, t3;


--echo #
--echo # Bug#46556 Returning incorrect, empty results for some IN subqueries 
--echo #           w/semijoin=on
--echo #

CREATE TABLE t0 (
  pk INTEGER,
  vkey VARCHAR(1),
  vnokey VARCHAR(1),
  PRIMARY KEY (pk),
  KEY vkey(vkey)
);

INSERT INTO t0 
VALUES (1,'g','g'), (2,'v','v'), (3,'t','t'), (4,'u','u'), (5,'n','n');

EXPLAIN EXTENDED SELECT vkey FROM t0 WHERE pk IN 
  (SELECT t1.pk FROM t0 t1 JOIN t0 t2 ON t2.vkey = t1.vnokey);

SELECT vkey FROM t0 WHERE pk IN 
  (SELECT t1.pk FROM t0 t1 JOIN t0 t2 ON t2.vkey = t1.vnokey);

DROP TABLE t0;

--echo # End of bug#46556


# The following test case fails when executed with subquery materialization
# (Bug#54281).  Hence, skip it if semijoin=off and materialization=on
if (`select (locate('materialization', @@optimizer_switch) = 0) OR locate('semijoin=on', @@optimizer_switch) + locate('materialization=off', @@optimizer_switch) > 0`) 
{

--echo
--echo Bug#48834: Procedure with view + subquery + semijoin=on 
--echo            crashes on second call.
--echo

CREATE TABLE t1 ( t1field integer, primary key (t1field));
CREATE TABLE t2 ( t2field integer, primary key (t2field));

CREATE VIEW v1 AS 
  SELECT t1field as v1field
  FROM t1 A 
  WHERE A.t1field IN (SELECT t1field FROM t2 );

CREATE VIEW v2 AS 
  SELECT t2field as v2field
  FROM t2 A 
  WHERE A.t2field IN (SELECT t2field FROM t2 );

DELIMITER |;
CREATE PROCEDURE p1 () 
  BEGIN 
    SELECT v1field
    FROM v1 
    WHERE v1field IN ( SELECT v2field as vf_inner FROM v2 );
  END|
DELIMITER ;|

INSERT INTO t1 VALUES (1),(2),(3);
INSERT INTO t2 VALUES (2),(3),(4);

CALL p1;
CALL p1;

DROP TABLE t1,t2;
DROP VIEW v1,v2;
DROP PROCEDURE p1;

--echo # End of BUG#48834
}


--echo #
--echo # Bug#46692 "Crash occurring on queries with nested FROM subqueries 
--echo # using materialization."
--echo #
CREATE TABLE t1 (
  pk INTEGER PRIMARY KEY,
  int_key INTEGER,
  KEY int_key(int_key)
);
INSERT INTO t1 VALUES (10,186),(11,NULL),(12,2),(13,3),(14,0),(15,133),(16,1);

CREATE TABLE t2 (
  pk INTEGER PRIMARY KEY,
  int_key INTEGER,
  KEY int_key(int_key)
);
INSERT INTO t2 VALUES (1,7),(2,2);

SELECT * FROM t1 WHERE (140, 4) IN 
  (SELECT t2.int_key, t2 .pk FROM t2 STRAIGHT_JOIN t1 ON t2.int_key);

DROP TABLE t1, t2;

--echo #
--echo # Bug#42353 "SELECT ... WHERE oe IN (SELECT w/ LEFT JOIN) query
--echo # causes crash."
--echo #
CREATE TABLE t1 (
  pk INTEGER PRIMARY KEY,
  int_nokey INTEGER,
  int_key INTEGER,
  date_key DATE,
  datetime_nokey DATETIME,
  varchar_nokey VARCHAR(1)
);

CREATE TABLE t2 (
  date_nokey DATE
);

CREATE TABLE t3 (
  pk INTEGER PRIMARY KEY,
  int_nokey INTEGER,
  date_key date,
  varchar_key VARCHAR(1),
  varchar_nokey VARCHAR(1),
  KEY date_key (date_key)
);

SELECT date_key FROM t1
WHERE (int_key, int_nokey)
  IN (SELECT  t3.int_nokey, t3.pk
      FROM t2 LEFT JOIN t3 ON (t2.date_nokey < t3.date_key) 
      WHERE t3.varchar_key <= t3.varchar_nokey OR t3.int_nokey <= t3.pk
     )
  AND (varchar_nokey <> 'f' OR NOT int_key < 7); 


--echo #
--echo # Bug#45933 "Crash in optimize_semijoin_nests on JOIN in subquery 
--echo # + AND in outer query".
--echo #
INSERT INTO t1 VALUES (10,7,5,'2009-06-16','2002-04-10 14:25:30','w'),
                      (11,7,0,'0000-00-00','0000-00-00 00:00:00','s'), 
		      (12,4,0,'2003-07-14','2006-09-14 04:01:02','y'), 
		      (13,0,4,'2002-07-25','0000-00-00 00:00:00','c'), 
		      (14,1,8,'2007-07-03','0000-00-00 00:00:00','q'), 
		      (15,6,5,'2001-11-12','0000-00-00 00:00:00',''), 
		      (16,2,9,'0000-00-00','0000-00-00 00:00:00','j'), 
		      (29,9,1,'0000-00-00','2003-08-11 00:00:00','m');
INSERT INTO t3 VALUES (1,9,'0000-00-00','b','b'),
                      (2,2,'2002-09-17','h','h');

SELECT t1.varchar_nokey FROM t1 JOIN t3 ON t1.datetime_nokey
WHERE t1.varchar_nokey 
  IN (SELECT varchar_nokey FROM t1 
      WHERE (pk) 
        IN (SELECT t3.int_nokey
            FROM t3 LEFT JOIN t1 ON t1.varchar_nokey
            WHERE t3.date_key BETWEEN '2008-06-07' AND '2006-06-26'
           )  
     );

DROP TABLE t1, t2, t3;

--echo #
--echo # Bug#45219 "Crash on SELECT DISTINCT query containing a  
--echo # LEFT JOIN in subquery"
--echo #

CREATE TABLE t1 (
  pk INTEGER NOT NULL,
  int_nokey INTEGER NOT NULL,
  datetime_key DATETIME NOT NULL,
  varchar_key VARCHAR(1) NOT NULL,
  PRIMARY KEY (pk),
  KEY datetime_key (datetime_key),
  KEY varchar_key (varchar_key)
);
INSERT INTO t1 VALUES
(1,9,'0000-00-00 00:00:00','p'),(2,0,'2002-02-09 07:38:13','v'),
(3,8,'2001-05-03 12:08:14','t'),(4,3,'0000-00-00 00:00:00','u'),
(5,7,'2009-07-28 03:43:30','n'),(6,0,'2009-08-04 00:00:00','l'),
(7,1,'0000-00-00 00:00:00','h'),(8,9,'0000-00-00 00:00:00','u'),
(9,0,'2005-08-02 17:16:54','n'),(10,9,'2002-12-21 00:00:00','j'),
(11,0,'2005-08-15 12:37:35','k'),(12,5,'0000-00-00 00:00:00','e'),
(13,0,'2006-03-10 00:00:00','i'),(14,8,'2005-05-16 11:02:36','u'),
(15,8,'2008-11-02 00:00:00','n'),(16,5,'2006-03-15 00:00:00','b'),
(17,1,'0000-00-00 00:00:00','x'),(18,7,'0000-00-00 00:00:00',''),
(19,0,'2008-12-17 20:15:40','q'),(20,9,'0000-00-00 00:00:00','u');

CREATE TABLE t2 LIKE t1;
INSERT INTO t2 VALUES 
(10,0,'2006-07-07 07:26:28','q'),(11,5,'2002-09-23 00:00:00','m'),
(12,7,'0000-00-00 00:00:00','j'),(13,1,'2006-06-07 00:00:00','z'),
(14,8,'2000-09-16 12:15:34','a'),(15,2,'2007-08-05 15:47:52',''),
(16,1,'0000-00-00 00:00:00','e'),(17,8,'2005-12-02 19:34:26','t'),
(18,5,'0000-00-00 00:00:00','q'),(19,4,'0000-00-00 00:00:00','b'),
(20,5,'2007-12-28 00:00:00','w'),(21,3,'2004-08-02 11:48:43','m'),
(22,0,'0000-00-00 00:00:00','x'),(23,8,'2004-04-19 12:18:43',''),
(24,0,'2009-04-27 00:00:00','w'),(25,4,'2006-10-20 14:52:15','x'),
(26,0,'0000-00-00 00:00:00','e'),(27,0,'2002-03-22 11:48:37','e'),
(28,2,'0000-00-00 00:00:00','p'),(29,0,'2001-01-04 03:55:07','x');

CREATE TABLE t3 LIKE t1;
INSERT INTO t3 VALUES
(10,8,'2007-08-19 08:08:38','i'),(11,0,'2000-05-21 03:51:51','');
 
SELECT DISTINCT datetime_key FROM t1
WHERE (int_nokey, pk)  
  IN (SELECT t3.pk, t3.pk FROM t2 LEFT JOIN t3 ON t3.varchar_key)  
  AND pk = 9;

DROP TABLE t1, t2, t3;

--echo #
--echo # Bug#46550 Azalea returning duplicate results for some IN subqueries
--echo # w/ semijoin=on
--echo #

--disable_warnings
DROP TABLE IF EXISTS t0, t1, t2;
--enable_warnings

CREATE TABLE t0 (
  int_key int(11) DEFAULT NULL,
  varchar_key varchar(1) DEFAULT NULL,
  varchar_nokey varchar(1) DEFAULT NULL,
  KEY int_key (int_key),
  KEY varchar_key (varchar_key,int_key)
);

INSERT INTO t0 VALUES
(1,'m','m'),
(40,'h','h'),
(1,'r','r'),
(1,'h','h'),
(9,'x','x'),
(NULL,'q','q'),
(NULL,'k','k'),
(7,'l','l'),
(182,'k','k'),
(202,'a','a'),
(7,'x','x'),
(6,'j','j'),
(119,'z','z'),
(4,'d','d'),
(5,'h','h'),
(1,'u','u'),
(3,'q','q'),
(7,'a','a'),
(3,'e','e'),
(6,'l','l');

CREATE TABLE t1 (
  int_key int(11) DEFAULT NULL,
  varchar_key varchar(1) DEFAULT NULL,
  varchar_nokey varchar(1) DEFAULT NULL,
  KEY int_key (int_key),
  KEY varchar_key (varchar_key,int_key)
);
INSERT INTO t1 VALUES (7,NULL,NULL),(4,'x','x');

CREATE TABLE t2 (
  int_key int(11) DEFAULT NULL,
  varchar_key varchar(1) DEFAULT NULL,
  varchar_nokey varchar(1) DEFAULT NULL,
  KEY int_key (int_key),
  KEY varchar_key (varchar_key,int_key)
);
INSERT INTO t2 VALUES (123,NULL,NULL);

SELECT int_key  
FROM t0  
WHERE varchar_nokey  IN (  
  SELECT t1 .varchar_key  from t1
);
SELECT t0.int_key  
FROM t0
WHERE t0.varchar_nokey  IN (  
  SELECT t1_1 .varchar_key  
  FROM t1 AS t1_1  JOIN t1 AS t1_2 ON t1_1 .int_key
);

EXPLAIN
SELECT t0.int_key  
FROM t0
WHERE t0.varchar_nokey  IN (  
  SELECT t1_1 .varchar_key  
  FROM t1 AS t1_1  JOIN t1 AS t1_2 ON t1_1 .int_key
);

SELECT t0.int_key  
FROM t0, t2
WHERE t0.varchar_nokey  IN (  
  SELECT t1_1 .varchar_key  
  FROM t1 AS t1_1  JOIN t1 AS t1_2 ON t1_1 .int_key  
);

EXPLAIN
SELECT t0.int_key  
FROM t0, t2
WHERE t0.varchar_nokey  IN (  
  SELECT t1_1 .varchar_key  
  FROM t1 AS t1_1  JOIN t1 AS t1_2 ON t1_1 .int_key  
);

DROP TABLE t0, t1, t2;

--echo # End of bug#46550



--echo
--echo Bug #48073 Subquery on char columns from view crashes Mysql
--echo

--disable_warnings
DROP TABLE IF EXISTS t1, t2;
DROP VIEW IF EXISTS v1;
--enable_warnings

CREATE TABLE t1 (
  city VARCHAR(50) NOT NULL,
  country_id SMALLINT UNSIGNED NOT NULL
);

INSERT INTO t1 VALUES 
('Batna',2),
('Bchar',2),
('Skikda',2),
('Tafuna',3),
('Algeria',2) ;

CREATE TABLE t2 (
  country_id SMALLINT UNSIGNED NOT NULL,
  country VARCHAR(50) NOT NULL
);

INSERT INTO t2 VALUES
(2,'Algeria'),
(3,'American Samoa') ;

CREATE VIEW v1 AS 
SELECT country_id, country 
FROM t2
WHERE LEFT(country,1) = "A" 
; 

SELECT city, country_id
FROM t1
WHERE city IN (
  SELECT country 
  FROM t2
  WHERE LEFT(country, 1) = "A" 
);

SELECT city, country_id
FROM t1
WHERE city IN (
  SELECT country 
  FROM v1
);

drop table t1, t2;
drop view v1;

--echo # End of bug#48073

--echo
--echo Bug#49097 subquery with view generates wrong result with
--echo           non-prepared statement
--echo

--disable_warnings
DROP TABLE IF EXISTS t1, t2;
DROP VIEW IF EXISTS v1;
--enable_warnings

CREATE TABLE t1 (
  city VARCHAR(50) NOT NULL,
  country_id SMALLINT UNSIGNED NOT NULL
);

INSERT INTO t1 VALUES 
('Batna',2),
('Bchar',2),
('Skikda',2),
('Tafuna',3),
('Algeria',2) ;

CREATE TABLE t2 (
  country_id SMALLINT UNSIGNED NOT NULL,
  country VARCHAR(50) NOT NULL
);

INSERT INTO t2 VALUES
(2,'Algeria'),
(3,'XAmerican Samoa') ;

CREATE VIEW v1 AS 
SELECT country_id, country 
FROM t2
WHERE LEFT(country,1) = "A" 
; 

SELECT city, country_id
FROM t1
WHERE country_id IN (
  SELECT country_id 
  FROM t2
  WHERE LEFT(country,1) = "A" 
);

SELECT city, country_id
FROM t1
WHERE country_id IN (
  SELECT country_id 
  FROM v1
);

PREPARE stmt FROM
"
SELECT city, country_id
FROM t1
WHERE country_id IN (
  SELECT country_id 
  FROM v1
);
";

execute stmt;

deallocate prepare stmt;
drop table t1, t2;
drop view v1;

--echo # End of Bug#49097

--echo # 
--echo # Bug#49198 Wrong result for second call of procedure
--echo #           with view in subselect.
--echo # 

CREATE TABLE t1 (t1field integer, primary key (t1field));
CREATE TABLE t2 (t2field integer, primary key (t2field));
CREATE TABLE t3 (t3field integer, primary key (t3field));

CREATE VIEW v2 AS SELECT * FROM t2;
CREATE VIEW v3 AS SELECT * FROM t3;

INSERT INTO t1 VALUES(1),(2);
INSERT INTO t2 VALUES(1),(2);
INSERT INTO t3 VALUES(1),(2);

PREPARE stmt FROM
"
SELECT t1field
FROM t1
WHERE t1field IN (SELECT * FROM v2);
";

EXECUTE stmt;
EXECUTE stmt;

PREPARE stmt FROM
"
EXPLAIN
SELECT t1field
FROM t1
WHERE t1field IN (SELECT * FROM v2)
  AND t1field IN (SELECT * FROM v3)
";

EXECUTE stmt;
EXECUTE stmt;

DROP TABLE t1, t2, t3;
DROP VIEW v2, v3;

--echo # End of Bug#49198

--echo # 
--echo # Bug#48623 Multiple subqueries are optimized incorrectly
--echo # 

CREATE TABLE ot(val VARCHAR(10));
CREATE TABLE it1(val VARCHAR(10));
CREATE TABLE it2(val VARCHAR(10));

INSERT INTO ot  VALUES('aaa'), ('bbb'), ('eee'), ('mmm'), ('ppp');
INSERT INTO it1 VALUES('aaa'), ('aaa'), ('bbb'), ('eee'), ('mmm'), ('ppp');
INSERT INTO it2 VALUES('aaa'), ('bbb'), ('eee'), ('mmm'), ('ppp');

EXPLAIN
SELECT *
FROM ot
WHERE ot.val IN (SELECT it1.val FROM it1
                 WHERE  it1.val LIKE 'a%' OR it1.val LIKE 'e%')
  AND ot.val IN (SELECT it2.val FROM it2
                 WHERE  it2.val LIKE 'a%' OR it2.val LIKE 'e%');

SELECT *
FROM ot
WHERE ot.val IN (SELECT it1.val FROM it1
                 WHERE  it1.val LIKE 'a%' OR it1.val LIKE 'e%')
  AND ot.val IN (SELECT it2.val FROM it2
                 WHERE  it2.val LIKE 'a%' OR it2.val LIKE 'e%');

DROP TABLE ot;
DROP TABLE it1;
DROP TABLE it2;

--echo # End of Bug#48623

--echo # 
--echo # Bug #51487 Assertion failure when semi-join flattening occurs
--echo #            for a subquery in HAVING 
--echo # 

CREATE TABLE t1 (a INT, b INT);
INSERT INTO t1 VALUES (1,10),(2,11),(1,13);

CREATE TABLE t2 AS SELECT * FROM t1;
CREATE TABLE t3 AS SELECT * FROM t1;

SELECT COUNT(*) FROM t1
GROUP BY t1.a 
HAVING t1.a IN (SELECT t3.a FROM t3
                WHERE t3.b IN (SELECT b FROM t2 WHERE t2.a=t1.a));

DROP TABLE t1, t2, t3;

--echo # End of Bug#51487

--echo # 
--echo # BUG#38075: Wrong result: rows matching a subquery with outer join not returned
--echo # 

--disable_warnings
DROP TABLE IF EXISTS ot1, it1, it2;
--enable_warnings

CREATE TABLE it2 (
  int_key int(11) NOT NULL,
  datetime_key datetime NOT NULL,
  KEY int_key (int_key),
  KEY datetime_key (datetime_key)
);
INSERT INTO it2 VALUES
  (5,'2002-04-10 14:25:30'), (0,'0000-00-00 00:00:00'),
  (0,'2006-09-14 04:01:02'), (4,'0000-00-00 00:00:00'),
  (8,'0000-00-00 00:00:00'), (5,'0000-00-00 00:00:00'),
  (9,'0000-00-00 00:00:00'), (8,'2007-04-01 11:04:17'),
  (1,'0000-00-00 00:00:00'), (7,'2009-01-12 00:00:00'),
  (0,'2009-06-05 00:00:00'), (3,'2006-02-14 18:06:35'),
  (5,'2006-02-21 07:08:16'), (0,'0000-00-00 00:00:00'),
  (7,'0000-00-00 00:00:00'), (0,'0000-00-00 00:00:00'),
  (0,'2007-02-13 00:00:00'), (1,'0000-00-00 00:00:00'),
  (0,'0000-00-00 00:00:00'), (1,'2003-08-11 00:00:00');
CREATE TABLE ot1 (
  int_nokey int(11) NOT NULL,
  int_key int(11) NOT NULL,
  KEY int_key (int_key)
);
INSERT INTO ot1 VALUES
  (5,0), (3,0), (0,2), (3,0), (1,3), (0,0), (1,7), (7,0), (1,7), (0,7),
  (0,9), (8,2), (4,4), (9,3), (0,9), (2,5), (0,5), (8,0), (5,8), (1,5);
CREATE TABLE it1 (
  int_nokey int(11) NOT NULL,
  int_key int(11) NOT NULL,
  KEY int_key (int_key)
);
INSERT INTO it1 VALUES
  (9,5), (0,4);
--sorted_result
SELECT int_key FROM ot1
WHERE int_nokey IN (SELECT it2.int_key
                    FROM it1 LEFT JOIN it2 ON it2.datetime_key);
EXPLAIN
SELECT int_key FROM ot1
WHERE int_nokey IN (SELECT it2.int_key
                    FROM it1 LEFT JOIN it2 ON it2.datetime_key);
DROP TABLE ot1, it1, it2;

--echo # End of BUG#38075

--echo # 
--echo # BUG#50089: Second call of procedure with view in subselect crashes server
--echo # 

CREATE TABLE t1(t1field INTEGER, PRIMARY KEY(t1field));

CREATE VIEW v1 AS 
  SELECT t1field AS v1field
  FROM t1 a
  WHERE a.t1field IN (SELECT t1field FROM t1);

INSERT INTO t1 VALUES(1),(2);

SELECT t1field
FROM t1
WHERE t1field IN (SELECT v1field FROM v1);

EXPLAIN
SELECT t1field
FROM t1
WHERE t1field IN (SELECT v1field FROM v1);

--sorted_result
SELECT t1.t1field
FROM t1 LEFT JOIN t1 AS t2 ON t1.t1field IN (SELECT v1field FROM v1);

EXPLAIN
SELECT t1field
FROM t1
WHERE t1field IN (SELECT v1field FROM v1);

delimiter |;
CREATE PROCEDURE p1() 
  BEGIN 
    SELECT t1field
    FROM t1
    WHERE t1field IN (SELECT v1field FROM v1);
  END|
delimiter ;|

CALL p1;
CALL p1;

PREPARE stmt FROM
"
SELECT t1field
FROM t1
WHERE t1field IN (SELECT v1field FROM v1);
";

EXECUTE stmt;
EXECUTE stmt;

DROP PROCEDURE p1;
DROP VIEW v1;
DROP TABLE t1;

--echo # End of BUG#50089

--echo #
--echo # Bug#45191: Incorrectly initialized semi-join led to a wrong result.
--echo #
CREATE TABLE STAFF (EMPNUM   CHAR(3) NOT NULL,
    EMPNAME  CHAR(20), GRADE DECIMAL(4), CITY CHAR(15));

CREATE TABLE PROJ (PNUM CHAR(3) NOT NULL,
    PNAME    CHAR(20), PTYPE CHAR(6),
    BUDGET   DECIMAL(9),
    CITY     CHAR(15));

CREATE TABLE WORKS (EMPNUM CHAR(3) NOT NULL,
    PNUM CHAR(3) NOT NULL, HOURS DECIMAL(5));
INSERT INTO STAFF VALUES ('E1','Alice',12,'Deale');
INSERT INTO STAFF VALUES ('E2','Betty',10,'Vienna');
INSERT INTO STAFF VALUES ('E3','Carmen',13,'Vienna');
INSERT INTO STAFF VALUES ('E4','Don',12,'Deale');
INSERT INTO STAFF VALUES ('E5','Ed',13,'Akron');

INSERT INTO PROJ VALUES  ('P1','MXSS','Design',10000,'Deale');
INSERT INTO PROJ VALUES  ('P2','CALM','Code',30000,'Vienna');
INSERT INTO PROJ VALUES  ('P3','SDP','Test',30000,'Tampa');
INSERT INTO PROJ VALUES  ('P4','SDP','Design',20000,'Deale');
INSERT INTO PROJ VALUES  ('P5','IRM','Test',10000,'Vienna');
INSERT INTO PROJ VALUES  ('P6','PAYR','Design',50000,'Deale');

INSERT INTO WORKS VALUES  ('E1','P1',40);
INSERT INTO WORKS VALUES  ('E1','P2',20);
INSERT INTO WORKS VALUES  ('E1','P3',80);
INSERT INTO WORKS VALUES  ('E1','P4',20);
INSERT INTO WORKS VALUES  ('E1','P5',12);
INSERT INTO WORKS VALUES  ('E1','P6',12);
INSERT INTO WORKS VALUES  ('E2','P1',40);
INSERT INTO WORKS VALUES  ('E2','P2',80);
INSERT INTO WORKS VALUES  ('E3','P2',20);
INSERT INTO WORKS VALUES  ('E4','P2',20);
INSERT INTO WORKS VALUES  ('E4','P4',40);
INSERT INTO WORKS VALUES  ('E4','P5',80);

explain SELECT EMPNUM, EMPNAME
FROM STAFF
WHERE EMPNUM IN
  (SELECT EMPNUM  FROM WORKS
   WHERE PNUM IN
     (SELECT PNUM  FROM PROJ));

SELECT EMPNUM, EMPNAME
FROM STAFF
WHERE EMPNUM IN
  (SELECT EMPNUM  FROM WORKS
   WHERE PNUM IN
     (SELECT PNUM  FROM PROJ));

drop table STAFF,WORKS,PROJ;

--echo # End of bug#45191

--echo #  
--echo # BUG#36896: Server crash on SELECT FROM DUAL
--echo # 
create table t1 (a int);
select 1 as res from dual where (1) in (select * from t1);
drop table t1;

--echo 
--echo BUG#40118 Crash when running Batched Key Access and requiring one match for each key
--echo
create table t0(a int);
insert into t0 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);
create table t1 (a int, key(a));
insert into t1 select * from t0;
alter table t1 add b int not null, add filler char(200);
insert into t1 select * from t1;
insert into t1 select * from t1;

select * from t0 where t0.a in (select t1.a from t1 where t1.b=0); 
drop table t0, t1;

--echo # 
--echo # BUG#32665 Query with dependent subquery is too slow
--echo #
create table t1 (
  idIndividual int primary key
);
insert into t1 values (1),(2);

create table t2 (
  idContact int primary key,
  contactType int,
  idObj int
);
insert into t2 values (1,1,1),(2,2,2),(3,3,3);

create table t3 (
  idAddress int primary key,
  idContact int,
  postalStripped varchar(100)
);

insert into t3 values (1,1, 'foo'), (2,2,'bar');

--echo The following must be converted to a semi-join:
explain extended SELECT a.idIndividual FROM t1 a 
WHERE a.idIndividual IN 
	(	SELECT c.idObj FROM t3 cona 
		INNER JOIN t2 c ON c.idContact=cona.idContact 
		WHERE cona.postalStripped='T2H3B2'
	);
drop table t1,t2,t3;

#
# Bug#11867 queries with ROW(,elems>) IN (SELECT DISTINCT <cols> FROM ...)
#

CREATE TABLE t1 (one int, two int, flag char(1));
CREATE TABLE t2 (one int, two int, flag char(1));
INSERT INTO t1 VALUES(1,2,'Y'),(2,3,'Y'),(3,4,'Y'),(5,6,'N'),(7,8,'N');
INSERT INTO t2 VALUES(1,2,'Y'),(2,3,'Y'),(3,4,'Y'),(5,6,'N'),(7,8,'N');

SELECT * FROM t1
  WHERE ROW(one,two) IN (SELECT DISTINCT one,two FROM t2 WHERE flag = 'N');
SELECT * FROM t1
  WHERE ROW(one,two) IN (SELECT DISTINCT one,two FROM t1 WHERE flag = 'N');

insert into t2 values (null,null,'N');
insert into t2 values (null,3,'0');
insert into t2 values (null,5,'0');
insert into t2 values (10,null,'0');
insert into t1 values (10,3,'0');
insert into t1 values (10,5,'0');
insert into t1 values (10,10,'0');
SELECT one,two,ROW(one,two) IN (SELECT one,two FROM t2 WHERE flag = 'N') as 'test' from t1;
SELECT one,two from t1 where ROW(one,two) IN (SELECT one,two FROM t2 WHERE flag = 'N');
SELECT one,two,ROW(one,two) IN (SELECT one,two FROM t2 WHERE flag = 'N' group by one,two) as 'test' from t1;
SELECT one,two,ROW(one,two) IN (SELECT one,two FROM t2 WHERE flag = '0') as 'test' from t1;
SELECT one,two,ROW(one,two) IN (SELECT one,two FROM t2 WHERE flag = '0' group by one,two) as 'test' from t1;
explain extended SELECT one,two,ROW(one,two) IN (SELECT one,two FROM t2 WHERE flag = '0') as 'test' from t1;
explain extended SELECT one,two from t1 where ROW(one,two) IN (SELECT one,two FROM t2 WHERE flag = 'N');
explain extended SELECT one,two,ROW(one,two) IN (SELECT one,two FROM t2 WHERE flag = '0' group by one,two) as 'test' from t1;
DROP TABLE t1,t2;


#
# Bug#12392 where cond with IN predicate for rows and NULL values in table
#

CREATE TABLE t1 (a char(5), b char(5));
INSERT INTO t1 VALUES (NULL,'aaa'), ('aaa','aaa');

SELECT * FROM t1 WHERE (a,b) IN (('aaa','aaa'), ('aaa','bbb'));

DROP TABLE t1;


#
# Bug#30788 Inconsistent retrieval of char/varchar
#

CREATE TABLE t1 (a CHAR(1), b VARCHAR(10));
INSERT INTO t1 VALUES ('a', 'aa');
INSERT INTO t1 VALUES ('a', 'aaa');
SELECT a,b FROM t1 WHERE b IN (SELECT a FROM t1);
CREATE INDEX I1 ON t1 (a);
CREATE INDEX I2 ON t1 (b);
EXPLAIN SELECT a,b FROM t1 WHERE b IN (SELECT a FROM t1);
SELECT a,b FROM t1 WHERE b IN (SELECT a FROM t1);

CREATE TABLE t2 (a VARCHAR(1), b VARCHAR(10));
INSERT INTO t2 SELECT * FROM t1;
CREATE INDEX I1 ON t2 (a);
CREATE INDEX I2 ON t2 (b);
EXPLAIN SELECT a,b FROM t2 WHERE b IN (SELECT a FROM t2);
SELECT a,b FROM t2 WHERE b IN (SELECT a FROM t2);
EXPLAIN
SELECT a,b FROM t1 WHERE b IN (SELECT a FROM t1 WHERE LENGTH(a)<500);
SELECT a,b FROM t1 WHERE b IN (SELECT a FROM t1 WHERE LENGTH(a)<500);

DROP TABLE t1,t2;


--echo #
--echo # BUG#45928 "Differing query results depending on MRR and
--echo # engine_condition_pushdown settings"
--echo #

CREATE TABLE `t1` (
  `pk` int(11) NOT NULL AUTO_INCREMENT,
  `time_nokey` time NOT NULL,
  `varchar_key` varchar(1) NOT NULL,
  `varchar_nokey` varchar(1) NOT NULL,
  PRIMARY KEY (`pk`),
  KEY `varchar_key` (`varchar_key`)
) AUTO_INCREMENT=12 DEFAULT CHARSET=latin1;
INSERT INTO `t1` VALUES (10,'00:00:00','i','i'),(11,'00:00:00','','');

SELECT `time_nokey` G1  FROM t1  WHERE ( `varchar_nokey`  , `varchar_key`  )  IN ( 
SELECT `varchar_nokey`  , `varchar_nokey`  )  AND `varchar_key`  >= 'c' HAVING G1  ORDER 
BY `pk`   ;

DROP TABLE t1;

--echo #
--echo # BUG#45863 "Assertion failed: (fixed == 0), function fix_fields(),
--echo #            file item.cc, line 4448"
--echo #
--disable_warnings
DROP TABLE IF EXISTS C, BB;
--enable_warnings

CREATE TABLE C (
  varchar_nokey varchar(1) NOT NULL
);
INSERT INTO C VALUES
  ('k'),('a'),(''),('u'),('e'),('v'),('i'),
  ('t'),('u'),('f'),('u'),('m'),('j'),('f'),
  ('v'),('j'),('g'),('e'),('h'),('z');
CREATE TABLE BB (
  varchar_nokey varchar(1) NOT NULL
);
INSERT INTO BB VALUES ('i'),('t');
-- error ER_BAD_FIELD_ERROR
SELECT varchar_nokey FROM C
WHERE (varchar_nokey, OUTR) IN (SELECT varchar_nokey
                                FROM BB);
-- error ER_BAD_FIELD_ERROR
SELECT varchar_nokey FROM C
WHERE (varchar_nokey, OUTR) IN (SELECT varchar_nokey, varchar_nokey
                                FROM BB);
DROP TABLE C,BB;

--echo #
--echo # During work with BUG#45863 I had problems with a query that was
--echo # optimized differently in regular and prepared mode.
--echo # Because there was a bug in one of the selected strategies, I became
--echo # aware of the problem. Adding an EXPLAIN query to catch this.

--disable_warnings
DROP TABLE IF EXISTS t1, t2, t3;
--enable_warnings

CREATE TABLE t1
 (EMPNUM   CHAR(3) NOT NULL,
  EMPNAME  CHAR(20),
  GRADE    DECIMAL(4),
  CITY     CHAR(15));

CREATE TABLE t2
 (PNUM     CHAR(3) NOT NULL,
  PNAME    CHAR(20),
  PTYPE    CHAR(6),
  BUDGET   DECIMAL(9),
  CITY     CHAR(15));

CREATE TABLE t3
 (EMPNUM   CHAR(3) NOT NULL,
  PNUM     CHAR(3) NOT NULL,
  HOURS    DECIMAL(5));

INSERT INTO t1 VALUES ('E1','Alice',12,'Deale');
INSERT INTO t1 VALUES ('E2','Betty',10,'Vienna');
INSERT INTO t1 VALUES ('E3','Carmen',13,'Vienna');
INSERT INTO t1 VALUES ('E4','Don',12,'Deale');
INSERT INTO t1 VALUES ('E5','Ed',13,'Akron');

INSERT INTO t2 VALUES ('P1','MXSS','Design',10000,'Deale');
INSERT INTO t2 VALUES ('P2','CALM','Code',30000,'Vienna');
INSERT INTO t2 VALUES ('P3','SDP','Test',30000,'Tampa');
INSERT INTO t2 VALUES ('P4','SDP','Design',20000,'Deale');
INSERT INTO t2 VALUES ('P5','IRM','Test',10000,'Vienna');
INSERT INTO t2 VALUES ('P6','PAYR','Design',50000,'Deale');

INSERT INTO t3 VALUES  ('E1','P1',40);
INSERT INTO t3 VALUES  ('E1','P2',20);
INSERT INTO t3 VALUES  ('E1','P3',80);
INSERT INTO t3 VALUES  ('E1','P4',20);
INSERT INTO t3 VALUES  ('E1','P5',12);
INSERT INTO t3 VALUES  ('E1','P6',12);
INSERT INTO t3 VALUES  ('E2','P1',40);
INSERT INTO t3 VALUES  ('E2','P2',80);
INSERT INTO t3 VALUES  ('E3','P2',20);
INSERT INTO t3 VALUES  ('E4','P2',20);
INSERT INTO t3 VALUES  ('E4','P4',40);
INSERT INTO t3 VALUES  ('E4','P5',80);

CREATE UNIQUE INDEX t1_IDX ON t1(EMPNUM);

EXPLAIN SELECT EMPNAME
FROM t1
WHERE EMPNUM IN
   (SELECT EMPNUM
    FROM t3
    WHERE PNUM IN
       (SELECT PNUM
        FROM t2
        WHERE PTYPE = 'Design'));

PREPARE stmt FROM "EXPLAIN SELECT EMPNAME
FROM t1
WHERE EMPNUM IN
   (SELECT EMPNUM
    FROM t3
    WHERE PNUM IN
       (SELECT PNUM
        FROM t2
        WHERE PTYPE = 'Design'))";
EXECUTE stmt;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

DROP INDEX t1_IDX ON t1;
CREATE INDEX t1_IDX ON t1(EMPNUM);

EXPLAIN SELECT EMPNAME
FROM t1
WHERE EMPNUM IN
   (SELECT EMPNUM
    FROM t3
    WHERE PNUM IN
       (SELECT PNUM
        FROM t2
        WHERE PTYPE = 'Design'));

PREPARE stmt FROM "EXPLAIN SELECT EMPNAME
FROM t1
WHERE EMPNUM IN
   (SELECT EMPNUM
    FROM t3
    WHERE PNUM IN
       (SELECT PNUM
        FROM t2
        WHERE PTYPE = 'Design'))";
EXECUTE stmt;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

DROP INDEX t1_IDX ON t1;

EXPLAIN SELECT EMPNAME
FROM t1
WHERE EMPNUM IN
   (SELECT EMPNUM
    FROM t3
    WHERE PNUM IN
       (SELECT PNUM
        FROM t2
        WHERE PTYPE = 'Design'));

PREPARE stmt FROM "EXPLAIN SELECT EMPNAME
FROM t1
WHERE EMPNUM IN
   (SELECT EMPNUM
    FROM t3
    WHERE PNUM IN
       (SELECT PNUM
        FROM t2
        WHERE PTYPE = 'Design'))";
EXECUTE stmt;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

DROP TABLE t1, t2, t3;

--echo #
--echo # BUG#45221 Query SELECT pk FROM C WHERE pk IN (SELECT int_key) failing
--echo #

CREATE TABLE t1 (
  i1_key INT,
  i2 INT,
  i3 INT,
  KEY i1_index (i1_key)
);

INSERT INTO t1 VALUES (9,1,2), (9,2,1);

CREATE TABLE t2 (
  pk INT NOT NULL,
  i1 INT,
  PRIMARY KEY (pk)
);

INSERT INTO t2 VALUES (9,1);

SELECT pk
FROM t2
WHERE 
   pk IN ( 
     SELECT i1_key
     FROM t1
     WHERE t1.i2 < t1.i3 XOR t2.i1 > 1
     ORDER BY t1.i2 desc);

DROP TABLE t1,t2;

--echo # BUG#50361 Doublenested noncorrelated subquery with FirstMatch and join cache wrong result
--echo #

CREATE TABLE t1(
  id INTEGER
  );
INSERT INTO t1 VALUES(10),(20);
create table t2 select * from t1;
create table t3 select * from t1;

SELECT *
FROM t1
WHERE 1 IN(SELECT 1
             FROM t2
             WHERE 1 IN(SELECT 1
                          FROM t3));

explain extended SELECT *
FROM t1
WHERE 1 IN(SELECT 1
             FROM t2
             WHERE 1 IN(SELECT 1
                          FROM t3));

delete from t2;
delete from t3;

INSERT INTO t1 VALUES(30),(40),(50),(60),(70),(80),(90);
insert into t2 select * from t1;
insert into t3 select * from t1;
create table t4 select * from t1;

SELECT *
FROM t1
WHERE 1 IN(SELECT 1
           FROM t2
           WHERE 1 IN(SELECT 1
                      FROM t3
                      WHERE 1 IN(SELECT 1
                                 FROM t4)));

explain SELECT *
FROM t1
WHERE 1 IN(SELECT 1
           FROM t2
           WHERE 1 IN(SELECT 1
                      FROM t3
                      WHERE 1 IN(SELECT 1
                                 FROM t4)));

SELECT *
FROM t1
WHERE 1 IN(SELECT 1
           FROM t1
           WHERE 1 IN(SELECT 1
                      FROM t1
                      WHERE 1 IN(SELECT 1
                                 FROM t1)));

drop table t1,t2,t3,t4;

--echo #
--echo # Bug#53236 Segfault in DTCollation::set(DTCollation&)
--echo #

CREATE TABLE t1 (
  pk INTEGER AUTO_INCREMENT,
  col_varchar VARCHAR(1),
  PRIMARY KEY (pk)
)
;

INSERT INTO t1 (col_varchar) 
VALUES
('w'),
('m')
;

SELECT  table1.pk
FROM ( t1 AS table1 JOIN t1 AS table2 ON (table1.col_varchar =
                                          table2.col_varchar) ) 
WHERE ( 1, 2 ) IN ( SELECT SUBQUERY1_t1.pk AS SUBQUERY1_field1,
                           SUBQUERY1_t1.pk AS SUBQUERY1_field2
                    FROM ( t1 AS SUBQUERY1_t1 JOIN t1 AS SUBQUERY1_t2
                           ON (SUBQUERY1_t2.col_varchar =
                               SUBQUERY1_t1.col_varchar) ) ) 
;

drop table t1;

--echo #
--echo # BUG#53298 "wrong result with semijoin (no semijoin strategy chosen)"
--echo #

create table t1 (uid int, fid int);
insert into t1 values (1,1), (3,1);

create table t2 (uid int, name varchar(128));
insert into t2 values (1, "A"), (2, "B");

create table t3 (uid int, fid int, index(uid));
insert into t3 values (1,3), (1,3);

create table t4 (uid int);
insert into t4 values (3);

explain select t2.uid from t2, t1 
  where t1.uid in (select t4.uid from t4, t3 where t3.uid=1 and t4.uid=t3.fid)
        and t2.uid=t1.fid;

--sorted_result
select t2.uid from t2, t1 
  where t1.uid in (select t4.uid from t4, t3 where t3.uid=1 and t4.uid=t3.fid)
        and t2.uid=t1.fid;

drop table t1,t2,t3,t4;


#
# BUG#46548 IN-subqueries return 0 rows with materialization=on
#
CREATE TABLE t1 (
  pk int,
  a varchar(1),
  b varchar(4),
  c varchar(4),
  d varchar(4),
  PRIMARY KEY (pk)
);
INSERT INTO t1 VALUES (1,'o','ffff','ffff','ffoo'),(2,'f','ffff','ffff','ffff');

CREATE TABLE t2 LIKE t1;
INSERT INTO t2 VALUES (1,'i','iiii','iiii','iiii'),(2,'f','ffff','ffff','ffff');

EXPLAIN SELECT pk FROM t1 WHERE (a) IN (SELECT a FROM t2 WHERE pk > 0);
SELECT pk FROM t1 WHERE (a) IN (SELECT a FROM t2 WHERE pk > 0);
SELECT pk FROM t1 WHERE (b,c,d) IN (SELECT b,c,d FROM t2 WHERE pk > 0);
DROP TABLE t1, t2;

#
# Bug #44303 Assertion failures in Field_new_decimal::store_decimal
#            when executing materialized InsideOut semijoin
# 
CREATE TABLE t1 (f1 INT, f2 DECIMAL(5,3)) ENGINE=MyISAM;
INSERT INTO t1 (f1, f2) VALUES (1, 1.789);
INSERT INTO t1 (f1, f2) VALUES (13, 1.454);
INSERT INTO t1 (f1, f2) VALUES (10, 1.668);

CREATE TABLE t2 LIKE t1;
INSERT INTO t2 VALUES (1, 1.789);
INSERT INTO t2 VALUES (13, 1.454);

EXPLAIN SELECT COUNT(*) FROM t1 WHERE (f1,f2) IN (SELECT f1,f2 FROM t2);
SELECT COUNT(*) FROM t1 WHERE (f1,f2) IN (SELECT f1,f2 FROM t2);

DROP TABLE t1, t2;


#
# Bug #27348: Assertion abort for a query with two subqueries to be flattened  
# Bug #35674: Range optimizer ignores conditions on inner tables in
#             semi-join IN subqueries 
#
CREATE TABLE t1 (
  ID int(11) NOT NULL auto_increment,
  Name char(35) NOT NULL default '',
  Country char(3) NOT NULL default '',
  Population int(11) NOT NULL default '0',
  PRIMARY KEY  (ID),
  INDEX (Population),
  INDEX (Country) 
);
CREATE TABLE t2 (
  Code char(3) NOT NULL default '',
  Name char(52) NOT NULL default '',
  SurfaceArea float(10,2) NOT NULL default '0.00',
  Population int(11) NOT NULL default '0',
  Capital int(11) default NULL,
  PRIMARY KEY  (Code),
  UNIQUE INDEX (Name),
  INDEX (Population)
);
CREATE TABLE t3 (
  Country char(3) NOT NULL default '',
  Language char(30) NOT NULL default '',
  Percentage float(3,1) NOT NULL default '0.0',
  PRIMARY KEY  (Country, Language),
  INDEX (Percentage)
);

--disable_query_log
INSERT INTO t1 VALUES
(1,'Kabul','AFG',1780000),(2,'Qandahar','AFG',237500),
(3,'Herat','AFG',186800),(4,'Mazar-e-Sharif','AFG',127800),
(5,'Amsterdam','NLD',731200),(6,'Rotterdam','NLD',593321),
(7,'Haag','NLD',440900),(8,'Utrecht','NLD',234323),
(9,'Eindhoven','NLD',201843),(10,'Tilburg','NLD',193238),
(11,'Groningen','NLD',172701),(12,'Breda','NLD',160398),
(13,'Apeldoorn','NLD',153491),(14,'Nijmegen','NLD',152463),
(15,'Enschede','NLD',149544),(16,'Haarlem','NLD',148772),
(17,'Almere','NLD',142465),(18,'Arnhem','NLD',138020),
(19,'Zaanstad','NLD',135621),(20,'Â´s-Hertogenbosch','NLD',129170),
(21,'Amersfoort','NLD',126270),(22,'Maastricht','NLD',122087),
(23,'Dordrecht','NLD',119811),(24,'Leiden','NLD',117196),
(25,'Haarlemmermeer','NLD',110722),(26,'Zoetermeer','NLD',110214),
(27,'Emmen','NLD',105853),(28,'Zwolle','NLD',105819),
(29,'Ede','NLD',101574),(30,'Delft','NLD',95268);

INSERT INTO t2 VALUES 
('AFG','Afghanistan',652090.00,22720000,1),
('NLD','Netherlands',41526.00,15864000,5),
('ANT','Netherlands Antilles',800.00,217000,33),
('ALB','Albania',28748.00,3401200,34),
('DZA','Algeria',2381741.00,31471000,35),
('ASM','American Samoa',199.00,68000,54),
('AND','Andorra',468.00,78000,55),
('AGO','Angola',1246700.00,12878000,56),
('AIA','Anguilla',96.00,8000,62),
('ATG','Antigua and Barbuda',442.00,68000,63),
('ARE','United Arab Emirates',83600.00,2441000,65),
('ARG','Argentina',2780400.00,37032000,69),
('ARM','Armenia',29800.00,3520000,126),
('ABW','Aruba',193.00,103000,129),
('AUS','Australia',7741220.00,18886000,135),
('AZE','Azerbaijan',86600.00,7734000,144);

INSERT INTO t3 VALUES 
('AFG','Pashto',52.4),('NLD','Dutch',95.6),
('ANT','Papiamento',86.2),('ALB','Albaniana',97.9),
('DZA','Arabic',86.0),('ASM','Samoan',90.6),
('AND','Spanish',44.6),('AGO','Ovimbundu',37.2),
('AIA','English',0.0),('ATG','Creole English',95.7),
('ARE','Arabic',42.0),('ARG','Spanish',96.8),
('ARM','Armenian',93.4),('ABW','Papiamento',76.7),
('AUS','English',81.2),('AZE','Azerbaijani',89.0),
('BHS','Creole English',89.7),('BHR','Arabic',67.7),
('BGD','Bengali',97.7),('BRB','Bajan',95.1),
('BEL','Dutch',59.2),('BLZ','English',50.8);
--enable_query_log

let $query=
SELECT Name FROM t2 
  WHERE t2.Code IN (SELECT Country FROM t1 WHERE Population > 5000000)
        AND
        t2.Code IN (SELECT Country FROM t3 
                           WHERE Language='English' AND Percentage > 10 AND
                                 t2.Population > 100000);
--eval EXPLAIN $query
--eval EXPLAIN FORMAT=JSON $query

DROP TABLE t1,t2,t3;

#
# BUG#30993: Subqueries: LooseScan strategy produces wrong query results, with duplicates
#

CREATE TABLE t1 (
  Code char(3) NOT NULL DEFAULT '',
  Name char(52) NOT NULL DEFAULT '',
  Continent enum('Asia','Europe','North America','Africa','Oceania','Antarctica','South America') NOT NULL DEFAULT 'Asia',
  Region char(26) NOT NULL DEFAULT '',
  SurfaceArea float(10,2) NOT NULL DEFAULT '0.00',
  IndepYear smallint(6) DEFAULT NULL,
  Population int(11) NOT NULL DEFAULT '0',
  LifeExpectancy float(3,1) DEFAULT NULL,
  GNP float(10,2) DEFAULT NULL,
  GNPOld float(10,2) DEFAULT NULL,
  LocalName char(45) NOT NULL DEFAULT '',
  GovernmentForm char(45) NOT NULL DEFAULT '',
  HeadOfState char(60) DEFAULT NULL,
  Capital int(11) DEFAULT NULL,
  Code2 char(2) NOT NULL DEFAULT '',
  PRIMARY KEY (Code)
);

CREATE TABLE t2 (
  ID int(11) NOT NULL AUTO_INCREMENT,
  Name char(35) NOT NULL DEFAULT '',
  CountryCode char(3) NOT NULL DEFAULT '',
  District char(20) NOT NULL DEFAULT '',
  Population int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (ID),
  KEY CountryCode (CountryCode)
);

--echo Fill the table with test data
--disable_query_log
insert into t2 (ID, Name, CountryCode, Population) values 
(1,'Kabul','AFG',1780000), (2,'Qandahar','AFG',237500), (3,'Herat','AFG',186800),
(4,'Mazar-e-Sharif','AFG',127800), (33,'Willemstad','ANT',2345), (34,'Tirana','ALB',270000),
(55,'Andorra la Vella','AND',21189), (61,'South Hill','AIA',961), (62,'The Valley','AIA',595),
(63,'Saint John´s','ATG',24000), (64,'Dubai','ARE',669181), (65,'Abu Dhabi','ARE',398695),
(66,'Sharja','ARE',320095), (67,'al-Ayn','ARE',225970), (68,'Ajman','ARE',114395),
(126,'Yerevan','ARM',1248700), (127,'Gjumri','ARM',211700), (128,'Vanadzor','ARM',172700),
(129,'Oranjestad','ABW',29034), (144,'Baku','AZE',1787800), (145,'Gäncä','AZE',299300),
(146,'Sumqayit','AZE',283000), (147,'Mingäçevir','AZE',93900), (148,'Nassau','BHS',172000),
(149,'al-Manama','BHR',148000), (150,'Dhaka','BGD',3612850), (151,'Chittagong','BGD',1392860),
(152,'Khulna','BGD',663340), (153,'Rajshahi','BGD',294056), (154,'Narayanganj','BGD',202134),
(155,'Rangpur','BGD',191398), (156,'Mymensingh','BGD',188713), (157,'Barisal','BGD',170232),
(158,'Tungi','BGD',168702), (159,'Jessore','BGD',139710), (160,'Comilla','BGD',135313),
(161,'Nawabganj','BGD',130577), (162,'Dinajpur','BGD',127815), (163,'Bogra','BGD',120170),
(164,'Sylhet','BGD',117396), (165,'Brahmanbaria','BGD',109032), (166,'Tangail','BGD',106004),
(167,'Jamalpur','BGD',103556), (168,'Pabna','BGD',103277), (169,'Naogaon','BGD',101266),
(170,'Sirajganj','BGD',99669), (171,'Narsinghdi','BGD',98342), (172,'Saidpur','BGD',96777),
(173,'Gazipur','BGD',96717), (174,'Bridgetown','BRB',6070), (175,'Antwerpen','BEL',446525),
(176,'Gent','BEL',224180), (177,'Charleroi','BEL',200827), (178,'Liège','BEL',185639),
(179,'Bruxelles [Brussel]','BEL',133859), (180,'Brugge','BEL',116246), (181,'Schaerbeek','BEL',105692),
(182,'Namur','BEL',105419), (183,'Mons','BEL',90935), (184,'Belize City','BLZ',55810),
(185,'Belmopan','BLZ',7105), (190,'Saint George','BMU',1800), (191,'Hamilton','BMU',1200),
(192,'Thimphu','BTN',22000), (201,'Sarajevo','BIH',360000), (202,'Banja Luka','BIH',143079),
(203,'Zenica','BIH',96027), (538,'Bandar Seri Begawan','BRN',21484), (539,'Sofija','BGR',1122302),
(540,'Plovdiv','BGR',342584), (541,'Varna','BGR',299801), (542,'Burgas','BGR',195255),
(543,'Ruse','BGR',166467), (544,'Stara Zagora','BGR',147939), (545,'Pleven','BGR',121952),
(546,'Sliven','BGR',105530), (547,'Dobric','BGR',100399), (548,'Šumen','BGR',94686),
(553,'George Town','CYM',19600), (584,'San José','CRI',339131), (1523,'Wien','AUT',1608144),
(1524,'Graz','AUT',240967), (1525,'Linz','AUT',188022), (1526,'Salzburg','AUT',144247),
(1527,'Innsbruck','AUT',111752), (1528,'Klagenfurt','AUT',91141), (1810,'Montréal','CAN',1016376),
(1811,'Calgary','CAN',768082), (1812,'Toronto','CAN',688275), (1813,'North York','CAN',622632),
(1814,'Winnipeg','CAN',618477), (1815,'Edmonton','CAN',616306), (1816,'Mississauga','CAN',608072),
(1817,'Scarborough','CAN',594501), (1818,'Vancouver','CAN',514008), (1819,'Etobicoke','CAN',348845),
(1820,'London','CAN',339917), (1821,'Hamilton','CAN',335614), (1822,'Ottawa','CAN',335277),
(1823,'Laval','CAN',330393), (1824,'Surrey','CAN',304477), (1825,'Brampton','CAN',296711),
(1826,'Windsor','CAN',207588), (1827,'Saskatoon','CAN',193647), (1828,'Kitchener','CAN',189959),
(1829,'Markham','CAN',189098), (1830,'Regina','CAN',180400), (1831,'Burnaby','CAN',179209),
(1832,'Québec','CAN',167264), (1833,'York','CAN',154980), (1834,'Richmond','CAN',148867),
(1835,'Vaughan','CAN',147889), (1836,'Burlington','CAN',145150), (1837,'Oshawa','CAN',140173),
(1838,'Oakville','CAN',139192), (1839,'Saint Catharines','CAN',136216), (1840,'Longueuil','CAN',127977),
(1841,'Richmond Hill','CAN',116428), (1842,'Thunder Bay','CAN',115913), (1843,'Nepean','CAN',115100),
(1844,'Cape Breton','CAN',114733), (1845,'East York','CAN',114034), (1846,'Halifax','CAN',113910),
(1847,'Cambridge','CAN',109186), (1848,'Gloucester','CAN',107314), (1849,'Abbotsford','CAN',105403),
(1850,'Guelph','CAN',103593), (1851,'Saint John´s','CAN',101936), (1852,'Coquitlam','CAN',101820),
(1853,'Saanich','CAN',101388), (1854,'Gatineau','CAN',100702), (1855,'Delta','CAN',95411),
(1856,'Sudbury','CAN',92686), (1857,'Kelowna','CAN',89442), (1858,'Barrie','CAN',89269),
(1890,'Shanghai','CHN',9696300), (1891,'Peking','CHN',7472000), (1892,'Chongqing','CHN',6351600),
(1893,'Tianjin','CHN',5286800), (1894,'Wuhan','CHN',4344600), (1895,'Harbin','CHN',4289800),
(1896,'Shenyang','CHN',4265200), (1897,'Kanton [Guangzhou]','CHN',4256300), (1898,'Chengdu','CHN',3361500),
(1899,'Nanking [Nanjing]','CHN',2870300), (1900,'Changchun','CHN',2812000), (1901,'Xi´an','CHN',2761400),
(1902,'Dalian','CHN',2697000), (1903,'Qingdao','CHN',2596000), (1904,'Jinan','CHN',2278100),
(1905,'Hangzhou','CHN',2190500), (1906,'Zhengzhou','CHN',2107200), (1907,'Shijiazhuang','CHN',2041500),
(1908,'Taiyuan','CHN',1968400), (1909,'Kunming','CHN',1829500), (1910,'Changsha','CHN',1809800),
(1911,'Nanchang','CHN',1691600), (1912,'Fuzhou','CHN',1593800), (1913,'Lanzhou','CHN',1565800),
(1914,'Guiyang','CHN',1465200), (1915,'Ningbo','CHN',1371200), (1916,'Hefei','CHN',1369100),
(1917,'Urumtši [Ürümqi]','CHN',1310100), (1918,'Anshan','CHN',1200000), (1919,'Fushun','CHN',1200000),
(1920,'Nanning','CHN',1161800), (1921,'Zibo','CHN',1140000), (1922,'Qiqihar','CHN',1070000),
(1923,'Jilin','CHN',1040000), (1924,'Tangshan','CHN',1040000), (1925,'Baotou','CHN',980000),
(1926,'Shenzhen','CHN',950500), (1927,'Hohhot','CHN',916700), (1928,'Handan','CHN',840000),
(1929,'Wuxi','CHN',830000), (1930,'Xuzhou','CHN',810000), (1931,'Datong','CHN',800000),
(1932,'Yichun','CHN',800000), (1933,'Benxi','CHN',770000), (1934,'Luoyang','CHN',760000),
(1935,'Suzhou','CHN',710000), (1936,'Xining','CHN',700200), (1937,'Huainan','CHN',700000),
(1938,'Jixi','CHN',683885), (1939,'Daqing','CHN',660000), (1940,'Fuxin','CHN',640000),
(1941,'Amoy [Xiamen]','CHN',627500), (1942,'Liuzhou','CHN',610000), (1943,'Shantou','CHN',580000),
(1944,'Jinzhou','CHN',570000), (1945,'Mudanjiang','CHN',570000), (1946,'Yinchuan','CHN',544500),
(1947,'Changzhou','CHN',530000), (1948,'Zhangjiakou','CHN',530000), (1949,'Dandong','CHN',520000),
(1950,'Hegang','CHN',520000), (1951,'Kaifeng','CHN',510000), (1952,'Jiamusi','CHN',493409),
(1953,'Liaoyang','CHN',492559), (1954,'Hengyang','CHN',487148), (1955,'Baoding','CHN',483155),
(1956,'Hunjiang','CHN',482043), (1957,'Xinxiang','CHN',473762), (1958,'Huangshi','CHN',457601),
(1959,'Haikou','CHN',454300), (1960,'Yantai','CHN',452127), (1961,'Bengbu','CHN',449245),
(1962,'Xiangtan','CHN',441968), (1963,'Weifang','CHN',428522), (1964,'Wuhu','CHN',425740),
(1965,'Pingxiang','CHN',425579), (1966,'Yingkou','CHN',421589), (1967,'Anyang','CHN',420332),
(1968,'Panzhihua','CHN',415466), (1969,'Pingdingshan','CHN',410775), (1970,'Xiangfan','CHN',410407),
(1971,'Zhuzhou','CHN',409924), (1972,'Jiaozuo','CHN',409100), (1973,'Wenzhou','CHN',401871),
(1974,'Zhangjiang','CHN',400997), (1975,'Zigong','CHN',393184), (1976,'Shuangyashan','CHN',386081),
(1977,'Zaozhuang','CHN',380846), (1978,'Yakeshi','CHN',377869), (1979,'Yichang','CHN',371601),
(1980,'Zhenjiang','CHN',368316), (1981,'Huaibei','CHN',366549), (1982,'Qinhuangdao','CHN',364972),
(1983,'Guilin','CHN',364130), (1984,'Liupanshui','CHN',363954), (1985,'Panjin','CHN',362773),
(1986,'Yangquan','CHN',362268), (1987,'Jinxi','CHN',357052), (1988,'Liaoyuan','CHN',354141),
(1989,'Lianyungang','CHN',354139), (1990,'Xianyang','CHN',352125), (1991,'Tai´an','CHN',350696),
(1992,'Chifeng','CHN',350077), (1993,'Shaoguan','CHN',350043), (1994,'Nantong','CHN',343341),
(1995,'Leshan','CHN',341128), (1996,'Baoji','CHN',337765), (1997,'Linyi','CHN',324720),
(1998,'Tonghua','CHN',324600), (1999,'Siping','CHN',317223), (2000,'Changzhi','CHN',317144),
(2001,'Tengzhou','CHN',315083), (2002,'Chaozhou','CHN',313469), (2003,'Yangzhou','CHN',312892),
(2004,'Dongwan','CHN',308669), (2005,'Ma´anshan','CHN',305421), (2006,'Foshan','CHN',303160),
(2007,'Yueyang','CHN',302800), (2008,'Xingtai','CHN',302789), (2009,'Changde','CHN',301276),
(2010,'Shihezi','CHN',299676), (2011,'Yancheng','CHN',296831), (2012,'Jiujiang','CHN',291187),
(2013,'Dongying','CHN',281728), (2014,'Shashi','CHN',281352), (2015,'Xintai','CHN',281248),
(2016,'Jingdezhen','CHN',281183), (2017,'Tongchuan','CHN',280657), (2018,'Zhongshan','CHN',278829),
(2019,'Shiyan','CHN',273786), (2020,'Tieli','CHN',265683), (2021,'Jining','CHN',265248),
(2022,'Wuhai','CHN',264081), (2023,'Mianyang','CHN',262947), (2024,'Luzhou','CHN',262892),
(2025,'Zunyi','CHN',261862), (2026,'Shizuishan','CHN',257862), (2027,'Neijiang','CHN',256012),
(2028,'Tongliao','CHN',255129), (2029,'Tieling','CHN',254842), (2030,'Wafangdian','CHN',251733),
(2031,'Anqing','CHN',250718), (2032,'Shaoyang','CHN',247227), (2033,'Laiwu','CHN',246833),
(2034,'Chengde','CHN',246799), (2035,'Tianshui','CHN',244974), (2036,'Nanyang','CHN',243303),
(2037,'Cangzhou','CHN',242708), (2038,'Yibin','CHN',241019), (2039,'Huaiyin','CHN',239675),
(2040,'Dunhua','CHN',235100), (2041,'Yanji','CHN',230892), (2042,'Jiangmen','CHN',230587),
(2043,'Tongling','CHN',228017), (2044,'Suihua','CHN',227881), (2045,'Gongziling','CHN',226569),
(2046,'Xiantao','CHN',222884), (2047,'Chaoyang','CHN',222394), (2048,'Ganzhou','CHN',220129),
(2049,'Huzhou','CHN',218071), (2050,'Baicheng','CHN',217987), (2051,'Shangzi','CHN',215373),
(2052,'Yangjiang','CHN',215196), (2053,'Qitaihe','CHN',214957), (2054,'Gejiu','CHN',214294),
(2055,'Jiangyin','CHN',213659), (2056,'Hebi','CHN',212976), (2057,'Jiaxing','CHN',211526),
(2058,'Wuzhou','CHN',210452), (2059,'Meihekou','CHN',209038), (2060,'Xuchang','CHN',208815),
(2061,'Liaocheng','CHN',207844), (2062,'Haicheng','CHN',205560), (2063,'Qianjiang','CHN',205504),
(2064,'Baiyin','CHN',204970), (2065,'Bei´an','CHN',204899), (2066,'Yixing','CHN',200824),
(2067,'Laizhou','CHN',198664), (2068,'Qaramay','CHN',197602), (2069,'Acheng','CHN',197595),
(2070,'Dezhou','CHN',195485), (2071,'Nanping','CHN',195064), (2072,'Zhaoqing','CHN',194784),
(2073,'Beipiao','CHN',194301), (2074,'Fengcheng','CHN',193784), (2075,'Fuyu','CHN',192981),
(2076,'Xinyang','CHN',192509), (2077,'Dongtai','CHN',192247), (2078,'Yuci','CHN',191356),
(2079,'Honghu','CHN',190772), (2080,'Ezhou','CHN',190123), (2081,'Heze','CHN',189293),
(2082,'Daxian','CHN',188101), (2083,'Linfen','CHN',187309), (2084,'Tianmen','CHN',186332),
(2085,'Yiyang','CHN',185818), (2086,'Quanzhou','CHN',185154), (2087,'Rizhao','CHN',185048),
(2088,'Deyang','CHN',182488), (2089,'Guangyuan','CHN',182241), (2090,'Changshu','CHN',181805),
(2091,'Zhangzhou','CHN',181424), (2092,'Hailar','CHN',180650), (2093,'Nanchong','CHN',180273),
(2094,'Jiutai','CHN',180130), (2095,'Zhaodong','CHN',179976), (2096,'Shaoxing','CHN',179818),
(2097,'Fuyang','CHN',179572), (2098,'Maoming','CHN',178683), (2099,'Qujing','CHN',178669),
(2100,'Ghulja','CHN',177193), (2101,'Jiaohe','CHN',176367), (2102,'Puyang','CHN',175988),
(2103,'Huadian','CHN',175873), (2104,'Jiangyou','CHN',175753), (2105,'Qashqar','CHN',174570),
(2106,'Anshun','CHN',174142), (2107,'Fuling','CHN',173878), (2108,'Xinyu','CHN',173524),
(2109,'Hanzhong','CHN',169930), (2110,'Danyang','CHN',169603), (2111,'Chenzhou','CHN',169400),
(2112,'Xiaogan','CHN',166280), (2113,'Shangqiu','CHN',164880), (2114,'Zhuhai','CHN',164747),
(2115,'Qingyuan','CHN',164641), (2116,'Aqsu','CHN',164092), (2117,'Jining','CHN',163552),
(2118,'Xiaoshan','CHN',162930), (2119,'Zaoyang','CHN',162198), (2120,'Xinghua','CHN',161910),
(2121,'Hami','CHN',161315), (2122,'Huizhou','CHN',161023), (2123,'Jinmen','CHN',160794),
(2124,'Sanming','CHN',160691), (2125,'Ulanhot','CHN',159538), (2126,'Korla','CHN',159344),
(2127,'Wanxian','CHN',156823), (2128,'Rui´an','CHN',156468), (2129,'Zhoushan','CHN',156317),
(2130,'Liangcheng','CHN',156307), (2131,'Jiaozhou','CHN',153364), (2132,'Taizhou','CHN',152442),
(2133,'Suzhou','CHN',151862), (2134,'Yichun','CHN',151585), (2135,'Taonan','CHN',150168),
(2136,'Pingdu','CHN',150123), (2137,'Ji´an','CHN',148583), (2138,'Longkou','CHN',148362),
(2139,'Langfang','CHN',148105), (2140,'Zhoukou','CHN',146288), (2141,'Suining','CHN',146086),
(2142,'Yulin','CHN',144467), (2143,'Jinhua','CHN',144280), (2144,'Liu´an','CHN',144248),
(2145,'Shuangcheng','CHN',142659), (2146,'Suizhou','CHN',142302), (2147,'Ankang','CHN',142170),
(2148,'Weinan','CHN',140169), (2149,'Longjing','CHN',139417), (2150,'Da´an','CHN',138963),
(2151,'Lengshuijiang','CHN',137994), (2152,'Laiyang','CHN',137080), (2153,'Xianning','CHN',136811),
(2154,'Dali','CHN',136554), (2155,'Anda','CHN',136446), (2156,'Jincheng','CHN',136396),
(2157,'Longyan','CHN',134481), (2158,'Xichang','CHN',134419), (2159,'Wendeng','CHN',133910),
(2160,'Hailun','CHN',133565), (2161,'Binzhou','CHN',133555), (2162,'Linhe','CHN',133183),
(2163,'Wuwei','CHN',133101), (2164,'Duyun','CHN',132971), (2165,'Mishan','CHN',132744),
(2166,'Shangrao','CHN',132455), (2167,'Changji','CHN',132260), (2168,'Meixian','CHN',132156),
(2169,'Yushu','CHN',131861), (2170,'Tiefa','CHN',131807), (2171,'Huai´an','CHN',131149),
(2172,'Leiyang','CHN',130115), (2173,'Zalantun','CHN',130031), (2174,'Weihai','CHN',128888),
(2175,'Loudi','CHN',128418), (2176,'Qingzhou','CHN',128258), (2177,'Qidong','CHN',126872),
(2178,'Huaihua','CHN',126785), (2179,'Luohe','CHN',126438), (2180,'Chuzhou','CHN',125341),
(2181,'Kaiyuan','CHN',124219), (2182,'Linqing','CHN',123958), (2183,'Chaohu','CHN',123676),
(2184,'Laohekou','CHN',123366), (2185,'Dujiangyan','CHN',123357), (2186,'Zhumadian','CHN',123232),
(2187,'Linchuan','CHN',121949), (2188,'Jiaonan','CHN',121397), (2189,'Sanmenxia','CHN',120523),
(2190,'Heyuan','CHN',120101), (2191,'Manzhouli','CHN',120023), (2192,'Lhasa','CHN',120000),
(2193,'Lianyuan','CHN',118858), (2194,'Kuytun','CHN',118553), (2195,'Puqi','CHN',117264),
(2196,'Hongjiang','CHN',116188), (2197,'Qinzhou','CHN',114586), (2198,'Renqiu','CHN',114256),
(2199,'Yuyao','CHN',114065), (2200,'Guigang','CHN',114025), (2201,'Kaili','CHN',113958),
(2202,'Yan´an','CHN',113277), (2203,'Beihai','CHN',112673), (2204,'Xuangzhou','CHN',112673),
(2205,'Quzhou','CHN',112373), (2206,'Yong´an','CHN',111762), (2207,'Zixing','CHN',110048),
(2208,'Liyang','CHN',109520), (2209,'Yizheng','CHN',109268), (2210,'Yumen','CHN',109234),
(2211,'Liling','CHN',108504), (2212,'Yuncheng','CHN',108359), (2213,'Shanwei','CHN',107847),
(2214,'Cixi','CHN',107329), (2215,'Yuanjiang','CHN',107004), (2216,'Bozhou','CHN',106346),
(2217,'Jinchang','CHN',105287), (2218,'Fu´an','CHN',105265), (2219,'Suqian','CHN',105021),
(2220,'Shishou','CHN',104571), (2221,'Hengshui','CHN',104269), (2222,'Danjiangkou','CHN',103211),
(2223,'Fujin','CHN',103104), (2224,'Sanya','CHN',102820), (2225,'Guangshui','CHN',102770),
(2226,'Huangshan','CHN',102628), (2227,'Xingcheng','CHN',102384), (2228,'Zhucheng','CHN',102134),
(2229,'Kunshan','CHN',102052), (2230,'Haining','CHN',100478), (2231,'Pingliang','CHN',99265),
(2232,'Fuqing','CHN',99193), (2233,'Xinzhou','CHN',98667), (2234,'Jieyang','CHN',98531),
(2235,'Zhangjiagang','CHN',97994), (2236,'Tong Xian','CHN',97168), (2237,'Ya´an','CHN',95900),
(2238,'Jinzhou','CHN',95761), (2239,'Emeishan','CHN',94000), (2240,'Enshi','CHN',93056),
(2241,'Bose','CHN',93009), (2242,'Yuzhou','CHN',92889), (2243,'Kaiyuan','CHN',91999),
(2244,'Tumen','CHN',91471), (2245,'Putian','CHN',91030), (2246,'Linhai','CHN',90870),
(2247,'Xilin Hot','CHN',90646), (2248,'Shaowu','CHN',90286), (2249,'Junan','CHN',90222),
(2250,'Huaying','CHN',89400), (2251,'Pingyi','CHN',89373), (2252,'Huangyan','CHN',89288),
(2413,'La Habana','CUB',2256000), (2414,'Santiago de Cuba','CUB',433180), (2415,'Camagüey','CUB',298726),
(2416,'Holguín','CUB',249492), (2417,'Santa Clara','CUB',207350), (2418,'Guantánamo','CUB',205078),
(2419,'Pinar del Río','CUB',142100), (2420,'Bayamo','CUB',141000), (2421,'Cienfuegos','CUB',132770),
(2422,'Victoria de las Tunas','CUB',132350), (2423,'Matanzas','CUB',123273), (2424,'Manzanillo','CUB',109350),
(2425,'Sancti-Spíritus','CUB',100751), (2426,'Ciego de Ávila','CUB',98505), (2430,'Nicosia','CYP',195000),
(2431,'Limassol','CYP',154400), (3245,'Zürich','CHE',336800), (3246,'Geneve','CHE',173500),
(3247,'Basel','CHE',166700), (3248,'Bern','CHE',122700), (3249,'Lausanne','CHE',114500),
(3339,'Praha','CZE',1181126), (3340,'Brno','CZE',381862), (3341,'Ostrava','CZE',320041),
(3342,'Plzen','CZE',166759), (3343,'Olomouc','CZE',102702), (3344,'Liberec','CZE',99155),
(3345,'Ceské Budejovice','CZE',98186), (3346,'Hradec Králové','CZE',98080), (3347,'Ústí nad Labem','CZE',95491),
(3348,'Pardubice','CZE',91309), (3520,'Minsk','BLR',1674000), (3521,'Gomel','BLR',475000),
(3522,'Mogiljov','BLR',356000), (3523,'Vitebsk','BLR',340000), (3524,'Grodno','BLR',302000),
(3525,'Brest','BLR',286000), (3526,'Bobruisk','BLR',221000), (3527,'Baranovitši','BLR',167000),
(3528,'Borisov','BLR',151000), (3529,'Pinsk','BLR',130000), (3530,'Orša','BLR',124000),
(3531,'Mozyr','BLR',110000), (3532,'Novopolotsk','BLR',106000), (3533,'Lida','BLR',101000),
(3534,'Soligorsk','BLR',101000), (3535,'Molodetšno','BLR',97000);

insert into t1 (Code, Name, Continent) values 
('AFG','Afghanistan','Asia'), ('ANT','Netherlands Antilles','North America'),
('ALB','Albania','Europe'), ('AND','Andorra','Europe'),
('AIA','Anguilla','North America'), ('ATG','Antigua and Barbuda','North America'),
('ARE','United Arab Emirates','Asia'), ('ARM','Armenia','Asia'),
('ABW','Aruba','North America'), ('AZE','Azerbaijan','Asia'),
('BHS','Bahamas','North America'), ('BHR','Bahrain','Asia'),
('BGD','Bangladesh','Asia'), ('BRB','Barbados','North America'),
('BEL','Belgium','Europe'), ('BLZ','Belize','North America'),
('BMU','Bermuda','North America'), ('BTN','Bhutan','Asia'),
('BIH','Bosnia and Herzegovina','Europe'), ('BRN','Brunei','Asia'),
('BGR','Bulgaria','Europe'), ('CYM','Cayman Islands','North America'),
('CRI','Costa Rica','North America'), ('AUT','Austria','Europe'),
('CAN','Canada','North America'), ('CHN','China','Asia'),
('CUB','Cuba','North America'), ('CYP','Cyprus','Asia'),
('CHE','Switzerland','Europe'), ('CZE','Czech Republic','Europe'),
('BLR','Belarus','Europe');
update t2 set population=6000000 where Name in ('Wien', 'Vancouver', 'Praha');
--enable_query_log

--echo This must not use LooseScan:
EXPLAIN SELECT Name FROM t1 
  WHERE t1.Code IN (
    SELECT t2.CountryCode FROM t2 WHERE Population > 5000000);

SELECT Name FROM t1 
  WHERE t1.Code IN (
    SELECT t2.CountryCode FROM t2 WHERE Population > 5000000);

drop table t1, t2;


#
# BUG#35160 "Subquery optimization: table pullout is not reflected in EXPLAIN EXTENDED"
#
create table t0 (a int);
insert into t0 values (0),(1),(2),(3),(4);

create table t1 (a int, b int, key(a));
insert into t1 select a,a from t0;

create table t2 (a int, b int, primary key(a));
insert into t2 select * from t1;

# Table t2 should be pulled out because t2.a=t0.a equality
--echo Table t2, unlike table t1, should be displayed as pulled out
explain extended select * from t0
where t0.a in ( select t1.a from t1,t2 where t2.a=t0.a and
t1.b=t2.b);

#
# BUG#46556 "Returning incorrect, empty results for some IN subqueries 
#            w/ semijoin=on"
#

# The above query did not have a valid plan before the fix of BUG#46556.
# Add some data that would cause wrong result with the old plan.
update t1 set a=3, b=11 where a=4;
update t2 set b=11 where a=3;

# the query just below may exhibit BUG#49129:
create temporary table tmp select * from t0 where t0.a in 
  (select t1.a from t1, t2 where t2.a=t0.a and t1.b=t2.b);
# detect it and warn in result file if it's the case:
create temporary table tmp_as_ref (a int);
insert into tmp_as_ref values(0),(1),(2),(3); # correct desired result
if (`select count(*) from tmp_as_ref left join tmp on tmp.a=tmp_as_ref.a
where tmp.a is null`)
{
  --echo 
  --echo # The result below is wrong due to Bug#49129
}
select * from tmp;

drop table t0, t1, t2, tmp, tmp_as_ref;

#
# BUG#35767: Processing of uncorrelated subquery with semi-join cause wrong result and crash
#
CREATE TABLE t1 (
  id int(11) NOT NULL,
  PRIMARY KEY (id));

CREATE TABLE t2 (
  id int(11) NOT NULL,
  fid int(11) NOT NULL,
  PRIMARY KEY (id));

insert into t1 values(1);
insert into t2 values(1,7503),(2,1);

--error 1054
explain select count(*) 
from t1 
where fid IN (select fid from t2 where (id between 7502 and 8420) order by fid );

drop table t1, t2;
 
#
# BUG#36137 "virtual longlong Item_in_subselect::val_int(): Assertion `0' failed."
#
create table t1 (a int, b int, key (a), key (b));
insert into t1 values (2,4),(2,4),(2,4);
select t1.a from t1 
where 
  t1.a in (select 1 from t1 where t1.a in (select 1 from t1) group by  t1.a);
drop table t1;

#
# BUG#36128: not in subquery causes crash in cleanup..
#
create table t1(a int,b int,key(a),key(b)); 
insert into t1 values (1,1),(2,2),(3,3);
select 1 from t1 
where t1.a not in (select 1 from t1 
                  where t1.a in (select 1 from t1) 
                  group by  t1.b);
drop table t1;

#
# BUG#33743 "nested subqueries, unique index, wrong result"
#
CREATE TABLE t1
 (EMPNUM   CHAR(3) NOT NULL,
  EMPNAME  CHAR(20),
  GRADE    DECIMAL(4),
  CITY     CHAR(15));

CREATE TABLE t2
 (PNUM     CHAR(3) NOT NULL,
  PNAME    CHAR(20),
  PTYPE    CHAR(6),
  BUDGET   DECIMAL(9),
  CITY     CHAR(15));

CREATE TABLE t3
 (EMPNUM   CHAR(3) NOT NULL,
  PNUM     CHAR(3) NOT NULL,
  HOURS    DECIMAL(5));

INSERT INTO t1 VALUES ('E1','Alice',12,'Deale');
INSERT INTO t1 VALUES ('E2','Betty',10,'Vienna');
INSERT INTO t1 VALUES ('E3','Carmen',13,'Vienna');
INSERT INTO t1 VALUES ('E4','Don',12,'Deale');
INSERT INTO t1 VALUES ('E5','Ed',13,'Akron');

INSERT INTO t2 VALUES ('P1','MXSS','Design',10000,'Deale');
INSERT INTO t2 VALUES ('P2','CALM','Code',30000,'Vienna');
INSERT INTO t2 VALUES ('P3','SDP','Test',30000,'Tampa');
INSERT INTO t2 VALUES ('P4','SDP','Design',20000,'Deale');
INSERT INTO t2 VALUES ('P5','IRM','Test',10000,'Vienna');
INSERT INTO t2 VALUES ('P6','PAYR','Design',50000,'Deale');

INSERT INTO t3 VALUES  ('E1','P1',40);
INSERT INTO t3 VALUES  ('E1','P2',20);
INSERT INTO t3 VALUES  ('E1','P3',80);
INSERT INTO t3 VALUES  ('E1','P4',20);
INSERT INTO t3 VALUES  ('E1','P5',12);
INSERT INTO t3 VALUES  ('E1','P6',12);
INSERT INTO t3 VALUES  ('E2','P1',40);
INSERT INTO t3 VALUES  ('E2','P2',80);
INSERT INTO t3 VALUES  ('E3','P2',20);
INSERT INTO t3 VALUES  ('E4','P2',20);
INSERT INTO t3 VALUES  ('E4','P4',40);
INSERT INTO t3 VALUES  ('E4','P5',80);


SELECT * FROM t1;
CREATE UNIQUE INDEX t1_IDX ON t1(EMPNUM);
--sorted_result
SELECT EMPNAME
FROM t1
WHERE EMPNUM IN
   (SELECT EMPNUM
    FROM t3
    WHERE PNUM IN
       (SELECT PNUM
        FROM t2
        WHERE PTYPE = 'Design'));

DROP INDEX t1_IDX ON t1;
CREATE INDEX t1_IDX ON t1(EMPNUM);
--sorted_result
SELECT EMPNAME
FROM t1
WHERE EMPNUM IN
   (SELECT EMPNUM
    FROM t3
    WHERE PNUM IN
       (SELECT PNUM
        FROM t2
        WHERE PTYPE = 'Design'));

DROP INDEX t1_IDX ON t1;
--sorted_result
SELECT EMPNAME
FROM t1
WHERE EMPNUM IN
   (SELECT EMPNUM
    FROM t3
    WHERE PNUM IN
       (SELECT PNUM
        FROM t2
        WHERE PTYPE = 'Design'));

DROP TABLE t1, t2, t3;

#
# BUG#33245 "Crash on VIEW referencing FROM table in an IN clause"
# 
CREATE TABLE t1 (f1 INT NOT NULL);
CREATE VIEW v1 (a) AS SELECT f1 IN (SELECT f1 FROM t1) FROM t1;
SELECT * FROM v1;
drop view v1;
drop table t1;


#
# BUG#35550 "Semi-join subquery in ON clause and no WHERE crashes the server"
#
create table t0 (a int);
insert into t0 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);

create table t1(a int, b int);
insert into t1 values (0,0),(1,1),(2,2);
create table t2 as select * from t1;

create table t3 (pk int, a int, primary key(pk));
insert into t3 select a,a from t0;

explain 
select * from t1 left join t2 on (t2.a= t1.a and t2.a in (select pk from t3));

drop table t0, t1, t2, t3;


#
# BUG#35468 "Slowdown and wrong result for uncorrelated subquery w/o where"
#

create table t0 (a int);
insert into t0 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);
create table t1 (a int) as select A.a + 10 *(B.a + 10*C.a) as a  from t0 A, t0 B, t0 C;
create table t2 (id int, a int, primary key(id), key(a)) as select a as id, a as a  from t1;
show create table t2;
set @a=0;
create table t3 as select * from t2 limit 0;
insert into t3 select @a:=@a+1, t2.a from t2, t0;
insert into t3 select @a:=@a+1, t2.a from t2, t0;
insert into t3 select @a:=@a+1, t2.a from t2, t0;

alter table t3 add primary key(id), add key(a);
--echo The following must use loose index scan over t3, key a:
explain select count(a) from t2 where a in ( SELECT  a FROM t3);
select count(a) from t2 where a in ( SELECT  a FROM t3);

drop table t0,t1,t2,t3;


--echo #
--echo # Bug#33062: subquery in stored routine cause crash
--echo #
CREATE TABLE t1(a INT);
CREATE TABLE t2(c INT);

DELIMITER //;

CREATE PROCEDURE p1(v1 int)
BEGIN
  SELECT 1 FROM t1 WHERE a = v1 AND a IN (SELECT c FROM t2);
END
//

CREATE PROCEDURE p2(v1 int)
BEGIN
  SELECT 1 FROM t1 WHERE a IN (SELECT c FROM t2);
END
//

CREATE PROCEDURE p3(v1 int)
BEGIN
  SELECT 1 
  FROM 
    t1 t01,t1 t02,t1 t03,t1 t04,t1 t05,t1 t06,t1 t07,t1 t08,
    t1 t09,t1 t10,t1 t11,t1 t12,t1 t13,t1 t14,t1 t15,t1 t16,
    t1 t17,t1 t18,t1 t19,t1 t20,t1 t21,t1 t22,t1 t23,t1 t24,
    t1 t25,t1 t26,t1 t27,t1 t28,t1 t29,t1 t30,t1 t31,t1 t32,
    t1 t33,t1 t34,t1 t35,t1 t36,t1 t37,t1 t38,t1 t39,t1 t40,
    t1 t41,t1 t42,t1 t43,t1 t44,t1 t45,t1 t46,t1 t47,t1 t48,
    t1 t49,t1 t50,t1 t51,t1 t52,t1 t53,t1 t54,t1 t55,t1 t56,
    t1 t57,t1 t58,t1 t59,t1 t60
  WHERE t01.a IN (SELECT c FROM t2);
END
//

CREATE PROCEDURE p4(v1 int)
BEGIN
  SELECT 1 
  FROM 
    t1 t01,t1 t02,t1 t03,t1 t04,t1 t05,t1 t06,t1 t07,t1 t08,
    t1 t09,t1 t10,t1 t11,t1 t12,t1 t13,t1 t14,t1 t15,t1 t16,
    t1 t17,t1 t18,t1 t19,t1 t20,t1 t21,t1 t22,t1 t23,t1 t24,
    t1 t25,t1 t26,t1 t27,t1 t28,t1 t29,t1 t30,t1 t31,t1 t32,
    t1 t33,t1 t34,t1 t35,t1 t36,t1 t37,t1 t38,t1 t39,t1 t40,
    t1 t41,t1 t42,t1 t43,t1 t44,t1 t45,t1 t46,t1 t47,t1 t48,
    t1 t49,t1 t50,t1 t51,t1 t52,t1 t53,t1 t54,t1 t55,t1 t56,
    t1 t57,t1 t58,t1 t59,t1 t60
  WHERE t01.a = v1 AND t01.a IN (SELECT c FROM t2);
END
//

DELIMITER ;//

CALL p1(1);
CALL p2(1);
CALL p3(1);
CALL p4(1);

DROP TABLE t1, t2;
DROP PROCEDURE p1;
DROP PROCEDURE p2;
DROP PROCEDURE p3;
DROP PROCEDURE p4;


--echo # 
--echo # Bug#48213 Materialized subselect crashes if using GEOMETRY type
--echo # 

CREATE TABLE t1 (
  pk int,
  a varchar(1),
  b varchar(4),
  c tinyblob,
  d blob,
  e mediumblob,
  f longblob,
  g tinytext,
  h text,
  i mediumtext,
  j longtext,
  k geometry,
  PRIMARY KEY (pk)
);

INSERT INTO t1 VALUES (1,'o','ffff','ffff','ffoo','ffff','ffff','ffff','ffff','ffff','ffff',GeomFromText('POLYGON((0 0, 0 2, 2 2, 2 0, 0 0))')), (2,'f','ffff','ffff','ffff', 'ffff','ffff','ffff','ffff','ffff','ffff',GeomFromText('POLYGON((0 0, 0 2, 2 2, 2 0, 0 0))'));

CREATE TABLE t2 LIKE t1;
INSERT INTO t2 VALUES (1,'i','iiii','iiii','iiii','iiii','ffff','ffff','ffff','ffff','ffff',GeomFromText('POLYGON((0 0, 0 2, 2 2, 2 0, 0 0))')), (2,'f','ffff','ffff','ffff','ffff','ffff','ffff','ffff','ffff','ffff',GeomFromText('POLYGON((0 0, 0 2, 2 2, 2 0, 0 0))'));

# Test that materialization is skipped for semijoins where materialized
# table would contain GEOMETRY or different kinds of BLOB/TEXT columns
let $query=
SELECT pk FROM t1 WHERE (a, b) IN (SELECT a, b FROM t2 WHERE pk > 0);
eval EXPLAIN EXTENDED $query;
eval $query;

let $query=
SELECT pk FROM t1 WHERE (b, c) IN (SELECT b, c FROM t2 WHERE pk > 0);
eval EXPLAIN EXTENDED $query;
eval $query;

let $query=
SELECT pk FROM t1 WHERE (b, d) IN (SELECT b, d FROM t2 WHERE pk > 0);
eval EXPLAIN EXTENDED $query;
eval $query;

let $query=
SELECT pk FROM t1 WHERE (b, e) IN (SELECT b, e FROM t2 WHERE pk > 0);
eval EXPLAIN EXTENDED $query;
eval $query;

let $query=
SELECT pk FROM t1 WHERE (b, f) IN (SELECT b, f FROM t2 WHERE pk > 0);
eval EXPLAIN EXTENDED $query;
eval $query;

let $query=
SELECT pk FROM t1 WHERE (b, g) IN (SELECT b, g FROM t2 WHERE pk > 0);
eval EXPLAIN EXTENDED $query;
eval $query;

let $query=
SELECT pk FROM t1 WHERE (b, h) IN (SELECT b, h FROM t2 WHERE pk > 0);
eval EXPLAIN EXTENDED $query;
eval $query;

let $query=
SELECT pk FROM t1 WHERE (b, i) IN (SELECT b, i FROM t2 WHERE pk > 0);
eval EXPLAIN EXTENDED $query;
eval $query;

let $query=
SELECT pk FROM t1 WHERE (b, j) IN (SELECT b, j FROM t2 WHERE pk > 0);
eval EXPLAIN EXTENDED $query;
eval $query;

let $query=
SELECT pk FROM t1 WHERE (b, k) IN (SELECT b, k FROM t2 WHERE pk > 0);
eval EXPLAIN EXTENDED $query;
eval $query;

DROP TABLE t1, t2;
--echo # End of Bug#48213

--echo # 
--echo # BUG#53060: LooseScan semijoin strategy does not return all rows
--echo # 

CREATE TABLE t1 (i INTEGER);
INSERT INTO t1 VALUES (1), (2), (3), (4), (5);
CREATE TABLE t2 (i INTEGER, j INTEGER, KEY k(i, j));
INSERT INTO t2 VALUES (1, 0), (1, 1), (2, 0), (2, 1);

EXPLAIN
SELECT * FROM t1 WHERE (i) IN (SELECT i FROM t2 where j > 0);
SELECT * FROM t1 WHERE (i) IN (SELECT i FROM t2 where j > 0);

DROP TABLE t1, t2;

--echo # End of BUG#53060
--echo #
--echo # Bug#53305 "Duplicate weedout + join buffer (join cache --level=7,8) loses rows"
--echo #

create table t1 (uid int, fid int, index(uid));
insert into t1 values
  (1,1), (1,2), (1,3), (1,4),
  (2,5), (2,6), (2,7), (2,8),
  (3,1), (3,2), (3,9);

create table t2 (uid int primary key, name varchar(128), index(name));
insert into t2 values 
  (1, "A"), (2, "B"), (3, "C"), (4, "D"), (5, "E"),
  (6, "F"), (7, "G"), (8, "H"), (9, "I");

create table t3 (uid int, fid int, index(uid));
insert into t3 values
  (1,1), (1,2), (1,3),(1,4),
  (2,5), (2,6), (2,7), (2,8),
  (3,1), (3,2), (3,9);

create table t4 (uid int primary key, name varchar(128), index(name));
insert into t4 values 
  (1, "A"), (2, "B"), (3, "C"), (4, "D"), (5, "E"),
  (6, "F"), (7, "G"), (8, "H"), (9, "I");

explain select name from t2, t1 
  where t1.uid in (select t4.uid from t4, t3 where t3.uid=1 and t4.uid=t3.fid)
        and t2.uid=t1.fid;

--sorted_result
select name from t2, t1 
  where t1.uid in (select t4.uid from t4, t3 where t3.uid=1 and t4.uid=t3.fid)
        and t2.uid=t1.fid;

drop table t1,t2,t3,t4;

--echo #
--echo # Bug#43768 Prepared query with nested subqueries core dump on second execution
--echo #

CREATE TABLE t1 (
  id INT PRIMARY KEY,
  partner_id VARCHAR(35)
);

INSERT INTO t1 VALUES
 (1, 'partner1'), (2, 'partner2'),
 (3, 'partner3'), (4, 'partner4');

CREATE TABLE t2 (
  id INT NOT NULL,
  t1_line_id INT,
  article_id VARCHAR(20),
  PRIMARY KEY(id, t1_line_id)
);

INSERT INTO t2 VALUES
 (1, 1, 'sup'), (2, 1, 'sup'),
 (2, 2, 'sup'), (2, 3, 'sup'),
 (2, 4, 'imp'), (3, 1, 'sup'),
 (4, 1, 'sup');

CREATE TABLE t3 (
  user_id VARCHAR(50),
  article_id VARCHAR(20) NOT NULL,
  PRIMARY KEY(user_id)
);

INSERT INTO t3 VALUES('nicke', 'imp');

EXPLAIN
SELECT t1.partner_id
FROM t1
WHERE t1.id IN (
    SELECT t2.id
    FROM t2
    WHERE article_id IN (
      SELECT article_id FROM t3
      WHERE user_id = 'nicke'
    )
  );

SELECT t1.partner_id
FROM t1
WHERE t1.id IN (
    SELECT t2.id
    FROM t2
    WHERE article_id IN (
      SELECT article_id FROM t3
      WHERE user_id = 'nicke'
    )
  );

PREPARE stmt FROM
'EXPLAIN SELECT t1.partner_id
FROM t1
WHERE t1.id IN (
    SELECT t2.id
    FROM t2
    WHERE article_id IN (
      SELECT article_id FROM t3
      WHERE user_id = \'nicke\'
    )
  )';
EXECUTE stmt;
EXECUTE stmt;

PREPARE stmt FROM
'SELECT t1.partner_id
FROM t1
WHERE t1.id IN (
    SELECT t2.id
    FROM t2
    WHERE article_id IN (
      SELECT article_id FROM t3
      WHERE user_id = \'nicke\'
    )
  )';
EXECUTE stmt;
EXECUTE stmt;

DROP TABLE t1,t2,t3;

--echo # End of Bug#43768

--echo #
--echo # Bug#53058 - semijoin execution of subquery with outerjoin yields wrong result
--echo #
CREATE TABLE t1 (i INTEGER);
CREATE TABLE t2 (i INTEGER);
CREATE TABLE t3 (i INTEGER);
INSERT INTO t1 VALUES (1), (2);
INSERT INTO t2 VALUES (6);
INSERT INTO t3 VALUES (1), (2);
explain extended SELECT * FROM t1 WHERE (t1.i) IN 
(SELECT t3.i FROM t3 LEFT JOIN t2 ON t2.i=t3.i);
SELECT * FROM t1 WHERE (t1.i) IN 
(SELECT t3.i FROM t3 LEFT JOIN t2 ON t2.i=t3.i);
drop table t1,t2,t3;

--echo #
--echo # BUG#49453: re-execution of prepared statement with view 
--echo #            and semijoin crashes
--echo #
CREATE TABLE t1 (city VARCHAR(50), country_id INT);
CREATE TABLE t2 (country_id INT, country VARCHAR(50));

INSERT INTO t1 VALUES 
  ('Batna',2),('Bchar',2),('Skikda',2),('Tafuna',3),('Algeria',2) ;
INSERT INTO t2 VALUES (2,'Algeria'),(2,'AlgeriaDup'),(3,'XAmerican Samoa');

CREATE VIEW v1 AS 
  SELECT country_id as vf_country_id
  FROM t2
  WHERE LEFT(country,1) = "A"; 

PREPARE stmt FROM "
SELECT city, country_id
FROM t1
WHERE country_id IN (SELECT vf_country_id FROM v1);
";

--echo
EXECUTE stmt;
EXECUTE stmt;

DROP TABLE t1,t2;
DROP VIEW v1;

--echo #
--echo # Bug#54437 Extra rows with LEFT JOIN + semijoin (firstmatch
--echo # and duplicates weedout)
--echo #
create table t1 (a int);
create table t2 (a int);
create table t3 (a int);
insert into t1 values(1),(1);
insert into t2 values(1),(1),(1),(1);
insert into t3 values(2),(2);

let $query=select * from t1 where t1.a in (select t2.a from t2 left join t3 on t2.a=t3.a);
eval explain $query;
eval $query;

drop table t1,t2,t3;

--echo #
--echo # Bug#55955: crash in MEMORY engine with IN(LEFT JOIN (JOIN))
--echo #
CREATE TABLE t1 (a INT);
CREATE TABLE t2 (a INT);
CREATE TABLE t3 (a INT);
INSERT INTO t1 VALUES(1),(1);
INSERT INTO t2 VALUES(1),(1);
INSERT INTO t3 VALUES(2),(2);

let $query=
SELECT * FROM t1
WHERE t1.a IN (SELECT t2.a
               FROM t2 LEFT JOIN (t2 AS t2inner, t3) ON t2.a=t3.a);
eval explain $query;
eval $query;

DROP TABLE t1,t2,t3;

--echo #
--echo # BUG#52329 - Wrong result: subquery materialization, IN, 
--echo #             non-null field followed by nullable
--echo #

CREATE TABLE t1 (a1 CHAR(8) NOT NULL, a2 char(8) NOT NULL);

CREATE TABLE t2a (b1 char(8), b2 char(8));
CREATE TABLE t2b (b1 CHAR(8), b2 char(8) NOT NULL);
CREATE TABLE t2c (b1 CHAR(8) NOT NULL, b2 char(8));

INSERT INTO t1 VALUES ('1 - 12', '2 - 22');

INSERT INTO t2a VALUES ('1 - 11', '2 - 21'),
                       ('1 - 11', '2 - 21'),
                       ('1 - 12', '2 - 22'),
                       ('1 - 12', '2 - 22'),
                       ('1 - 13', '2 - 23');

INSERT INTO t2b SELECT * FROM t2a;
INSERT INTO t2c SELECT * FROM t2a;

SELECT * FROM t1
WHERE (a1, a2) IN (
      SELECT b1, b2 FROM t2c WHERE b1 > '0' GROUP BY b1, b2);

SELECT * FROM t1
WHERE (a1, a2) IN (
      SELECT b1, b2 FROM t2a WHERE b1 > '0');


SELECT * FROM t1
WHERE (a1, a2) IN (
      SELECT b1, b2 FROM t2b WHERE b1 > '0');


SELECT * FROM t1
WHERE (a1, a2) IN (
      SELECT b1, b2 FROM t2c WHERE b1 > '0');


DROP TABLE t1,t2a,t2b,t2c;

--echo # End BUG#52329

--echo #
--echo # Bug#45174: Incorrectly applied equality propagation caused wrong
--echo # result on a query with a materialized semi-join.
--echo #

CREATE TABLE t1 (
  varchar_nokey varchar(1) NOT NULL
);

INSERT INTO t1 VALUES
 ('v'), ('u'), ('n'), ('l'), ('h'), ('u'), ('n'), ('j'), ('k'),
 ('e'), ('i'), ('u'), ('n'), ('b'), ('x'), (''), ('q'), ('u');

CREATE TABLE t2 (
  pk int NOT NULL,
  varchar_key varchar(1) NOT NULL,
  varchar_nokey varchar(1) NOT NULL,
  PRIMARY KEY(pk),
  KEY varchar_key(varchar_key)
);

INSERT INTO t2 VALUES
 (11,'m','m'), (12,'j','j'), (13,'z','z'), (14,'a','a'), (15,'',''),
 (16,'e','e'), (17,'t','t'), (19,'b','b'), (20,'w','w'), (21,'m','m'),
 (23,'',''), (24,'w','w'), (26,'e','e'), (27,'e','e'), (28,'p','p');

let $query=
SELECT varchar_nokey
FROM t1
WHERE (varchar_nokey, varchar_nokey) IN (SELECT varchar_key, varchar_nokey
                                         FROM t2  
                                         WHERE varchar_nokey < 'n' XOR pk);
eval $query;
eval explain $query;

DROP TABLE t1, t2;

--echo # End of the test for bug#45174.

--echo #
--echo # Bug#50019: Wrong result for IN-query with materialization
--echo #

CREATE TABLE t1(i INT);
INSERT INTO t1 VALUES (1), (2), (3), (4), (5), (6), (7), (8), (9), (10);
CREATE TABLE t2(i INT);
INSERT INTO t2 VALUES (1), (2), (3), (4), (5), (6), (7), (8), (9), (10);
CREATE TABLE t3(i INT);
INSERT INTO t3 VALUES (1), (2), (3), (4), (5), (6), (7), (8), (9), (10);

let $query=
SELECT * FROM t1 WHERE t1.i IN (SELECT t2.i
                                FROM t2 JOIN t3
                                WHERE t2.i + t3.i = 5);
eval $query;
eval explain $query;

DROP TABLE t1,t2,t3;

--echo # End of the test for bug#50019.

--echo #
--echo # Bug#52068: Optimizer generates invalid semijoin materialization plan
--echo #

CREATE TABLE ot1(a INTEGER);
INSERT INTO ot1 VALUES(5), (8);
CREATE TABLE it2(a INTEGER);
INSERT INTO it2 VALUES(9), (5), (1), (8);
CREATE TABLE it3(a INTEGER);
INSERT INTO it3 VALUES(7), (1), (0), (5), (1), (4);
CREATE TABLE ot4(a INTEGER);
INSERT INTO ot4 VALUES(1), (3), (5), (7), (9), (7), (3), (1);

let $query=
SELECT * FROM ot1,ot4
WHERE (ot1.a,ot4.a) IN (SELECT it2.a,it3.a
                        FROM it2,it3);

eval $query;
eval explain $query;

DROP TABLE IF EXISTS ot1, ot4, it2, it3;

--echo # End of the test for bug#52068.

--echo #
--echo # Bug#57623: subquery within before insert trigger causes crash (sj=on)
--echo #

CREATE TABLE ot1(a INT);
CREATE TABLE ot2(a INT);
CREATE TABLE ot3(a INT);
CREATE TABLE it1(a INT);

INSERT INTO ot1 VALUES(0),(1),(2),(3),(4),(5),(6),(7);
INSERT INTO ot2 VALUES(0),(2),(4),(6);
INSERT INTO ot3 VALUES(0),(3),(6);
INSERT INTO it1 VALUES(0),(1),(2),(3),(4),(5),(6),(7);

let $query=
SELECT *
FROM   ot1
     LEFT JOIN
       (ot2 JOIN ot3 on ot2.a=ot3.a)
     ON ot1.a=ot2.a AND ot1.a IN (SELECT a from it1);

eval explain $query;
--sorted_result
eval $query;
eval prepare s from '$query';
--sorted_result
execute s;
--sorted_result
execute s;
deallocate prepare s;

DROP TABLE ot1, ot2, ot3, it1;

--echo # End of the test for bug#57623.

--echo #
--echo # Bug#11766739: Crash in tmp_table_param::init() with semijoin=on
--echo #

CREATE TABLE t1 (f1 INTEGER) ENGINE=MyISAM;
CREATE TABLE t2 (f1 INTEGER, f2 INTEGER) ENGINE=MyISAM;

INSERT INTO t1 VALUES (1);
INSERT INTO t2 VALUES (1,1), (2,1);

let $query=
SELECT * FROM t2
WHERE f2 IN (SELECT t1.f1
             FROM t1 LEFT OUTER JOIN (t2 AS b1 JOIN t2 AS b2 ON TRUE) ON TRUE);
eval EXPLAIN $query;
eval $query;

DROP TABLE t1, t2;

--echo # End of the test for bug#11766739.

--echo #
--echo # Bug#11766642: crash in Item_field::register_field_in_read_map with view
--echo #
CREATE TABLE t1(a INT);
CREATE VIEW v1 AS SELECT a FROM t1;

INSERT INTO t1 VALUES (0),(1),(2);

SELECT a FROM t1 WHERE a IN 
 (SELECT a XOR a FROM v1)
ORDER BY a;

DROP TABLE t1;
DROP VIEW v1;

--echo #
--echo # Bug#12546542 MISSING ROW WHEN USING OPTIMIZER_JOIN_CACHE_LEVEL>=3
--echo #
CREATE TABLE t1 (
  f2 varchar(1024)
);
INSERT INTO t1 VALUES ('v'),('we');
CREATE TABLE t2 (
  col_varchar_1024_utf8 varchar(1024) CHARACTER SET utf8 DEFAULT NULL,
  col_int_key int,
  col_int int
);
INSERT INTO t2 VALUES ('we',4,NULL),('v',1305673728,6);
CREATE TABLE t3 (
  col_int_key int,
  col_int int
);
INSERT INTO t3 VALUES (4,4);

SELECT *
FROM t1
WHERE f2 IN (SELECT a1.col_varchar_1024_utf8 AS f2
             FROM t2 AS a1 LEFT JOIN t3 AS a2
             ON a1.col_int_key = a2.col_int_key
             WHERE a1.col_int BETWEEN 1 AND 10 OR a2.col_int IS NOT NULL);
DROP TABLE t1,t2,t3;

--echo #
--echo # BUG#12616344 - JCL: DIFFERENT RESULT SET AND DIFFERENT AMOUNT
--echo # OF ROWS WHEN JCL>=3
--echo #
# duplicate of Bug#12546542 but still tested as query is different
CREATE TABLE t1 (col_int_nokey int, col_int_key int, col_varchar_key varchar(1));
INSERT INTO t1 VALUES (0,4,'c'),(1,6,'u');
CREATE TABLE t2 (pk int, col_int_nokey int, col_varchar_nokey varchar(1));
INSERT INTO t2 VALUES (1,4,'b'),(94,6,'u');
CREATE TABLE t3 (pk int, col_int_nokey int, col_varchar_key varchar(1));
INSERT INTO t3 VALUES (1,4,'j'),(2,6,'v');
SELECT table2.col_int_key
from t3 as table1 join t1 as table2 on table2.col_int_nokey 
where table1.col_int_nokey in 
( 
 select subquery2_t2.col_int_nokey 
 from t3 as subquery2_t1 
      right join
        t2 as subquery2_t2
        join t1 as subquery2_t3
        on subquery2_t3.col_int_key = subquery2_t2.col_int_nokey
      on subquery2_t3.col_varchar_key = subquery2_t2.col_varchar_nokey 
 where subquery2_t1.col_varchar_key != table1.col_varchar_key
       or subquery2_t2.pk <= table1.pk
);

DROP TABLE t1,t2,t3;

--echo #
--echo # Bug#12608157: ASSERT IN FIELD_LONG::VAL_INT WHEN USING MEMORY ENGINE
--echo #

CREATE TABLE t1 (i1 int);
INSERT INTO t1 VALUES (1);

CREATE TABLE t2 (i1 int, i2 int) ENGINE=memory;
INSERT INTO t2 VALUES (1, 2),(7, 3);

SELECT GRANDPARENT1.i1
FROM t2 AS GRANDPARENT1 
WHERE GRANDPARENT1.i2
      IN ( SELECT PARENT1.i2
           FROM t2 AS PARENT1 JOIN t1 AS PARENT2 ON (PARENT1.i1 = PARENT2.i1)
           WHERE 
              GRANDPARENT1.i1 IN ( SELECT CHILD1.i1 FROM t2 AS CHILD1 )
           ORDER BY PARENT1.i1)
ORDER BY GRANDPARENT1.i2 ;

DROP TABLE t1,t2;

--echo #
--echo # Bug#12640083: Same query executed as WHERE subquery gives different
--echo #              results on IN() compare
--echo #

CREATE TABLE t1 (
  pk int NOT NULL,
  col_varchar_1024_utf8_key varchar(1024) CHARACTER SET utf8 DEFAULT NULL,
  col_varchar_10_latin1_key varchar(10) DEFAULT NULL,
  PRIMARY KEY (pk),
  KEY col_varchar_1024_utf8_key(col_varchar_1024_utf8_key(333)),
  KEY col_varchar_10_latin1_key(col_varchar_10_latin1_key)
);

INSERT INTO t1 VALUES
(1, 'a', 'a'),
(2, 'ab', 'ab'),
(3, 'abc', 'abc'),
(4, 'abcd', 'abcd');

CREATE TABLE t2 (
  pk int NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (pk)
) ENGINE=Innodb;

CREATE TABLE t3
SELECT alias1.col_varchar_10_latin1_key
FROM t1 AS alias1
     LEFT JOIN t1 AS alias2
       JOIN t2 AS alias3
       ON alias2.col_varchar_10_latin1_key
     ON alias1.col_varchar_1024_utf8_key
WHERE alias1.pk AND alias1.pk < 3 OR alias1.pk AND alias3.pk; 

let $query=
SELECT * 
FROM t3 
WHERE col_varchar_10_latin1_key IN (
    SELECT alias1.col_varchar_10_latin1_key 
    FROM t1 AS alias1
         LEFT JOIN t1 AS alias2
           JOIN t2 AS alias3
           ON alias2.col_varchar_10_latin1_key
         ON alias1.col_varchar_1024_utf8_key
    WHERE alias1.pk AND alias1.pk < 3 OR alias1.pk AND alias3.pk);

eval EXPLAIN $query;
eval $query;

DROP TABLE t1, t2, t3;

--echo # End of the test for bug#12640083.

--echo #
--echo # Bug#12603200 - Assert in QUICK_INDEX_MERGE_SELECT::need_sorted_output
--echo #

CREATE TABLE t1 (
  pk int NOT NULL,
  col_int_key int NOT NULL,
  col_varchar_nokey varchar(1) NOT NULL,
  col_varchar_key varchar(1) NOT NULL,
  PRIMARY KEY(pk),
  KEY col_int_key(col_int_key),
  KEY col_varchar_key(col_varchar_key, col_int_key)
) engine=innodb;

INSERT INTO t1 VALUES
(1,7,'a','a'),
(2,0,'v','v'),
(3,9,'c','c'),
(4,3,'m','m'),
(5,2,'a','a'),
(6,1,'d','d'),
(7,8,'y','y'),
(8,6,'t','t'),
(11,7,'a','x'),
(12,0,'v','v'),
(13,9,'c','c'),
(14,3,'m','m'),
(15,2,'a','x'),
(16,1,'d','d'),
(17,8,'y','y'),
(18,6,'t','u');

CREATE TABLE t2 (
  pk int NOT NULL,
  col_int_key int NOT NULL,
  col_varchar_key varchar(1) NOT NULL,
  PRIMARY KEY(pk),
  KEY col_varchar_key(col_varchar_key, col_int_key)
) engine=innodb;

INSERT INTO t2(pk,col_int_key,col_varchar_key) VALUES
(8,7,'c'),
(11,4,'l'),
(12,7,'b'),
(13,0,'c'),
(14,2,'i'),
(15,9,'h'),
(16,4,'q'),
(17,1,'m'),
(18,9,'b'),
(19,2,'e'),
(20,1,'c'),
(21,7,'z'),
(22,4,'l'),
(23,7,'z'),
(24,0,'c'),
(25,2,'i'),
(26,9,'h'),
(27,4,'q'),
(28,0,'a'),
(29,1,'d');

let $query=
SELECT outr.col_varchar_key AS x, outr.pk AS y
FROM t1 AS outr
WHERE outr.col_varchar_key IN (SELECT innr.col_varchar_key
                               FROM t2 AS innr
                               WHERE innr.col_varchar_key = 'a' OR innr.pk = 8)
  AND outr.col_varchar_nokey < 't'
ORDER BY outr.col_varchar_key, outr.pk;

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
ANALYZE TABLE t2;
-- enable_result_log
-- enable_query_log

eval EXPLAIN $query;
eval $query;

DROP TABLE t1, t2;

--echo # End of bug#12603200

--echo #
--echo # Bug#12603183: Segfault in hp_movelink
--echo #

CREATE TABLE t1 (
  col_varchar_key varchar(1) ,
  col_varchar_nokey varchar(1) ,
  KEY col_varchar_key(col_varchar_key)
);

INSERT INTO t1 VALUES
('i','i'),
('h','h'),
('q','q'),
('a','a'),
('v','v'),
('u','u'),
('s','s'),
('y','y'),
('z','z'),
('h','h'),
('p','p'),
('e','e'),
('i','i'),
('y','y'),
('w','w');

CREATE TABLE t2 (
  col_varchar_nokey varchar(1)
);

INSERT INTO t2 VALUES
('b');

let $query=
SELECT grandparent1.col_varchar_nokey
FROM t1 AS grandparent1 LEFT JOIN t2 AS grandparent2 USING (col_varchar_nokey)
WHERE (grandparent1.col_varchar_key) IN
   (SELECT parent1.col_varchar_nokey
    FROM t1 AS parent1
    WHERE parent1.col_varchar_key IN
       (SELECT child1.col_varchar_nokey AS c1
        FROM t1 AS child1 LEFT JOIN t2 AS child2
                ON (child1.col_varchar_key > child2.col_varchar_nokey)));

eval EXPLAIN $query;
--sorted_result
eval $query;

DROP TABLE t1, t2;

--echo # End of test for bug#12603183.

--echo #
--echo # Bug#12818569: Diff nr of rows returned when using IN/ALL+subquery
--echo #

CREATE TABLE t1 (
 col_int_key INT NOT NULL,
 col_datetime_key DATETIME NOT NULL,
 col_varchar_key VARCHAR(1) NOT NULL,
 KEY col_int_key (col_int_key),
 KEY col_datetime_key(col_datetime_key),
 KEY col_varchar_key (col_varchar_key,col_int_key)
) ENGINE=InnoDB;

INSERT INTO t1 VALUES
 (7,'2004-06-06 04:22:12','v'), (0,'2005-11-13 01:12:31','s'),
 (9,'2002-05-04 01:50:00','l'), (3,'2004-10-27 10:28:45','y'),
 (4,'2006-07-22 05:24:23','c'), (2,'2002-05-16 21:34:03','i'),
 (5,'2008-04-17 10:45:30','h'), (3,'2009-04-21 02:58:02','q'),
 (1,'2008-01-11 11:01:51','a'), (3,'1900-01-01 00:00:00','v'),
 (6,'2007-05-17 18:24:57','u'), (7,'2007-08-07 00:00:00','s'),
 (5,'2001-08-28 00:00:00','y'), (1,'2004-04-16 00:27:28','z'),
 (204,'2005-05-03 07:06:22','h'), (224,'2009-03-11 17:09:50','p'),
 (9,'2007-12-08 01:54:28','e'), (5,'2009-07-28 18:19:54','i'),
 (0,'2008-06-08 00:00:00','y'), (3,'2005-02-09 09:20:26','w');

CREATE TABLE t2 (
 col_varchar_nokey VARCHAR(1) NOT NULL
) ENGINE=InnoDB;

INSERT INTO t2 VALUES ('v'), ('y'), ('j'), ('c'), ('d'), ('r');

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
ANALYZE TABLE t2;
-- enable_result_log
-- enable_query_log

let $query=
SELECT col_varchar_key
FROM t1
WHERE col_varchar_key IN (SELECT col_varchar_nokey
                          FROM t2)
ORDER BY col_datetime_key LIMIT 4;

eval explain $query;
eval $query;

DROP TABLE t1, t2;

--echo # End of test for bug#12818569.

--echo #
--echo # Bug#12803439: Assert in replace_subcondition() on update query
--echo #

CREATE TABLE t1(a INTEGER);

INSERT INTO t1 values(1), (2);

CREATE TABLE t2(a INTEGER);

INSERT INTO t2 VALUES(1), (3);

SELECT *
FROM t1
WHERE a IN (SELECT a
            FROM t2
            HAVING a IN (SELECT a
                         FROM t2)
            )
HAVING a IN (SELECT a
             FROM t2);

DROP TABLE t1, t2;

--echo # End of test for bug#12803439.

--echo #
--echo # Bug#12797534: Segfault in hp_movelink still exists
--echo #

CREATE TABLE t1 (
 g1 VARCHAR(1) NOT NULL
) ENGINE=InnoDB;

INSERT INTO t1 VALUES ('d'), ('s');

CREATE TABLE t2 (
 pk INT NOT NULL,
 col_int_key INT NOT NULL,
 col_varchar_key VARCHAR(1) NOT NULL,
 col_varchar_nokey VARCHAR(1) NOT NULL,
 PRIMARY KEY (pk),
 KEY col_varchar_key(col_varchar_key, col_int_key)
) ENGINE=InnoDB;

INSERT INTO t2 VALUES
 (1,4,'j','j'), (2,6,'v','v'), (3,3,'c','c'), (4,5,'m','m'),
 (5,3,'d','d'), (6,246,'d','d'), (7,2,'y','y'), (8,9,'t','t'),
 (9,3,'d','d'), (10,8,'s','s'), (11,1,'r','r'), (12,8,'m','m'),
 (13,8,'b','b'), (14,5,'x','x'), (15,7,'g','g'), (16,5,'p','p'),
 (17,1,'q','q'), (18,6,'w','w'), (19,2,'d','d'), (20,9,'e','e');

CREATE TABLE t3 (
 pk INTEGER NOT NULL,
 PRIMARY KEY (pk)
) ENGINE=InnoDB;

INSERT INTO t3 VALUES (10);

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
ANALYZE TABLE t2;
ANALYZE TABLE t3;
ANALYZE TABLE grandparent1;
ANALYZE TABLE parent1;
-- enable_result_log
-- enable_query_log

let $query=
SELECT *
FROM t1
WHERE g1 NOT IN
   (SELECT  grandparent1.col_varchar_nokey AS g1
    FROM t2 AS grandparent1
    WHERE grandparent1.col_varchar_key IN
       (SELECT parent1.col_varchar_nokey AS p1
        FROM t2 AS parent1 LEFT JOIN t3 AS parent2 USING (pk)
        )
      AND grandparent1.col_varchar_key IS NOT NULL
    );

eval EXPLAIN $query;
eval $query;

DROP TABLE t1, t2, t3;

CREATE TABLE t1 (
 pk INTEGER AUTO_INCREMENT,
 col_int_key INTEGER ,
 col_varchar_key VARCHAR(1) ,
 col_varchar_nokey VARCHAR(1) ,
 PRIMARY KEY (pk),
 KEY (col_varchar_key,col_int_key)
 ) ENGINE=INNODB;
INSERT INTO t1 (col_int_key,col_varchar_key,col_varchar_nokey) VALUES
 (0,'x','x'), (1,'j','j'), (1,'r','r'), (9,'v','v'), (5,'r','r');

CREATE TABLE t2 (
 pk INTEGER AUTO_INCREMENT,
 col_int_key INTEGER ,
 col_varchar_key VARCHAR(1) ,
 col_varchar_nokey VARCHAR(1) ,
 PRIMARY KEY (pk),
 KEY (col_int_key),
 KEY (col_varchar_key,col_int_key)
 ) AUTO_INCREMENT=10 ENGINE=INNODB;
INSERT INTO t2 (col_int_key, col_varchar_key, col_varchar_nokey) VALUES
 (NULL,'x','x'), (NULL,'j','j'), (8,'c','c');

CREATE TABLE t3
SELECT outr.col_varchar_nokey AS x
FROM t1 AS outr
WHERE outr.col_varchar_nokey IN
   (SELECT innr.col_varchar_nokey AS y
    FROM t2 AS innr
    WHERE innr.col_int_key IS NULL)
  AND outr.col_varchar_nokey IS NOT NULL
  AND NOT col_varchar_key IS NULL;

SELECT *
FROM t3
WHERE x NOT IN
   (SELECT outr.col_varchar_nokey AS x
    FROM t1 AS outr
    WHERE outr.col_varchar_nokey IN
       (SELECT innr.col_varchar_nokey AS y
        FROM t2 AS innr
        WHERE innr.col_int_key IS NULL)
      AND outr.col_varchar_nokey IS NOT NULL
      AND NOT col_varchar_key IS NULL);

DROP TABLE t1, t2, t3;

--echo # End of test for bug#12797534.

--echo #
--echo # Bug#12714094: Assert in optimize_semijoin_nests()
--echo #

CREATE TABLE it (
  pk int NOT NULL,
  col_varchar VARCHAR(10) DEFAULT NULL,
  PRIMARY KEY (pk)
) ENGINE=MyISAM;

INSERT INTO it VALUES (1, 'g');

CREATE TABLE ot
  SELECT alias1.pk AS field1
  FROM it AS alias1
    LEFT JOIN it AS alias2
    ON alias1.col_varchar = alias2.col_varchar
;
SELECT *
FROM ot
WHERE field1 IN (
  SELECT alias1.pk
  FROM it AS alias1
    LEFT JOIN it AS alias2
    ON alias1.col_varchar = alias2.col_varchar
);

DROP TABLE it, ot;

--echo # End of test for bug#12714094

--echo #
--echo # Bug#12867557: Valgrind: conditional jump/move at key_cmp
--echo #

CREATE TABLE t1 (
 pk INTEGER AUTO_INCREMENT,
 col_int_key INTEGER,
 PRIMARY KEY (pk),
 KEY (col_int_key)
) AUTO_INCREMENT=10;

INSERT INTO t1 (col_int_key) VALUES (8);

CREATE TABLE t2 (
 pk INTEGER AUTO_INCREMENT,
 col_int_key INTEGER,
 col_time_key TIME,
 PRIMARY KEY (pk),
 KEY (col_int_key),
 KEY (col_time_key)
)  AUTO_INCREMENT=10;

INSERT INTO t2 (col_int_key, col_time_key)
VALUES
 (8, '22:55:23.019225'), (7, '10:19:31.050677'), (1, '14:40:36.038608'),
 (7, '04:37:47.062416'), (9, '19:34:06.054514'), (NULL,'20:35:33.022996'),
 (1, NULL), (9, '14:43:37.057393'), (2, '02:23:09.043438'),
 (9, '01:22:45.041064'), (2, '00:00:00'), (4, '00:13:25.038482'),
 (0, '03:47:16.042671'), (4, '01:41:48.007423'), (8, '00:00:00'),
 (NULL, '22:32:04.047407'), (NULL, '16:44:14.028443'), (0, '17:38:37.059754'),
 (NULL, '08:46:48.042388'), (8, '14:11:27.044095');

CREATE TABLE t0
SELECT DISTINCT grandparent1.col_time_key AS g1
FROM t2 AS grandparent1
WHERE grandparent1.col_int_key IN
   (SELECT parent1.col_int_key AS p1
    FROM t1 AS parent1)
 AND grandparent1.pk > 9;

UPDATE t0
SET g1 = g1
WHERE g1 IN
   (SELECT grandparent1.col_time_key AS g1
    FROM t2 AS grandparent1
    WHERE grandparent1.col_int_key IN
       (SELECT parent1.col_int_key AS p1
        FROM t1 AS parent1)
      AND grandparent1.pk > 9);

DROP TABLE t0, t1, t2;

--echo # End of test for bug#12867557

--echo #
--echo # Bug#12711441: crash in fix_after_pullout
--echo #

CREATE TABLE t1 (
  pk int NOT NULL,
  col_int_nokey int DEFAULT NULL,
  col_int_key int DEFAULT NULL,
  col_time_key time DEFAULT NULL,
  col_varchar_key varchar(1) DEFAULT NULL,
  PRIMARY KEY (pk)
);

CREATE VIEW v1 AS SELECT * FROM t1;

CREATE TABLE t2 (
  col_int_key int DEFAULT NULL,
  col_varchar_key varchar(1) DEFAULT NULL,
  col_varchar_nokey varchar(1) DEFAULT NULL,
  KEY col_varchar_key(col_varchar_key, col_int_key)
);

CREATE TABLE t3 (
  pk int NOT NULL,
  col_int_key INT DEFAULT NULL,
  PRIMARY KEY (pk)
);

CREATE TABLE t4 (
  col_int_nokey INT DEFAULT NULL,
  col_varchar_key varchar(1) DEFAULT NULL,
  col_varchar_nokey varchar(1) DEFAULT NULL,
  KEY col_varchar_key(col_varchar_key)
);

CREATE TABLE ts
  SELECT alias1.col_time_key AS field1
  FROM v1 AS alias1
       RIGHT JOIN t3 AS alias2
       ON alias2.col_int_key = alias1.col_int_nokey
  WHERE alias1.pk >= SOME(
    SELECT SQ1_alias1.pk AS SQ1_field1
    FROM t3 AS SQ1_alias1
         INNER JOIN (t2 AS SQ1_alias2
           INNER JOIN t4 AS SQ1_alias3
           ON SQ1_alias3.col_varchar_key = SQ1_alias2.col_varchar_nokey)
         ON SQ1_alias3.col_int_nokey = SQ1_alias2.col_int_key
    WHERE SQ1_alias2.col_varchar_key <= alias1.col_varchar_key
      AND SQ1_alias3.col_varchar_nokey <> alias1.col_varchar_key)
;

SELECT * FROM ts WHERE field1 IN (
  SELECT alias1.col_time_key AS field1
  FROM v1 AS alias1
       RIGHT JOIN t3 AS alias2
       ON alias2.col_int_key = alias1.col_int_nokey
  WHERE alias1.pk >= SOME(
    SELECT SQ1_alias1.pk AS SQ1_field1
    FROM t3 AS SQ1_alias1
         INNER JOIN (t2 AS SQ1_alias2
           INNER JOIN t4 AS SQ1_alias3
           ON SQ1_alias3.col_varchar_key = SQ1_alias2.col_varchar_nokey)
          ON SQ1_alias3.col_int_nokey = SQ1_alias2.col_int_key
    WHERE SQ1_alias2.col_varchar_key <= alias1.col_varchar_key
      AND SQ1_alias3.col_varchar_nokey <> alias1.col_varchar_key)
);

DROP TABLE t1, t2, t3, t4, ts;
DROP VIEW v1;

--echo # End of test for bug#12711441.

--echo #
--echo # Bug#12664936: Same query executed as where subquery ...
--echo #

CREATE TABLE t1 (
  col_varchar_key VARCHAR(1),
  KEY col_varchar_key (col_varchar_key)
);

INSERT INTO t1 VALUES
 ('o'), ('w'), ('m'), ('q'),
 ('f'), ('p'), ('j'), ('c');

CREATE TABLE t2 (
  col_int_nokey INTEGER,
  col_int_key INTEGER,
  col_varchar_key varchar(1),
  KEY col_int_key (col_int_key)
);

INSERT INTO t2 VALUES
 (8,5,'u'),(4,5,'p'),(8,1,'o'),(NULL,7,'v'),
 (1,2,'g'),(2,1,'q'),(NULL,7,'l'),(3,1,'n');

CREATE TABLE t4
SELECT t2.col_int_nokey, t2.col_varchar_key
FROM t1 JOIN t2 ON t2.col_varchar_key = t1.col_varchar_key
WHERE t2.col_int_key = 1;

let $query=
SELECT *
FROM t4
WHERE (col_int_nokey, col_varchar_key) IN
   (SELECT t2.col_int_nokey, t2.col_varchar_key
    FROM t1 JOIN t2 ON t2.col_varchar_key = t1.col_varchar_key
    WHERE t2.col_int_key = 1
);

eval EXPLAIN $query;
eval $query;

DROP TABLE t1, t2, t4;

--echo # End of test for bug#12664936.

--echo #
--echo # Bug#13340270: assertion table->sort.record_pointers == __null
--echo #

CREATE TABLE t1 (
  pk int NOT NULL,
  col_int_key int DEFAULT NULL,
  col_varchar_key varchar(1) DEFAULT NULL,
  col_varchar_nokey varchar(1) DEFAULT NULL,
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key),
  KEY col_varchar_key (col_varchar_key, col_int_key)
) ENGINE=InnoDB;

INSERT INTO t1 VALUES
(10,8,'x','x'),
(11,7,'d','d'),
(12,1,'r','r'),
(13,7,'f','f'),
(14,9,'y','y'),
(15,NULL,'u','u'),
(16,1,'m','m'),
(17,9,NULL,NULL),
(18,2,'o','o'),
(19,9,'w','w'),
(20,2,'m','m'),
(21,4,'q','q');

let $query=
  SELECT alias1.col_varchar_nokey AS field1
  FROM t1 AS alias1 JOIN t1 AS alias2
    ON alias2.col_int_key = alias1.pk OR
       alias2.col_int_key = alias1.col_int_key
  WHERE alias1.pk = 58 OR alias1.col_varchar_key = 'o'
;

eval CREATE TABLE t2
  $query
;

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
ANALYZE TABLE t2;
-- enable_result_log
-- enable_query_log

eval EXPLAIN SELECT *
FROM t2
WHERE (field1) IN ($query);

eval SELECT *
FROM t2
WHERE (field1) IN ($query);

DROP TABLE t1, t2;

--echo # End of test for bug#13340270.

--echo #
--echo # Bug#13335319: Seg fault when analyzing FirstMatch semi-join strategy
--echo #

CREATE TABLE ot1(a INTEGER);

INSERT INTO ot1 VALUES(1), (2), (3);

CREATE TABLE ot2(a INTEGER);

INSERT INTO ot2 VALUES(1), (2), (4), (6), (8), (10);

CREATE TABLE it1(a INTEGER);

INSERT INTO it1 VALUES(1), (3), (5), (7);

CREATE TABLE it2(a INTEGER);

INSERT INTO it2 VALUES(1), (3), (5), (7), (9);

let $query=
SELECT ot1.a, ot2.a
FROM ot1, ot2
WHERE ot1.a IN (SELECT a FROM it1) AND
      ot2.a IN (SELECT a FROM it2);

eval explain $query;
eval $query;

DROP TABLE ot1, ot2, it1, it2;

--echo # End of test for bug#13335319.

--echo #
--echo # Bug#13334882: Assertion keypart_map failed in MyIsam function
--echo #

CREATE TABLE t1 (
  pk int NOT NULL,
  col_int_nokey INT NOT NULL,
  col_int_key INT NOT NULL,
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key)
) ENGINE=MyISAM;

INSERT INTO t1 VALUES
(1,4,0),
(2,6,8),
(3,3,1),
(7,2,6),
(8,9,1),
(9,3,6),
(10,8,2),
(11,1,4),
(12,8,8),
(13,8,4),
(14,5,4);

CREATE TABLE t2 (
  pk int NOT NULL,
  col_int_nokey int NOT NULL,
  col_int_key int NOT NULL,
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key)
) ENGINE=MyISAM;

INSERT INTO t2 VALUES
(10,8,7);

CREATE TABLE t3
SELECT grandparent1.col_int_nokey AS g1
FROM t1 AS grandparent1
WHERE (grandparent1.col_int_nokey, grandparent1.col_int_key) IN
   (SELECT parent1.col_int_key AS p1,
           parent1.col_int_key AS p2
    FROM t1 AS parent1
      LEFT JOIN t2 AS parent2
      ON parent1.col_int_nokey = parent2.col_int_key
    )
  AND grandparent1.col_int_key <> 3
;

let $query=
SELECT * FROM t3
WHERE g1 NOT IN
   (SELECT grandparent1.col_int_nokey AS g1
    FROM t1 AS grandparent1
    WHERE (grandparent1.col_int_nokey, grandparent1.col_int_key) IN
       (SELECT parent1.col_int_key AS p1,
               parent1.col_int_key AS p2
        FROM t1 AS parent1
        LEFT JOIN t2 AS parent2
        ON parent1.col_int_nokey = parent2.col_int_key
        )
      AND grandparent1.col_int_key <> 3
);

eval explain $query;
eval explain format=json $query;
eval $query;

DROP TABLE t1, t2, t3;

--echo # End of test for bug#13334882.

--echo #
--echo # Bug#13339643: Assertion on JOIN::flatten_subqueries on second execution
--echo #

CREATE TABLE t1 (
  col_int_nokey INT,
  col_varchar_nokey VARCHAR(1)
);

INSERT INTO t1 VALUES
 (1,'o'),
 (2,'t');

CREATE TABLE t2 LIKE t1;

INSERT INTO t2 VALUES
 (1,'o'),
 (4,'f');

CREATE VIEW v_t2 AS SELECT * FROM t2;

CREATE TABLE t3 LIKE t1;

INSERT INTO t3 VALUES
 (1,'o'),
 (4,'f');

let $query=
SELECT alias1.col_varchar_nokey
FROM t1 AS alias1
     INNER JOIN v_t2 AS alias2
     ON alias2.col_int_nokey = alias1.col_int_nokey AND
          'o' IN (SELECT col_varchar_nokey
                  FROM t3);
eval explain $query;
eval $query;

eval PREPARE stmt FROM "$query";
EXECUTE stmt;

DROP VIEW v_t2;
DROP TABLE t1, t2, t3;
--echo # End of test for bug#13339643.

--echo #
--echo # Bug#13424134: Wrong result on JOIN + nested WHERE ... IN clauses
--echo #

CREATE TABLE t1 (
  pk int NOT NULL,
  col_int_nokey int NOT NULL,
  col_int_key int NOT NULL,
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key)
) ENGINE=MyIsam;

INSERT INTO t1 VALUES
 (10,1,7), (13,7,3), (18,0,1), (23,8,1);

CREATE TABLE t2 (
  pk int NOT NULL,
  col_int_key int NOT NULL,
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key)
) ENGINE=MyIsam;

INSERT INTO t2 VALUES (1,7);

let $query=
SELECT t1a.*
FROM t1 AS t1a
  JOIN t1 AS t1b USING ( col_int_nokey )
WHERE t1a.col_int_key IN (
  SELECT pk
  FROM t2
  WHERE col_int_key IN (
    SELECT col_int_nokey
    FROM t1
  )
);

eval EXPLAIN $query;
eval $query;

ALTER TABLE t1 ENGINE=Innodb;
ALTER TABLE t2 ENGINE=Innodb;

eval $query;

DROP TABLE t1, t2;

--echo # End of test for bug#13424134.

--echo #
--echo # Bug#13414014: Extra rows in result on semijoin query with where ...
--echo #

CREATE TABLE t1 (
  c INT,
  d INT,
  a VARCHAR(1),
  b VARCHAR(1),
  KEY a (a)
);

INSERT INTO t1 VALUES
 (NULL,8,'x','x'), (7,4,'q','q'), (6,8,'c','c');

CREATE TABLE t2 (
  a VARCHAR(1),
  KEY a (a)
);

INSERT INTO t2 VALUES
 ('c'), (NULL), ('x'), ('q');

let $query=
SELECT *
FROM t2 AS ot
WHERE (a, a) IN
   (SELECT a, b
    FROM t1 AS it
    WHERE it.a = 'x' OR it.c > it.d
   )
;

eval explain $query;
eval $query;

DROP TABLE t1, t2;

--echo # End of test for bug#13414014.

--echo #
--echo # Bug#13545215: Missing rows on nested in-subquery with materialization
--echo #

CREATE TABLE t1 (
  col_int_key int,
  col_varchar_key varchar(1),
  col_varchar_nokey varchar(1),
  KEY col_int_key (col_int_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
) ;

INSERT INTO t1 VALUES
 (8,'x','x'), (0,'p','p'), (8,'c','c');

CREATE TABLE t2 (
  pk int NOT NULL,
  col_varchar_key varchar(1),
  col_varchar_nokey varchar(1),
  PRIMARY KEY (pk),
  KEY col_varchar_key (col_varchar_key)
);

INSERT INTO t2 VALUES
 (1,'v','v'), (2,'v','v'), (3,'c','c'), (4,NULL,NULL),
 (5,'x','x'), (6,'i','i'), (7,'e','e'), (8,'p','p');

CREATE TABLE t3 (
 col_int_nokey int
);

INSERT INTO t3 VALUES (7);

let $query=
SELECT grandparent1.col_varchar_nokey
FROM t1 AS grandparent1 JOIN t1 AS grandparent2 USING (col_int_key)
WHERE grandparent1.col_varchar_key IN (
    SELECT col_varchar_nokey
    FROM t2 AS parent1
    WHERE col_varchar_key IN (
        SELECT child1.col_varchar_nokey
        FROM t2 AS child1 LEFT JOIN t3 AS child2
             ON child1.pk < child2.col_int_nokey
        )
    );

eval explain $query;
--sorted_result
eval $query;

DROP TABLE t1, t2, t3;

--echo # End of test for bug#13545215.

--echo #
--echo # BUG#13553211 - MISSING ROWS ON SELECT WITH IN-SUBQUERY AND
--echo # MATERIALIZATION + SEMIJOIN ON
--echo #
CREATE TABLE t1 (
  col_int_key int(11) DEFAULT NULL,
  col_varchar_key varchar(1) DEFAULT NULL,
  col_varchar_nokey varchar(1) DEFAULT NULL,
  KEY col_int_key (col_int_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
);

INSERT INTO t1 VALUES (4,'v','v');
INSERT INTO t1 VALUES (62,'v','v');
INSERT INTO t1 VALUES (7,'c','c');
INSERT INTO t1 VALUES (1,NULL,NULL);

let $query=
SELECT
  alias1.col_varchar_nokey AS a1_nokey,
  alias1.col_varchar_key AS a1_key,
  alias2.col_varchar_nokey AS a2_nokey
FROM
  t1 AS alias1, t1 AS alias2
WHERE
  (alias1.col_varchar_nokey,alias2.col_varchar_nokey)
  IN
  (
    SELECT
      SQ2_alias2.col_varchar_nokey, SQ2_alias1.col_varchar_key
    FROM
      t1 AS SQ2_alias1, t1 AS SQ2_alias2
  )
;

eval EXPLAIN $query;
--sorted_result
eval $query;

DROP TABLE t1;

--echo #
--echo # Bug#13541406: Wrong result with loosescan on select .. where .. in
--echo #

CREATE TABLE t1 (
  col_int_key INT NOT NULL,
  col_varchar_nokey VARCHAR(1) NOT NULL,
  KEY col_int_key (col_int_key)
) ENGINE=InnoDB;

INSERT INTO t1 VALUES
 (7,'v'), (0,'s'), (9,'l'), (3,'y'), (4,'c'), (2,'i'), (5,'h'), (3,'q'),
 (1,'a'), (3,'v'), (6,'u'), (7,'s'), (5,'y'), (1,'z'), (204,'h'), (224,'p'),
 (9,'e'), (5,'i'), (0,'y'), (3,'w');

CREATE TABLE t2 (
  pk INT NOT NULL,
  col_int_key INT NOT NULL,
  col_varchar_key VARCHAR(1) NOT NULL,
  col_varchar_nokey VARCHAR(1) NOT NULL,
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
) ENGINE=InnoDB;

INSERT INTO t2 VALUES
 (1,0,'j','j'), (2,8,'v','v'), (3,1,'c','c'), (4,8,'m','m'),
 (5,9,'d','d'), (6,24,'d','d'), (7,6,'y','y'), (8,1,'t','t'),
 (9,6,'d','d'), (10,2,'s','s'), (11,4,'r','r'), (12,8,'m','m'),
 (13,4,'b','b'), (14,4,'x','x'), (15,7,'g','g'), (16,4,'p','p'),
 (17,1,'q','q'), (18,9,'w','w'), (19,4,'d','d'), (20,8,'e','e');

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
ANALYZE TABLE t2;
-- enable_result_log
-- enable_query_log

let $query=
SELECT ot1.col_int_key AS field1
FROM t2 AS ot1, t2 AS ot2
WHERE (ot1.col_varchar_key, ot2.col_varchar_nokey) IN (
    SELECT it2.col_varchar_nokey, it1.col_varchar_key
    FROM t2 AS it1 JOIN t1 AS it2 ON it2.col_int_key = it1.pk);

--echo # This query should never use a LooseScan strategy

eval explain $query;
--sorted_result
eval $query;

DROP TABLE t1, t2;

--echo # End of test for bug#13541406.

--echo #
--echo # Bug#13576391: Missing rows on select with in-subquery and
--echo # batched-key-access=on and semijoin
--echo #

CREATE TABLE t1 (
 col_int_nokey int NOT NULL,
 col_varchar_key varchar(1) NOT NULL,
 KEY col_varchar_key (col_varchar_key)
) engine=InnoDB;

INSERT INTO t1 VALUES
 (1,'v'), (7,'s'), (4,'l'), (7,'y'), (0,'c'), (2,'i'), (9,'h'), (4,'q'),
 (0,'a'), (9,'v'), (1,'u'), (3,'s'), (8,'y'), (8,'z'), (18,'h'), (84,'p'),
 (6,'e'), (3,'i'), (6,'y'), (6,'w');

CREATE TABLE t2 (
 col_int_nokey int NOT NULL,
 col_varchar_nokey varchar(1) NOT NULL
) engine=InnoDB;

INSERT INTO t2 VALUES
 (4,'j'), (6,'v'), (3,'c'), (5,'m'), (3,'d'), (246,'d'), (2,'y'), (9,'t'),
 (3,'d'), (8,'s'), (1,'r'), (8,'m'), (8,'b'), (5,'x'), (7,'g'), (5,'p'),
 (1,'q'), (6,'w'), (2,'d'), (9,'e');

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
ANALYZE TABLE t2;
-- enable_result_log
-- enable_query_log

let $query=
SELECT col_varchar_nokey
FROM t2 AS ot
WHERE col_varchar_nokey IN (
  SELECT col_varchar_key
  FROM t1 AS it
  WHERE it.col_int_nokey <= it.col_int_nokey
    AND NOT ot.col_int_nokey < 2
)
ORDER BY col_varchar_nokey;

eval explain $query;
eval $query;

ALTER TABLE t1 ENGINE=MyISAM;
ALTER TABLE t2 ENGINE=MyISAM;

eval explain $query;
eval $query;

ALTER TABLE t1 ENGINE=Memory;
ALTER TABLE t2 ENGINE=Memory;

eval explain $query;
eval $query;

DROP TABLE t1, t2;

--echo # End of test for bug#13576391.

--echo #
--echo # Bug #13589848 "MISSING ROW ON SELECT WITH NESTED IN CLAUSES WHEN LOOSESCAN=ON"
--echo #
CREATE TABLE t1 (
  id INT,
  col_varchar_key VARCHAR(1),
  col_varchar_nokey VARCHAR(1),
  KEY (col_varchar_key)
);

INSERT INTO t1 VALUES (100,'m','m'),
(200,'b','b'), (300,'x','x');

CREATE TABLE t2 (
  col_varchar_key VARCHAR(1),
  col_varchar_nokey VARCHAR(1),
  KEY (col_varchar_key)
);

INSERT INTO t2 VALUES ('b','b');

CREATE TABLE t3 (
  col_varchar_key VARCHAR(1),
  col_varchar_nokey VARCHAR(1),
  KEY (col_varchar_key)
);

INSERT INTO t3 VALUES ('k','k');

let $query=SELECT GP1.id
FROM t1 AS GP1 JOIN t3 AS GP2
     ON GP2.col_varchar_key <> GP1.col_varchar_nokey
WHERE (GP1.col_varchar_nokey, GP1.col_varchar_nokey)
      IN (
           SELECT col_varchar_nokey, col_varchar_nokey
           FROM t1
           WHERE col_varchar_nokey
           IN ( SELECT col_varchar_key
                FROM t2 LEFT JOIN t3 USING (col_varchar_key) )
         )
;

eval EXPLAIN $query;
eval $query;

DROP TABLE t1,t2,t3;

--echo #
--echo # Bug #13596176: Missing row on select with nested in clause when
--echo #                matr=on and bnl=off + MyISAM
--echo #

CREATE TABLE t1 (
  int_key int DEFAULT NULL,
  vc_key varchar(1) DEFAULT NULL,
  vc_nokey varchar(1) DEFAULT NULL,
  KEY int_key (int_key),
  KEY vc_key (vc_key, int_key)
) ENGINE=MyISAM;

INSERT INTO t1 VALUES
 (8,'x','x'), (7,'d','d'), (1,'r','r'), (7,'f','f'),
 (9,'y','y'), (NULL,'u','u'), (1,'m','m'), (9,NULL,NULL),
 (2,'o','o'), (9,'w','w'), (2,'m','m'), (4,'q','q'),
 (0,NULL,NULL), (4,'d','d'), (8,'g','g'), (NULL,'x','x'),
 (NULL,'f','f'), (0,'p','p'), (NULL,'j','j'), (8,'c','c');

CREATE TABLE t2 (
  int_key int DEFAULT NULL,
  vc_key varchar(1) DEFAULT NULL,
  KEY int_key (int_key),
  KEY vc_key (vc_key, int_key)
) ENGINE=MyISAM;

INSERT INTO t2 VALUES (8,'g');

let $query=
SELECT vc_key
FROM t1 as outr
WHERE (vc_nokey, vc_key ) IN
 (SELECT vc_nokey, vc_nokey
  FROM t1 middle
  WHERE vc_nokey IN
   (SELECT child1.vc_key
    FROM t2 AS child1 JOIN t1 AS child2 USING (int_key)
   )
 );

eval explain $query;
eval $query;

DROP TABLE t1, t2;

--echo # End of test for bug#13596176.

--echo #
--echo # BUG#11754478: MAX/MIN + SUBQUERY + AND FAILS TO RETURN ANY ROWS
--echo # BUG#13599013: MAX/MIN + SUBQUERY IN WHERE CLAUSE MATCHING NO 
--echo #               ROWS + INDEX DOES NOT RETURN NULL
--echo #

CREATE TABLE t1 (
  pk int(11) PRIMARY KEY,
  int_key int(11),
  KEY int_key (int_key)
);

INSERT INTO t1 VALUES (1,0),(2,0),(3,2),(4,0),(5,3),(6,0);

SELECT MIN(int_key) FROM t1 WHERE (4, 4) IN (SELECT 1, 2);
SELECT MIN(int_key) FROM t1 WHERE (4, 4) IN (SELECT 4, 4);
SELECT MIN(pk) FROM t1 WHERE pk IN (SELECT int_key FROM t1) AND pk = 6;

DROP TABLE t1;

--echo # BUG#13726217: Crash in Item_ident::fix_after_pullout()

CREATE TABLE t1(a INTEGER) engine=innodb;
INSERT INTO t1 VALUES (0);

SELECT 0
FROM t1
WHERE 0 IN
   (SELECT 0
    FROM t1
    WHERE 0 LIKE
       (SELECT elt(a, 0) AS b
        FROM t1
        GROUP BY a
        HAVING b
       )
   );

DROP TABLE t1;

--echo # End of test for bug#13726217.

--echo # BUG#13773979: Missing rows on second execution of prepared statement

CREATE TABLE t1 (
  col_int_nokey INT,
  col_int_key INT,
  col_varchar_key VARCHAR(1)
);

INSERT INTO t1 VALUES
 (1,7,'v'), (7,0,'s'), (4,9,'l'), (7,3,'y'),
 (2,2,'i'), (9,5,'h'), (0,1,'a'), (9,3,'v');

CREATE VIEW v1 AS SELECT * FROM t1;

let $query=
SELECT *
FROM t1
WHERE col_int_key IN (
  SELECT alias1.col_int_nokey AS field1
  FROM v1 AS alias1
  WHERE alias1.col_varchar_key < 'v'
);
eval $query;
eval prepare stmt FROM "$query";
execute stmt;
execute stmt;

DEALLOCATE PREPARE stmt;

DROP VIEW v1;
DROP TABLE t1;

--echo # End of test for bug#13773979.

--echo #
--echo # BUG#13685026 ASSERTION CUR_SJ_INNER_TABLES == 0 IN
--echo # --OPTIMIZE_TABLE_ORDER::CHOOSE_TABLE_ORDER
--echo #

CREATE TABLE t1 (
  col_int_key INT(11) NOT NULL,
  col_datetime_key DATETIME NOT NULL,
  col_varchar_key VARCHAR(1) NOT NULL,
  col_varchar_nokey VARCHAR(1) NOT NULL,
  KEY col_int_key (col_int_key),
  KEY col_datetime_key (col_datetime_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
);

INSERT INTO t1 VALUES (0,'2002-02-13 17:30:06','j','j');
INSERT INTO t1 VALUES (8,'2008-09-27 00:34:58','v','v');

CREATE TABLE t2 (
  col_int_key INT(11) NOT NULL,
  col_datetime_key DATETIME NOT NULL,
  col_varchar_key VARCHAR(1) NOT NULL,
  col_varchar_nokey VARCHAR(1) NOT NULL,
  KEY col_int_key (col_int_key),
  KEY col_datetime_key (col_datetime_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
);

INSERT INTO t2 VALUES (7,'2003-08-21 00:00:00','b','b');

SET @old_depth=@@optimizer_search_depth;
SET optimizer_search_depth=4;

let $query=SELECT col_datetime_key
FROM t1 as outr
WHERE col_datetime_key IN (
  SELECT alias1.col_datetime_key
  FROM t1 AS alias1
    LEFT JOIN t1 as alias3
      STRAIGHT_JOIN ( t2 AS alias4
        JOIN t1 AS alias5
        ON alias5.col_varchar_key <= alias4.col_varchar_nokey )
      ON alias5.col_int_key < alias4.col_int_key
    ON alias5.col_varchar_key = alias4.col_varchar_key
);

eval EXPLAIN $query;
eval $query;

DROP TABLE t1,t2;
SET @@optimizer_search_depth=@old_depth;

--echo #
--echo # BUG#13848789: SEGFAULT IN JOIN_READ_NEXT_SAME AT 
--echo #               SQL/SQL_EXECUTOR.CC ON HAVING...IN...JOIN
--echo #

CREATE TABLE t1 (
  col_int_key INT,
  col_varchar_key VARCHAR(1),
  KEY col_int_key (col_int_key),
  KEY col_varchar_key (col_varchar_key)
);

INSERT INTO t1 VALUES (8,'x');

CREATE TABLE t2 (
  col_varchar_key VARCHAR(1),
  KEY col_varchar_key (col_varchar_key)
);

INSERT INTO t2 VALUES ('x'), ('y');

let $query= SELECT MIN(col_int_key)
FROM t1 as t1_outer
HAVING (1, 2) IN (
  SELECT t1_inner.col_int_key, MAX(t1_inner.col_int_key)
  FROM t1 as t1_inner JOIN t2
    ON t2.col_varchar_key = t1_inner.col_varchar_key
);


--eval explain $query
--eval $query

DROP TABLE t1,t2;

--echo # Bug#13838810: Segfault in evaluate_null_complemented_join_record

CREATE TABLE t1 (
  pk int NOT NULL,
  col_int_nokey int DEFAULT NULL,
  col_int_key int DEFAULT NULL,
  col_varchar_key varchar(1) DEFAULT NULL,
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
) ENGINE=InnoDB;

INSERT INTO t1 VALUES (10,NULL,8,'x');

CREATE TABLE t2 (
  pk int NOT NULL,
  col_varchar_nokey varchar(1) DEFAULT NULL,
  PRIMARY KEY (pk)
) ENGINE=InnoDB;

INSERT INTO t2 VALUES (1,'x');

CREATE TABLE t3 (
  pk int NOT NULL,
  col_varchar_key varchar(1) DEFAULT NULL,
  col_varchar_nokey varchar(1) DEFAULT NULL,
  PRIMARY KEY (pk),
  KEY col_varchar_key (col_varchar_key)
) ENGINE=InnoDB;

INSERT INTO t3 VALUES
 (1,'v','v'), (2,'v','v'), (3,'c','c'), (4,NULL,NULL);

let $query=
SELECT table1.pk,table2.pk, table3.pk
FROM t2 AS table1
     LEFT JOIN t1 AS table2
       LEFT JOIN t1 AS table3
       ON table3.col_int_key = table2.col_int_key
     ON table3.pk = table2.col_int_nokey AND
        table1.col_varchar_nokey IN (
        SELECT subquery3_t1.col_varchar_nokey
        FROM t3 AS subquery3_t1
             LEFT JOIN t1 AS subquery3_t2
             ON subquery3_t2.col_varchar_key = subquery3_t1.col_varchar_key
        WHERE subquery3_t2.col_int_nokey <> 9
    )
;

eval EXPLAIN $query;
eval $query;

DROP TABLE t1, t2, t3;

--echo Extra test case for specific code coverage

CREATE TABLE t1(pk INTEGER);
INSERT INTO t1 VALUES(1), (2);

let $query=
SELECT *
FROM t1 AS ot1 LEFT JOIN t1 AS ot2
     ON ot1.pk=ot2.pk AND
        ot2.pk IN
           (SELECT it1.pk
            FROM t1 AS it1 LEFT JOIN t1 AS it2 ON it1.pk=it2.pk);
eval explain $query;
eval $query;

DROP TABLE t1;

--echo # End of test for bug#13838810.

--echo #
--echo # BUG#13685026 ASSERTION CUR_SJ_INNER_TABLES == 0 IN
--echo # --OPTIMIZE_TABLE_ORDER::CHOOSE_TABLE_ORDER
--echo #

CREATE TABLE t1 (
  col_int_key INT(11) NOT NULL,
  col_datetime_key DATETIME NOT NULL,
  col_varchar_key VARCHAR(1) NOT NULL,
  col_varchar_nokey VARCHAR(1) NOT NULL,
  KEY col_int_key (col_int_key),
  KEY col_datetime_key (col_datetime_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
);

INSERT INTO t1 VALUES (0,'2002-02-13 17:30:06','j','j');
INSERT INTO t1 VALUES (8,'2008-09-27 00:34:58','v','v');

CREATE TABLE t2 (
  col_int_key INT(11) NOT NULL,
  col_datetime_key DATETIME NOT NULL,
  col_varchar_key VARCHAR(1) NOT NULL,
  col_varchar_nokey VARCHAR(1) NOT NULL,
  KEY col_int_key (col_int_key),
  KEY col_datetime_key (col_datetime_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
);

INSERT INTO t2 VALUES (7,'2003-08-21 00:00:00','b','b');

SET @old_depth=@@optimizer_search_depth;
SET optimizer_search_depth=4;

let $query=SELECT col_datetime_key
FROM t1 as outr
WHERE col_datetime_key IN (
  SELECT alias1.col_datetime_key
  FROM t1 AS alias1
    LEFT JOIN t1 as alias3
      STRAIGHT_JOIN ( t2 AS alias4
        JOIN t1 AS alias5
        ON alias5.col_varchar_key <= alias4.col_varchar_nokey )
      ON alias5.col_int_key < alias4.col_int_key
    ON alias5.col_varchar_key = alias4.col_varchar_key
);

eval EXPLAIN $query;
eval $query;

DROP TABLE t1,t2;
SET @@optimizer_search_depth=@old_depth;

--echo #
--echo # Bug#13845930: Segfault in st_join_table::and_with_condition
--echo #

CREATE TABLE t1 (
  col_int INTEGER
);

CREATE TABLE t2 (
  col_varchar_1 VARCHAR(1),
  col_varchar_2 VARCHAR(1)
);

INSERT INTO t2 VALUES ('x','x'), ('c','c');

PREPARE stmt FROM '
SELECT alias2.col_varchar_2 AS field1
FROM t2 AS alias1
     JOIN
      (t2 AS alias2
       LEFT JOIN t2 AS alias3
       ON (8, 92) IN
          (SELECT sq1_alias1.col_int,
                  sq1_alias2.col_int
           FROM t1 AS sq1_alias1 JOIN t1 AS sq1_alias2
          )
      )
     ON alias3.col_varchar_1 = alias2.col_varchar_2
';

EXECUTE stmt;
EXECUTE stmt;

DEALLOCATE prepare stmt;

DROP TABLE t1, t2;

--echo # End of test for bug#13845930.

--echo #
--echo # Bug#13855925: Assert 'prebuilt->search_tuple->n_fields > 0'
--echo #               in ha_innobase::index_read
--echo #

CREATE TABLE t1 (
  pk INTEGER AUTO_INCREMENT,
  col_int_nokey INT,
  col_int_key INT,
  col_varchar_key VARCHAR(1),
  col_varchar_nokey VARCHAR(1),
  PRIMARY KEY (pk),
  KEY (col_varchar_key)
) ENGINE=INNODB;

INSERT INTO t1 (
  col_int_key, col_int_nokey,
  col_varchar_key, col_varchar_nokey
) VALUES
 (4, 2, 'v','v'), (62, 150, 'v','v'), (7, NULL, 'c','c'), (1, 2, NULL, NULL),
 (0, 5, 'x','x'), (7, 3, 'i','i'), (7, 1, 'e','e'), (1, 4, 'p','p'),
 (7, NULL, 's','s'), (1, 2, 'j','j'), (5, 6, 'z','z'), (2, 6, 'c','c'),
 (0, 8, 'a','a'), (1, 2, 'q','q'), (8, 6, 'y','y'), (1, 8, NULL, NULL),
 (1, 3, 'r','r'), (9, 3, 'v','v'), (1, 9, NULL, NULL), (5, 6, 'r','r');

CREATE TABLE t2 (
  pk INT AUTO_INCREMENT,
  col_int_nokey INT,
  col_int_key INT,
  PRIMARY KEY (pk),
  KEY (col_int_key)
) AUTO_INCREMENT=10 ENGINE=INNODB;

INSERT INTO t2 (col_int_key, col_int_nokey) VALUES
 (8, NULL), (7, 8), (1, 1), (7, 9), (9, 4), (NULL, 3), (1, 2), (9, NULL),
 (2, 2), (9, NULL), (2, 6), (4, 7), (0, 2), (4, 5), (8, 7), (NULL, 6),
 (NULL, 6), (0, 2), (NULL, 9), (8, 6);

CREATE TABLE t3 (
  pk INT AUTO_INCREMENT,
  col_varchar_key VARCHAR(1),
  PRIMARY KEY (pk),
  KEY (col_varchar_key)
) ENGINE=INNODB;

INSERT INTO t3 (col_varchar_key) VALUES
 ('c'), ('c'), ('q'), ('g'), ('e'), ('l'), (NULL), ('c'), ('h'), ('d'),
 ('c'), ('i'), ('t'), ('g'), ('q'), ('l'), ('n'), ('z'), ('n'), ('r'), ('p');

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
ANALYZE TABLE t2;
ANALYZE TABLE t3;
-- enable_result_log
-- enable_query_log

CREATE VIEW v1 AS
SELECT table2.col_varchar_nokey AS field1
FROM t2 AS table1
     INNER JOIN (t1 AS table2
       STRAIGHT_JOIN t2 AS table3
       ON table3.col_int_key = table2.pk AND
          table3.col_int_nokey = ANY
           (SELECT subquery1_t2.col_int_nokey AS subquery1_field1
            FROM t2 AS subquery1_t1
                 RIGHT OUTER JOIN t1 AS subquery1_t2
                   INNER JOIN t1 AS subquery1_t3
                   ON subquery1_t3.col_int_key = subquery1_t2.pk
                 ON subquery1_t3.col_varchar_key=subquery1_t2.col_varchar_nokey
            WHERE subquery1_t1.pk > 1
           )
      )
     ON table3.col_int_key IN
       (SELECT subquery2_t1.col_int_key AS subquery2_field1
        FROM t2 AS subquery2_t1
             RIGHT OUTER JOIN t3 AS subquery2_t2
               LEFT OUTER JOIN t1 AS subquery2_t3
               ON subquery2_t3.col_varchar_key = subquery2_t2.col_varchar_key
             ON subquery2_t3.pk = subquery2_t2.pk
       )
;

explain SELECT * FROM v1;
SELECT * FROM v1;

DROP VIEW v1;
DROP TABLE t1,t2,t3;

--echo # End of test for bug#13855925.

--echo #
--echo # Bug#13897959: Segfault in setup_semijoin_dups_elimination()
--echo #

CREATE TABLE t1 (
  col_datetime_key DATETIME DEFAULT NULL,
  KEY col_datetime_key (col_datetime_key)
) ENGINE=MyISAM;

INSERT INTO t1 VALUES
 ('2001-04-18 00:00:00'), ('2008-12-18 19:39:55'),
 ('2000-08-01 12:19:39'), ('2004-09-25 21:29:06'),
 ('2009-09-20 09:11:48'), ('2004-03-27 09:32:04');

CREATE TABLE t2 (
  col_date_nokey date DEFAULT NULL,
  col_time_key time DEFAULT NULL,
  col_datetime_key datetime DEFAULT NULL,
  col_varchar_key varchar(1) DEFAULT NULL,
  col_varchar_nokey varchar(1) DEFAULT NULL,
  KEY col_time_key (col_time_key),
  KEY col_datetime_key (col_datetime_key),
  KEY col_varchar_key(col_varchar_key)
) ENGINE=MyISAM;

INSERT INTO t2 VALUES ('1900-01-01',NULL,'2001-11-04 19:07:55','x','x');

SELECT grandparent1.col_varchar_nokey
FROM t2 AS grandparent1 LEFT JOIN t1 USING (col_datetime_key)
WHERE grandparent1.col_varchar_nokey IN (
  SELECT col_varchar_nokey
  FROM t2 AS parent1
  WHERE parent1.col_time_key > grandparent1.col_date_nokey
);

DROP TABLE t1, t2;

--echo # End of test for bug#13897959.

--echo #
--echo # Bug#13898625 ASSERT `(REMAINING_TABLES_AFTER != 0) ...' IN
--echo # BEST_EXTENSION_BY_LIMITED_SEARCH
--echo #

CREATE TABLE t1 (
  pk int(11) NOT NULL,
  col_int_nokey INT,
  col_int_key INT,
  col_varchar_key VARCHAR(1),
  col_varchar_nokey VARCHAR(1),
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
);

INSERT INTO t1 VALUES (26,6,NULL,'f','f');
INSERT INTO t1 VALUES (29,6,8,'c','c');

CREATE TABLE t2 (
  pk INT NOT NULL,
  col_int_nokey INT,
  col_int_key INT,
  col_varchar_key VARCHAR(1),
  col_varchar_nokey VARCHAR(1),
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
);

INSERT INTO t2 VALUES (1,2,4,'v','v');
INSERT INTO t2 VALUES (2,150,62,'v','v');
INSERT INTO t2 VALUES (5,5,0,'x','x');
INSERT INTO t2 VALUES (6,3,7,'i','i');
INSERT INTO t2 VALUES (7,1,7,'e','e');

CREATE VIEW view_c AS SELECT * FROM t2;

let $query=
  SELECT SUM( alias1.col_varchar_key ) AS field1
  FROM t1 AS alias1
    RIGHT JOIN t2 AS alias2
      INNER JOIN t1 AS alias3
      ON (alias3.col_varchar_key = alias2.col_varchar_key )
    ON ( "v" ) IN (
        SELECT sq1_alias1.col_varchar_nokey AS sq1_field1
        FROM t1 AS sq1_alias1
    )
  WHERE alias3.pk IN (
    SELECT sq2_alias1.col_int_key AS sq2_field1
    FROM ( view_c AS sq2_alias1, t1 AS sq2_alias2 )
  )
;

eval PREPARE prep_stmt_7430 FROM '$query';
EXECUTE prep_stmt_7430;
EXECUTE prep_stmt_7430;
eval EXPLAIN $query;

# Assertion sj_inner_tables == ((remaining_tables |
# new_join_tab->table->map) & sj_inner_tables)

let $query=
  SELECT SUM( alias1.col_varchar_key ) AS field1
  FROM t1 AS alias1
    RIGHT JOIN t2 AS alias2
      INNER JOIN t1 AS alias3
      ON (alias3.col_varchar_key = alias2.col_varchar_key )
    ON ( "v" ) IN (
        SELECT sq1_alias1.col_varchar_nokey AS sq1_field1
        FROM t1 AS sq1_alias1
    )
  WHERE alias3.pk IN (
    SELECT sq2_alias1.col_int_key AS sq2_field1
    FROM ( view_c AS sq2_alias1 , t1 AS sq2_alias2 )
    WHERE sq2_alias1.col_varchar_nokey <> alias2.col_varchar_key
      AND sq2_alias1.col_varchar_key < "l"
  )
;

eval PREPARE prep_stmt_7430 FROM '$query';
EXECUTE prep_stmt_7430;
EXECUTE prep_stmt_7430;
eval EXPLAIN $query;

DROP TABLE t1,t2;
DROP VIEW view_c;

--echo #
--echo # Bug#13902463 SEGFAULT IN BITMAP<64U>::MERGE OR ADD_KEY_FIELD
--echo # ON SECOND EXEC OF PREP STMT
--echo #

CREATE TABLE t1 (
  pk INT,
  col_int_nokey INT,
  col_int_key INT,
  col_varchar_key VARCHAR(1),
  col_varchar_nokey VARCHAR(1),
  KEY col_varchar_key (col_varchar_key)
);

CREATE VIEW view_b AS SELECT * FROM t1;

let $query=
  SELECT alias2.col_varchar_nokey AS field1
  FROM t1 AS alias1
    INNER JOIN t1 AS alias2
    ON (alias1.col_varchar_key = alias2.col_varchar_nokey
      AND ( alias1.col_int_key ) IN (
        SELECT t1.col_int_nokey
        FROM t1
      )
    )
  WHERE alias1.col_varchar_key IN (
    SELECT sq2_alias2.col_varchar_nokey AS sq2_field1
    FROM view_b AS sq2_alias1
      INNER JOIN t1 AS sq2_alias2
      ON (sq2_alias2.col_varchar_key = sq2_alias1.col_varchar_key )
    WHERE sq2_alias1.pk > alias2.pk
  )
;

eval PREPARE prep_stmt_20421 FROM '$query';
EXECUTE prep_stmt_20421;
EXECUTE prep_stmt_20421;
eval EXPLAIN $query;

ALTER TABLE t1 DROP INDEX col_varchar_key;
# Assertion join->best_read < double(1.79769313486231570815e+308L)
eval PREPARE prep_stmt_20421 FROM '$query';
EXECUTE prep_stmt_20421;
EXECUTE prep_stmt_20421;
eval EXPLAIN $query;

DROP TABLE t1;
DROP VIEW view_b;

--echo #
--echo # Bug#13907277: Segfault in evaluate_null_complemented_join_record
--echo #

CREATE TABLE t1 (
  pk INTEGER,
  col_varchar_nokey VARCHAR(1),
  col_varchar_key VARCHAR(1),
  PRIMARY KEY (pk)
);
INSERT INTO t1 VALUES (1, 'x', 'x');

CREATE TABLE t2 (
  pk INTEGER,
  PRIMARY KEY (pk)
);
INSERT INTO t2 VALUES (1);

CREATE TABLE t3 (
  pk INTEGER,
  col_int_nokey INTEGER,
  col_int_key INTEGER,
  col_varchar_nokey VARCHAR(1),
  PRIMARY KEY (pk)
);
INSERT INTO t3 VALUES (1, 6, 5, 'r');

let $query=
SELECT outer_t1.pk, outer_t2.pk
FROM t3 AS outer_t1
     RIGHT JOIN t2 AS outer_t2
     ON outer_t1.col_int_nokey IN
        (SELECT inner_t1.col_int_nokey
         FROM t3 AS inner_t1
              LEFT JOIN t1 AS inner_t2
                INNER JOIN t1 AS inner_t3
                ON inner_t3.pk = inner_t2.pk
              ON inner_t3.col_varchar_nokey = inner_t2.col_varchar_key
        );

eval explain $query;
eval $query;

DROP TABLE t1, t2, t3;

--echo # End of test for bug#13907277.

--echo #
--echo # Bug#13955713: Assert 'JOIN->best_read < ...' on second execution
--echo #

CREATE TABLE t1 (
  pk INTEGER,
  col_varchar_key VARCHAR(1),
  col_varchar_nokey VARCHAR(1)
);

PREPARE stmt FROM "
SELECT MIN(alias2.col_varchar_key) AS field1
FROM t1 AS alias1
     INNER JOIN (t1 AS alias2
       INNER JOIN t1 AS alias3
       ON 8 IN
         (SELECT sq1_alias1.pk AS sq1_field2
          FROM t1 AS sq1_alias1
          WHERE 9 IN
             (SELECT SUM(t1_sq1_alias1.pk) AS t1_sq1_field2
              FROM t1 AS t1_sq1_alias1
             )
         )
     )
     ON alias3.col_varchar_nokey = alias2.col_varchar_key
WHERE EXISTS
   (SELECT sq2_alias1.pk AS sq2_field1
    FROM t1 AS sq2_alias1
    WHERE sq2_alias1.col_varchar_key < alias1.col_varchar_nokey
   )
";

EXECUTE stmt;
EXECUTE stmt;

DEALLOCATE PREPARE stmt;
DROP TABLE t1;

--echo # End of test for bug#13955713.

--echo #
--echo # Bug#13956813: Segfault in memcpy from Join_cache::write_record_data()
--echo #

CREATE TABLE t1 (
  pk INT,
  col_varchar_key VARCHAR(1),
  col_varchar_nokey VARCHAR(1)
);

CREATE TABLE t2 (
  pk INT,
  col_varchar_key VARCHAR(1),
  col_varchar_nokey VARCHAR(1)
);

INSERT INTO t2 VALUES
 (10,'j','j'), (11,'z','z'), (12,'c','c'), (13,'a','a'),
 (14,'q','q'), (15,'y','y'), (16,NULL,NULL), (17,'r','r'),
 (18,'v','v'), (19,NULL,NULL), (20,'r','r');

CREATE TABLE t3 (
  pk INT,
  col_int_key INT,
  col_varchar_key VARCHAR(1),
  KEY col_int_key (col_int_key)
);

INSERT INTO t3 VALUES
 (15,NULL,'u'), (16,1,'m'), (17,9,NULL), (18,2,'o'),
 (19,9,'w'), (20,2,'m'), (21,4,'q'), (22,0,NULL),
 (23,4,'d'), (24,8,'g'), (25,NULL,'x'), (26,NULL,'f'),
 (27,0,'p'), (28,NULL,'j'), (29,8,'c');

CREATE VIEW view_inline_0 AS
SELECT t1.*
FROM t1 INNER JOIN t3
     ON t1.pk = t3.pk;

CREATE VIEW view_inline_1 AS
SELECT sq2_alias2.col_varchar_key AS sq2_field1,
       sq2_alias1.col_varchar_key AS sq2_field2
FROM t3 AS sq2_alias1 LEFT OUTER JOIN t3 AS sq2_alias2
     ON sq2_alias1.pk = sq2_alias2.col_int_key;

CREATE VIEW view_inline_2 AS
SELECT 'p', 'p' UNION SELECT 'k', 's';

let $query=
SELECT SUM(alias1.col_varchar_nokey) AS field2
FROM t2 AS alias2
     LEFT JOIN (SELECT * FROM view_inline_0) AS alias1
     ON alias2.col_varchar_key = alias1.col_varchar_key AND
        (alias2.col_varchar_nokey, alias2.col_varchar_key) IN
        (SELECT * FROM view_inline_1
        )
WHERE (alias1.col_varchar_key, alias1.col_varchar_nokey) IN
   (SELECT * FROM view_inline_2
   );

eval explain $query;
eval $query;

DROP VIEW view_inline_0, view_inline_1, view_inline_2;
DROP TABLE t1, t2, t3;

--echo # End of test for bug#13956813.

--echo #
--echo # Bug#13974177: Assert !(tab->table->regginfo.not_exists_optimize...
--echo #

CREATE TABLE t1 (
  pk INTEGER AUTO_INCREMENT,
  col_int_nokey INTEGER,
  col_int_key INTEGER,
  col_varchar_key VARCHAR(1),
  col_varchar_nokey VARCHAR(1),
  PRIMARY KEY (pk),
  KEY (col_int_key),
  KEY (col_varchar_key, col_int_key)
);

INSERT INTO t1(col_int_key, col_int_nokey, col_varchar_key, col_varchar_nokey)
VALUES
 (0, 4, 'j', 'j'), (8, 6, 'v', 'v'), (1, 3, 'c', 'c'), (8, 5, 'm', 'm'),
 (9, 3, 'd', 'd'), (24, 246, 'd', 'd'), (6, 2, 'y', 'y'), (1, 9, 't', 't'),
 (6, 3, 'd', 'd'), (2, 8, 's', 's'), (4, 1, 'r', 'r'), (8, 8, 'm', 'm'),
 (4, 8, 'b', 'b'), (4, 5, 'x', 'x'), (7, 7, 'g', 'g'), (4, 5, 'p', 'p'),
 (1, 1, 'q', 'q'), (9, 6, 'w', 'w'), (4, 2, 'd', 'd'), (8, 9, 'e', 'e');

CREATE TABLE t2 (
  pk INTEGER AUTO_INCREMENT,
  col_int_nokey INTEGER NOT NULL,
  col_time_key TIME NOT NULL,
  col_time_nokey TIME NOT NULL,
  PRIMARY KEY (pk),
  KEY (col_time_key)
) ENGINE=InnoDB;

INSERT INTO t2 (col_int_nokey, col_time_key, col_time_nokey) VALUES
 (7, '00:00:00', '00:00:00'), (0, '00:00:00', '00:00:00'),
 (9, '06:35:17', '06:35:17'), (3, '18:07:14', '18:07:14'),
 (4, '20:36:52', '20:36:52'), (2, '21:29:07', '21:29:07'),
 (5, '23:45:57', '23:45:57'), (3, '22:54:57', '22:54:57'),
 (1, '18:45:09', '18:45:09'), (3, '14:30:46', '14:30:46'),
 (6, '19:23:43', '19:23:43'), (7, '03:39:30', '03:39:30'),
 (5, '23:37:52', '23:37:52'), (1, '16:59:30', '16:59:30'),
 (204, '22:21:15', '22:21:15'), (224, '12:24:37', '12:24:37'),
 (9, '15:02:08', '15:02:08'), (5, '23:59:59', '23:59:59'),
 (0, '08:23:30', '08:23:30'), (3, '08:32:22', '08:32:22');

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
ANALYZE TABLE t2;
-- enable_result_log
-- enable_query_log

let $query=
SELECT ot1.col_int_key AS x
FROM t1 AS ot2
      LEFT JOIN t1 AS ot1
      ON ot2.col_varchar_nokey > ot1.col_varchar_key
WHERE (ot1.col_int_nokey, ot1.pk) IN
   (SELECT it1.pk AS x,
           it1.col_int_nokey AS y
    FROM t2 AS it2
         LEFT JOIN t2 AS it1
         ON it2.col_time_nokey = it1.col_time_key
   ) AND ot1.pk IS NULL
;

eval explain $query;
eval $query;

DROP TABLE t1, t2;

--echo # End of test for bug#13974177.

--echo #
--echo # Bug#13971022: Assert 'keyparts > 0' failed in create_ref_for_key...
--echo #

CREATE TABLE t1 (
  pk INT,
  col_int_key INT,
  col_varchar_key VARCHAR(1),
  PRIMARY KEY (pk),
  KEY col_varchar_key (col_varchar_key,col_int_key)
);

CREATE TABLE t2 (
  pk INT,
  col_int_key INT,
  col_varchar_key VARCHAR(1),
  col_varchar_nokey VARCHAR(1),
  PRIMARY KEY (pk)
) ENGINE=InnoDB;

CREATE TABLE t3 (
  i INT
);

let $query=
SELECT table1.pk AS field1
FROM ( SELECT subquery1_t1. *
       FROM t2 AS subquery1_t1
            JOIN t2 AS subquery1_t2
            ON subquery1_t2.pk = subquery1_t1.pk) AS table1
     STRAIGHT_JOIN t2 AS table2
     ON table1.col_int_key IN (SELECT 7 FROM t3)
WHERE table1.col_varchar_nokey IN
   (SELECT subquery3_t1.col_varchar_key AS subquery3_field1
    FROM t1 AS subquery3_t1
   )
;

eval explain $query;
eval $query;

DROP TABLE t1, t2, t3;

--echo # End of test for bug#13971022.

--echo #
--echo # Bug#13623473 "MISSING ROWS ON SELECT AND JOIN WITH
--echo # TIME/DATETIME COMPARE" - Subquery part of test.
--echo #

SET TIMESTAMP=UNIX_TIMESTAMP('2012-01-31 10:14:35');

CREATE TABLE t1 (
  pk INT NOT NULL,
  col_int_nokey INT,
  col_int_key INT NOT NULL,
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key)
) ENGINE=InnoDB;

INSERT INTO t1 VALUES (10,1,7), (11,7,0), (12,4,9), (13,7,3),
(14,0,4), (15,2,2), (16,9,5), (17,4,3), (18,0,1), (19,9,3), (20,1,6),
(21,3,7), (22,8,5), (23,8,1), (24,18,204), (25,84,224), (26,6,9),
(27,3,5), (28,6,0), (29,6,3);

CREATE TABLE t2 (
  col_int_nokey INT NOT NULL,
  col_datetime_key DATETIME NOT NULL,
  col_varchar_key VARCHAR(1) NOT NULL,
  KEY col_datetime_key (col_datetime_key),
  KEY col_varchar_key (col_varchar_key)
) ENGINE=InnoDB;

INSERT INTO t2 VALUES (1,'2001-11-04 19:07:55','k');

CREATE TABLE t3 (
  col_time_key TIME,
  KEY col_time_key (col_time_key)
) ENGINE=InnoDB;

INSERT INTO t3 VALUES ('21:22:34'), ('10:50:38'), ('00:21:38'),
('04:08:02'), ('16:25:11'), ('10:14:58'), ('19:47:59'), ('11:14:24'),
('00:00:00'), ('00:00:00'), ('15:57:25'), ('07:05:51'), ('19:22:21'),
('03:53:16'), ('09:16:38'), ('15:37:26'), ('00:00:00'), ('05:03:03'),
('02:59:24'), ('00:01:58');

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
ANALYZE TABLE t2;
ANALYZE TABLE t3;
-- enable_result_log
-- enable_query_log

let $query=
SELECT outr.col_int_nokey
FROM t2 as outr
  STRAIGHT_JOIN t3 AS outr2
  ON outr2.col_time_key > outr.col_datetime_key
WHERE outr.col_int_nokey IN (
  SELECT col_int_key
  FROM t1 AS innr
  WHERE innr.pk >= innr.col_int_nokey
) AND (
  outr.col_int_nokey <= 6
  OR
  outr.col_varchar_key IS NULL
);
eval EXPLAIN EXTENDED $query;
--sorted_result
eval $query;

DROP TABLE t1,t2,t3;

SET TIMESTAMP = DEFAULT;

--echo # End of test for bug#13623473.

--echo #
--echo # Bug#13980954: Missing data on left join + null value + where..in
--echo #

CREATE TABLE t1 (
  ik INT,
  vc varchar(1)
);

INSERT INTO t1 VALUES (8, 'x'), (NULL, 'x');

CREATE TABLE t2 (
  ik INT,
  vc varchar(1)
);

INSERT INTO t2 VALUES
 (0, 'x'), (7, 'i'), (7, 'e'), (1, 'p'), (7, 's'), (1, 'j');

let $query=
SELECT t2.vc, t2.ik AS t2_ik, t1.ik AS t1_ik
FROM t2 LEFT JOIN t1 ON t2.vc=t1.vc
WHERE t2.vc IN (SELECT vc FROM t2 AS t3);

eval explain format=json $query;
eval $query;

DROP TABLE t1, t2;

--echo # End of test for bug#13980954.

--echo #
--echo # Bug#14048292: Segfault in Item_field::result_type on 2nd execution
--echo #               of prep stmt with join of view
--echo #

CREATE TABLE t1 (
  col_int INT
);

INSERT INTO t1 VALUES (0), (1);

CREATE VIEW view_t1 AS SELECT * FROM t1;

let $query=
SELECT alias1.col_int
FROM t1 AS alias1
     LEFT JOIN view_t1 AS alias2
     ON alias1.col_int IN
       (SELECT sq1_alias1.col_int
        FROM t1 AS sq1_alias1
       );

eval explain $query;

eval PREPARE stmt FROM "$query";
EXECUTE stmt;
EXECUTE stmt;

DEALLOCATE PREPARE stmt;
DROP VIEW view_t1;
DROP TABLE t1;

--echo # End of test for bug#14048292.

--echo #
--echo # Bug#14064201: Missing data on join of derived table + WHERE .. IN
--echo #               with two operands
--echo #

CREATE TABLE t1 (
  col_varchar_nokey VARCHAR(1)
);

INSERT INTO t1 VALUES
 ('v'), ('s'), ('l'), ('y'), ('c'), ('i'), ('h'), ('q'), ('a'), ('v'),
 ('u'), ('s'), ('y'), ('z'), ('h'), ('p'), ('e'), ('i'), ('y'), ('w');

CREATE TABLE t2 (
  col_varchar_key VARCHAR(1),
  col_varchar_nokey VARCHAR(1),
  KEY col_varchar_key(col_varchar_key)
);

INSERT INTO t2 VALUES
 ('j','j'), ('v','v'), ('c','c'), ('m','m'), ('d','d'), ('d','d'), ('y','y');

let $query=
SELECT *
FROM (SELECT * FROM t2) AS derived1
     LEFT JOIN t1
     USING (col_varchar_nokey)
WHERE (col_varchar_nokey, col_varchar_nokey) IN
   (SELECT col_varchar_nokey, col_varchar_key
    FROM t2 AS derived2
   );

eval explain format=json $query;
eval $query;

DROP TABLE t1, t2;

CREATE TABLE t1 (
  col_int_nokey int NOT NULL,
  col_int_key int NOT NULL,
  KEY col_int_key (col_int_key)
);

INSERT INTO t1 VALUES
 (1,7), (7,0), (4,9), (7,3), (0,4), (2,2), (9,5), (4,3), (0,1), (9,3),
 (1,6), (3,7), (8,5), (8,1), (18,204), (84,224), (6,9), (3,5), (6,0), (6,3);

CREATE TABLE t2 (
  col_int_nokey int NOT NULL,
  col_int_key int NOT NULL,
  KEY col_int_key (col_int_key)
);

INSERT INTO t2 VALUES
 (4,0), (6,8), (3,1), (5,8), (3,9), (246,24), (2,6), (9,1), (3,6), (8,2),
 (1,4), (8,8), (8,4), (5,4), (7,7), (5,4), (1,1), (6,9), (2,4), (9,8);

let $query=
SELECT grandparent1.*
FROM t1 AS grandparent1
     LEFT JOIN t1 USING (col_int_nokey)
WHERE (col_int_nokey, col_int_nokey) IN
   (SELECT col_int_nokey, col_int_key
    FROM t2
   );

eval explain format=json $query;
eval $query;

DROP TABLE t1, t2;

CREATE TABLE t1 (  
  pk int,  
  col_int_key int,  
  col_datetime_key datetime,  
  col_varchar_key varchar(1),  
  col_varchar_nokey varchar(1),  
  PRIMARY KEY (pk),  
  KEY col_int_key (col_int_key),  
  KEY col_datetime_key (col_datetime_key),  
  KEY col_varchar_key (col_varchar_key,col_int_key)
) engine=MyISAM;
 
INSERT INTO t1 VALUES
 (10,7,'2004-06-06 04:22:12','v','v'), (11,0,'2005-11-13 01:12:31','s','s'),
 (12,9,'2002-05-04 01:50:00','l','l'), (13,3,'2004-10-27 10:28:45','y','y'),
 (14,4,'2006-07-22 05:24:23','c','c'), (15,2,'2002-05-16 21:34:03','i','i'),
 (16,5,'2008-04-17 10:45:30','h','h'), (17,3,'2009-04-21 02:58:02','q','q'),
 (18,1,'2008-01-11 11:01:51','a','a'), (19,3,'1900-01-01 00:00:00','v','v'),
 (20,6,'2007-05-17 18:24:57','u','u'), (21,7,'2007-08-07 00:00:00','s','s'),
 (22,5,'2001-08-28 00:00:00','y','y'), (23,1,'2004-04-16 00:27:28','z','z'),
 (24,204,'2005-05-03 07:06:22','h','h'), (25,224,'2009-03-11 17:09:50','p','p'),
 (26,9,'2007-12-08 01:54:28','e','e'), (27,5,'2009-07-28 18:19:54','i','i'),
 (28,0,'2008-06-08 00:00:00','y','y'), (29,3,'2005-02-09 09:20:26','w','w');
 
CREATE TABLE t2 (
  pk int,
  col_int_key int,
  col_datetime_key datetime,
  col_varchar_key varchar(1),
  col_varchar_nokey varchar(1),
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key),
  KEY col_datetime_key (col_datetime_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
) engine=MyISAM;
 
INSERT INTO t2 VALUES
 (1,0,'2002-02-13 17:30:06','j','j'), (2,8,'2008-09-27 00:34:58','v','v'),
 (3,1,'2007-05-28 00:00:00','c','c'), (4,8,'2009-07-25 09:21:20','m','m'),
 (5,9,'2002-01-16 00:00:00','d','d'), (6,24,'2006-10-12 04:32:53','d','d'),
 (7,6,'2001-02-15 03:08:38','y','y'), (8,1,'2004-10-02 20:31:15','t','t'),
 (9,6,'2002-08-20 22:48:00','d','d'), (10,2,'1900-01-01 00:00:00','s','s'),
 (11,4,'2005-08-15 00:00:00','r','r'), (12,8,'1900-01-01 00:00:00','m','m'),
 (13,4,'2008-05-16 08:09:06','b','b'), (14,4,'2001-01-20 12:47:23','x','x'),
 (15,7,'2008-07-02 00:00:00','g','g'), (16,4,'1900-01-01 00:00:00','p','p'),
 (17,1,'2002-12-08 11:34:58','q','q'), (18,9,'1900-01-01 00:00:00','w','w'),
 (19,4,'1900-01-01 00:00:00','d','d'), (20,8,'2002-08-25 20:35:06','e','e');
 
SELECT alias1.col_datetime_key
FROM t2 AS alias1
     RIGHT JOIN t2 AS alias2
       JOIN t2 AS alias3
       ON alias3.pk = alias2.pk
     ON alias3.col_varchar_nokey = alias2.col_varchar_key OR
        alias2.col_varchar_nokey
WHERE (alias2.col_varchar_key, alias2.col_varchar_key) IN
       (SELECT sq2_alias2.col_varchar_key, sq2_alias1.col_varchar_nokey
        FROM t1 AS sq2_alias1, t1 AS sq2_alias2
        WHERE sq2_alias2.col_int_key < 2);
 
ALTER TABLE t1 DISABLE KEYS;
ALTER TABLE t2 DISABLE KEYS;

let $query=
SELECT alias1.col_datetime_key
FROM t2 AS alias1
     RIGHT JOIN t2 AS alias2
       JOIN t2 AS alias3
       ON alias3.pk = alias2.pk
     ON alias3.col_varchar_nokey = alias2.col_varchar_key OR
        alias2.col_varchar_nokey
WHERE (alias2.col_varchar_key, alias2.col_varchar_key) IN
       (SELECT sq2_alias2.col_varchar_key, sq2_alias1.col_varchar_nokey
        FROM t1 AS sq2_alias1, t1 AS sq2_alias2
        WHERE sq2_alias2.col_int_key < 2);

eval explain format=json $query;
eval $query;

DROP TABLE t1, t2;

--echo # End of test for bug#14064201.

set @@optimizer_switch=@old_opt_switch;
# New tests go here.

--echo # End of 5.6 tests
