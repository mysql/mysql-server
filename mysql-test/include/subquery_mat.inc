#
# Hash semi-join regression tests
# (WL#1110: Subquery optimization: materialization)
#

# Force the feature, to test it as much as possible:
set @old_opt_switch=@@optimizer_switch;
set optimizer_switch='subquery_materialization_cost_based=off';
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
--disable_warnings
drop table if exists t1, t2, t3, t1i, t2i, t3i;
drop view if exists v1, v2, v1m, v2m;
--enable_warnings

create table t1 (a1 char(8), a2 char(8)) charset utf8mb4;
create table t2 (b1 char(8), b2 char(8)) charset utf8mb4;
create table t3 (c1 char(8), c2 char(8)) charset utf8mb4;

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
create table t1i (a1 char(8), a2 char(8)) charset utf8mb4;
create table t2i (b1 char(8), b2 char(8)) charset utf8mb4;
create table t3i (c1 char(8), c2 char(8)) charset utf8mb4;
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
explain
select * from t1 where a1 in (select b1 from t2 where b1 > '0');
select * from t1 where a1 in (select b1 from t2 where b1 > '0');

explain
select * from t1 where a1 in (select b1 from t2 where b1 > '0' group by b1);
select * from t1 where a1 in (select b1 from t2 where b1 > '0' group by b1);

explain
select * from t1 where (a1, a2) in (select b1, b2 from t2 where b1 > '0' group by b1, b2);
select * from t1 where (a1, a2) in (select b1, b2 from t2 where b1 > '0' group by b1, b2);

explain
select * from t1 where (a1, a2) in (select b1, min(b2) from t2 where b1 > '0' group by b1);
select * from t1 where (a1, a2) in (select b1, min(b2) from t2 where b1 > '0' group by b1);

# indexed columns
explain
select * from t1i where a1 in (select b1 from t2i where b1 > '0');
select * from t1i where a1 in (select b1 from t2i where b1 > '0');

explain
select * from t1i where a1 in (select b1 from t2i where b1 > '0' group by b1);
select * from t1i where a1 in (select b1 from t2i where b1 > '0' group by b1);

explain
select * from t1i where (a1, a2) in (select b1, b2 from t2i where b1 > '0');
select * from t1i where (a1, a2) in (select b1, b2 from t2i where b1 > '0');

explain
select * from t1i where (a1, a2) in (select b1, b2 from t2i where b1 > '0' group by b1, b2);
select * from t1i where (a1, a2) in (select b1, b2 from t2i where b1 > '0' group by b1, b2);

explain
select * from t1i where (a1, a2) in (select b1, min(b2) from t2i where b1 > '0' group by b1);
select * from t1i where (a1, a2) in (select b1, min(b2) from t2i where b1 > '0' group by b1);

# BUG#31639: Wrong plan for uncorrelated subquery when loose scan is applicable.
explain
select * from t1 where (a1, a2) in (select b1, max(b2) from t2i group by b1);
select * from t1 where (a1, a2) in (select b1, max(b2) from t2i group by b1);

prepare st1 from "explain select * from t1 where (a1, a2) in (select b1, max(b2) from t2i group by b1)";
execute st1;
execute st1;
prepare st2 from "select * from t1 where (a1, a2) in (select b1, max(b2) from t2i group by b1)";
execute st2;
execute st2;

explain
select * from t1 where (a1, a2) in (select b1, min(b2) from t2i where b1 > '0' group by b1);
select * from t1 where (a1, a2) in (select b1, min(b2) from t2i where b1 > '0' group by b1);
-- error 1235
select * from t1 where (a1, a2) in (select b1, min(b2) from t2i limit 1,1);

# materialize the result of ORDER BY
# non-indexed fields
explain
select * from t1 where (a1, a2) in (select b1, b2 from t2 order by b1, b2);
select * from t1 where (a1, a2) in (select b1, b2 from t2 order by b1, b2);
# indexed fields
explain
select * from t1i where (a1, a2) in (select b1, b2 from t2i order by b1, b2);
select * from t1i where (a1, a2) in (select b1, b2 from t2i order by b1, b2);

--echo /******************************************************************************
--echo * Views, UNIONs, several levels of nesting.
--echo ******************************************************************************/
--echo # materialize the result of subquery over temp-table view

--source include/turn_off_only_full_group_by.inc

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

--source include/restore_sql_mode_after_turn_off_only_full_group_by.inc
drop view v1, v2, v1m, v2m;

# nested subqueries, views
explain
select * from t1
where (a1, a2) in (select b1, b2 from t2 where b1 >  '0') and
      (a1, a2) in (select c1, c2 from t3
                   where (c1, c2) in (select b1, b2 from t2i where b2 > '0'));
select * from t1
where (a1, a2) in (select b1, b2 from t2 where b1 >  '0') and
      (a1, a2) in (select c1, c2 from t3
                   where (c1, c2) in (select b1, b2 from t2i where b2 > '0'));

explain
select * from t1i
where (a1, a2) in (select b1, b2 from t2i where b1 >  '0') and
      (a1, a2) in (select c1, c2 from t3i
                   where (c1, c2) in (select b1, b2 from t2i where b2 > '0'));
select * from t1i
where (a1, a2) in (select b1, b2 from t2i where b1 >  '0') and
      (a1, a2) in (select c1, c2 from t3i
                   where (c1, c2) in (select b1, b2 from t2i where b2 > '0'));

explain
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
explain
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
explain
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
explain
select * from t1
where (a1, a2) in (select * from t1 where a1 > '0' UNION select * from t2 where b1 < '9') and
      (a1, a2) in (select c1, c2 from t3
                   where (c1, c2) in (select b1, b2 from t2i where b2 > '0'));
select * from t1
where (a1, a2) in (select * from t1 where a1 > '0' UNION select * from t2 where b1 < '9') and
      (a1, a2) in (select c1, c2 from t3
                   where (c1, c2) in (select b1, b2 from t2i where b2 > '0'));
# as above, with a join conditon between the outer references
explain
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
explain
select * from t3
where c1 in (select a1 from t1 where a1 > '0' UNION select b1 from t2 where b1 < '9');
select * from t3
where c1 in (select a1 from t1 where a1 > '0' UNION select b1 from t2 where b1 < '9');

# correlation
explain
select * from t1
where (a1, a2) in (select b1, b2 from t2
                   where b2 in (select c2 from t3 t3a where c1 = a1) or
                         b2 in (select c2 from t3 t3b where c2 LIKE '%03')) and
      (a1, a2) in (select c1, c2 from t3 t3c
                   where (c1, c2) in (select b1, b2 from t2i where b2 > '0' or b2 = a2));

# subquery has no tables
explain
select * from t1 where (a1, a2) in (select '1 - 01', '2 - 01');
select * from t1 where (a1, a2) in (select '1 - 01', '2 - 01');
explain
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
--source include/turn_off_only_full_group_by.inc

explain
select * from t1 group by (select col from columns limit 1);
select * from t1 group by (select col from columns limit 1);

explain
select * from t1 group by (a1 in (select col from columns));
select * from t1 group by (a1 in (select col from columns));

--source include/restore_sql_mode_after_turn_off_only_full_group_by.inc

/* ORDER BY clause */
explain
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


explain select bin(a1), bin(a2)
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

explain select bin(a1), a2
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

explain
select a from t1 where a in (select c from t2 where d >= 20);
select a from t1 where a in (select c from t2 where d >= 20);

create index it1a on t1(a);

explain
select a from t1 where a in (select c from t2 where d >= 20);
select a from t1 where a in (select c from t2 where d >= 20);

# the first outer row has a matching inner row
insert into t2 values (1,10);

explain
select a from t1 where a in (select c from t2 where d >= 20);
select a from t1 where a in (select c from t2 where d >= 20);

# cacheing for IN predicates inside a having clause - here the cached
# items are changed to point to temporary tables.
explain
select a from t1 group by a having a in (select c from t2 where d >= 20);
select a from t1 group by a having a in (select c from t2 where d >= 20);

# create an index that can be used for the outer query GROUP BY 
create index iab on t1(a, b);
explain
select a from t1 group by a having a in (select c from t2 where d >= 20);
select a from t1 group by a having a in (select c from t2 where d >= 20);

explain
select a from t1 group by a
having a in (select c from t2 where d >= some(select e from t3 where max(b)=e));
select a from t1 group by a
having a in (select c from t2 where d >= some(select e from t3 where max(b)=e));
explain
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

CREATE TABLE t1 (a varchar(5), b varchar(10)) charset utf8mb4;
INSERT INTO t1 VALUES
  ('AAA', 5), ('BBB', 4), ('BBB', 1), ('CCC', 2),
  ('CCC', 7), ('AAA', 2), ('AAA', 4), ('BBB', 3), ('AAA', 8);

SELECT * FROM t1 WHERE (a,b) = ANY (SELECT a, max(b) FROM t1 GROUP BY a);
EXPLAIN
SELECT * FROM t1 WHERE (a,b) = ANY (SELECT a, max(b) FROM t1 GROUP BY a);

ALTER TABLE t1 ADD INDEX(a);

--let $query=SELECT * FROM t1 WHERE (a,b) = ANY (SELECT a, max(b) FROM t1 GROUP BY a)
FLUSH STATUS;
--eval $query
SHOW SESSION STATUS LIKE 'Sort_scan%';
--eval EXPLAIN $query

DROP TABLE t1;


#
# Bug#36011 Server crash with explain on query with dependent
#           subqueries
#

CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1),(2);
EXPLAIN SELECT 1 FROM t1 WHERE 1 IN (SELECT 1 FROM t1 GROUP BY a);
EXPLAIN SELECT 1 FROM t1 WHERE 1 IN (SELECT 1 FROM t1 WHERE a > 3 GROUP BY a);
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
) charset latin1;
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
SELECT i, j FROM t1 LEFT JOIN t2 ON (j) IN (SELECT k FROM t3);
SELECT i, j FROM t1 LEFT JOIN t2 ON (j) IN (SELECT k FROM t3);

EXPLAIN
SELECT i, j FROM t1 LEFT JOIN t2 ON (j) IN (SELECT max(k) FROM t3);
SELECT i, j FROM t1 LEFT JOIN t2 ON (j) IN (SELECT max(k) FROM t3);

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
--source include/turn_off_only_full_group_by.inc

SELECT COUNT( i ),i
FROM t1
HAVING ('c')  
  IN (SELECT t2.c FROM (t2 JOIN t3));

--source include/restore_sql_mode_after_turn_off_only_full_group_by.inc
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

--echo #
--echo # Bug#59833 - materialization=on/off leads to different result set
--echo #             when using IN
--echo #

CREATE TABLE t1 (
  pk int NOT NULL,
  f1 int DEFAULT NULL,
  PRIMARY KEY (pk)
) ENGINE=MyISAM;

CREATE TABLE t2 (
  pk int NOT NULL,
  f1 int DEFAULT NULL,
  PRIMARY KEY (pk)
) ENGINE=MyISAM;

INSERT INTO t1 VALUES (10,0);
INSERT INTO t2 VALUES (10,0),(11,0);

let $query=
SELECT * FROM t1 JOIN t2 USING (f1)
WHERE t1.f1 IN (SELECT t1.pk FROM t1 ORDER BY t1.f1);

eval explain $query;
eval $query;

DROP TABLE t1, t2;

--echo # End Bug#59833

--echo #
--echo # Bug#11852644 - CRASH IN ITEM_REF::SAVE_IN_FIELD ON SELECT DISTINCT
--echo #

CREATE TABLE t1 (
  col_varchar_key varchar(1) DEFAULT NULL,
  col_varchar_nokey varchar(1) DEFAULT NULL,
  KEY col_varchar_key (col_varchar_key)) 
;

INSERT INTO t1 VALUES
('v','v'),('r','r');

CREATE TABLE t2 (
  col_varchar_key varchar(1) DEFAULT NULL,
  col_varchar_nokey varchar(1) DEFAULT NULL,
  KEY col_varchar_key(col_varchar_key)) 
;

INSERT INTO t2 VALUES
('r','r'),('c','c');

CREATE VIEW v3 AS SELECT * FROM t2;

SELECT DISTINCT alias2.col_varchar_key 
FROM t1 AS alias1 JOIN v3 AS alias2 
ON alias2.col_varchar_key = alias1.col_varchar_key
HAVING col_varchar_key IN (SELECT col_varchar_nokey FROM t2)
;

DROP TABLE t1, t2;
DROP VIEW v3;

--echo # End Bug#11852644

--echo
--echo # Bug#12668294 - GROUP BY ON EMPTY RESULT GIVES EMPTY ROW
--echo # INSTEAD OF NULL WHEN MATERIALIZATION ON
--echo

CREATE TABLE t1 (col_int_nokey INT) ENGINE=MEMORY;
CREATE TABLE t2 (col_int_nokey INT) ENGINE=MEMORY;
INSERT INTO t2 VALUES (8),(7);
CREATE TABLE t3 (col_int_nokey INT) ENGINE=MEMORY;
INSERT INTO t3 VALUES (7);

SELECT MIN(t3.col_int_nokey),t1.col_int_nokey AS field3
FROM t3
     LEFT JOIN t1
     ON t1.col_int_nokey
WHERE (194, 200) IN (
                     SELECT SQ4_alias1.col_int_nokey,
                     SQ4_alias2.col_int_nokey
                     FROM t2 AS SQ4_alias1
                          JOIN
                          t2 AS SQ4_alias2
                          ON SQ4_alias2.col_int_nokey = 5
                    )
GROUP BY field3 ;

DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3; 

--echo #
--echo # Bug#13419028 - SUBQUERY MATERIALIZATION NOT USED IN CREATE
--echo # SELECT
--echo #

CREATE TABLE t1(a int);
INSERT INTO t1 values(1),(2);
CREATE TABLE t2(a int);
INSERT INTO t2 values(1),(2);

EXPLAIN SELECT * FROM t1 WHERE a IN (SELECT * FROM t2);
FLUSH STATUS;
SELECT * FROM t1 WHERE a IN (SELECT * FROM t2);
CREATE TABLE t3 SELECT * FROM t1 WHERE a IN (SELECT * FROM t2);
SELECT * FROM t3;
# prove that subquery materialization was used:
SHOW STATUS LIKE "CREATED_TMP_TABLES";
DROP TABLE t1,t2,t3;

--echo #
--echo # Bug#13552968: Extra row with materialization on join + subquery in
--echo #

CREATE TABLE t1 (
  col_varchar_nokey varchar(1) NOT NULL
) ENGINE=MyISAM;

INSERT INTO t1 VALUES ('b');

CREATE TABLE t2 (
  col_varchar_nokey varchar(1) NOT NULL
) ENGINE=MyISAM;

INSERT INTO t2 VALUES ('k');

CREATE TABLE t3 (
  col_varchar_nokey varchar(1) NOT NULL
) ENGINE=MyISAM;

let $query=
SELECT STRAIGHT_JOIN *
FROM t1 LEFT JOIN t2 ON t1.col_varchar_nokey IN (SELECT col_varchar_nokey
                                                 FROM t3);

eval explain $query;
eval $query;

DROP TABLE t1, t2, t3;

--echo # End of test for bug#13552968

--echo #
--echo # Bug#13591383: Assertion !(*tab->on_expr_ref && .. && is_expensive())
--echo # in join_read_const_table()
--echo #

CREATE TABLE t1 (v INTEGER) ENGINE=MyISAM;
INSERT INTO t1 VALUES(1);

CREATE TABLE t2 (v INTEGER) ENGINE=MyISAM;

SELECT *
FROM t1 LEFT JOIN t2
     ON t2.v IN(SELECT v FROM t1);

DROP TABLE t1, t2;

--echo # End of test for bug#13591383.

--echo #
--echo # Bug#13607423: Assertion !(*tab->on_expr_ref->is_expensive())
--echo # in join_read_const_table()
--echo #

CREATE TABLE t1 (
  pk int NOT NULL,
  col_int_nokey int DEFAULT NULL,
  col_int_key int DEFAULT NULL,
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key)
) ENGINE=MyISAM;

INSERT INTO t1 VALUES (1,2,4), (2,150,62);

CREATE TABLE t2 (
  pk int NOT NULL,
  col_int_key int DEFAULT NULL,
  PRIMARY KEY (pk)
) ENGINE=MyISAM;

INSERT INTO t2 VALUES (1,7);

let $query=
SELECT table1.pk, table2.pk
FROM t2 AS table1 LEFT JOIN t2 AS table2
     ON table2.pk = table1.pk AND
        table2.col_int_key IN
         (SELECT col_int_key
          FROM t1 AS innr
          WHERE innr.col_int_nokey > innr.col_int_nokey
          GROUP BY col_int_key
          HAVING COUNT(*) > 0
         );

eval explain $query;
FLUSH STATUS;
eval $query;
SHOW SESSION STATUS LIKE 'Sort_scan%';

DROP TABLE t1, t2;

--echo # End of test for bug#13607423.

--echo
--echo Test of WL#6094 "Allow subquery materialization in NOT IN if all
--echo columns are not nullable"
--echo

# We want to test WL#6094 only, not WL#6095, so we use 2 columns.

create table t1(a int not null);
create table t2(a int not null);
insert into t1 values(1),(2);
insert into t2 values(1),(2);

--echo Test in SELECT list

--echo
let $query=select a, (a,a) in (select a,a from t2) from t1;

--echo cols not nullable => subq materialization
eval explain $query;
eval $query;

--echo
let $query=select t1.a, t2.a, (t1.a,t1.a) in (select a,a from t2 as t3)
from t1 join t2 on t1.a+t2.a=1000;
--echo cols not nullable => subq materialization
eval explain $query;
eval $query;

--echo
let $query=select t1.a, t2.a, (t2.a,t2.a) in (select a,a from t2 as t3)
from t1 left join t2 on t1.a+t2.a=1000;

--echo t2.a is not nullable, but in the query it may appear as NULL
--echo as it's in an outer join. So, no materialization.
eval explain $query;
eval $query;

--echo
alter table t2 modify a int;
let $query=select t1.a, t2.a, (t1.a,t1.a) in (select a,a from t2 as t3)
from t1 join t2 on t1.a+t2.a=1000;
--echo two nullable inner cols => no subq materialization
eval explain $query;
eval $query;
alter table t2 modify a int not null;

--echo
--echo Test in WHERE
--echo
let $query=select t1.a, t2.a
from t1 join t2 on t1.a+t2.a=3
where (t2.a,t2.a) in (select a,a from t2 as t3);
--echo top-level => subq materialization. With one exception: if
--echo semijoin is enabled in @@optimizer_switch, semijoin is chosen,
--echo then rejected (due to outer join), and in that case, the
--echo fallback is IN->EXISTS, subq-materialization is not tried...
eval explain $query;
eval $query;

--echo
let $query=select t1.a, t2.a
from t1 join t2 on t1.a+t2.a=3
where (t2.a,t2.a) not in (select a,a from t2 as t3);
--echo cols not nullable => subq materialization
eval explain $query;
eval $query;

drop table t1,t2;

--echo
--echo Test of WL6095 "Allow subquery materialization in NOT IN if
--echo single-column subquery"
--echo

# We want to test WL#6095 only, not WL#6094, so we use nullable columns.

create table t1(a int null);
create table t2(a int null);
insert into t1 values(1),(2);
insert into t2 values(1),(2);

--echo
let $query=select a, a in (select a from t2) from t1;

--echo one col => subq materialization
eval explain $query;
eval $query;

--echo
let $query=select t1.a, t2.a, t2.a in (select * from t2 as t3)
from t1 left join t2 on t1.a+t2.a=1000;

--echo t2.a is not nullable, but in the query it may appear as NULL
--echo as it's in an outer join. But there is only one inner column so
--echo materialization is possible
eval explain $query;
eval $query;

--echo
let $query=select t1.a, t2.a, (t2.a,t2.a) in (select a,a from t2 as t3)
from t1 left join t2 on t1.a+t2.a=1000;

--echo _two_ outer columns, nullable => no materialization
eval explain $query;
eval $query;

drop table t1,t2;

--echo
--echo Test in HAVING

create table t1(a int, b int);
create table t2(a int);
insert into t1 values(1,1),(1,2),(1,3),(2,1),(2,2),(2,3);
insert into t2 values(10),(20);

let $query=select t1.a as z, sum(t1.b) from t1 group by t1.a
having (z in (select * from t2)) is null;

--echo no NULLs.

eval explain $query;
eval $query;

--echo one outer NULL
insert into t1 values(null,null);

eval explain $query;
eval $query;

--echo one outer NULL and one inner NULL
insert into t2 values(null);

eval explain $query;
eval $query;

--echo one inner NULL
delete from t1 where a is null;

eval explain $query;
eval $query;

drop table t1,t2;

--echo
--echo Verify that an inner NULL is looked up only once (result is
--echo cached).

create table t1(a int);
create table t2(a int);
insert into t1 values(1),(2),(3),(4),(5),(6);
insert into t1 select * from t1; # t1 has 12 rows
insert into t2 values(10),(20),(NULL);

let $query=select a, (a in (select * from t2)) from t1;

eval explain $query;
flush status;
eval $query;
--echo There will be one look-up in the temporary table for each row
--echo of t1 (12), plus one additional look-up to check whether table
--echo contains a NULL value.
show status like "handler_read_key";

drop table t1,t2;

--echo #
--echo # Bug#13495157 - SUBQUERY MATERIALIZATION NOT USED FOR CERTAIN
--echo # STATEMENTS
--echo #

CREATE TABLE t1(a INT);
INSERT INTO t1 VALUES(1),(2),(3);
CREATE TABLE t2(a INT);
INSERT INTO t2 VALUES(1),(2),(4);

--echo # subquery materialization used for SELECT:
EXPLAIN SELECT * FROM t1 WHERE a IN (SELECT * FROM t2 WHERE a <> 2);
SELECT * FROM t1 WHERE a IN (SELECT * FROM t2 WHERE a <> 2);

--echo # Also used for INSERT SELECT:
# a) all different tables:
CREATE TABLE t3 SELECT * FROM t1;
EXPLAIN INSERT INTO t3 SELECT * FROM t1 WHERE a IN (SELECT * FROM t2 WHERE a <> 2);
INSERT INTO t3 SELECT * FROM t1 WHERE a IN (SELECT * FROM t2 WHERE a <> 2);
--sorted_result
SELECT * FROM t3;

# b) insert into subquery's selected table
EXPLAIN INSERT INTO t2 SELECT * FROM t1 WHERE a IN (SELECT * FROM t2 WHERE a <> 2);
INSERT INTO t2 SELECT * FROM t1 WHERE a IN (SELECT * FROM t2 WHERE a <> 2);
--sorted_result
SELECT * FROM t2;

# c) insert into subquery's and query's selected table
EXPLAIN INSERT INTO t2 SELECT * FROM t2 WHERE a IN (SELECT * FROM t2 WHERE a <> 2);
INSERT INTO t2 SELECT * FROM t2 WHERE a IN (SELECT * FROM t2 WHERE a <> 2);
--sorted_result
SELECT * FROM t2;

--echo # Not used for single-table UPDATE, DELETE:
# a) all different tables
EXPLAIN SELECT * FROM t2 WHERE a IN (SELECT * FROM t1);
EXPLAIN UPDATE t2 SET a=a-1 WHERE a IN (SELECT * FROM t1);
UPDATE t2 SET a=a-1 WHERE a IN (SELECT * FROM t1);
--sorted_result
SELECT * FROM t2;
EXPLAIN DELETE FROM t2 WHERE a IN (SELECT * FROM t1);
DELETE FROM t2 WHERE a IN (SELECT * FROM t1);
--sorted_result
SELECT * FROM t2;

# b) update/delete in subquery's selected table: forbidden
--error ER_UPDATE_TABLE_USED
EXPLAIN UPDATE t2 SET a=a-1 WHERE a IN (SELECT * FROM t2);
--error ER_UPDATE_TABLE_USED
EXPLAIN DELETE FROM t2 WHERE a IN (SELECT * FROM t2);

# Put some content so that future queries have rows to modify:
UPDATE t2 SET a=3 WHERE a=0;

--echo # Used for multi-table UPDATE, DELETE:

# a) all different tables
EXPLAIN SELECT * FROM t2,t3 WHERE t2.a IN (SELECT * FROM t1 WHERE a <> 2);
EXPLAIN UPDATE t2,t3 SET t2.a=t2.a-2 WHERE t2.a IN (SELECT * FROM t1 WHERE a <> 2);
UPDATE t2,t3 SET t2.a=t2.a-2 WHERE t2.a IN (SELECT * FROM t1 WHERE a <> 2);
--sorted_result
SELECT * FROM t2;
EXPLAIN DELETE t2.* FROM t2,t3 WHERE t2.a IN (SELECT * FROM t1 WHERE a <> 2);
DELETE t2.* FROM t2,t3 WHERE t2.a IN (SELECT * FROM t1 WHERE a <> 2);
--sorted_result
SELECT * FROM t2;

# b) update/delete in subquery's selected table: forbidden
--error ER_UPDATE_TABLE_USED
EXPLAIN UPDATE t2,t3 SET t2.a=10 WHERE t2.a IN (SELECT * FROM t2 WHERE a <> 2);
--error ER_UPDATE_TABLE_USED
EXPLAIN DELETE t2.* FROM t2,t3 WHERE t2.a IN (SELECT * FROM t2 WHERE a <> 2);

DROP TABLE t1,t2,t3;

--echo #
--echo # Test that subquery materialization only does one lookup: does
--echo # not try to read the next row if the first row failed the
--echo # subquery's WHERE. We use a case where index lookup is not
--echo # enough to satisfy IN(), because index has length two when the
--echo # outer value has length three, and thus the post-filtering
--echo # WHERE added by subselect_hash_sj_engine::setup() makes the
--echo # decision.
--echo #
create table t1 (a varchar(3));
create table t2 (a varchar(2));
insert into t1 values('aaa'), ('aaa');
insert into t2 values('aa'), ('aa');
let $query=select * from t1 where a in (select a from t2);
eval explain $query;
flush status;
eval $query;
show status like "handler_read%";
drop table t1,t2;

--echo #
--echo # Bug#13655791 DIFFERENT RESULT WITH WL6094 ON QUERY WITH XOR
--echo # IN WHERE CLAUSE + MYISAM
--echo #

CREATE TABLE t1 (
  pk int NOT NULL,
  col_varchar_nokey varchar(1) DEFAULT NULL,
  PRIMARY KEY (pk)
);

INSERT INTO t1 VALUES (10,'x');

CREATE TABLE t2 (
  pk int NOT NULL,
  col_varchar_nokey varchar(1) DEFAULT NULL,
  PRIMARY KEY (pk)
);

INSERT INTO t2 VALUES (1,'v'), (5,'x'), (11,'z'), (12,'c'), (15,'y');

CREATE TABLE t3 (
  pk int NOT NULL,
  col_int_key int DEFAULT NULL,
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key)
);

INSERT INTO t3 VALUES (10,8);

CREATE TABLE t4 (
  pk int NOT NULL,
  col_varchar_nokey varchar(1) DEFAULT NULL,
  PRIMARY KEY (pk)
);

INSERT INTO t4 VALUES (1,'x');

let $query=
SELECT outr.pk, outr.col_varchar_nokey, outr2.col_varchar_nokey
FROM t2 AS outr2
  JOIN t4 AS outr
  ON (outr2.col_varchar_nokey > outr.col_varchar_nokey)
WHERE
  outr.col_varchar_nokey IN (
    SELECT innr.col_varchar_nokey
    FROM t3 AS innr2
      LEFT JOIN t1 AS innr
      ON (innr2.col_int_key >= innr.pk)
  )
  XOR outr.pk < 6
;

eval EXPLAIN $query;
FLUSH STATUS;
eval $query;
SHOW STATUS LIKE "HANDLER_READ%";

DROP TABLE t1,t2,t3,t4;

--echo #
--echo # Bug#13727407: Assert !item->const_item() || !item->not_null_tables()
--echo #

CREATE TABLE t1 (
  col_int_key INT,
  KEY col_int_key (col_int_key)
);

INSERT INTO t1 VALUES (1);

CREATE TABLE t2 (
  col_int_key INT,
  col_time_key TIME,
  col_datetime_nokey DATETIME,
  KEY col_int_key (col_int_key),
  KEY col_time_key (col_time_key)
);

INSERT INTO t2 VALUES
 (7,'14:03:03','2001-11-28 00:50:27'), (1,'01:46:09','2007-10-09 19:53:04');

let $query=
SELECT col_datetime_nokey AS x
FROM t2 AS outr
WHERE col_int_key IN (
  SELECT STRAIGHT_JOIN col_int_key
  FROM t1
) AND outr.col_int_key = 0
HAVING x = '2000-09-09'
ORDER BY col_time_key;

eval EXPLAIN $query;
eval $query;

DROP TABLE t1, t2;

--echo #
--echo # Bug#13838501 ASSERTION `TABLE->FILE->INITED' FAILED IN
--echo # SUBSELECT_HASH_SJ_ENGINE::EXEC
--echo #

CREATE TABLE t1
(c1 bigint,c2 char,pk INT,c3 char,c4 int,c5 INT,key (c5))
ENGINE=InnoDB;
INSERT INTO t1 VALUES (763078661862588416,0,1,'',1,'');
CREATE TABLE t2 (c4k int,c4 int,cminnuk INT,key (cminnuk)) ENGINE=InnoDB;
INSERT INTO t2 VALUES(0,'','');
CREATE TABLE t3
(c4 int,pk INT,c1 bigint,cyk year,cy year,key (cyk))
ENGINE=InnoDB;
INSERT INTO t3 VALUES(0,8,'',0,'');
let $query=
SELECT o.c2 AS x FROM t1 AS o
WHERE o.c1 IN
       (SELECT innr.c4 AS y
        FROM t2 AS innr2 JOIN t3 AS innr
             ON (innr2.c4k=innr.c4)
        WHERE innr.c1=6 OR NOT innr.c1=innr.pk
        ORDER BY innr.c4)
      AND o.c4=7 XOR o.pk=3 ORDER BY o.pk;
eval EXPLAIN $query;
eval $query;

DROP TABLE t1,t2,t3;

--echo # End of 5.6 tests

--echo #
--echo # Bug #18770217 	DIFFERENT RESULT WITH SP ON 2ND EXECUTION OF QUERY WITH STRAIGHT_JOIN IN NOT IN
--echo #

# If IN->EXISTS is used, the WHERE clause of the EXISTS subquery looks
# like:
# alias1.col_varchar_key=sq1_alias1.col_varchar_nokey AND (scalar subquery)
# and because the equality is false for all rows, the scalar subquery
# is not evaluted - produces no ER_SUBQUERY_NO_1_ROW.

if (`select locate('materialization=on', @@optimizer_switch) > 0`) 
{

CREATE TABLE t1 (
  col_int_key INT,
  col_varchar_key VARCHAR(1),
  col_varchar_nokey VARCHAR(1),
  KEY col_int_key (col_int_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
) charset utf8mb4;

INSERT INTO t1 VALUES (8,'x','x');
INSERT INTO t1 VALUES (NULL,'x','x');

CREATE TABLE t2 (
  col_int_key INT,
  col_varchar_key VARCHAR(1),
  col_varchar_nokey VARCHAR(1),
  KEY col_int_key (col_int_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
) charset utf8mb4;

INSERT INTO t2 VALUES (0,'x','x');

CREATE TABLE t3 (
  col_int_key INT,
  col_varchar_key VARCHAR(1),
  col_varchar_nokey VARCHAR(1),
  KEY col_int_key (col_int_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
) charset utf8mb4;

INSERT INTO t3 VALUES (8,'g','g');

CREATE VIEW view_d AS SELECT * FROM t2;

let $query=
SELECT alias2.col_int_key
FROM t1 AS alias1 , t2 AS alias2
WHERE alias1.col_varchar_key NOT IN (
  SELECT sq1_alias1.col_varchar_nokey
  FROM t3 AS sq1_alias1
    JOIN ( t1 AS sq1_alias2
      STRAIGHT_JOIN view_d AS sq1_alias3 )
  WHERE (
    SELECT col_int_key
    FROM t1 AS c_sq1_alias1
    WHERE c_sq1_alias1.col_varchar_key = sq1_alias3.col_varchar_nokey
  )
)
;

# Show materialization is used
eval EXPLAIN $query;

--error ER_SUBQUERY_NO_1_ROW
eval $query;

eval CREATE PROCEDURE stored_proc_2581 () LANGUAGE SQL $query;

--error ER_SUBQUERY_NO_1_ROW
CALL stored_proc_2581;
--error ER_SUBQUERY_NO_1_ROW
CALL stored_proc_2581;
DROP PROCEDURE stored_proc_2581;

eval PREPARE s FROM '$query';
--error ER_SUBQUERY_NO_1_ROW
EXECUTE s;
--error ER_SUBQUERY_NO_1_ROW
EXECUTE s;

DROP VIEW view_d;
DROP TABLE t1,t2,t3;

}

--echo #
--echo # Bug#19297190 NOT IN DOESN'T RETURN EXPECTED RESULT
--echo #

CREATE TABLE t1 (a VARCHAR(500) CHARACTER SET UTF8) ENGINE=INNODB;

# Fill the table with 32 distinct long rows (>32kB of data)
SET @str= repeat("a",450);
SET @num=1000;
INSERT INTO t1 VALUES (CONCAT((@num:=@num+1), @str));
INSERT INTO t1 SELECT CONCAT((@num:=@num+1), @str) FROM t1;
INSERT INTO t1 SELECT CONCAT((@num:=@num+1), @str) FROM t1;
INSERT INTO t1 SELECT CONCAT((@num:=@num+1), @str) FROM t1;
INSERT INTO t1 SELECT CONCAT((@num:=@num+1), @str) FROM t1;
INSERT INTO t1 SELECT CONCAT((@num:=@num+1), @str) FROM t1;
SELECT COUNT(*) FROM t1;

ANALYZE TABLE t1;

let $query=
SELECT COUNT(*)
FROM t1
WHERE t1.a NOT IN (
  SELECT t2.a FROM t1 as t2
);

#  If subq-mat is used, this will force MEMORY->disk-based tmp table conversion:
set @save_heap_size= @@max_heap_table_size;
set @@max_heap_table_size= 16384;

# Test both disk-based engines:
set @saved_engine=@@internal_tmp_disk_storage_engine;
set global internal_tmp_disk_storage_engine = 'myisam';

# Subq-materialization can be used and gives correct result:
eval EXPLAIN $query;
eval $query;

set global internal_tmp_disk_storage_engine = 'innodb';
eval EXPLAIN $query;
eval $query;

DROP TABLE t1;
set global internal_tmp_disk_storage_engine = @saved_engine;
set @@max_heap_table_size= @save_heap_size;

--echo #
--echo # Bug#19805761 ASSERTION FAILURE IN SUBSELECT_HASH_SJ_ENGINE::EXEC WITH SUBQUERY IN LEFT ARGUME
--echo #

CREATE TABLE t1(a INT) ENGINE=INNODB;
INSERT INTO t1 VALUES(1);
CREATE TABLE t2 LIKE t1;
ANALYZE TABLE t1,t2;

let $query=
SELECT (SELECT 1,2 FROM t2) NOT IN (SELECT 1,2 FROM t1) FROM t1;

eval EXPLAIN $query;
eval $query;

DROP TABLE t1,t2;

--echo #
--echo # Bug#20729351 ASSERTION FAILED: ITEM_IN->LEFT_EXPR->ELEMENT_INDEX(0)->MAYBE_NULL
--echo #

CREATE TABLE t1 (a LONGBLOB) ENGINE=INNODB;
INSERT INTO t1 VALUES ('a'), ('a'), ('a');
CREATE TABLE t2 (a INT) ENGINE=INNODB;
INSERT INTO t2 VALUES(1), (1), (1), (1);
ANALYZE TABLE t1, t2;
SELECT
 (SELECT NULL IN (NULL) FROM t1 WHERE a) =
  ANY(SELECT 1 FROM t2)
FROM t1;
DROP TABLE t1, t2;

--echo #
--echo # Bug#22089623 ASSERTION IN ITEM_FUNC_TRIG_COND::VAL_INT()
--echo #              WITH SUBQUERY ON LEFT SIDE OF IN
--echo #

--let $query= SELECT (SELECT NULL, NULL) IN (SELECT 1, 2)
--eval EXPLAIN $query
--eval $query
--let $query= SELECT (SELECT 1, 1) IN (SELECT 1, 2)
--eval EXPLAIN $query
--eval $query
--let $query= SELECT (SELECT 1, 2) IN (SELECT 1, 2)
--eval EXPLAIN $query
--eval $query
--let $query= SELECT (SELECT NULL, 2) IN (SELECT 1, 2)
--eval EXPLAIN $query
--eval $query
--let $query= SELECT (SELECT 1, NULL) IN (SELECT 1, 2)
--eval EXPLAIN $query
--eval $query
--let $query= SELECT (SELECT NULL, 1) IN (SELECT 1, 2)
--eval EXPLAIN $query
--eval $query
--let $query= SELECT (SELECT 2, NULL) IN (SELECT 1, 2)
--eval EXPLAIN $query
--eval $query
--let $query= SELECT (SELECT 1, 2 UNION SELECT 1, 2) IN (SELECT 1, 2)
--eval EXPLAIN $query
--eval $query
--let $query= SELECT (SELECT 1, 1 UNION SELECT 1, 1) IN (SELECT 1, 2)
--eval EXPLAIN $query
--eval $query
--let $query= SELECT (SELECT NULL, NULL UNION SELECT NULL, NULL) IN (SELECT 1, 2)
--eval EXPLAIN $query
--eval $query
--let $query= SELECT (SELECT 1, 1 UNION SELECT 2, 2) IN (SELECT 1, 2)
--eval EXPLAIN $query
--error ER_SUBQUERY_NO_1_ROW
--eval $query
--let $query= SELECT (SELECT 1, 2 WHERE FALSE UNION SELECT 1, 2 WHERE FALSE) IN (SELECT 1, 2)
--eval EXPLAIN $query
--eval $query
--let $query= SELECT (SELECT 1, 2 UNION ALL SELECT 1, 2) IN (SELECT 1, 2)
--eval EXPLAIN $query
--error ER_SUBQUERY_NO_1_ROW
--eval $query
--let $query= SELECT (SELECT 1, 1 UNION ALL SELECT 1, 1) IN (SELECT 1, 2)
--eval EXPLAIN $query
--error ER_SUBQUERY_NO_1_ROW
--eval $query
--let $query= SELECT (SELECT NULL, NULL UNION ALL SELECT NULL, NULL) IN (SELECT 1, 2)
--eval EXPLAIN $query
--error ER_SUBQUERY_NO_1_ROW
--eval $query
--let $query= SELECT (SELECT 1, 1 UNION ALL SELECT 2, 2) IN (SELECT 1, 2)
--eval EXPLAIN $query
--error ER_SUBQUERY_NO_1_ROW
--eval $query
--let $query= SELECT (SELECT 1, 2 WHERE FALSE UNION ALL SELECT 1, 2 WHERE FALSE) IN (SELECT 1, 2)
--eval EXPLAIN $query
--eval $query

--echo #
--echo # Bug#20536077: WRONG RESULT FOR SELECT NULL,NULL IN (SUBQUERY)
--echo #

--echo # The following queries correctly returned NULL.
CREATE TABLE s (s INT) ENGINE=InnoDB;
INSERT INTO s VALUES(1);
SELECT NULL IN (SELECT 1);
SELECT NULL = (SELECT 1);
SELECT NULL = (SELECT 1 FROM s);
SELECT (NULL, NULL) IN (SELECT 1, 2);

--echo # The following queries returned 0 instead of NULL.
SELECT NULL IN (SELECT 1 FROM s);
SELECT (SELECT NULL, NULL FROM DUAL) IN (SELECT 1, 2 FROM s) FROM s;
SELECT (SELECT NULL,NULL) IN (SELECT 1, 2);
CREATE TABLE u(a INT, b INT) ENGINE=MyISAM;
INSERT INTO u VALUES (NULL, NULL);
SELECT (SELECT * FROM u) IN (SELECT 1, 2 FROM s) FROM s;
SELECT (SELECT * FROM u) IN (SELECT 1, 2);

DROP TABLE s, u;


set @@optimizer_switch=@old_opt_switch;
SET sql_mode = default;
