#
# Hash semi-join regression tests
# (WL#1110: Subquery optimization: materialization)
#

--disable_warnings
drop table if exists t1, t2, t3, t1i, t2i, t3i;
drop view if exists v1, v2, v1m, v2m;
--enable_warnings

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

# Indexed columns
create table t1i (a1 char(8), a2 char(8));
create table t2i (b1 char(8), b2 char(8));
create table t3i (c1 char(8), c2 char(8));
create index it1i1 on t1i (a1);
create index it1i2 on t1i (a2);
create index it1i3 on t1i (a1, a2);

create index it2i1 on t2i (b1);
create index it2i2 on t2i (b2);
create index it2i3 on t2i (b1, b2);

create index it3i1 on t3i (c1);
create index it3i2 on t3i (c2);
create index it3i3 on t3i (c1, c2);

insert into t1i select * from t1;
insert into t2i select * from t2;
insert into t3i select * from t3;

/******************************************************************************
* Simple tests.
******************************************************************************/
# non-indexed nullable fields
explain extended
select * from t1 where a1 in (select b1 from t2 where b1 > '0');
select * from t1 where a1 in (select b1 from t2 where b1 > '0');

explain extended
select * from t1 where a1 in (select b1 from t2 where b1 > '0' group by b1);
select * from t1 where a1 in (select b1 from t2 where b1 > '0' group by b1);

explain extended
select * from t1 where (a1, a2) in (select b1, b2 from t2 where b1 > '0' group by b1, b2);
select * from t1 where (a1, a2) in (select b1, b2 from t2 where b1 > '0' group by b1, b2);

explain extended
select * from t1 where (a1, a2) in (select b1, min(b2) from t2 where b1 > '0' group by b1);
select * from t1 where (a1, a2) in (select b1, min(b2) from t2 where b1 > '0' group by b1);

# indexed columns
explain extended
select * from t1i where a1 in (select b1 from t2i where b1 > '0');
select * from t1i where a1 in (select b1 from t2i where b1 > '0');

explain extended
select * from t1i where a1 in (select b1 from t2i where b1 > '0' group by b1);
select * from t1i where a1 in (select b1 from t2i where b1 > '0' group by b1);

explain extended
select * from t1i where (a1, a2) in (select b1, b2 from t2i where b1 > '0');
select * from t1i where (a1, a2) in (select b1, b2 from t2i where b1 > '0');

explain extended
select * from t1i where (a1, a2) in (select b1, b2 from t2i where b1 > '0' group by b1, b2);
select * from t1i where (a1, a2) in (select b1, b2 from t2i where b1 > '0' group by b1, b2);

explain extended
select * from t1i where (a1, a2) in (select b1, min(b2) from t2i where b1 > '0' group by b1);
select * from t1i where (a1, a2) in (select b1, min(b2) from t2i where b1 > '0' group by b1);

# BUG#31639: Wrong plan for uncorrelated subquery when loose scan is applicable.
explain extended
select * from t1 where (a1, a2) in (select b1, max(b2) from t2i group by b1);
select * from t1 where (a1, a2) in (select b1, max(b2) from t2i group by b1);

prepare st1 from "explain select * from t1 where (a1, a2) in (select b1, max(b2) from t2i group by b1)";
execute st1;
execute st1;
prepare st2 from "select * from t1 where (a1, a2) in (select b1, max(b2) from t2i group by b1)";
execute st2;
execute st2;

explain extended
select * from t1 where (a1, a2) in (select b1, min(b2) from t2i where b1 > '0' group by b1);
select * from t1 where (a1, a2) in (select b1, min(b2) from t2i where b1 > '0' group by b1);
-- error 1235
select * from t1 where (a1, a2) in (select b1, min(b2) from t2i limit 1,1);

# materialize the result of ORDER BY
# non-indexed fields
explain extended
select * from t1 where (a1, a2) in (select b1, b2 from t2 order by b1, b2);
select * from t1 where (a1, a2) in (select b1, b2 from t2 order by b1, b2);
# indexed fields
explain extended
select * from t1i where (a1, a2) in (select b1, b2 from t2i order by b1, b2);
select * from t1i where (a1, a2) in (select b1, b2 from t2i order by b1, b2);

/******************************************************************************
* Views, UNIONs, several levels of nesting.
******************************************************************************/
# materialize the result of subquery over temp-table view

create algorithm=merge view v1 as
select b1, c2 from t2, t3 where b2 > c2;

create algorithm=merge view v2 as
select b1, c2 from t2, t3 group by b2, c2;

create algorithm=temptable view v1m as
select b1, c2 from t2, t3 where b2 > c2;

create algorithm=temptable view v2m as
select b1, c2 from t2, t3 group by b2, c2;

select * from v1 where (c2, b1) in (select c2, b1 from v2 where b1 is not null);
select * from v1 where (c2, b1) in (select distinct c2, b1 from v2 where b1 is not null);

select * from v1m where (c2, b1) in (select c2, b1 from v2m where b1 is not null);
select * from v1m where (c2, b1) in (select distinct c2, b1 from v2m where b1 is not null);

drop view v1, v2, v1m, v2m;

# nested subqueries, views
explain extended
select * from t1
where (a1, a2) in (select b1, b2 from t2 where b1 >  '0') and
      (a1, a2) in (select c1, c2 from t3
                   where (c1, c2) in (select b1, b2 from t2i where b2 > '0'));
select * from t1
where (a1, a2) in (select b1, b2 from t2 where b1 >  '0') and
      (a1, a2) in (select c1, c2 from t3
                   where (c1, c2) in (select b1, b2 from t2i where b2 > '0'));

explain extended
select * from t1i
where (a1, a2) in (select b1, b2 from t2i where b1 >  '0') and
      (a1, a2) in (select c1, c2 from t3i
                   where (c1, c2) in (select b1, b2 from t2i where b2 > '0'));
select * from t1i
where (a1, a2) in (select b1, b2 from t2i where b1 >  '0') and
      (a1, a2) in (select c1, c2 from t3i
                   where (c1, c2) in (select b1, b2 from t2i where b2 > '0'));

explain extended
select * from t1
where (a1, a2) in (select b1, b2 from t2
                   where b2 in (select c2 from t3 where c2 LIKE '%02') or
                         b2 in (select c2 from t3 where c2 LIKE '%03')) and
      (a1, a2) in (select c1, c2 from t3
                   where (c1, c2) in (select b1, b2 from t2i where b2 > '0'));
select * from t1
where (a1, a2) in (select b1, b2 from t2
                   where b2 in (select c2 from t3 where c2 LIKE '%02') or
                         b2 in (select c2 from t3 where c2 LIKE '%03')) and
      (a1, a2) in (select c1, c2 from t3
                   where (c1, c2) in (select b1, b2 from t2i where b2 > '0'));

# as above with correlated innermost subquery
explain extended
select * from t1
where (a1, a2) in (select b1, b2 from t2
                   where b2 in (select c2 from t3 t3a where c1 = a1) or
                         b2 in (select c2 from t3 t3b where c2 LIKE '%03')) and
      (a1, a2) in (select c1, c2 from t3 t3c
                   where (c1, c2) in (select b1, b2 from t2i where b2 > '0'));
select * from t1
where (a1, a2) in (select b1, b2 from t2
                   where b2 in (select c2 from t3 t3a where c1 = a1) or
                         b2 in (select c2 from t3 t3b where c2 LIKE '%03')) and
      (a1, a2) in (select c1, c2 from t3 t3c
                   where (c1, c2) in (select b1, b2 from t2i where b2 > '0'));


# multiple levels of nesting subqueries, unions
explain extended
(select * from t1
where (a1, a2) in (select b1, b2 from t2
                   where b2 in (select c2 from t3 where c2 LIKE '%02') or
                         b2 in (select c2 from t3 where c2 LIKE '%03')
                   group by b1, b2) and
      (a1, a2) in (select c1, c2 from t3
                   where (c1, c2) in (select b1, b2 from t2i where b2 > '0')))
UNION
(select * from t1i
where (a1, a2) in (select b1, b2 from t2i where b1 >  '0') and
      (a1, a2) in (select c1, c2 from t3i
                   where (c1, c2) in (select b1, b2 from t2i where b2 > '0')));

(select * from t1
where (a1, a2) in (select b1, b2 from t2
                   where b2 in (select c2 from t3 where c2 LIKE '%02') or
                         b2 in (select c2 from t3 where c2 LIKE '%03')
                   group by b1, b2) and
      (a1, a2) in (select c1, c2 from t3
                   where (c1, c2) in (select b1, b2 from t2i where b2 > '0')))
UNION
(select * from t1i
where (a1, a2) in (select b1, b2 from t2i where b1 >  '0') and
      (a1, a2) in (select c1, c2 from t3i
                   where (c1, c2) in (select b1, b2 from t2i where b2 > '0')));


# UNION of subqueries as a subquery (thus it is not computed via materialization)
explain extended
select * from t1
where (a1, a2) in (select * from t1 where a1 > '0' UNION select * from t2 where b1 < '9') and
      (a1, a2) in (select c1, c2 from t3
                   where (c1, c2) in (select b1, b2 from t2i where b2 > '0'));
select * from t1
where (a1, a2) in (select * from t1 where a1 > '0' UNION select * from t2 where b1 < '9') and
      (a1, a2) in (select c1, c2 from t3
                   where (c1, c2) in (select b1, b2 from t2i where b2 > '0'));
# as above, with a join conditon between the outer references
explain extended
select * from t1, t3
where (a1, a2) in (select * from t1 where a1 > '0' UNION select * from t2 where b1 < '9') and
      (c1, c2) in (select c1, c2 from t3
                   where (c1, c2) in (select b1, b2 from t2i where b2 > '0')) and
       a1 = c1;
select * from t1, t3
where (a1, a2) in (select * from t1 where a1 > '0' UNION select * from t2 where b1 < '9') and
      (c1, c2) in (select c1, c2 from t3
                   where (c1, c2) in (select b1, b2 from t2i where b2 > '0')) and
       a1 = c1;


/******************************************************************************
* Negative tests, where materialization should not be applied.
******************************************************************************/
# UNION in a subquery
explain extended
select * from t3
where c1 in (select a1 from t1 where a1 > '0' UNION select b1 from t2 where b1 < '9');
select * from t3
where c1 in (select a1 from t1 where a1 > '0' UNION select b1 from t2 where b1 < '9');

# correlation
explain extended
select * from t1
where (a1, a2) in (select b1, b2 from t2
                   where b2 in (select c2 from t3 t3a where c1 = a1) or
                         b2 in (select c2 from t3 t3b where c2 LIKE '%03')) and
      (a1, a2) in (select c1, c2 from t3 t3c
                   where (c1, c2) in (select b1, b2 from t2i where b2 > '0' or b2 = a2));

# subquery has no tables
explain extended
select * from t1 where (a1, a2) in (select '1 - 01', '2 - 01');
select * from t1 where (a1, a2) in (select '1 - 01', '2 - 01');
explain extended
select * from t1 where (a1, a2) in (select '1 - 01', '2 - 01' from dual);
select * from t1 where (a1, a2) in (select '1 - 01', '2 - 01' from dual);


/******************************************************************************
* Subqueries in other uncovered clauses.
******************************************************************************/

/* SELECT clause */
select ((a1,a2) IN (select * from t2 where b2 > '0')) IS NULL from t1;

/* GROUP BY clause */
create table columns (col int key);
insert into columns values (1), (2);

explain extended
select * from t1 group by (select col from columns limit 1);
select * from t1 group by (select col from columns limit 1);

explain extended
select * from t1 group by (a1 in (select col from columns));
select * from t1 group by (a1 in (select col from columns));

/* ORDER BY clause */
explain extended
select * from t1 order by (select col from columns limit 1);
select * from t1 order by (select col from columns limit 1);

/******************************************************************************
* Column types/sizes that affect materialization.
******************************************************************************/

# test for BIT fields
create table t1bit (a1 bit(3), a2 bit(3));
create table t2bit (b1 bit(3), b2 bit(3));

insert into t1bit values (b'000', b'100');
insert into t1bit values (b'001', b'101');
insert into t1bit values (b'010', b'110');

insert into t2bit values (b'001', b'101');
insert into t2bit values (b'010', b'110');
insert into t2bit values (b'110', b'111');


explain extended select bin(a1), bin(a2)
from t1bit
where (a1, a2) in (select b1, b2 from t2bit);

select bin(a1), bin(a2)
from t1bit
where (a1, a2) in (select b1, b2 from t2bit);

drop table t1bit, t2bit;

# test mixture of BIT and BLOB
create table t1bb (a1 bit(3), a2 blob(3));
create table t2bb (b1 bit(3), b2 blob(3));

insert into t1bb values (b'000', '100');
insert into t1bb values (b'001', '101');
insert into t1bb values (b'010', '110');

insert into t2bb values (b'001', '101');
insert into t2bb values (b'010', '110');
insert into t2bb values (b'110', '111');

explain extended select bin(a1), a2
from t1bb
where (a1, a2) in (select b1, b2 from t2bb);

select bin(a1), a2
from t1bb
where (a1, a2) in (select b1, b2 from t2bb);

drop table t1bb, t2bb;
drop table t1, t2, t3, t1i, t2i, t3i, columns;

/******************************************************************************
* Test the cache of the left operand of IN.
******************************************************************************/

# Test that default values of Cached_item are not used for comparison
create table t1 (s1 int);
create table t2 (s2 int);
insert into t1 values (5),(1),(0);
insert into t2 values (0), (1);
--sorted_result
select s2 from t2 where s2 in (select s1 from t1);
drop table t1, t2;

create table t1 (a int not null, b int not null);
create table t2 (c int not null, d int not null);
create table t3 (e int not null);

# the first outer row has no matching inner row
insert into t1 values (1,10);
insert into t1 values (1,20);
insert into t1 values (2,10);
insert into t1 values (2,20);
insert into t1 values (2,30);
insert into t1 values (3,20);
insert into t1 values (4,40);

insert into t2 values (2,10);
insert into t2 values (2,20);
insert into t2 values (2,40);
insert into t2 values (3,20);
insert into t2 values (4,10);
insert into t2 values (5,10);

insert into t3 values (10);
insert into t3 values (10);
insert into t3 values (20);
insert into t3 values (30);

explain extended
select a from t1 where a in (select c from t2 where d >= 20);
select a from t1 where a in (select c from t2 where d >= 20);

create index it1a on t1(a);

explain extended
select a from t1 where a in (select c from t2 where d >= 20);
select a from t1 where a in (select c from t2 where d >= 20);

# the first outer row has a matching inner row
insert into t2 values (1,10);

explain extended
select a from t1 where a in (select c from t2 where d >= 20);
select a from t1 where a in (select c from t2 where d >= 20);

# cacheing for IN predicates inside a having clause - here the cached
# items are changed to point to temporary tables.
explain extended
select a from t1 group by a having a in (select c from t2 where d >= 20);
select a from t1 group by a having a in (select c from t2 where d >= 20);

# create an index that can be used for the outer query GROUP BY 
create index iab on t1(a, b);
explain extended
select a from t1 group by a having a in (select c from t2 where d >= 20);
select a from t1 group by a having a in (select c from t2 where d >= 20);

explain extended
select a from t1 group by a
having a in (select c from t2 where d >= some(select e from t3 where max(b)=e));
select a from t1 group by a
having a in (select c from t2 where d >= some(select e from t3 where max(b)=e));
explain extended
select a from t1
where a in (select c from t2 where d >= some(select e from t3 where b=e));
--sorted_result
select a from t1
where a in (select c from t2 where d >= some(select e from t3 where b=e));

drop table t1, t2, t3;

#
# BUG#36133 "Assertion `exec_method != MATERIALIZATION || (exec_method == MATERIALIZATION &&"
#
create table t2 (a int, b int, key(a), key(b));
insert into t2 values (3,3),(3,3),(3,3);
select 1 from t2 where  
    t2.a > 1 
  or 
    t2.a = 3 and not t2.a not in (select t2.b from t2);
drop table t2;

#
# BUG#37896 Assertion on entry of Item_in_subselect::exec on subquery with AND NOT
#
create table t1 (a1 int key);
create table t2 (b1 int);
insert into t1 values (5);

# The queries below do not give correct result when executed with subquery 
# materialization (Bug#40037). Execution with semijoin is correct
explain select min(a1) from t1 where 7 in (select b1 from t2 group by b1);
select min(a1) from t1 where 7 in (select b1 from t2 group by b1);
explain select min(a1) from t1 where 7 in (select b1 from t2);
select min(a1) from t1 where 7 in (select b1 from t2);
drop table t1,t2;

#
# BUG#36752 "subquery materialization produces wrong results when comparing different types"
#
create table t1 (a char(2), b varchar(10));
insert into t1 values ('a',  'aaa');
insert into t1 values ('aa', 'aaaa');

explain select a,b from t1 where b in (select a from t1);
select a,b from t1 where b in (select a from t1);
prepare st1 from "select a,b from t1 where b in (select a from t1)";
execute st1;
execute st1;
drop table t1;

#
# Test for Bug#16603 GROUP BY in a row subquery with a quantifier
#                    when an index is defined on the grouping field

CREATE TABLE t1 (a varchar(5), b varchar(10));
INSERT INTO t1 VALUES
  ('AAA', 5), ('BBB', 4), ('BBB', 1), ('CCC', 2),
  ('CCC', 7), ('AAA', 2), ('AAA', 4), ('BBB', 3), ('AAA', 8);

SELECT * FROM t1 WHERE (a,b) = ANY (SELECT a, max(b) FROM t1 GROUP BY a);
EXPLAIN
SELECT * FROM t1 WHERE (a,b) = ANY (SELECT a, max(b) FROM t1 GROUP BY a);

ALTER TABLE t1 ADD INDEX(a);

SELECT * FROM t1 WHERE (a,b) = ANY (SELECT a, max(b) FROM t1 GROUP BY a);
EXPLAIN
SELECT * FROM t1 WHERE (a,b) = ANY (SELECT a, max(b) FROM t1 GROUP BY a);

DROP TABLE t1;


#
# Bug#36011 Server crash with explain extended on query with dependent
#           subqueries
#

CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1),(2);
EXPLAIN EXTENDED SELECT 1 FROM t1 WHERE 1 IN (SELECT 1 FROM t1 GROUP BY a);
EXPLAIN EXTENDED SELECT 1 FROM t1 WHERE 1 IN (SELECT 1 FROM t1 WHERE a > 3 GROUP BY a);
DROP TABLE t1;


--echo #
--echo # BUG#49630: Segfault in select_describe() with double 
--echo #            nested subquery and materialization
--echo #

CREATE TABLE t1 (t1i int);
CREATE TABLE t2 (t2i int);
CREATE TABLE t3 (t3i int);
CREATE TABLE t4 (t4i int);

INSERT INTO t1 VALUES (1); # Note: t1 must be const table
INSERT INTO t2 VALUES (1),(2);
INSERT INTO t3 VALUES (1),(2);
INSERT INTO t4 VALUES (1),(2);

--echo
EXPLAIN 
SELECT t1i
FROM t1 JOIN t4 ON t1i=t4i  
WHERE (t1i)  IN (  
   SELECT t2i
   FROM t2  
   WHERE (t2i)  IN (  
     SELECT t3i
     FROM t3  
     GROUP BY t3i
     )  
 );

DROP TABLE t1,t2,t3,t4;

--echo #
--echo # BUG#46680 - Assertion failed in file item_subselect.cc, 
--echo #             line 305 crashing on HAVING subquery
--echo #

--echo # Create tables
--echo #

CREATE TABLE t1 (
  pk INT,
  v VARCHAR(1) DEFAULT NULL,
  PRIMARY KEY(pk)
);
CREATE TABLE t2 LIKE t1;
CREATE TABLE t3 LIKE t1;
CREATE TABLE empty1 (a int);

INSERT INTO t1 VALUES (1,'c'),(2,NULL);
INSERT INTO t2 VALUES (3,'m'),(4,NULL);
INSERT INTO t3 VALUES (1,'n');

--echo
--echo #
--echo # 1) Test that subquery materialization is setup for query with
--echo #    premature optimize() exit due to "Impossible WHERE"
--echo #
SELECT MIN(t2.pk)
FROM t2 JOIN t1 ON t1.pk=t2.pk
WHERE 'j'
HAVING ('m') IN ( 
SELECT v
FROM t2);

--echo
EXPLAIN
SELECT MIN(t2.pk)
FROM t2 JOIN t1 ON t1.pk=t2.pk
WHERE 'j'
HAVING ('m') IN ( 
SELECT v
FROM t2);

--echo 
--echo #
--echo # 2) Test that subquery materialization is setup for query with
--echo #    premature optimize() exit due to "No matching min/max row"
--echo #
SELECT MIN(t2.pk)
FROM t2 
WHERE t2.pk>10
HAVING ('m') IN ( 
SELECT v
FROM t2);

--echo
EXPLAIN
SELECT MIN(t2.pk)
FROM t2 
WHERE t2.pk>10
HAVING ('m') IN ( 
SELECT v
FROM t2);

--echo
--echo #
--echo # 3) Test that subquery materialization is setup for query with
--echo #    premature optimize() exit due to "Select tables optimized away"
--echo #
SELECT MIN(pk)
FROM t1
WHERE pk=NULL
HAVING ('m') IN ( 
SELECT v
FROM t2);

--echo
EXPLAIN
SELECT MIN(pk)
FROM t1
WHERE pk=NULL
HAVING ('m') IN ( 
SELECT v
FROM t2);

--echo 
--echo #
--echo # 4) Test that subquery materialization is setup for query with
--echo #    premature optimize() exit due to "No matching row in const table"
--echo #
--echo
SELECT MIN(a)
FROM (SELECT a FROM empty1) tt
HAVING ('m') IN ( 
SELECT v
FROM t2);

--echo
EXPLAIN 
SELECT MIN(a)
FROM (SELECT a FROM empty1) tt
HAVING ('m') IN ( 
SELECT v
FROM t2);

--echo 
--echo #
--echo # 5) Test that subquery materialization is setup for query with
--echo #    premature optimize() exit due to "Impossible WHERE noticed 
--echo #    after reading const tables"
--echo #
SELECT min(t1.pk)
FROM t1
WHERE t1.pk IN (SELECT 1 from t3 where pk>10)
HAVING ('m') IN ( 
SELECT v
FROM t2);

--echo
EXPLAIN
SELECT min(t1.pk)
FROM t1
WHERE t1.pk IN (SELECT 1 from t3 where pk>10)
HAVING ('m') IN ( 
SELECT v
FROM t2);

--echo #
--echo # Cleanup for BUG#46680
--echo #
DROP TABLE IF EXISTS t1,t2,t3,empty1;


--echo #
--echo # BUG#52344 - Subquery materialization: 
--echo #  	     Assertion if subquery in on-clause of outer join
--echo #

CREATE TABLE t1 (i INTEGER);
INSERT INTO t1 VALUES (10);

CREATE TABLE t2 (j INTEGER);
INSERT INTO t2 VALUES (5);

CREATE TABLE t3 (k INTEGER);

EXPLAIN
SELECT i FROM t1 LEFT JOIN t2 ON (j) IN (SELECT k FROM t3);
SELECT i FROM t1 LEFT JOIN t2 ON (j) IN (SELECT k FROM t3);

EXPLAIN
SELECT i FROM t1 LEFT JOIN t2 ON (j) IN (SELECT max(k) FROM t3);
SELECT i FROM t1 LEFT JOIN t2 ON (j) IN (SELECT max(k) FROM t3);

DROP TABLE t1, t2, t3;

--echo # End BUG#52344


#
# Bug #52538 Valgrind bug: Item_in_subselect::init_left_expr_cache()
#
CREATE TABLE t1 (
  pk INTEGER AUTO_INCREMENT,
  col_int_nokey INTEGER,
  col_int_key INTEGER,

  col_varchar_key VARCHAR(1),

  PRIMARY KEY (pk),
  KEY (col_int_key),
  KEY (col_varchar_key, col_int_key)
)
;

INSERT INTO t1 (
  col_int_key, col_int_nokey, col_varchar_key
) 
VALUES
(2, NULL, 'w'),
(9, 7, 'm'),
(3, 9, 'm'),
(9, 7, 'k'),
(NULL, 4, 'r'),
(9, 2, 't'),
(3, 6, 'j'),
(8, 8, 'u'),
(8, NULL, 'h'),
(53, 5, 'o'),
(0, NULL, NULL),
(5, 6, 'k'),
(166, 188, 'e'),
(3, 2, 'n'),
(0, 1, 't'),
(1, 1, 'c'),
(9, 0, 'm'),
(5, 9, 'y'),
(6, NULL, 'f'),
(2, 4, 'd')
;

SELECT table2.col_varchar_key AS field1,
       table2.col_int_nokey AS field2
FROM ( t1 AS table1 LEFT OUTER JOIN t1 AS table2
       ON (table2.col_varchar_key = table1.col_varchar_key  ) ) 
WHERE table1.pk = 6
HAVING  ( field2 ) IN 
( SELECT SUBQUERY2_t2.col_int_nokey AS SUBQUERY2_field2 
  FROM ( t1 AS SUBQUERY2_t1 JOIN t1 AS SUBQUERY2_t2
         ON (SUBQUERY2_t2.col_varchar_key = SUBQUERY2_t1.col_varchar_key ) ) )
ORDER BY field2 
;

drop table t1;


--echo #
--echo # BUG#53103: MTR test ps crashes in optimize_cond() 
--echo #            when running with --debug
--echo #

CREATE TABLE t1(track varchar(15));

INSERT INTO t1 VALUES ('CAD'), ('CAD');

PREPARE STMT FROM
"SELECT 1 FROM t1
  WHERE
        track IN (SELECT track FROM t1
                                    GROUP BY track 
                                      HAVING track>='CAD')";
EXECUTE STMT ;
EXECUTE STMT ;

DEALLOCATE PREPARE STMT;
DROP TABLE t1; 

--echo # End of BUG#53103

--echo #
--echo # BUG#54511 - Assertion failed: cache != 0L in file 
--echo #             sql_select.cc::sub_select_cache on HAVING
--echo #

CREATE TABLE t1 (i int(11));
CREATE TABLE t2 (c char(1));
CREATE TABLE t3 (c char(1));

# These records are needed for the test to fail with MyISAM. The test
# fails with InnoDB without these (difference due to optimization of
# aggregates available only in MyISAM)
INSERT INTO t1 VALUES (1), (2);
INSERT INTO t2 VALUES ('a'), ('b');
INSERT INTO t3 VALUES ('x'), ('y');

SELECT COUNT( i ),i
FROM t1
HAVING ('c')  
  IN (SELECT t2.c FROM (t2 JOIN t3));

DROP TABLE t1,t2,t3;

--echo # End BUG#54511

--echo #
--echo # BUG#56367 - Assertion exec_method != EXEC_MATERIALIZATION...
--echo #             on subquery in FROM
--echo #

CREATE TABLE t1 (a INTEGER);

CREATE TABLE t2 (b INTEGER);
INSERT INTO t2 VALUES (1);

let $query =
SELECT a FROM (
  SELECT t1.* FROM t1 LEFT JOIN t2 ON t1.a > 3 OR t2.b IN (SELECT a FROM t1)
) table1;
eval explain $query;
eval $query;

DROP TABLE t1, t2;

--echo # End BUG#56367

