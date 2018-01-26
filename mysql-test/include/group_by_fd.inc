--source include/have_optimizer_trace.inc

let $DEFAULT_TRACE_MEM_SIZE=1048576; # 1MB
eval set optimizer_trace_max_mem_size=$DEFAULT_TRACE_MEM_SIZE;

set end_markers_in_json=on;
set optimizer_trace="enabled=on";

SET @old_sql_mode = @@sql_mode;
SET @@sql_mode='ONLY_FULL_GROUP_BY';

--echo #
--echo # Bug#16021396 ONLY_FULL_GROUP_BY REJECTS VALID QUERY USING VIEW
--echo #
create table t1(a int, b int, c int) engine=InnoDB;
let $count=2;
while($count)
{
if ($count == 2)
{
let $viewtype=merge;
}
if ($count == 1)
{
let $viewtype=temptable;
}
dec $count;
eval create algorithm=$viewtype view v1 as select t1.a*2 as a, t1.b*2 as b, t1.c*2 as c from t1;
show create view v1;
select sin(b) as z from t1 group by sin(b);
select sin(b) as z from v1 group by sin(b);
select sin(b) as z from t1 group by b;
select sin(b) as z from v1 group by b;
select sin(b) as z from v1 group by z;
drop view v1;
}
drop table t1;

--echo # From testcase of Bug#16903135:

CREATE TABLE group_by_test2 (
 id int unsigned primary key,
 cat int unsigned not null,
 name varchar(10),
 num int unsigned
);

INSERT INTO group_by_test2 (id,cat,name,num) VALUES
(1,10,'foo',2),
(2,11,'foo',1),
(3,22,'bar',3),
(4,23,'bar',7),
(5,34,'test',7);

let $query=
SELECT
 cat,
 name,
 SUM(num)
FROM
 group_by_test2
GROUP BY
 cat;

--error ER_WRONG_FIELD_WITH_GROUP
eval $query;

ALTER TABLE group_by_test2 ADD UNIQUE INDEX (cat);
eval $query;
--error ER_WRONG_FIELD_WITH_GROUP
eval $query WITH ROLLUP;
--echo # Expressions of group columns are ok:
SELECT
 cat,
length(cat),
 SUM(num)
FROM
 group_by_test2
GROUP BY
 cat
WITH ROLLUP;

DROP TABLE group_by_test2;

--echo # Test from Bug #18993257 SELECT AGGR + NON-AGGR FROM JOIN WITH VIEW IS NOT REJECTED BY ONLY_FULL_GROUP_BY

CREATE TABLE t1 (
  col_int_key INT,
  col_varchar_key VARCHAR(1)
);

CREATE TABLE t2 (
    pk INTEGER,
    col_int_key INTEGER
);

CREATE VIEW view_b AS SELECT * FROM t2;

--error ER_MIX_OF_GROUP_FUNC_AND_FIELDS
SELECT MIN( alias2.col_int_key ),
       alias2.col_int_key
FROM t1 AS alias1, t2 AS alias2
WHERE alias1.col_int_key IS NULL;

--echo # Same with view:
--error ER_MIX_OF_GROUP_FUNC_AND_FIELDS
SELECT MIN( alias2.col_int_key ),
       alias2.col_int_key
FROM t1 AS alias1, view_b AS alias2
WHERE alias1.col_int_key IS NULL;

DROP TABLE t1, t2;
DROP VIEW view_b;

--echo #
--echo # WL#2489; Recognizing some functional dependencies
--echo #

--source include/show_json_object.inc
let $show_trace=
SELECT show_json_object('"functional_dependencies_of_GROUP_columns": {', TRACE)
FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;

create table t1(
a int,
b int not null,
c int not null,
d int,
unique key(b,c),
unique key(b,d)
);

--error ER_WRONG_FIELD_WITH_GROUP
select sin(a) as z from t1 group by b;
--error ER_WRONG_FIELD_WITH_GROUP
select sin(a) as z from t1 group by d,b;

--echo # {b,c} UNIQUE NOT NULL => {c,b}->a
select sin(a) as z from t1 group by c,b;
select sin(a+b*c) as z from t1 group by c,b;
--echo # In PS mode, we see nothing below, because only_full_group_by
--echo # checks are done at PREPARE, whereas trace below is from EXECUTE.
eval $show_trace;

--echo # With outer references:
--error ER_WRONG_FIELD_WITH_GROUP
select (select sin(a)) as z from t1 group by d,b;
select (select sin(a)) as z from t1 group by c,b;
--echo # If key columns are in function, functional dependency disappears
--error ER_WRONG_FIELD_WITH_GROUP
select sin(a) as z from t1 group by c*2,b*2;

--echo # FDs are recognized, like in SQL standard:
--echo # b=2 => c->{b,c} => c->a as (b,c) is unique not null.
select sin(a) as z from t1 where b=2 group by c;
eval $show_trace;

--echo # t2.a=t1.a => {t1.b,t1.c}->t2.a
select sin(t2.a) as z from t1, t1 as t2
 where t2.a=t1.a group by t1.b,t1.c;
eval $show_trace;

--echo # t2.b=t1.b and t2.c=t1.c => {t1.b,t1.c}->{all cols of t2}
select sin(t2.a) as z from t1, t1 as t2
 where t2.b=t1.b and t2.c=t1.c group by t1.b,t1.c;
eval $show_trace;

--echo # t2.b=t1.b and t2.c=t1.c => {t1.b,t1.c}->{all cols of t2}
--echo # Moreover, {t1.b,t1.c}->{t1.d}.
--echo # So t3.b=t2.b and t3.c=t1.d => {t1.b,t1.c}->{all cols of t3}.
select t3.a from t1, t1 as t2, t1 as t3
where
 t3.b=t2.b and t3.c=t1.d and
 t2.b=t1.b and t2.c=t1.c
group by t1.b,t1.c;
eval $show_trace;

--echo # 3 tables:
--echo # {t1.b,t1.c}->{t1.*}->{t2.b,t2.c}->{t2.*}->{t3.pk}->{t3.b}
create table t3(pk int primary key, b int);
select t3.b from  t1,t1 as t2,t3
  where t3.pk=t2.d and t2.b=t1.b and t2.c=t1.a
  group by t1.b,t1.c;
drop table t3;

--echo # With subq
select (select t1.b from t1
  where t2.b=t1.b
  group by t1.a) from t1 as t2;

--echo # Outer join.
create table t2 like t1;
delete from t1;
insert into t1 (a,b) values(1,10),(2,20);
insert into t2 (a,b) values(1,-10);
--echo # In result, t2.a is NULL for both rows, values of t1.a are 1 and 2
select t1.a,t2.a from t1 left join t2 on t2.a=t1.a and t2.b=t1.b;
--echo # So this query would choose one arbitrary value of t1.a - wrong:
--error ER_WRONG_FIELD_WITH_GROUP
select t1.a from t1 left join t2 on t2.a=t1.a and t2.b=t1.b group by t2.a;

--echo # Also problem for FD with constant:
select t1.a,t2.a from t1 left join t2 on 42=t1.a and t2.b=t1.b;
--error ER_WRONG_FIELD_WITH_GROUP
select t1.a from t1 left join t2 on 42=t1.a and t2.b=t1.b group by t2.a;

--error ER_WRONG_FIELD_WITH_GROUP
select t1.a from t1 left join t2 on t2.b=t1.a group by t2.b;
--error ER_WRONG_FIELD_WITH_GROUP
select t1.a from t1 left join t2 on 42=t1.a group by t2.a;

--error ER_WRONG_FIELD_WITH_GROUP
select t1.c from t1 left join t2 on t1.a=t1.c group by t1.a;
--error ER_WRONG_FIELD_WITH_GROUP
select t1.b from t1 left join t2 on t1.c=t1.b and t1.a=t1.c group by t1.a;
select t1.a,t2.c from t1 left join t2 on t1.a=t2.c
and cos(t2.c+t2.b)>0.5 and sin(t1.a+t2.d)<0.9 group by t1.a;

--echo # with keys:
select t1.a,t2.d from t1 left join t2 on t1.a=t2.c and t1.d=t2.b
and cos(t2.c+t2.b)>0.5 and sin(t1.a+t2.d)<0.9 group by t1.a,t1.d;

--echo # with non-determinism:
--error ER_WRONG_FIELD_WITH_GROUP
select t1.a,t2.c from t1 left join t2 on t1.a=t2.c
and cos(t2.c+rand())>0.5 group by t1.a;
select t1.a, ANY_VALUE(t2.c) from t1 left join t2 on t1.a=t2.c
and cos(t2.c+rand())>0.5 group by t1.a;

--echo # with parameter:
prepare s from 'select t1.a,t2.c from t1 left join t2 on t1.a=t2.c
and cos(t2.c+ ? )>0.5 group by t1.a';
execute s using @dummy;

--echo # No OR
--error ER_WRONG_FIELD_WITH_GROUP
select t1.a,t2.c from t1 left join t2 on t1.a=t2.c
and cos(t2.c+t2.b)>0.5 OR sin(t2.d)<0.9 group by t1.a;

--echo # With subq
--error ER_WRONG_FIELD_WITH_GROUP
select t2.b from t1 left join t1 as t2 on t1.a=t2.b and t1.b group by t1.a;
--error ER_WRONG_FIELD_WITH_GROUP
select t2.b from t1 left join t1 as t2 on t1.a=t2.b and (select t1.b) group by t1.a;

--echo # Test ANY_VALUE:
select ANY_VALUE(t1.b) from t1 left join t2 on t1.c=t1.b and t1.a=t1.c group by t1.a;
--error ER_WRONG_FIELD_WITH_GROUP
select 3+(5*t1.b) from t1 left join t2 on t1.c=t1.b and t1.a=t1.c group by t1.a;
select 3+(5*ANY_VALUE(t1.b)) from t1 left join t2 on t1.c=t1.b and t1.a=t1.c group by t1.a;
delete from t1;
insert into t1 (a,b) values(1,10),(1,20),(2,30),(2,40);
--error ER_MIX_OF_GROUP_FUNC_AND_FIELDS
select a, sum(b) from t1;
select any_value(a), sum(b) from t1;
--echo # different order of input rows, different "any_value":
select any_value(a), sum(b) from (select * from t1 order by a desc) as d;
--error ER_WRONG_FIELD_WITH_GROUP
select a,b,sum(c) from t1 group by a;
select a,any_value(b),sum(c) from t1 group by a;
select a,any_value(b),sum(c)
from (select * from t1 order by a desc, b desc) as d
group by a;

--echo # With view.

let $count=3;
while($count)
{
set @optimizer_switch_saved=@@optimizer_switch;
let $source_query=select t1.a*2 as a, t1.b as b, t1.c as c, t1.d as d, t1.a as e from t1;
if ($count == 3)
{
--echo # Merged view
eval create algorithm=merge view v1 as $source_query;
show create view v1;
let $source=v1;
let $source_no_alias=v1;
}
if ($count == 2)
{
--echo # Materialized view
eval create algorithm=temptable view v1 as $source_query;
show create view v1;
let $source=v1;
let $source_no_alias=v1;
}
if ($count == 1)
{
--echo # Materialized derived table
set optimizer_switch='derived_merge=off';
let $source=($source_query) as v1;
let $source_no_alias=($source_query);
}

--error ER_WRONG_FIELD_WITH_GROUP
eval select sin(a) as z from $source group by b;
--error ER_WRONG_FIELD_WITH_GROUP
eval select sin(a) as z from $source group by d,b;
eval select sin(a) as z from $source group by c,b;
--error ER_WRONG_FIELD_WITH_GROUP
eval select (select sin(a)) as z from $source group by d,b;
eval select (select sin(a)) as z from $source group by c,b;
--error ER_WRONG_FIELD_WITH_GROUP
eval select sin(a) as z from t1 group by c*2,b*2;
eval select sin(a+b*c) as z from $source group by c,b;
eval $show_trace;

eval select sin(a) as z from $source where b=2 group by c;

--echo # {v1.b,v1.c}->v1.a->t2.a
eval select sin(t2.a) as z from $source, $source_no_alias as t2
 where t2.a=v1.a group by v1.b,v1.c;
eval $show_trace;

eval select sin(t2.a) as z from $source, $source_no_alias as t2
 where t2.b=v1.b and t2.c=v1.c group by v1.b,v1.c;

--echo # With materialized view we get more dependencies than needed, due to implementation.
eval select t3.a from $source, $source_no_alias as t2, $source_no_alias as t3
where
 t3.b=t2.b and t3.c=v1.d and
 t2.b=v1.b and t2.c=v1.c
group by v1.b,v1.c;
eval $show_trace;

--echo # If we simply went to real_item(), we would have WHERE 2*a=b, or
--echo # GROUP BY 2*t1.a, so we would not find FDs. The original item
--echo # (direct_view_ref here) must also be considered!
eval select a from $source where a=b group by b;
eval $show_trace;
eval select b from $source where a=b group by a;
eval $show_trace;

eval select v1.c from $source where v1.c=v1.a group by v1.a;
eval $show_trace;
eval select v1.a from $source group by v1.e;
eval $show_trace;
eval select v1.c from $source where v1.c=v1.a group by v1.e;
eval $show_trace;
--echo # View appears only in WHERE
eval select t2.d from $source, t1 as t2 where v1.a=t2.d and v1.e=t2.a group by t2.a;
eval $show_trace;

drop view if exists v1;

--echo # FD due to view's WHERE:

let $source_query=select t1.a*2 as a, t1.b as b from t1;
if ($count == 3)
{
eval create algorithm=merge view v1 as $source_query;
}
if ($count == 2)
{
eval create algorithm=temptable view v1 as $source_query;
}
if ($count == 1)
{
let $source=($source_query) as v1;
let $source_no_alias=($source_query);
}

--error ER_WRONG_FIELD_WITH_GROUP
eval select a from $source group by b;
drop view if exists v1;

let $source_query=select t1.a*2 as a, t1.b as b from t1 where t1.a=t1.b;
if ($count == 3)
{
--echo # Merged view
eval create algorithm=merge view v1 as $source_query;
}
if ($count == 2)
{
--echo # Materialized view
eval create algorithm=temptable view v1 as $source_query;
}
if ($count == 1)
{
--echo # Derived table
let $source=($source_query) as v1;
let $source_no_alias=($source_query);
}
eval select a from $source group by b;
eval $show_trace;

drop view if exists v1;

--echo # Aggregates in view

let $source_query=select sum(t1.a) as a, sum(t1.b) as b from t1;
if ($count == 3)
{
--echo # Aggregates => skipping Merged view
}
if ($count == 2)
{
--echo # Materialized view
eval create algorithm=temptable view v1 as $source_query;
}
if ($count == 1)
{
--echo # Derived table
let $source=($source_query) as v1;
let $source_no_alias=($source_query);
}
if ($count <= 2)
{
eval select a from $source group by b;
eval $show_trace;
}

drop view if exists v1;

--echo # Aggregates + GROUP BY in view

--echo # We group by b*5, to show that it works with GROUP expressions, not only fields.
let $source_query=select a, b*5 as b, sum(t1.c) as c, sum(t1.d) as d from t1 group by a,b*5;
if ($count == 3)
{
--echo # Aggregates => skipping Merged view
}
if ($count == 2)
{
--echo # Materialized view
eval create algorithm=temptable view v1 as $source_query;
}
if ($count == 1)
{
--echo # Derived table
let $source=($source_query) as v1;
let $source_no_alias=($source_query);
}
if ($count <= 2)
{
--error ER_WRONG_FIELD_WITH_GROUP
eval select a from $source group by b;
--error ER_WRONG_FIELD_WITH_GROUP
eval select c from $source group by b;
--error ER_WRONG_FIELD_WITH_GROUP
eval select c from $source group by b,d;
--error ER_WRONG_FIELD_WITH_GROUP
eval select a,c from $source group by a;
eval select d,c from $source group by b,a;
eval $show_trace;
}

drop view if exists v1;

dec $count;
set optimizer_switch=@optimizer_switch_saved;
}

--echo # Derived table in merged view
create algorithm=temptable view v2
as select a as a, 2*a as b from t1;
create algorithm=merge view v1
 as select v2.a as a, 3*v2.b as b from v2;
select v1.b from v1 group by v1.a;
eval $show_trace;

select t2.a from t1 as t2, v1 where t2.a=v1.b group by v1.a;
eval $show_trace;

--echo # FDs in a view are those of the underlying query expression.
--echo # FDs in a query expression: expressions in the SELECT list must be
--echo # deterministic.
drop view v1;
create algorithm=merge view v1
 as select v2.a as a, rand()*v2.b as b from v2;
--error ER_WRONG_FIELD_WITH_GROUP
select v1.b from v1 group by v1.a;
drop view v1;
create algorithm=temptable view v1
 as select v2.a as a, rand()*v2.b as b from v2;
--error ER_WRONG_FIELD_WITH_GROUP
select v1.b from v1 group by v1.a;

drop view v1,v2;

--echo # Item_direct_view_ref pointing to Item_direct_view_ref pointing to
--echo # Item_field (a rare case!)
create algorithm=merge view v2 as select 2*a as a, 2*b as b from t1;
create algorithm=merge view v1 as select a, 3*b as b from v2 where a=b;
select 1 from (select a,b+0 from v1 group by a) as d;
drop view v1,v2;

--echo # Some fun cases with aggregates in derived table.

--echo # Inner query is valid: t1.a -> t1.b (equality in WHERE). Outer query:
--echo # d.b -> t1.b (underlying column of d.b) -> t1.a (equality)
--echo # -> sum(1) (because t1.a is all group columns so determines
--echo # sum()) -> d.s (because sum() is underlying of d.s)
select d.s from
 (select b, sum(1) as s from t1 where a=b group by a) as d
group by d.b;

alter table t1 add column pk int primary key auto_increment;
--echo # Inner query: t1.pk -> t1.* (pk). Outer query:
--echo # d.b,d.c -> t1.b,t1.c (their underlying columns) -> t1.pk (because
--echo # t1.b,t1.c is unique) -> sum(1) (because t1.pk is all group columns so
--echo # determines sum()) -> d.s (because sum() is underlying of d.s)
select d.s from
 (select b, c, sum(d) as s from t1 group by pk) as d
group by d.b,d.c;

--echo # Outer query:
--echo # d.c -> t1.b*3 (underlying column of d.c) -> sum(a) (because t1.b*3
--echo # is all group expressions) -> d.s
select d.s from
 (select b*3 as c, sum(a) as s from t1 group by b*3) as d
group by d.c;

drop table t1,t2;


--echo # Testcase from Reviewers

create table customer1(pk int primary key, a int);
create table customer2(pk int primary key, b int);
CREATE algorithm=merge VIEW customer as SELECT pk,a,b
FROM customer1 JOIN customer2 USING (pk);

let $query=select customer.pk, customer.b
from customer
group by customer.pk;

eval $query;
--echo # View is merged. Show FDs. Note that in --ps-protocol, the trace
--echo # is that of execution, so contains no group-by checks.
eval $show_trace;

drop view customer;
CREATE algorithm=temptable VIEW customer as SELECT pk,a,b
FROM customer1 JOIN customer2 USING (pk);
eval $query;
eval $show_trace;

--echo # Benefit from outer-join-to-inner conversion.
insert into customer1 values(0,10),(1,20);
insert into customer2 values(0,10),(1,20);

--echo # 1) no conversion, no FD from customer2.b to customer1.a.
--error ER_WRONG_FIELD_WITH_GROUP
explain select customer1.a, count(*)
 from customer1 left join customer2 on customer1.a=customer2.b
 where customer1.pk in (7,9)
 group by customer2.b;

--echo # 2) null-complemented row can't pass WHERE => conversion
--echo # => FD from customer2.b to customer1.a.
explain select customer1.a, count(*)
 from customer1 left join customer2 on customer1.a=customer2.b
 where customer2.pk in (7,9)
 group by customer2.b;

drop table customer1,customer2;
drop view customer;

--echo # FDs of JOIN...USING and NATURAL JOIN

create table t1(pk int primary key, a int);
create table t2(pk int primary key, b int);

select t1.pk, t1.a from t1 join t2 on t1.pk=t2.pk group by t1.pk;
select t1.pk, t1.a from t1 join t2 using(pk) group by t1.pk;
select t1.pk, t1.a from t1 natural join t2 group by t1.pk;
select t1.pk, t1.a from t1 left join t2 using(pk) group by t1.pk;
select t1.pk, t1.a from t1 natural left join t2 group by t1.pk;

select t1.pk, t2.b from t1 join t2 on t1.pk=t2.pk group by t1.pk;
select t1.pk, t2.b from t1 join t2 using(pk) group by t1.pk;
select t1.pk, t2.b from t1 natural join t2 group by t1.pk;
select t1.pk, t2.b from t1 left join t2 using(pk) group by t1.pk;
eval $show_trace;
select t1.pk, t2.b from t1 natural left join t2 group by t1.pk;

--echo # Equivalent queries, with RIGHT JOIN
select t1.pk, t2.b from t2 right join t1 using(pk) group by t1.pk;
select t1.pk, t2.b from t2 natural right join t1 group by t1.pk;

--error ER_WRONG_FIELD_WITH_GROUP
select t1.pk, t2.b from t1 left join t2 on t1.pk>t2.pk group by t1.pk;
--echo # Even in --ps-protocol we see the group-by checks in trace because
--echo # there has been no execution (due to error).
eval $show_trace;

--error ER_WRONG_FIELD_WITH_GROUP
select t1.pk, t2.b from t2 right join t1 on t1.pk>t2.pk group by t1.pk;

drop table t1,t2;

--echo # Cases where FDs from weak side do not propagate

create table t1(a int, b int);
insert into t1 values(null,0),(null,1);

let $query=
select d.a,d.c
from t1 left join (select a, coalesce(a,3) as c from t1) as d
on t1.b>0;
eval $query;
--echo # Now group it by d.a:
--error ER_WRONG_FIELD_WITH_GROUP
eval $query group by d.a;
eval $show_trace;

let $query=
select d.a,d.c
from t1 left join (select a, count(a) as c from t1 group by a) as d
on t1.b+d.c>0;
eval $query;
--echo # Now group it by d.a:
--error ER_WRONG_FIELD_WITH_GROUP
eval $query group by d.a;
eval $show_trace;

let $query=
select d.m,d.c
from t1 left join (select max(a) as m, count(a) as c from t1) as d
on t1.b+d.c>0;
eval $query;
--echo # Now group it by d.m:
--error ER_WRONG_FIELD_WITH_GROUP
eval $query group by d.m;
eval $show_trace;
--echo # Now group it by d.c which is non-nullable:
eval $query group by d.c;
eval $show_trace;

drop table t1;
create table t1(pk int primary key, a int);

--echo # Not valid per the standard, because:
--echo # 1) In t3, t3.pk->t3.a holds.
--echo # 2) In R1 the result of "(t2 left join t3 on 1)", t3.pk->t3.a
--echo # holds, by application of: there is a functional dependency in the
--echo # weak side t3, and t3.pk is not nullable in t3.
--echo # 3) In R2 the result of "t1 left join (t2 left join t3 on 1) on 1",
--echo # t3.pk->t3.a doesn't hold anymore, because: it's a dependency in the
--echo # weak side (weak side is R1), and t3.pk is nullable _when
--echo # seen as a column of R1_ (in R1 t3.pk can be NULL, if the row of t3
--echo # is actually a null-complemented one).
--echo # But for us it is valid, because we have refined the logic: the
--echo # pk-based FD satisfies the requirement that a NULL value of t3.pk
--echo # implies a NULL value of t3.a (indeed, the NULL value of t3.pk can
--echo # only come from null-complementing of the row of t3 in R1, in which
--echo # case t3.a is also NULL).
select t3.a
 from t1 left join (t1 as t2 left join t1 as t3 on 1) on 1
 group by t3.pk;
eval $show_trace;

--echo # Outer reference - why we use resolved_used_tables():
select (select t1.a from t1 as t2 limit 1) from t1 group by pk;

--echo # We don't build the FD list if not needed
select t1.a*3 from t1 group by t1.a;
eval $show_trace;

drop table t1;

--echo # Tricky cases with "ON col=literal" propagating.
create table t1(a int, b int);
insert into t1 values();
insert into t1 values(10,11);
create table t2(c int, d int);
insert into t2 values(2,3);

let $query=
select t4.d
 from t1 left join (t2 as t3 join t2 as t4 on t4.d=3) on t1.a=10;
eval $query;
--echo # Equivalent to T1 LJ (T2, T3) ON T4.D=3 AND T1.A=10
--echo # (this is what simplify_joins() does).
--echo # For T4.D=3, DJS is {T1.A} which is not group column.
--error ER_WRONG_FIELD_WITH_GROUP
eval $query group by "";

let $query=
select t4.d
 from t1 left join (t2 as t3 left join t2 as t4 on t4.d=3) on t1.a=10;
eval $query;
--echo # For T4.D=3, DJS={}, not NULL-friendly, and embedding is on weak side
--echo # so FD cannot propagate.
--error ER_WRONG_FIELD_WITH_GROUP
eval $query group by "";

let $query=
select t4.d
 from t1 join (t2 as t3 left join t2 as t4 on t4.d=3) on t1.a=10;
eval $query;
--echo # For T4.D=3, DJS={}, not NULL-friendly, but embedding is on weak side
--echo # so FD can propagate.
eval $query group by "";

--echo # With a view

create view v1 as select a as a, 2*a as b, coalesce(a,3) as c from t1;

let $query=
select v1.b from t1 left join v1 on 1;
eval $query;
--echo # If v1.a is NULL then v1.b is NULL: a->b is NULL-friendly
eval $query group by v1.a;

let $query=
select v1.c from t1 left join v1 on 1;
eval $query;
--echo # If v1.a is NULL then v1.c may not be NULL: a->c is not NULL-friendly
--error ER_WRONG_FIELD_WITH_GROUP
eval $query group by v1.a;

drop view v1;

--echo # Constant view item
create view v1 as select a as a, 2 as b from t1;
let $query=
select t1.a, v1.b from t1 left join v1 on t1.a is not null;
--echo # Because of BUG#17023060, the result is wrong, should be
--echo # [10,2 ; 10,2 ; NULL,NULL], which would show that {}->{v1.b} does not
--echo # hold in the result, even though v1.b is constant (=2) in v1.
eval $query;
--echo # We correctly reject this:
--error ER_WRONG_FIELD_WITH_GROUP
eval $query group by v1.a;

drop view v1;
drop table t1,t2;

create table emp(empno int, ename char(1), deptno int);
create table dept(deptno int primary key, dname char(1));
let $view_rest=
VIEW empdept AS
SELECT emp.empno, emp.ename, dept.deptno, dept.dname
FROM emp LEFT OUTER JOIN dept ON (emp.deptno = dept.deptno);

let $query=
SELECT dname, COUNT(*)
FROM empdept
GROUP BY deptno;

eval CREATE algorithm=merge $view_rest;
eval EXPLAIN $query;
--echo # There is pk-based FD dept.Depno->dept.dname in dept
--echo # and it propagates in the view-which-became-nest because it is
--echo # NULL-friendly.
eval $query;
drop view empdept;

eval CREATE algorithm=temptable $view_rest;
eval EXPLAIN $query;
--echo # There is pk-based FD dept.Depno->dept.dname in dept
--echo # and it propagates in the materialized view because it is
--echo # NULL-friendly, and then in the top query because the view is not in
--echo # the weak side of an outer join.
eval $query;

--echo # More tests, for code coverage.

--echo # UNION in derived table
--error ER_WRONG_FIELD_WITH_GROUP
select emp.ename
from
(select 1 as empno union select 2) deriv,
emp
where emp.empno=deriv.empno
group by emp.empno;

drop view empdept;

--echo # Make the key-searching loop meet view columns which don't wrap a
--echo # column (CONCAT).

CREATE VIEW empdept AS
SELECT emp.empno, emp.ename, dept.deptno, concat("",dept.dname) as dname
FROM emp LEFT JOIN dept ON (emp.deptno = dept.deptno);

--error ER_WRONG_FIELD_WITH_GROUP
SELECT ename, COUNT(*) FROM empdept WHERE empno=dname and empno=deptno GROUP BY empno;

drop table emp,dept;
drop view empdept;

--echo #
--echo # Bug#19636980 ASSERT `TABLE->OUTER_JOIN' FAILED IN GROUP_CHECK::FIND_FD_IN_JOINED_TABLE
--echo #

CREATE TABLE t1 (
  c1 INT,
  c2 INT,
  c4 DATE,
  c5 VARCHAR(1)
);

CREATE TABLE t2 (
  c1 INT,
  c2 INT,
  c3 INT,
  c5 VARCHAR(1)
);

--echo # alias1.c5 is not FD, the error is detected at SELECT time
CREATE VIEW v1 AS
  SELECT alias1.c4 AS field1
  FROM t1 AS alias1
  INNER JOIN t1 AS alias2
  ON 1
GROUP BY field1
ORDER BY alias1.c5;

--error ER_WRONG_FIELD_WITH_GROUP
SELECT * FROM v1;

DROP VIEW v1;

--echo # alias1.c5 is FD (WHERE contains: alias1.c5='d')
CREATE VIEW v1 AS
  SELECT alias1.c4 AS field1, alias1.c4 AS field2
  FROM t1 AS alias1
    INNER JOIN t1 AS alias2
    ON (alias2.c1 = alias1.c2)
  WHERE ( NOT EXISTS (
    SELECT SQ1_alias1.c5 AS SQ1_field1
    FROM t2 AS SQ1_alias1
    WHERE SQ1_alias1.c3 < alias1.c1
  ))
    AND (alias1.c5 = alias1.c5
      AND alias1.c5 = 'd'
    )
GROUP BY field1, field2
ORDER BY alias1.c5, field1, field2
;

SELECT * FROM v1;

DROP VIEW v1;
DROP TABLE t1,t2;

--echo #
--echo # Bug#19636409 ASSERT `(MAP_OF_NEW_EQ_FDS...` IN GROUP_CHECK::IS_FD_ON_SOURCE ON SELECT
--echo #

CREATE TABLE t1 (
  pk int NOT NULL,
  c1 datetime,
  c2 varchar(1),
  c3 date,
  c4 date,
  c5 varchar(1),
  PRIMARY KEY (pk)
);

CREATE VIEW v1 AS
  SELECT c3 AS subfield11,
         pk AS subfield12,
         c2 AS subfield13
  FROM t1
  GROUP BY subfield11, subfield12
;

--echo # This query is valid. Indeed: field3 i.e. t1.c2 is part of GROUP BY,
--echo # so, in a group:
--echo # - all rows have the same value of t1.c2, noted val_c2,
--echo # - so all rows, which necessarily match WHERE, matched with a row of
--echo # v1 having subfield11=val_c2 and subfield12=val_c2 (due to IN
--echo # predicate),
--echo # - thus both grouping columns of v1's query expression are constant
--echo # (to val_c2)
--echo # - thus v1.subfield13 is constant too (grouping columns determine the
--echo # SELECT list)
--echo # - so, due to IN, this determines the value of t1.pk (t1 is the top
--echo # query's table).
--echo # - so t1.pk is constant, so all columns of t1 are constant.
--echo # Which proves that it makes sense to search FDs in semijoin
--echo # equalities.
--echo # In other words: a semijoin is like a join except that it eliminates
--echo # duplicates, but duplicates are irrelevant to the decision whether a
--echo # column is functionally dependent on a set of columns.

SELECT c5 AS field1,
       c4 AS field2,
       c2 AS field3,
       c1 AS field4
FROM t1
WHERE ( c2, c2, pk ) IN (
  SELECT * FROM v1
)
GROUP BY field1, field3, field3;

--echo #
--echo # Bug#19687724 FUNCTIONAL DEPENDENCIES ARE NOT RECOGNIZED IN EQUALITIES BETWEEN ROWS
--echo #

--echo # Inspired by query above, but with a join:
SELECT c5 AS field1,
       c4 AS field2,
       c2 AS field3,
       c1 AS field4
FROM t1, v1
WHERE ( c2, c2, pk ) = (subfield11, subfield12, subfield13)
GROUP BY field1, field3, field3;

--echo # With constants:
SELECT c5 AS field1,
       c4 AS field2,
       c2 AS field3,
       c1 AS field4
FROM t1
WHERE ( c2, c2, pk ) = (1, 2, 3)
GROUP BY field1, field3, field3;

DROP TABLE t1;

CREATE TABLE t1(a INT, b INT, c INT, d INT);
SELECT a,b,c,d FROM t1 WHERE a=c AND b=d GROUP by a,b;
SELECT a,b,c,d FROM t1 WHERE (a,b)=(c,d) GROUP BY a,b;
--error ER_WRONG_FIELD_WITH_GROUP
SELECT a,b,c,d FROM t1 WHERE (a,b)=(c,d+1) GROUP BY a,b;
DROP TABLE t1;

DROP VIEW v1;

--echo #
--echo # Bug #20031708 ASSERT ON GROUP_CHECK::IS_FD_ON_SOURCE
--echo #

CREATE TABLE t1 (
  col_varchar_10_utf8 VARCHAR(10) CHARACTER SET utf8,
  col_int_key INT,
  pk INT PRIMARY KEY
);

CREATE TABLE t2 (
  col_varchar_10_utf8 VARCHAR(10) CHARACTER SET utf8 DEFAULT NULL,
  col_int_key INT DEFAULT NULL,
  pk INT PRIMARY KEY
);

CREATE VIEW v2 AS SELECT * FROM t2;

--echo # The reporter's testcase:

let $query=
SELECT COUNT(*), t1.col_int_key
FROM v2 LEFT OUTER JOIN t1
     ON v2.col_varchar_10_utf8 = t1.col_varchar_10_utf8
WHERE v2.pk = 4;

--error ER_MIX_OF_GROUP_FUNC_AND_FIELDS
eval $query;

DROP VIEW v2;

--echo # A variant: one column is an expression:
CREATE VIEW v2 AS SELECT
 CONCAT(col_varchar_10_utf8,' ') AS col_varchar_10_utf8,
 col_int_key,
 pk
FROM t2;

--error ER_MIX_OF_GROUP_FUNC_AND_FIELDS
eval $query;

DROP VIEW v2;

--echo # Query used in the commit comment: view column involving two tables
CREATE VIEW v2 AS
 SELECT t1.pk, t2.col_int_key+1 as c, t1.pk+t2.col_int_key as p
 FROM t1, t2;

--echo # FDs will be discovered in this order: {}->v2.pk, v2.pk->v2.c,
--echo # v2.c->v2.p
SELECT COUNT(*), v2.p
FROM v2
WHERE v2.c=v2.p and v2.c=v2.pk AND v2.pk = 4;
DROP VIEW v2;

--echo # If in the query specification defining a view, a base table's pk is
--echo # determined,
--echo # and the view's column is a function of this base table's columns,
--echo # then the view's column is also determined.
--echo # So, in this view's result, {v2.pk}->{v2.coa}:
CREATE ALGORITHM=MERGE VIEW v2 AS
 SELECT t2.pk, COALESCE(t2.pk, 3) AS coa
 FROM t1 LEFT JOIN t2 ON 0;

--echo # And thus {pk}->{coa} holds in the result of this query using the view;
--echo # if there is NULL-complementing in the LEFT JOIN below then (pk,coa)
--echo # will be (NULL,NULL) and if there is not it will be (non-NULL,3):
--echo # v2.coa is determined by v2.pk. The key fact is that v2.pk is not
--echo # NULLable so this is a NFFD.
SELECT v2.pk, v2.coa
 FROM t1 LEFT JOIN v2 AS v2 ON 0
 GROUP BY v2.pk;

DROP VIEW v2;
DROP TABLE t1,t2;

--echo #
--echo # Bug#21807579 FUNCTIONAL DEPENDENCIES ARE NOT RECOGNIZED IN GENERATED COLUMNS
--echo #

CREATE TABLE t ( a INT, c INT GENERATED ALWAYS AS (a+2), d INT GENERATED ALWAYS AS (c+2) );
--echo # {a}->{c}, {c}->{d} and {a}->{d} hold.
SELECT c FROM t GROUP BY a;
SELECT d FROM t GROUP BY c;
SELECT d FROM t GROUP BY a;
SELECT 1+c FROM t GROUP BY a;
SELECT 1+d FROM t GROUP BY c;
SELECT 1+d FROM t GROUP BY a;
--echo # {t2.a}->{t2.d}->{t1.c}->{t1.d}
SELECT t1.d FROM t as t1, t as t2 WHERE t2.d=t1.c GROUP BY t2.a;
--error ER_WRONG_FIELD_WITH_GROUP
SELECT t1.d FROM t as t1, t as t2 WHERE t2.d>t1.c GROUP BY t2.a;
DROP TABLE t;

--echo # A non-NULL-friendly dependency.
CREATE TABLE t ( a INT,
c INT GENERATED ALWAYS AS (COALESCE(a,3)) );
INSERT INTO t (a) VALUES(NULL);
CREATE TABLE u ( a INT );
INSERT INTO u VALUES(0),(1);
--echo # Even though {a}->{c} holds in 't', it doesn't propagate to the left
--echo # join's result
SELECT t.a,t.c FROM u LEFT JOIN t ON u.a>0;
--error ER_WRONG_FIELD_WITH_GROUP
SELECT t.a,t.c FROM u LEFT JOIN t ON u.a>0 GROUP BY t.a;
DROP TABLE t,u;

--echo #
--echo # Bug #22924183 	GROUP BY ON DERIVED TABLE, EXPRESSION OF AGGREGATE AND GROUP COLUMN: ERROR
--echo #

CREATE TABLE coll (
  doc text
) ENGINE=InnoDB;

SELECT (je+1)+count(*) FROM (SELECT doc+1 AS je FROM coll) AS dt GROUP BY je;

SELECT je+(1+count(*)) FROM (SELECT doc+1 AS je FROM coll) AS dt GROUP BY je;

DROP TABLE coll;

--echo #
--echo # Bug#27114719 FUNCTIONAL DEPENDENCY CHECK FAILS ON FIRST CALL, SUCCEEDS ON NEXT
--echo #

CREATE TABLE t ( a INT, c INT GENERATED ALWAYS AS (a+2), d INT GENERATED
ALWAYS AS (c+2) );
# Was wrongly accepted, on first run, in 8.0
--error ER_WRONG_FIELD_WITH_GROUP
SELECT t1.d FROM t as t1, t as t2 WHERE t2.d>t1.c GROUP BY t2.a;
--error ER_WRONG_FIELD_WITH_GROUP
SELECT t1.d FROM t as t1, t as t2 WHERE t2.d>t1.c GROUP BY t2.a;
FLUSH TABLES;
# Was wrongly accepted, on all runs, in 8.0 and 5.7
--error ER_WRONG_FIELD_WITH_GROUP
SELECT (SELECT t1.c FROM t as t1 GROUP BY -3) FROM t as t2;

# Verify the DISTINCT check works on generated columns too (doesn't
# have the above bug, because check doesn't need to dive into gcols).
--error ER_FIELD_IN_ORDER_NOT_SELECT
SELECT DISTINCT t1.a FROM t as t1 ORDER BY t1.d LIMIT 1;
--error ER_FIELD_IN_ORDER_NOT_SELECT
SELECT (SELECT DISTINCT t1.a FROM t as t1 ORDER BY t1.d LIMIT 1) FROM t as t2;
DROP TABLE t;

--echo #
--echo # Bug#22279903 ISSUE WITH ONLY_FULL_GROUP_BY AND UNIQUE KEY WITH NULL VALUES
--echo #

CREATE TABLE t(a INT NULL, b INT NOT NULL, c INT, UNIQUE(a,b));
INSERT INTO t VALUES (NULL, 1, 4);
INSERT INTO t VALUES (NULL, 1, 5);
# we see that the first and second fields don't determine the third, so
# it is incorrect to accept:
--error ER_WRONG_FIELD_WITH_GROUP
SELECT a,b,c FROM t GROUP BY a,b;

# However, if we exclude null values of "a" then group columns do
# determine {c}:
SELECT a,b,c FROM t WHERE a IS NOT NULL GROUP BY a,b;
# Test various predicates:
SELECT a,b,c FROM t WHERE NOT (a IS NULL) GROUP BY a,b;
SELECT a,b,c FROM t WHERE a > 3 GROUP BY a,b;
SELECT a,b,c FROM t WHERE a = 3 GROUP BY b;
SELECT a,b,c FROM t WHERE a BETWEEN 3 AND 6 GROUP BY a,b;
SELECT a,b,c FROM t WHERE a <> 3 GROUP BY a,b;
SELECT a,b,c FROM t WHERE a IN (3,4) GROUP BY a,b;
SELECT a,b,c FROM t WHERE a IN (SELECT b FROM t) GROUP BY a,b;
SELECT a,b,c FROM t WHERE a IS TRUE GROUP BY a,b;
SELECT a,b,c FROM t WHERE a IS FALSE GROUP BY a,b;
SELECT a,b,c FROM t WHERE NOT(a IN (3,4)) GROUP BY a,b;
SELECT a,b,c FROM t WHERE a NOT IN (3,4) GROUP BY a,b;
SELECT a,b,c FROM t WHERE a LIKE "%abc%" GROUP BY a,b;
SELECT a,b,c FROM t WHERE a NOT LIKE "%abc%" GROUP BY a,b;

# IS TRUE is not an equality, any non-zero number passes,
# so it doesn't determine the value of 'a'.
--error ER_WRONG_FIELD_WITH_GROUP
SELECT a,b,c FROM t WHERE a IS TRUE GROUP BY b;

# <=> lets NULLs pass, is potentially dangerous:
--error ER_WRONG_FIELD_WITH_GROUP
SELECT a,b,c FROM t WHERE a<=>NULL GROUP BY b;

# Conditions work equally well in merged derived table:
SELECT a,b,c FROM
 (SELECT * FROM t WHERE a IS NOT NULL) AS dt GROUP BY a,b;
# And in materialized one. How this works:
# in top query we add dt.a and dt.b to FD list; this adds t.a and t.b
# to derived table's inner FD list; later we search if dt.c is FD; we
# fail, and search into the derived table's body: there, dt.c is t.c;
# we search if t.c is FD in the derived table's body; so we scan the
# WHERE clause, find 'a IS NOT NULL', conclude that (a,b) is like a PK
# of 't', so t.c is FD in the derived table's body, so dt.c is also
# FD.
SELECT /*+ NO_MERGE() */ a,b,c FROM
 (SELECT * FROM t WHERE a IS NOT NULL) AS dt GROUP BY a,b;
eval $show_trace;

# IS NULL doesn't help us distinguish two NULLs in a group:
--error ER_WRONG_FIELD_WITH_GROUP
SELECT a,b,c FROM t WHERE a IS NULL GROUP BY a,b;
--error ER_WRONG_FIELD_WITH_GROUP
SELECT a,b,c FROM t WHERE NOT(a IS NOT NULL) GROUP BY a,b;

# join condition on strong-side column gives no information
--error ER_WRONG_FIELD_WITH_GROUP
SELECT t.* FROM t LEFT JOIN t AS t1 ON t.a IS NOT NULL
GROUP BY t.a,t.b;
# but on weak-side column it does
SELECT t.* FROM t RIGHT JOIN t AS t1 ON t.a IS NOT NULL
GROUP BY t.a,t.b;

DROP TABLE t;

# A case which mixes different types of FDs.

CREATE TABLE t1 (a INT, b INT);

CREATE TABLE t2 (b INT);

CREATE TABLE t3 (
b INT NULL,
c INT NULL,
d INT NULL,
e INT NULL,
UNIQUE KEY (b,d,e));

# t1.b is in GROUP BY and t1.a = 2
# => t1.a and t1.b are known;
# with t2.b=t1.b
# => t2.b is known;
# with t3.e=1 and t3.d=1 and t2.b=t3.b,
# => t3.b and t3.e and t3.d are known and
# we can treat them as non-nullable so treat unique key of t3
# as PK,
# => t3.c is known.
SELECT * FROM t1, t2, t3
WHERE t2.b = t1.b AND t2.b = t3.b
AND t3.d = 1 AND t3.e = 1 AND t3.d IS NOT NULL AND t1.a = 2
GROUP BY t1.b;

# Remove IS NOT NULL condition, still works
SELECT * FROM t1, t2, t3
WHERE t2.b = t1.b AND t2.b = t3.b
AND t3.d = 1 AND t3.e = 1 AND t1.a = 2
GROUP BY t1.b;
eval $show_trace;

# Remove t3.d = 1, IS NOT NULL is not enough of course.
--error ER_WRONG_FIELD_WITH_GROUP
SELECT * FROM t1, t2, t3
WHERE t2.b = t1.b AND t2.b = t3.b
AND t3.e = 1 AND t3.d IS NOT NULL AND t1.a = 2
GROUP BY t1.b;

DROP TABLE t1, t2, t3;

DROP FUNCTION show_json_object;
