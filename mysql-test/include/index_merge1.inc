# include/index_merge1.inc
#
# Index merge tests
#
# The variables
#     $engine_type         -- storage engine to be tested
#     $merge_table_support -- 1 storage engine supports merge tables
#                          -- 0 storage engine does not support merge tables
# have to be set before sourcing this script.
#
# Note: The comments/expectations refer to MyISAM.
#       They might be not valid for other storage engines.
#
# Last update:
# 2006-08-02 ML test refactored
#               old name was t/index_merge.test
#               main code went into include/index_merge1.inc
#
--source include/elide_costs.inc

--echo #---------------- Index merge test 1 -------------------------------------------
--echo # (Start of test file: index_merge1.inc)

eval SET SESSION DEFAULT_STORAGE_ENGINE = $engine_type;

--disable_warnings
drop table if exists t0, t1, t2, t3, t4;
--enable_warnings

--echo # Create and fill a table with simple keys
create table t0
(
  key1 int not null,
  key2 int not null,
  key3 int not null,
  key4 int not null,
  key5 int not null,
  key6 int not null,
  key7 int not null,
  key8 int not null,
  INDEX i1(key1),
  INDEX i2(key2),
  INDEX i3(key3),
  INDEX i4(key4),
  INDEX i5(key5),
  INDEX i6(key6),
  INDEX i7(key7),
  INDEX i8(key8)
);

--disable_query_log
insert into t0 values (1,1,1,1,1,1,1,1023),(2,2,2,2,2,2,2,1022);

let $1=9;
set @d=2;
while ($1)
{
  eval insert into t0 select key1+@d, key2+@d, key3+@d, key4+@d, key5+@d,
                             key6+@d, key7+@d, key8-@d from t0;
  eval set @d=@d*2;
  dec $1;
}
--enable_query_log

analyze table t0;

--echo # 1. One index
explain select * from t0 where key1 < 3 or key1 > 1020;

--echo # 2. Simple cases
explain
select * from t0 where key1 < 3 or key2 > 1020;
select * from t0 where key1 < 3 or key2 > 1020;

if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select * from t0 where key1 < 2 or key2 <3;

if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain
select * from t0 where (key1 > 30 and key1<35) or (key2 >32 and key2 < 40);
--echo # Bug#21277: InnoDB, wrong result set, index_merge strategy,
--echo #            second index not evaluated
select * from t0 where (key1 > 30 and key1<35) or (key2 >32 and key2 < 40);

--echo # 3. Check that index_merge doesn't break "ignore/force/use index"
if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select * from t0 ignore index (i2) where key1 < 3 or key2 <4;

if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select * from t0 where (key1 < 3 or key2 <4) and key3 = 50;

if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select * from t0 use index (i1,i2) where (key1 < 2 or key2 <3) and key3 = 50;

if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select * from t0 where (key1 > 1 or key2  > 2);

if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select * from t0 force index (i1,i2) where (key1 > 1 or key2  > 2);


--echo # 4. Check if conjuncts are grouped by keyuse
if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain
  select * from t0 where key1<2 or key2<3 or (key1>5 and key1<7) or
  (key1>10 and key1<12) or (key2>100 and key2<102);

--echo # 5. Check index_merge with conjuncts that are always true/false
--echo #    verify fallback to "range" if there is only one non-confluent condition
if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select * from t0 where key2 = 45 or key1 <=> null;

if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select * from t0 where key2 = 45 or key1 is not null;

if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select * from t0 where key2 = 45 or key1 is null;

--echo #   the last conj. is always false and will be discarded
if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select * from t0 where key2=10 or key3=3 or key4 <=> null;

--echo #   the last conj. is always true and will cause 'all' scan
if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select * from t0 where key2=10 or key3=3 or key4 is null;

--echo #   some more complicated cases

if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select key1 from t0 where (key1 <=> null) or (key2 < 2) or
                                   (key3=10) or (key4 <=> null);

if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select key1 from t0 where (key1 <=> null) or (key1 < 5) or
                                   (key3=10) or (key4 <=> null);

--echo # 6.Several ways to do index_merge, (ignored) index_merge vs. range
if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select * from t0 where
  (key1 < 2 or key2 < 2) and (key3 < 3 or key4 < 3) and (key5 < 5 or key6 < 5);

if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain
select * from t0 where (key1 < 2 or key2 < 4) and (key1 < 5 or key3 < 3);

select * from t0 where (key1 < 2 or key2 < 4) and (key1 < 5 or key3 < 3);


if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select * from t0 where
  (key1 < 3 or key2 < 2) and (key3 < 3 or key4 < 3) and (key5 < 2 or key6 < 2);

if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select * from t0 where
  (key1 < 3 or key2 < 3) and (key3 < 70);

if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select * from t0 where
  (key1 < 3 or key2 < 3) and (key3 < 1000);


--echo # 7. Complex cases
--echo #   tree_or(List<SEL_IMERGE>, range SEL_TREE).
if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select * from t0 where
    ((key1 < 3 or key2 < 3) and (key2 <4 or key3 < 3))
  or
    key2 > 4;

if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select * from t0 where
    ((key1 < 4 or key2 < 4) and (key2 <4 or key3 < 3))
  or
    key1 < 5;

select * from t0 where
    ((key1 < 4 or key2 < 4) and (key2 <4 or key3 < 3))
  or
    key1 < 5;

--echo #   tree_or(List<SEL_IMERGE>, List<SEL_IMERGE>).
if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select * from t0 where
    ((key1 < 2 or key2 < 2) and (key3 <4 or key5 < 3))
  or
    ((key5 < 3 or key6 < 3) and (key7 <3 or key8 < 3));

if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select * from t0 where
    ((key3 <3 or key5 < 4) and (key1 < 3 or key2 < 3))
  or
    ((key7 <5 or key8 < 3) and (key5 < 4 or key6 < 4));

if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select * from t0 where
    ((key3 <3 or key5 < 4) and (key1 < 3 or key2 < 4))
  or
    ((key3 <4 or key5 < 2) and (key5 < 5 or key6 < 3));

if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select * from t0 where
    ((key3 <4 or key5 < 3) and (key1 < 3 or key2 < 3))
  or
    (((key3 <5 and key7 < 5) or key5 < 2) and (key5 < 4 or key6 < 4));

if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select * from t0 where
    ((key3 <5 or key5 < 4) and (key1 < 4 or key2 < 4))
  or
    ((key3 >5 or key5 < 2) and (key5 < 5 or key6 < 6));

if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select * from t0 force index(i1, i2, i3, i4, i5, i6 ) where
    ((key3 <3 or key5 < 4) and (key1 < 3 or key2 < 3))
  or
    ((key3 >4 or key5 < 2) and (key5 < 5 or key6 < 4));

--echo # Can't merge any indexes here (predicate on key3 is always true)
if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select * from t0 force index(i1, i2, i3, i4, i5, i6 ) where
    ((key3 <5 or key5 < 4) and (key1 < 4 or key2 < 4))
  or
    ((key3 >=5 or key5 < 2) and (key5 < 5 or key6 < 6));

--echo # 8. Verify that "order by" after index merge uses filesort
select * from t0 where key1 < 3 or key8 < 2 order by key1;

if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain
select * from t0 where key1 < 3 or key8 < 2 order by key1;

--echo # 9. Check that index_merge cost is compared to 'index' where possible
create table t2 like t0;
insert into t2 select * from t0;

alter table t2 add index i1_3(key1, key3);
alter table t2 add index i2_3(key2, key3);
alter table t2 drop index i1;
alter table t2 drop index i2;
alter table t2 add index i321(key3, key2, key1);

-- disable_query_log
-- disable_result_log
analyze table t2;
-- enable_result_log
-- enable_query_log

--echo #   index_merge vs 'index', index_merge is better.
if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select key3 from t2 where key1 = 100 or key2 = 100;

--echo #   index_merge vs 'index', 'index' is better.
if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select key3 from t2 where key1 <100 or key2 < 100;

--echo #   index_merge vs 'all', index_merge is better.
if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select key7 from t2 where key1 <100 or key2 < 100;

--echo # 10. Multipart keys.
create table t4 (
  key1a int not null,
  key1b int not null,
  key2  int not null,
  key2_1 int not null,
  key2_2 int not null,
  key3  int not null,
  index i1a (key1a, key1b),
  index i1b (key1b, key1a),
  index i2_1(key2, key2_1),
  index i2_2(key2, key2_1)
);

insert into t4 select key1,key1,key1 div 10, key1 % 10, key1 % 10, key1 from t0;

-- disable_query_log
-- disable_result_log
analyze table t4;
-- enable_result_log
-- enable_query_log

--echo #   the following will be handled by index_merge:
select * from t4 where key1a = 3 or key1b = 4;
explain select * from t4 where key1a = 3 or key1b = 4;

--echo #   and the following will not
explain select * from t4 where key2 = 1 and (key2_1 = 1 or key3 = 5);

explain select * from t4 where key2 = 1 and (key2_1 = 1 or key2_2 = 5);

if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select * from t4 where key2_1 = 1 or key2_2 = 5;


--echo # 11. Multitable selects
create table t1 like t0;
insert into t1 select * from t0;

-- disable_query_log
-- disable_result_log
analyze table t1;
-- enable_result_log
-- enable_query_log

--echo #  index_merge on first table in join
explain select * from t0 left join t1 on (t0.key1=t1.key1)
  where t0.key1=3 or t0.key2=4;

select * from t0 left join t1 on (t0.key1=t1.key1)
  where t0.key1=3 or t0.key2=4;

explain
select * from t0,t1 where (t0.key1=t1.key1) and ( t0.key1=3 or t0.key2=4);

--echo #  index_merge vs. ref
if (!$index_merge_random_rows_in_EXPLAIN)
{
#this plan varies too much for InnoDB
explain
select * from t0,t1 where (t0.key1=t1.key1) and
  (t0.key1=3 or t0.key2=4) and t1.key1<200;
}

--echo #  index_merge vs. ref
explain
select * from t0,t1 where (t0.key1=t1.key1) and
  (t0.key1=3 or t0.key2<4) and t1.key1=2;

--echo #  index_merge on second table in join
explain select * from t0,t1 where t0.key1 = 5 and
  (t1.key1 = t0.key1 or t1.key8 = t0.key1);

--echo # Fix for bug#1974
if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select * from t0,t1 where t0.key1 < 3 and
  (t1.key1 = t0.key1 or t1.key8 = t0.key1);

--echo #  index_merge inside union
explain select * from t1 where key1=3 or key2=4
  union select * from t1 where key1<4 or key3=5;

--echo #  index merge in subselect
explain select * from (select * from t1 where key1 = 3 or key2 =3) as z where key8 >5;

--echo # 12. check for long index_merges.
create table t3 like t0;
insert into t3 select * from t0;
alter table t3 add key9 int not null, add index i9(key9);
alter table t3 add keyA int not null, add index iA(keyA);
alter table t3 add keyB int not null, add index iB(keyB);
alter table t3 add keyC int not null, add index iC(keyC);
update t3 set key9=key1,keyA=key1,keyB=key1,keyC=key1;

-- disable_query_log
-- disable_result_log
analyze table t3;
-- enable_result_log
-- enable_query_log

explain select * from t3 where
  key1=1 or key2=2 or key3=3 or key4=4 or
  key5=5 or key6=6 or key7=7 or key8=8 or
  key9=9 or keyA=10 or keyB=11 or keyC=12;

select * from t3 where
  key1=1 or key2=2 or key3=3 or key4=4 or
  key5=5 or key6=6 or key7=7 or key8=8 or
  key9=9 or keyA=10 or keyB=11 or keyC=12;

--echo # Test for Bug#3183
explain select * from t0 where key1 < 3 or key2 < 4;
--echo # Bug#21277: InnoDB, wrong result set, index_merge strategy,
--echo #            second index not evaluated
select * from t0 where key1 < 3 or key2 < 4;

update t0 set key8=123 where key1 < 3 or key2 < 4;

-- disable_query_log
-- disable_result_log
analyze table t0;
-- enable_result_log
-- enable_query_log

--echo # Bug#21277: InnoDB, wrong result set, index_merge strategy,
--echo #            second index not evaluated
select * from t0 where key1 < 3 or key2 < 4;

delete from t0 where key1 < 3 or key2 < 4;
-- disable_query_log
-- disable_result_log
analyze table t0;
-- enable_result_log
-- enable_query_log

select * from t0 where key1 < 3 or key2 < 4;
select count(*) from t0;

--echo # Test for BUG#4177
drop table t4;
create table t4 (a int);
insert into t4 values (1),(4),(3);
-- disable_query_log
-- disable_result_log
analyze table t4;
-- enable_result_log
-- enable_query_log

set @save_join_buffer_size=@@join_buffer_size;
set join_buffer_size= 4096;

if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
explain select max(a.key1 + b.key1 + a.key2 + b.key2 + a.key3 + b.key3 + a.key4 + b.key4 + a.key5 + b.key5)
 from t0 as a force index(i1,i2), t0 as b force index (i1,i2)
  where (a.key1 < 500000 or a.key2 < 3)
  and   (b.key1 < 500000 or b.key2 < 3);

select max(a.key1 + b.key1 + a.key2 + b.key2 + a.key3 + b.key3 + a.key4 + b.key4 + a.key5 + b.key5)
 from t0 as a force index(i1,i2), t0 as b force index (i1,i2)
  where (a.key1 < 500000 or a.key2 < 3)
  and   (b.key1 < 500000 or b.key2 < 3);

update t0 set key1=1;
-- disable_query_log
-- disable_result_log
analyze table t0;
-- enable_result_log
-- enable_query_log

if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 # 11 #
}
explain select max(a.key1 + b.key1 + a.key2 + b.key2 + a.key3 + b.key3 + a.key4 + b.key4 + a.key5 + b.key5)
 from t0 as a force index(i1,i2), t0 as b force index (i1,i2)
  where (a.key1 = 1 or a.key2 = 1)
  and   (b.key1 = 1 or b.key2 = 1);

select max(a.key1 + b.key1 + a.key2 + b.key2 + a.key3 + b.key3 + a.key4 + b.key4 + a.key5 + b.key5)
 from t0 as a force index(i1,i2), t0 as b force index (i1,i2)
  where (a.key1 = 1 or a.key2 = 1)
  and   (b.key1 = 1 or b.key2 = 1);

alter table t0 add filler1 char(200), add filler2 char(200), add filler3 char(200);
update t0 set key2=1, key3=1, key4=1, key5=1,key6=1,key7=1 where key7 < 500;

-- disable_query_log
-- disable_result_log
analyze table t0;
-- enable_result_log
-- enable_query_log

--echo # The next query will not use index i7 in intersection if the OS doesn't
--echo # support file sizes > 2GB. (ha_myisam::ref_length depends on this and index
--echo # scan cost estimates depend on ha_myisam::ref_length)
if (!$index_merge_random_rows_in_EXPLAIN)
{
  # Too unstable for innodb
  --replace_column 10 # 11 #
  --replace_result "4,4,4,4,4,4,4" X "4,4,4,4,4,4" X "i6,i7" "i6,i7?" "i6" "i6,i7?"
  explain select max(a.key1 + b.key1 + a.key2 + b.key2 + a.key3 + b.key3 + a.key4 + b.key4 + a.key5 + b.key5)
   from t0 as a, t0 as b
   where (a.key1 = 1 and a.key2 = 1 and a.key3 = 1 and a.key4=1 and a.key5=1 and a.key6=1 and a.key7 = 1 or a.key8=1)
    and (b.key1 = 1 and b.key2 = 1 and b.key3 = 1 and b.key4=1 and b.key5=1 and b.key6=1 and b.key7 = 1 or b.key8=1);
}
select max(a.key1 + b.key1 + a.key2 + b.key2 + a.key3 + b.key3 + a.key4 + b.key4 + a.key5 + b.key5)
 from t0 as a, t0 as b
 where (a.key1 = 1 and a.key2 = 1 and a.key3 = 1 and a.key4=1 and a.key5=1 and a.key6=1 and a.key7 = 1 or a.key8=1)
  and (b.key1 = 1 and b.key2 = 1 and b.key3 = 1 and b.key4=1 and b.key5=1 and b.key6=1 and b.key7 = 1 or b.key8=1);

set join_buffer_size= @save_join_buffer_size;
--echo # Test for BUG#4177 ends

drop table t0, t1, t2, t3, t4;

--echo # BUG#16166
CREATE TABLE t1 (
  cola char(3) not null, colb char(3) not null,  filler char(200),
  key(cola), key(colb)
);
INSERT INTO t1 VALUES ('foo','bar', 'ZZ'),('fuz','baz', 'ZZ');

--disable_query_log
let $1=9;
while ($1)
{
  eval INSERT INTO t1 SELECT * from t1 WHERE cola = 'foo';
  dec $1;
}

let $1=13;
while ($1)
{
  eval INSERT INTO t1 SELECT * from t1 WHERE cola <> 'foo';
  dec $1;
}

--enable_query_log

OPTIMIZE TABLE t1;
select count(*) from t1;

-- disable_query_log
-- disable_result_log
analyze table t1;
-- enable_result_log
-- enable_query_log

if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 # 11 #
}
explain select * from t1 WHERE cola = 'foo' AND colb = 'bar';

# Demonstrates printing index merge in EXPLAIN FORMAT=tree.
--skip_if_hypergraph   # Different query plan.
--replace_regex $elide_costs
explain format=tree select * from t1 WHERE cola = 'foo' AND colb = 'bar';

if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 # 11 #
}
explain select * from t1 force index(cola,colb) WHERE cola = 'foo' AND colb = 'bar';
drop table t1;

if ($merge_table_support)
{
--echo #
--echo # BUG#17314: Index_merge/intersection not choosen by the
--echo #            optimizer for MERGE tables
--echo #
create table t0 (a int);
insert into t0 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);
create table t1 (
  a int, b int,
  filler1 char(200), filler2 char(200),
  key(a),key(b)
);
insert into t1 select @v:= A.a, @v, 't1', 'filler2' from t0 A, t0 B, t0 C;
create table t2 like t1;

create table t3 (
  a int, b int,
  filler1 char(200), filler2 char(200),
  key(a),key(b)
) engine=merge union=(t1,t2);

-- disable_query_log
-- disable_result_log
analyze table t0;
analyze table t1;
analyze table t2;
analyze table t3;
-- enable_result_log
-- enable_query_log

--replace_column 10 #
explain select * from t1 where a=1 and b=1;
--replace_column 10 #
explain select * from t3 where a=1 and b=1;

drop table t3;
drop table t0, t1, t2;
}

--echo #
--echo # BUG#20256 - LOCK WRITE - MyISAM
--echo #
CREATE TABLE t1(a INT);
INSERT INTO t1 VALUES(1);
CREATE TABLE t2(a INT, b INT, dummy CHAR(16) DEFAULT '', KEY(a), KEY(b));
INSERT INTO t2(a,b) VALUES
(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),
(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),
(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),
(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),
(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),
(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),
(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),
(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),
(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),
(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),
(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),
(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),
(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),
(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),
(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),
(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),
(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),
(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),(0,0),
(1,2);
LOCK TABLES t1 WRITE, t2 WRITE;
INSERT INTO t2(a,b) VALUES(1,2);
SELECT t2.a FROM t1,t2 WHERE t2.b=2 AND t2.a=1;
UNLOCK TABLES;
DROP TABLE t1, t2;

--echo #
--echo # BUG#29740: HA_KEY_SCAN_NOT_ROR wasn't set for HEAP engine
--echo # 
CREATE TABLE `t1` (
  `a` int(11) DEFAULT NULL,
  `filler` char(200) DEFAULT NULL,
  `b` int(11) DEFAULT NULL,
  KEY `a` (`a`),
  KEY `b` (`b`)
) ENGINE=MEMORY DEFAULT CHARSET=latin1;

insert into t1 values
(0, 'filler', 0), (1, 'filler', 1), (2, 'filler', 2), (3, 'filler', 3), 
(4, 'filler', 4), (5, 'filler', 5), (6, 'filler', 6), (7, 'filler', 7), 
(8, 'filler', 8), (9, 'filler', 9), (0, 'filler', 0), (1, 'filler', 1), 
(2, 'filler', 2), (3, 'filler', 3), (4, 'filler', 4), (5, 'filler', 5), 
(6, 'filler', 6), (7, 'filler', 7), (8, 'filler', 8), (9, 'filler', 9), 
(10, 'filler', 10), (11, 'filler', 11), (12, 'filler', 12), (13, 'filler', 13),
(14, 'filler', 14), (15, 'filler', 15), (16, 'filler', 16), (17, 'filler', 17), 
(18, 'filler', 18), (19, 'filler', 19), (4, '5      ', 0), (5, '4      ', 0), 
(4, '4      ', 0), (4, 'qq     ', 5), (5, 'qq     ', 4), (4, 'zz     ', 4);

create table t2(
  `a` int(11) DEFAULT NULL,
  `filler` char(200) DEFAULT NULL,
  `b` int(11) DEFAULT NULL,
  KEY USING BTREE (`a`),
  KEY USING BTREE (`b`)
) ENGINE=MEMORY DEFAULT CHARSET=latin1;
insert into t2 select * from t1;

-- disable_query_log
-- disable_result_log
analyze table t1;
analyze table t2;
-- enable_result_log
-- enable_query_log

--echo must use sort-union rather than union:
--replace_column 10 #
explain select * from t1 where a=4 or b=4;
--sorted_result
select * from t1 where a=4 or b=4;
--sorted_result
select * from t1 ignore index(a,b) where a=4 or b=4;

--echo must use union, not sort-union:
--replace_column 10 #
explain select * from t2 where a=4 or b=4;
--sorted_result
select * from t2 where a=4 or b=4;

drop table t1, t2;

--echo #
--echo # Bug #37943: Reproducible mysqld crash/sigsegv in sel_trees_can_be_ored
--echo #

CREATE TABLE t1 (a varchar(8), b set('a','b','c','d','e','f','g','h'),
                 KEY b(b), KEY a(a));
INSERT INTO t1 VALUES ('y',''), ('z','');

#should not crash
SELECT b,a from t1 WHERE (b!='c' AND b!='f' && b!='h') OR 
  (a='pure-S') OR (a='DE80337a') OR (a='DE80799');

DROP TABLE t1;

--echo #
--echo # BUG#40974: Incorrect query results when using clause
--echo #            evaluated using range check
--echo #
create table t0 (a int);
insert into t0 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);

create table t1 (a int);
insert into t1 values (1),(2);
create table t2(a int, b int);
insert into t2 values (1,1), (2, 1000);
create table t3 (a int, b int, filler char(100), key(a), key(b));

insert into t3 select 1000, 1000,'filler' from t0 A, t0 B, t0 C; 
insert into t3 values (1,1,'data');
insert into t3 values (1,1,'data');
-- echo The plan should be ALL/ALL/ALL(Range checked for each record (index map: 0x3)

-- disable_query_log
-- disable_result_log
analyze table t0;
analyze table t1;
analyze table t2;
analyze table t3;
-- enable_result_log
-- enable_query_log

if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 # 11 #
}
explain select * from t1 
where exists (select 1 from t2, t3 
              where t2.a=t1.a and (t3.a=t2.b or t3.b=t2.b or t3.b=t2.b+1));

select * from t1 
where exists (select 1 from t2, t3 
              where t2.a=t1.a and (t3.a=t2.b or t3.b=t2.b or t3.b=t2.b+1));

drop table t0, t1, t2, t3;

--echo #
--echo # BUG#44810: index merge and order by with low sort_buffer_size 
--echo # crashes server!
--echo #
CREATE TABLE t1(a VARCHAR(128),b VARCHAR(128),KEY(A),KEY(B)) charset latin1;
INSERT INTO t1 VALUES (REPEAT('a',128),REPEAT('b',128));
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
-- disable_query_log
-- disable_result_log
analyze table t1;
-- enable_result_log
-- enable_query_log

SET SESSION sort_buffer_size=1;

if ($index_merge_random_rows_in_EXPLAIN)
{
  --replace_column 10 #
}
EXPLAIN 
SELECT * FROM t1 FORCE INDEX(a,b) WHERE a LIKE 'a%' OR b LIKE 'b%' 
  ORDER BY a,b;

--echo # we don't actually care about the result : we're checking if it crashes
--disable_result_log
SELECT * FROM t1 FORCE INDEX(a,b) WHERE a LIKE 'a%' OR b LIKE 'b%' 
  ORDER BY a,b;
--enable_result_log

SET SESSION sort_buffer_size=DEFAULT;
DROP TABLE t1;


--echo End of 5.0 tests
