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

--echo #---------------- Index merge test 1 -------------------------------------------

eval SET SESSION STORAGE_ENGINE = $engine_type;

--disable_warnings
drop table if exists t0, t1, t2, t3, t4;
--enable_warnings

# Create and fill a table with simple keys
create table t0
(
  key1 int not null,
  INDEX i1(key1)
);

--disable_query_log
insert into t0 values (1),(2),(3),(4),(5),(6),(7),(8);

let $1=7;
set @d=8;
while ($1)
{
  eval insert into t0 select key1+@d from t0;
  eval set @d=@d*2;
  dec $1;
}
--enable_query_log

alter table t0 add key2 int not null, add index i2(key2);
alter table t0 add key3 int not null, add index i3(key3);
alter table t0 add key4 int not null, add index i4(key4);
alter table t0 add key5 int not null, add index i5(key5);
alter table t0 add key6 int not null, add index i6(key6);
alter table t0 add key7 int not null, add index i7(key7);
alter table t0 add key8 int not null, add index i8(key8);

update t0 set key2=key1,key3=key1,key4=key1,key5=key1,key6=key1,key7=key1,key8=1024-key1;
analyze table t0;

# 1. One index
explain select * from t0 where key1 < 3 or key1 > 1020;

# 2. Simple cases
explain
select * from t0 where key1 < 3 or key2 > 1020;
select * from t0 where key1 < 3 or key2 > 1020;

explain select * from t0 where key1 < 3 or key2 <4;

explain
select * from t0 where (key1 > 30 and key1<35) or (key2 >32 and key2 < 40);
# Bug#21277: InnoDB, wrong result set, index_merge strategy, second index not evaluated
select * from t0 where (key1 > 30 and key1<35) or (key2 >32 and key2 < 40);

# 3. Check that index_merge doesn't break "ignore/force/use index"
explain select * from t0 ignore index (i2) where key1 < 3 or key2 <4;
explain select * from t0 where (key1 < 3 or key2 <4) and key3 = 50;
explain select * from t0 use index (i1,i2) where (key1 < 3 or key2 <4) and key3 = 50;

explain select * from t0 where (key1 > 1 or key2  > 2);
explain select * from t0 force index (i1,i2) where (key1 > 1 or key2  > 2);


# 4. Check if conjuncts are grouped by keyuse
explain
  select * from t0 where key1<3 or key2<3 or (key1>5 and key1<8) or
  (key1>10 and key1<12) or (key2>100 and key2<110);

# 5. Check index_merge with conjuncts that are always true/false
#    verify fallback to "range" if there is only one non-confluent condition
explain select * from t0 where key2 = 45 or key1 <=> null;

explain select * from t0 where key2 = 45 or key1 is not null;
explain select * from t0 where key2 = 45 or key1 is null;

#   the last conj. is always false and will be discarded
explain select * from t0 where key2=10 or key3=3 or key4 <=> null;

#   the last conj. is always true and will cause 'all' scan
explain select * from t0 where key2=10 or key3=3 or key4 is null;

#   some more complicated cases
explain select key1 from t0 where (key1 <=> null) or (key2 < 5) or
                                   (key3=10) or (key4 <=> null);
explain select key1 from t0 where (key1 <=> null) or (key1 < 5) or
                                   (key3=10) or (key4 <=> null);

# 6.Several ways to do index_merge, (ignored) index_merge vs. range
explain select * from t0 where
  (key1 < 3 or key2 < 3) and (key3 < 4 or key4 < 4) and (key5 < 5 or key6 < 5);

explain
select * from t0 where (key1 < 3 or key2 < 6) and (key1 < 7 or key3 < 4);

select * from t0 where (key1 < 3 or key2 < 6) and (key1 < 7 or key3 < 4);


explain select * from t0 where
  (key1 < 3 or key2 < 3) and (key3 < 4 or key4 < 4) and (key5 < 2 or key6 < 2);

#   now index_merge is not used at all when "range" is possible
explain select * from t0 where
  (key1 < 3 or key2 < 3) and (key3 < 100);

#   this even can cause "all" scan:
explain select * from t0 where
  (key1 < 3 or key2 < 3) and (key3 < 1000);


# 7. Complex cases
#   tree_or(List<SEL_IMERGE>, range SEL_TREE).
explain select * from t0 where
    ((key1 < 4 or key2 < 4) and (key2 <5 or key3 < 4))
  or
    key2 > 5;

explain select * from t0 where
    ((key1 < 4 or key2 < 4) and (key2 <5 or key3 < 4))
  or
    key1 < 7;

select * from t0 where
    ((key1 < 4 or key2 < 4) and (key2 <5 or key3 < 4))
  or
    key1 < 7;

#   tree_or(List<SEL_IMERGE>, List<SEL_IMERGE>).
explain select * from t0 where
    ((key1 < 4 or key2 < 4) and (key3 <5 or key5 < 4))
  or
    ((key5 < 5 or key6 < 6) and (key7 <7 or key8 < 4));

explain select * from t0 where
    ((key3 <5 or key5 < 4) and (key1 < 4 or key2 < 4))
  or
    ((key7 <7 or key8 < 4) and (key5 < 5 or key6 < 6));

explain select * from t0 where
    ((key3 <5 or key5 < 4) and (key1 < 4 or key2 < 4))
  or
    ((key3 <7 or key5 < 2) and (key5 < 5 or key6 < 6));

explain select * from t0 where
    ((key3 <5 or key5 < 4) and (key1 < 4 or key2 < 4))
  or
    (((key3 <7 and key7 < 6) or key5 < 2) and (key5 < 5 or key6 < 6));

explain select * from t0 where
    ((key3 <5 or key5 < 4) and (key1 < 4 or key2 < 4))
  or
    ((key3 >=5 or key5 < 2) and (key5 < 5 or key6 < 6));

explain select * from t0 force index(i1, i2, i3, i4, i5, i6 ) where
    ((key3 <5 or key5 < 4) and (key1 < 4 or key2 < 4))
  or
    ((key3 >=5 or key5 < 2) and (key5 < 5 or key6 < 6));

# 8. Verify that "order by" after index merge uses filesort
select * from t0 where key1 < 5 or key8 < 4 order by key1;

explain
select * from t0 where key1 < 5 or key8 < 4 order by key1;

# 9. Check that index_merge cost is compared to 'index' where possible
create table t2 like t0;
insert into t2 select * from t0;

alter table t2 add index i1_3(key1, key3);
alter table t2 add index i2_3(key2, key3);
alter table t2 drop index i1;
alter table t2 drop index i2;
alter table t2 add index i321(key3, key2, key1);

#   index_merge vs 'index', index_merge is better.
explain select key3 from t2 where key1 = 100 or key2 = 100;

#   index_merge vs 'index', 'index' is better.
explain select key3 from t2 where key1 <100 or key2 < 100;

#   index_merge vs 'all', index_merge is better.
explain select key7 from t2 where key1 <100 or key2 < 100;

# 10. Multipart keys.
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

#   the following will be handled by index_merge:
select * from t4 where key1a = 3 or key1b = 4;
explain select * from t4 where key1a = 3 or key1b = 4;

#   and the following will not
explain select * from t4 where key2 = 1 and (key2_1 = 1 or key3 = 5);

explain select * from t4 where key2 = 1 and (key2_1 = 1 or key2_2 = 5);

explain select * from t4 where key2_1 = 1 or key2_2 = 5;


# 11. Multitable selects
create table t1 like t0;
insert into t1 select * from t0;

#  index_merge on first table in join
explain select * from t0 left join t1 on (t0.key1=t1.key1)
  where t0.key1=3 or t0.key2=4;

select * from t0 left join t1 on (t0.key1=t1.key1)
  where t0.key1=3 or t0.key2=4;

explain
select * from t0,t1 where (t0.key1=t1.key1) and ( t0.key1=3 or t0.key2=4);

#  index_merge vs. ref
explain
select * from t0,t1 where (t0.key1=t1.key1) and
  (t0.key1=3 or t0.key2=4) and t1.key1<200;

#  index_merge vs. ref
explain
select * from t0,t1 where (t0.key1=t1.key1) and
  (t0.key1=3 or t0.key2<4) and t1.key1=2;

#  index_merge on second table in join
explain select * from t0,t1 where t0.key1 = 5 and
  (t1.key1 = t0.key1 or t1.key8 = t0.key1);

# Fix for bug#1974
explain select * from t0,t1 where t0.key1 < 3 and
  (t1.key1 = t0.key1 or t1.key8 = t0.key1);

#  index_merge inside union
explain select * from t1 where key1=3 or key2=4
  union select * from t1 where key1<4 or key3=5;

#  index merge in subselect
explain select * from (select * from t1 where key1 = 3 or key2 =3) as Z where key8 >5;

# 12. check for long index_merges.
create table t3 like t0;
insert into t3 select * from t0;
alter table t3 add key9 int not null, add index i9(key9);
alter table t3 add keyA int not null, add index iA(keyA);
alter table t3 add keyB int not null, add index iB(keyB);
alter table t3 add keyC int not null, add index iC(keyC);
update t3 set key9=key1,keyA=key1,keyB=key1,keyC=key1;

explain select * from t3 where
  key1=1 or key2=2 or key3=3 or key4=4 or
  key5=5 or key6=6 or key7=7 or key8=8 or
  key9=9 or keyA=10 or keyB=11 or keyC=12;

select * from t3 where
  key1=1 or key2=2 or key3=3 or key4=4 or
  key5=5 or key6=6 or key7=7 or key8=8 or
  key9=9 or keyA=10 or keyB=11 or keyC=12;

# Test for Bug#3183
explain select * from t0 where key1 < 3 or key2 < 4;
# Bug#21277: InnoDB, wrong result set, index_merge strategy, second index not evaluated
select * from t0 where key1 < 3 or key2 < 4;

update t0 set key8=123 where key1 < 3 or key2 < 4;
# Bug#21277: InnoDB, wrong result set, index_merge strategy, second index not evaluated
select * from t0 where key1 < 3 or key2 < 4;

delete from t0 where key1 < 3 or key2 < 4;
select * from t0 where key1 < 3 or key2 < 4;
select count(*) from t0;

# Test for BUG#4177
drop table t4;
create table t4 (a int);
insert into t4 values (1),(4),(3);
set @save_join_buffer_size=@@join_buffer_size;
set join_buffer_size= 4096;
explain select max(A.key1 + B.key1 + A.key2 + B.key2 + A.key3 + B.key3 + A.key4 + B.key4 + A.key5 + B.key5)
 from t0 as A force index(i1,i2), t0 as B force index (i1,i2)
  where (A.key1 < 500000 or A.key2 < 3)
  and   (B.key1 < 500000 or B.key2 < 3);

select max(A.key1 + B.key1 + A.key2 + B.key2 + A.key3 + B.key3 + A.key4 + B.key4 + A.key5 + B.key5)
 from t0 as A force index(i1,i2), t0 as B force index (i1,i2)
  where (A.key1 < 500000 or A.key2 < 3)
  and   (B.key1 < 500000 or B.key2 < 3);

update t0 set key1=1;
explain select max(A.key1 + B.key1 + A.key2 + B.key2 + A.key3 + B.key3 + A.key4 + B.key4 + A.key5 + B.key5)
 from t0 as A force index(i1,i2), t0 as B force index (i1,i2)
  where (A.key1 = 1 or A.key2 = 1)
  and   (B.key1 = 1 or B.key2 = 1);

select max(A.key1 + B.key1 + A.key2 + B.key2 + A.key3 + B.key3 + A.key4 + B.key4 + A.key5 + B.key5)
 from t0 as A force index(i1,i2), t0 as B force index (i1,i2)
  where (A.key1 = 1 or A.key2 = 1)
  and   (B.key1 = 1 or B.key2 = 1);

alter table t0 add filler1 char(200), add filler2 char(200), add filler3 char(200);
update t0 set key2=1, key3=1, key4=1, key5=1,key6=1,key7=1 where key7 < 500;

# The next query will not use index i7 in intersection if the OS doesn't
# support file sizes > 2GB. (ha_myisam::ref_length depends on this and index
# scan cost estimates depend on ha_myisam::ref_length)
--replace_column 9 #
--replace_result "4,4,4,4,4,4,4" X "4,4,4,4,4,4" X "i6,i7" "i6,i7?" "i6" "i6,i7?"
explain select max(A.key1 + B.key1 + A.key2 + B.key2 + A.key3 + B.key3 + A.key4 + B.key4 + A.key5 + B.key5)
 from t0 as A, t0 as B
 where (A.key1 = 1 and A.key2 = 1 and A.key3 = 1 and A.key4=1 and A.key5=1 and A.key6=1 and A.key7 = 1 or A.key8=1)
  and (B.key1 = 1 and B.key2 = 1 and B.key3 = 1 and B.key4=1 and B.key5=1 and B.key6=1 and B.key7 = 1 or B.key8=1);

select max(A.key1 + B.key1 + A.key2 + B.key2 + A.key3 + B.key3 + A.key4 + B.key4 + A.key5 + B.key5)
 from t0 as A, t0 as B
 where (A.key1 = 1 and A.key2 = 1 and A.key3 = 1 and A.key4=1 and A.key5=1 and A.key6=1 and A.key7 = 1 or A.key8=1)
  and (B.key1 = 1 and B.key2 = 1 and B.key3 = 1 and B.key4=1 and B.key5=1 and B.key6=1 and B.key7 = 1 or B.key8=1);

set join_buffer_size= @save_join_buffer_size;
# Test for BUG#4177 ends

drop table t0, t1, t2, t3, t4;

# BUG#16166
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
explain select * from t1 WHERE cola = 'foo' AND colb = 'bar';
explain select * from t1 force index(cola,colb) WHERE cola = 'foo' AND colb = 'bar';
drop table t1;

if ($merge_table_support)
{
#
# BUG#17314: Index_merge/intersection not choosen by the optimizer for MERGE tables
#
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

--replace_column 9 #
explain select * from t1 where a=1 and b=1;
--replace_column 9 #
explain select * from t3 where a=1 and b=1;

drop table t3;
drop table t0, t1, t2;
}

#
# BUG#20256 - LOCK WRITE - MyISAM
#
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

#
# BUG#29740: HA_KEY_SCAN_NOT_ROR wasn't set for HEAP engine
# 
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

--echo must use sort-union rather than union:
--replace_column 9 #
explain select * from t1 where a=4 or b=4;
--sorted_result
select * from t1 where a=4 or b=4;
--sorted_result
select * from t1 ignore index(a,b) where a=4 or b=4;

--echo must use union, not sort-union:
--replace_column 9 #
explain select * from t2 where a=4 or b=4;
--sorted_result
select * from t2 where a=4 or b=4;

drop table t1, t2;

#
# Bug #37943: Reproducible mysqld crash/sigsegv in sel_trees_can_be_ored
#

CREATE TABLE t1 (a varchar(8), b set('a','b','c','d','e','f','g','h'),
                 KEY b(b), KEY a(a));
INSERT INTO t1 VALUES ('y',''), ('z','');

#should not crash
SELECT b,a from t1 WHERE (b!='c' AND b!='f' && b!='h') OR 
  (a='pure-S') OR (a='DE80337a') OR (a='DE80799');

DROP TABLE t1;

--echo #
--echo # BUG#40974: Incorrect query results when using clause evaluated using range check
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
CREATE TABLE t1(a VARCHAR(128),b VARCHAR(128),KEY(A),KEY(B));
INSERT INTO t1 VALUES (REPEAT('a',128),REPEAT('b',128));
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
SET SESSION sort_buffer_size=1;
EXPLAIN 
SELECT * FROM t1 FORCE INDEX(a,b) WHERE a LIKE 'a%' OR b LIKE 'b%' 
  ORDER BY a,b;
# we don't actually care about the result : we're checking if it crashes
--disable_result_log
SELECT * FROM t1 FORCE INDEX(a,b) WHERE a LIKE 'a%' OR b LIKE 'b%' 
  ORDER BY a,b;
--enable_result_log

SET SESSION sort_buffer_size=DEFAULT;
DROP TABLE t1;


--echo End of 5.0 tests
