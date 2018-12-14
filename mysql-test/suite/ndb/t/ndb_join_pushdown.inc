########################################
# Define two connections as we want DDL to use its own connection
# in order to keep DDL statistics counting out of the way
# of the SPJ testing
########################################
connect (spj,localhost,root,,test);
connect (ddl,localhost,root,,test);

select @@session.optimizer_switch;

connection spj;
# Save old mysqld counter values.
create temporary table server_counts_at_startup
       select * from performance_schema.global_status 
       where variable_name in 
       ('Ndb_pruned_scan_count',
        'Ndb_sorted_scan_count',
        'Ndb_pushed_queries_defined',
        'Ndb_pushed_queries_dropped');

##############
# Test start

--error 0,1193
set @save_debug = @@global.debug;
set @save_ndb_join_pushdown = @@session.ndb_join_pushdown;
set ndb_join_pushdown = true;

connection ddl;
create table t1 (
  a int not null,
  b int not null,
  c int not null,
  d int not null,
  primary key (`a`,`b`)
) engine=ndbcluster
partition by key() partitions 8;

connection spj;
insert into t1 values
(1,1,1,1), (2,2,2,2), (3,3,3,3), (4,4,4,4),
(1,2,5,1), (1,3,1,2), (1,4,2,3),
(2,1,3,4), (2,3,4,5), (2,4,5,1),
(3,1,1,2), (3,2,2,3), (3,4,3,4),
(4,1,4,5), (4,2,5,1), (4,3,1,2);

explain
select *
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c;
--sorted_result
select *
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c;

# Check that new JSON explain format is also handled:
explain format=JSON
select *
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c;

# Check that we do not push an operation if this prevents us from using
# 'join buffer'.
explain
select straight_join count(*) 
from t1 as x1 
join t1 as x2 on x1.d > x2.a + 1000 
join t1 as x3 on x1.c=x3.a and x1.d=x3.b;
select straight_join count(*) 
from t1 as x1 
join t1 as x2 on x1.d > x2.a + 1000 
join t1 as x3 on x1.c=x3.a and x1.d=x3.b;

# Check that we do not push an operation if this prevents us from using
# 'join buffer'.
explain select *
from t1 as x1 
join t1 as x2 on x1.a=1 and x1.c=x2.a and x1.d=x2.b 
join t1 as x3 
join t1 as x4 where x4.a=x3.c and x4.b=x1.d;
--sorted_result
select * 
from t1 as x1 
join t1 as x2 on x1.a=1 and x1.c=x2.a and x1.d=x2.b 
join t1 as x3 
join t1 as x4 where x4.a=x3.c and x4.b=x1.d;

explain
select *
from t1
left join t1 as t2 on t2.a = t1.b and t2.b = t1.c;
--sorted_result
select *
from t1
left join t1 as t2 on t2.a = t1.b and t2.b = t1.c;

explain
select *
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
join t1 as t3 on t3.a = t2.a and t3.b = t2.b
where t1.a = 1 and t1.b = 1;
--sorted_result
select *
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
join t1 as t3 on t3.a = t2.a and t3.b = t2.b
where t1.a = 1 and t1.b = 1;

explain
select *
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
join t1 as t3 on t3.a = t2.a and t3.b = t2.b;
--sorted_result
select *
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
join t1 as t3 on t3.a = t2.a and t3.b = t2.b;

explain
select *
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
left join t1 as t3 on t3.a = t2.a and t3.b = t2.b;
--sorted_result
select *
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
left join t1 as t3 on t3.a = t2.a and t3.b = t2.b;

explain
select *
from t1
left join t1 as t2 on t2.a = t1.b and t2.b = t1.c
left join t1 as t3 on t3.a = t2.a and t3.b = t2.b;
--sorted_result
select *
from t1
left join t1 as t2 on t2.a = t1.b and t2.b = t1.c
left join t1 as t3 on t3.a = t2.a and t3.b = t2.b;

set ndb_join_pushdown=true;

--replace_result p0,p1,p2,p3,p4,p5,p6,p7 pXYZ p0 pX p5 pX
explain
select *
from t1
join t1 as t2 on t2.a = t1.c and t2.b = t1.d
where t1.a = 2 and t1.b = 3;
select *
from t1
join t1 as t2 on t2.a = t1.c and t2.b = t1.d
where t1.a = 2 and t1.b = 3;

################################################################
# Bug#13901890 SQL NODE CRASHES DURING INSERT SELECT
#
# Incorrect lifetime handling for NdbQuery objects where
# the root operation was a 'const' lookup operation.
#
# 'virtual handler::index_read_idx_map()' was incorectly 
# implemented by ha_ndbcluster, such that it failed to 
# 'close' the table when the single 'const' row has been read.
# Neither was it registered as 'open' by using the ::ha_index_init().
# (Called ha_ndbcluster::index_init() directly)
# This resulted in that NdbQuery::close() was never called
# on these 'const' SPJ queries. Instead NdbTransaction end 
# forcefully whiped them away by calling NdbQuery::release().
# However, as there still was dangling pointer refs to 
# them from ha_ndbcluster::m_active_query, we may later refer
# them and crash!
#
#  - Same query as above.
#  - Added explict table locks as cleanup of these is one 
#    (of several?) way to cause released NdbQuery objects to
#    be refered.
################################################################

# LOCK'ed tables invokes the same code path as executing
# an 'INSERT... SELECT' inside a procedure invoked from a trigger.
# ... and is a much simpler testcase..... 
LOCK TABLES t1 read, t1 as t2 read;

--replace_result p0,p1,p2,p3,p4,p5,p6,p7 pXYZ p0 pX p5 pX
explain
select *
from t1
join t1 as t2 on t2.a = t1.c and t2.b = t1.d
where t1.a = 2 and t1.b = 3;

#Returns empty result set
select *
from t1
join t1 as t2 on t2.a = t1.c and t2.b = t1.d
where t1.a = 2 and t1.b = 3;

#Returns a single row
select *
from t1
join t1 as t2 on t2.a = t1.c and t2.b = t1.d
where t1.a = 3 and t1.b = 3;
UNLOCK TABLES;

--replace_result p0,p1,p2,p3,p4,p5,p6,p7 pXYZ p0 pX p5 pX
explain
select *
from t1
left join t1 as t2 on t2.a = t1.c and t2.b = t1.d
where t1.a = 2 and t1.b = 3;
select *
from t1
left join t1 as t2 on t2.a = t1.c and t2.b = t1.d
where t1.a = 2 and t1.b = 3;

--replace_result p0,p1,p2,p3,p4,p5,p6,p7 pXYZ p0 pX p5 pX
explain
select *
from t1
left join t1 as t2 on t2.a = t1.c and t2.b = t1.d
where t1.a = 2 and t1.b = 3
order by t1.c;
select *
from t1
left join t1 as t2 on t2.a = t1.c and t2.b = t1.d
where t1.a = 2 and t1.b = 3
order by t1.c;

set ndb_join_pushdown=false;
--replace_result p0,p1,p2,p3,p4,p5,p6,p7 pXYZ p0 pX p5 pX
explain
select *
from t1
join t1 as t2 on t2.a = t1.c and t2.b = t1.d
where t1.a = 2 and t1.b = 3;
select *
from t1
join t1 as t2 on t2.a = t1.c and t2.b = t1.d
where t1.a = 2 and t1.b = 3;

--replace_result p0,p1,p2,p3,p4,p5,p6,p7 pXYZ p0 pX p5 pX
explain
select *
from t1
left join t1 as t2 on t2.a = t1.c and t2.b = t1.d
where t1.a = 2 and t1.b = 3;
select *
from t1
left join t1 as t2 on t2.a = t1.c and t2.b = t1.d
where t1.a = 2 and t1.b = 3;

set ndb_join_pushdown=true;
explain
select *
from t1
join t1 as t2 on t2.a = t1.c and t2.b = t1.d
where t1.a = 1 and t1.b = 1;
select *
from t1
join t1 as t2 on t2.a = t1.c and t2.b = t1.d
where t1.a = 1 and t1.b = 1;

explain
select *
from t1
left join t1 as t2 on t2.a = t1.c and t2.b = t1.d
where t1.a = 1 and t1.b = 1;
select *
from t1
left join t1 as t2 on t2.a = t1.c and t2.b = t1.d
where t1.a = 1 and t1.b = 1;

set ndb_join_pushdown=false;
explain
select *
from t1
join t1 as t2 on t2.a = t1.c and t2.b = t1.d
where t1.a = 1 and t1.b = 1;
select *
from t1
join t1 as t2 on t2.a = t1.c and t2.b = t1.d
where t1.a = 1 and t1.b = 1;

explain
select *
from t1
left join t1 as t2 on t2.a = t1.c and t2.b = t1.d
where t1.a = 1 and t1.b = 1;
select *
from t1
left join t1 as t2 on t2.a = t1.c and t2.b = t1.d
where t1.a = 1 and t1.b = 1;

## Join as 't1 ALL' -> 't2 RANGE' -> 't3 EQ_REF'
## Possibly joinable starting with 't2 - RANGE' as root.
## However t3's join condition 't3.a = t1.c' refers t1 which is
## outside the scope of current queryplan. The equality set 
## should be consulted in order to replace 't1.c' with 't2.a'
## inside the scope
set ndb_join_pushdown=true;
explain
select *
from t1
join t1 as t2 on t2.a = t1.c
join t1 as t3 on t3.a = t1.c and t3.b = t2.b;
--sorted_result
select *
from t1
join t1 as t2 on t2.a = t1.c
join t1 as t3 on t3.a = t1.c and t3.b = t2.b;

## Join as 'x ALL' -> 'y ALL' -> 'z EQ_REF'
## As Scan vs. scan is not pushable, only y,z is pushed
## However join cond on z refer x which is a
## (pseudo constant) paramValue wrt. the pushed join.
## As we have a dependency on previous rows these 
## should not be join cached (ref. HA_PUSH_BLOCK_JOINCACHE)
explain
select straight_join *
  from (t1 as x cross join t1 as y)
     join t1 as z on z.a=x.a and z.b=y.b;
--sorted_result
select straight_join *
  from (t1 as x cross join t1 as y)
     join t1 as z on z.a=x.a and z.b=y.b;


## Some variants of the above where t3 has a join conditions in t1
## where t1 is outside scope of pushed join (as above). However, in
## these tests t3 is also linked with t2 through another join condition.
## This makes t3 join pushable by specifying the value of t1.c as a
## paramValue()
explain
select *
from t1
  straight_join t1 as t2 on t2.a = t1.b+0 and t2.b = t1.c
  straight_join t1 as t3 on t3.a = t1.b and t3.b = t2.b;
--sorted_result
select *
from t1
  straight_join t1 as t2 on t2.a = t1.b+0 and t2.b = t1.c
  straight_join t1 as t3 on t3.a = t1.b and t3.b = t2.b;

--replace_column 10 # 11 #
explain
select *
from t1
  straight_join t1 as t2 on t2.a = t1.b+0 and t2.b = t1.c
  straight_join t1 as t3 on t3.a = t1.b and t3.b = t2.b
where t1.a=1 and t1.d=1;
--sorted_result
select *
from t1
  straight_join t1 as t2 on t2.a = t1.b+0 and t2.b = t1.c
  straight_join t1 as t3 on t3.a = t1.b and t3.b = t2.b
where t1.a=1 and t1.d=1;

explain
select *
from t1
  straight_join t1 as t2 on t2.a = t1.b+0
  straight_join t1 as t3 on t3.a = t1.b and t3.b = t2.b;
--sorted_result
select *
from t1
  straight_join t1 as t2 on t2.a = t1.b+0
  straight_join t1 as t3 on t3.a = t1.b and t3.b = t2.b;


## Create a non-ndb table used as a tool to force part of
## a query to be non-pushable.
connection ddl;
create table t1_myisam (
  a int not null,
  b int not null,
  c int not null,
  d int not null,
  primary key (`a`,`b`)
) engine=myisam;

connection spj;
insert into t1_myisam values
(1,1,1,1), (2,2,1,1), (3,3,1,1), (4,4,1,1);


## Optimizer will use the equality set to replace 't2.a' 
## in the term 't3.a = t2.a' with 't1.c' (as 't2.a = t1.c').
## Furthermore the MyIsam table t1 is const table optimized making
## 't1.c' a const_item. This constant value has not yet been materialized
## into the key_buffer when the definition for the linked query is
## created. However, it is always available through the 
## Field defining the KEY_PART of this JT_EQ_REF.
##

set ndb_join_pushdown=true;

explain
select *
from t1_myisam as t1
join t1 as t2 on t2.a = t1.c and t2.b = t1.d
join t1 as t3 on t3.a = t2.a and t3.b = t2.b
where t1.a=2 and t1.b=2;
--sorted_result
select *
from t1_myisam as t1
join t1 as t2 on t2.a = t1.c and t2.b = t1.d
join t1 as t3 on t3.a = t2.a and t3.b = t2.b
where t1.a=2 and t1.b=2;

## Bug#28898811 INCORRECTLY REJECT JOIN TO BE PUSHED DOWN
##
## .. also served to detect (also see ndb_endian.test):
## Bug#29010641 FAILS TO HANDLE DIFFERENT ENDIANNESS FORMATS IN PUSHED JOIN KEYS
##
## Tables not beings considered in the join pushdown analysis,
## e.g. a table not being a ndb table, where also incorrectly
## excluded as a table which could be referred in the join conditions
## from a pushed child table (on .. t3.b = t1.b <-, below)
##
explain
select *
from t1_myisam as t1
join t1 as t2 on t2.a = t1.c
join t1 as t3 on t3.a = t2.b and t3.b = t1.b;
--sorted_result
select *
from t1_myisam as t1
join t1 as t2 on t2.a = t1.c
join t1 as t3 on t3.a = t2.b and t3.b = t1.b;

connection ddl;
drop table t1_myisam;

#
# Test scans with filter. These should be pushed as linked operations
# where the root operation is a scan *with* a (pushed) filter and a 
# primary key lookup child operation.
#

connection spj;
set ndb_join_pushdown=true;

# Table scan
--replace_column 10 # 11 #
explain select *
from t1
join t1 as t2 on t2.a = t1.c and t2.b = t1.d
where t1.d = 3;
--sorted_result
select *
from t1
join t1 as t2 on t2.a = t1.c and t2.b = t1.d
where t1.d = 3;

# Ordered index scan
--replace_column 10 # 11 #
explain select *
from t1 
join t1 as t2 on t2.a = t1.c and t2.b = t1.d 
where t1.a > 2 and t1.d = 3;
--sorted_result
select * 
from t1 
join t1 as t2 on t2.a = t1.c and t2.b = t1.d 
where t1.a > 2 and t1.d = 3;

# Sorted scan of ordered index.
--replace_column 10 # 11 #
explain select *
from t1 join t1 as t2 on t2.a = t1.c and t2.b = t1.d 
where t1.d = 3 
order by t1.a;
--sorted_result
select * 
from t1 join t1 as t2 on t2.a = t1.c and t2.b = t1.d 
where t1.d = 3 
order by t1.a;

#
# Test index scan w/ equal-bounds (low == high)
# NOTE: There used to be temp restriction of not allowing ordered 
# index scans to be pushed. (Has later been lifted)
# SQL stmt. are therefore written with pushable JT_REFs from table 2 -> 
#
set ndb_join_pushdown=true;
explain
select *
from t1
join t1 as t2 on t2.a = t1.c
join t1 as t3 on t3.a = t2.c and t3.b = t2.d
where t1.a = 1 and t1.b = 1;
--sorted_result
select *
from t1
join t1 as t2 on t2.a = t1.c
join t1 as t3 on t3.a = t2.c and t3.b = t2.d
where t1.a = 1 and t1.b = 1;

explain
select *
from t1
left join t1 as t2 on t2.a = t1.c
left join t1 as t3 on t3.a = t2.c and t3.b = t2.d
where t1.a = 1 and t1.b = 1;
--sorted_result
select *
from t1
left join t1 as t2 on t2.a = t1.c
left join t1 as t3 on t3.a = t2.c and t3.b = t2.d
where t1.a = 1 and t1.b = 1;

set ndb_join_pushdown=false;
explain
select *
from t1
join t1 as t2 on t2.a = t1.c
join t1 as t3 on t3.a = t2.c and t3.b = t2.d
where t1.a = 1 and t1.b = 1;
--sorted_result
select *
from t1
join t1 as t2 on t2.a = t1.c
join t1 as t3 on t3.a = t2.c and t3.b = t2.d
where t1.a = 1 and t1.b = 1;

explain
select *
from t1
left join t1 as t2 on t2.a = t1.c
left join t1 as t3 on t3.a = t2.c and t3.b = t2.d
where t1.a = 1 and t1.b = 1;
--sorted_result
select *
from t1
left join t1 as t2 on t2.a = t1.c
left join t1 as t3 on t3.a = t2.c and t3.b = t2.d
where t1.a = 1 and t1.b = 1;

# JT_REF as root operations is now supported as pushed joins
set ndb_join_pushdown=true;
explain
select *
from t1 as t2
join t1 as t3 on t3.a = t2.c and t3.b = t2.d
where t2.a = 1;
--sorted_result
select *
from t1 as t2
join t1 as t3 on t3.a = t2.c and t3.b = t2.d
where t2.a = 1;

# Test multiparent pushed joins where it is not possible
# to find a single common parent by using the equality set
#
# NOTE: We should take care to join the multiparent linked
# table on field refs. not also being refered from other join expr.
# as this will make them candidates for equality set replacement.
#
set ndb_join_pushdown=true;

# t3 refer both t1,t2 as parrent.
# t1 should be identifed as a grandparent available
# through its child t2.
explain
select straight_join *
from t1
 join t1 as t2 on t2.a = t1.a and t2.b = t1.b
 join t1 as t3 on t3.a = t2.c and t3.b = t1.c;
--sorted_result
select straight_join *
from t1
 join t1 as t2 on t2.a = t1.a and t2.b = t1.b
 join t1 as t3 on t3.a = t2.c and t3.b = t1.c;

# t4 is pushable iff we force an artificial parental dependency between t2 & t3.
explain
select straight_join *
from t1
 join t1 as t2 on t2.a = t1.a and t2.b = t1.b
 join t1 as t3 on t3.a = t1.c and t3.b = t1.d
 join t1 as t4 on t4.a = t3.c and t4.b = t2.c;
--sorted_result
select straight_join *
from t1
 join t1 as t2 on t2.a = t1.a and t2.b = t1.b
 join t1 as t3 on t3.a = t1.c and t3.b = t1.d
 join t1 as t4 on t4.a = t3.c and t4.b = t2.c;

# t3 is a child of t2 and grandchild of t1
# t4 is a child of t3 and grandchild of t2
explain
select straight_join *
from t1
 join t1 as t2 on t2.a = t1.a and t2.b = t1.b
 join t1 as t3 on t3.a = t1.c and t3.b = t2.d
 join t1 as t4 on t4.a = t3.c and t4.b = t2.c;
--sorted_result
select straight_join *
from t1
 join t1 as t2 on t2.a = t1.a and t2.b = t1.b
 join t1 as t3 on t3.a = t1.c and t3.b = t2.d
 join t1 as t4 on t4.a = t3.c and t4.b = t2.c;

# t3 is a child of t2 and grandchild of t1
# t4 is a child of t3 and grandgrandchild of t1
explain
select straight_join *
from t1
 join t1 as t2 on t2.a = t1.a and t2.b = t1.b
 join t1 as t3 on t3.a = t1.c and t3.b = t2.d
 join t1 as t4 on t4.a = t3.c and t4.b = t1.d;
--sorted_result
select straight_join *
from t1
 join t1 as t2 on t2.a = t1.a and t2.b = t1.b
 join t1 as t3 on t3.a = t1.c and t3.b = t2.d
 join t1 as t4 on t4.a = t3.c and t4.b = t1.d;

# Some testcases where t4 is not directly pushable, but
# may be made pushable by equality set replacement.
#
# BEWARE: mysqld optimizer may do its own replacement
#    before ha_ndbcluster_push analyze the AQP. We therefore 
#    provide multiple similar testcases and hope that
#    some of them will trigger the replacement code in 
#    ha_ndbcluster_push :-o
explain
select straight_join *
from t1
 join t1 as t2 on t2.a = t1.a and t2.b = t1.b
 join t1 as t3 on t3.a = t1.c and t3.b = t1.d
 join t1 as t4 on t4.a = t3.a and t4.b = t2.c;
--sorted_result
select straight_join *
from t1
 join t1 as t2 on t2.a = t1.a and t2.b = t1.b
 join t1 as t3 on t3.a = t1.c and t3.b = t1.d
 join t1 as t4 on t4.a = t3.a and t4.b = t2.c;

explain
select straight_join *
from t1
 join t1 as t2 on t2.a = t1.a and t2.b = t1.b
 join t1 as t3 on t3.a = t1.c and t3.b = t1.d
 join t1 as t4 on t4.a = t3.b and t4.b = t2.c;
--sorted_result
select straight_join *
from t1
 join t1 as t2 on t2.a = t1.a and t2.b = t1.b
 join t1 as t3 on t3.a = t1.c and t3.b = t1.d
 join t1 as t4 on t4.a = t3.b and t4.b = t2.c;

explain
select straight_join *
from t1
 join t1 as t2 on t2.a = t1.a and t2.b = t1.b
 join t1 as t3 on t3.a = t1.c and t3.b = t1.d
 join t1 as t4 on t4.a = t3.c and t4.b = t2.a;
--sorted_result
select straight_join *
from t1
 join t1 as t2 on t2.a = t1.a and t2.b = t1.b
 join t1 as t3 on t3.a = t1.c and t3.b = t1.d
 join t1 as t4 on t4.a = t3.c and t4.b = t2.a;

explain
select straight_join *
from t1
 join t1 as t2 on t2.a = t1.a and t2.b = t1.b
 join t1 as t3 on t3.a = t1.c and t3.b = t1.d
 join t1 as t4 on t4.a = t3.c and t4.b = t2.b;
--sorted_result
select straight_join *
from t1
 join t1 as t2 on t2.a = t1.a and t2.b = t1.b
 join t1 as t3 on t3.a = t1.c and t3.b = t1.d
 join t1 as t4 on t4.a = t3.c and t4.b = t2.b;

explain
select straight_join *
from t1
 join t1 as t2 on t2.a = t1.a and t2.b = t1.b
 join t1 as t3 on t3.a = t1.c and t3.b = t1.d
 join t1 as t4 on t4.a = t1.c and t4.b = t2.c;
--sorted_result
select straight_join *
from t1
 join t1 as t2 on t2.a = t1.a and t2.b = t1.b
 join t1 as t3 on t3.a = t1.c and t3.b = t1.d
 join t1 as t4 on t4.a = t1.c and t4.b = t2.c;

explain
select straight_join *
from t1
 join t1 as t2 on t2.a = t1.a and t2.b = t1.b
 join t1 as t3 on t3.a = t1.c and t3.b = t1.d
 join t1 as t4 on t4.a = t3.c and t4.b = t1.b;
--sorted_result
select straight_join *
from t1
 join t1 as t2 on t2.a = t1.a and t2.b = t1.b
 join t1 as t3 on t3.a = t1.c and t3.b = t1.d
 join t1 as t4 on t4.a = t3.c and t4.b = t1.b;

# Added more multidependency tests;
#

explain
select straight_join *
from t1
 join t1 as t2  on t2.a = t1.a and t2.b = t1.b
 join t1 as t2x on t2x.a = t2.c and t2x.b = t2.d
 join t1 as t3x on t3x.a = t1.c and t3x.b = t1.d
 join t1 as t4  on t4.a = t3x.c and t4.b = t2x.c;

explain
select straight_join *
from t1
 join t1 as t2  on t2.a = t1.a and t2.b = t1.b
 join t1 as t2x on t2x.a = t2.c and t2x.b = t2.d
 join t1 as t3  on t3.a = t1.c and t3.b = t1.d
 join t1 as t3x on t3x.a = t3.c and t3x.b = t3.d
 join t1 as t4  on t4.a = t3x.c and t4.b = t2x.c; 

explain
select straight_join *
from t1
 join t1 as t2  on t2.a = t1.a and t2.b = t1.b
 join t1 as t3  on t3.a = t1.c and t3.b = t1.d
 join t1 as t2x on t2x.a = t2.c and t2x.b = t2.d
 join t1 as t3x on t3x.a = t3.c and t3x.b = t3.d
 join t1 as t4  on t4.a = t3x.c and t4.b = t2x.c; 

# 't3' is not referred as ancestor and should not be
# included in the forced dependencies
# (Depends directly on 't1' and can be bushy wrt. to
#  the other tables)
explain
select straight_join *
from t1
 join t1 as t2  on t2.a = t1.a and t2.b = t1.b
 join t1 as t2x on t2x.a = t2.c and t2x.b = t2.d
 join t1 as t3  on t3.a = t1.c and t3.b = t1.d
 join t1 as t3x on t3x.a = t1.c and t3x.b = t1.d
 join t1 as t4  on t4.a = t3x.c and t4.b = t2x.c;

# 't3' is still independent - see comment above.
explain
select straight_join *
from t1
 join t1 as t2  on t2.a = t1.a and t2.b = t1.b
 join t1 as t2x on t2x.a = t2.c and t2x.b = t2.d
 join t1 as t3  on t3.a = t1.c and t3.b = t1.b
 join t1 as t3x on t3x.a = t1.c and t3x.b = t1.d
 join t1 as t4  on t4.a = t3x.c and t4.b = t2x.c;

## It should not be possible to force grandparent dependencies
## via a previously outer joined table:
explain
select straight_join *
from t1
 left join t1 as t2 on t2.a = t1.a and t2.b = t1.b
 join t1 as t3 on t3.a = t1.c and t3.b = t1.d
 left join t1 as t4 on t4.a = t3.c and t4.b = t2.c;

explain
select straight_join *
from t1
 join t1 as t2 on t2.a = t1.a and t2.b = t1.b
 left join t1 as t3 on t3.a = t1.c and t3.b = t1.d
 left join t1 as t4 on t4.a = t3.c and t4.b = t2.c;

explain
select straight_join *
from t1
 left join t1 as t2 on t2.a = t1.a and t2.b = t1.b
 join t1 as t3 on t3.a = t1.a
 left join t1 as t4 on t4.a = t3.c and t4.b = t2.c;

explain
select straight_join *
from t1
 join t1 as t2 on t2.a = t1.a and t2.b = t1.b
 left join t1 as t3 on t3.a = t1.a
 left join t1 as t4 on t4.a = t3.c and t4.b = t2.c;

explain
select straight_join *
from t1
 left join t1 as t2 on t2.a = t1.a and t2.b = t1.b
 join t1 as t3 on t3.a = t1.a
 left join t1 as t4 on t4.a = t3.c;

explain
select straight_join *
from t1
 join t1 as t2 on t2.a = t1.a and t2.b = t1.b
 left join t1 as t3 on t3.a = t1.a
 left join t1 as t4 on t4.a = t3.c;

# Bug#16199028:
# Tescase where a too strict 'root' was checked:
# In these cases t4 was not pushed as it was incorrectly
# concluded that its grandparent ref of 't2' through 't3'
# introduced dependencies on the outer joined t1.
#
explain
select straight_join *
from
 ( t1 as t0 left join t1 as t1 on t1.a = t0.a and t1.b = t0.b
 )
 left join
 ( t1 as t2 join t1 as t3 on t3.a = t2.c and t3.b = t2.d
            join t1 as t4 on t4.a = t3.c and t4.b = t2.c
 )
 on t2.a = t1.a and t2.b = t1.b;

explain
select straight_join *
from
  t1 as x1
  left join 
  ( t1 as x2 join t1 as x3 on x3.a=x2.c
             join t1 as x4 on x4.a=x2.d
             join t1 as x5 on x5.a=x3.d and x5.b=x4.d
  )
  on x2.a=x1.c and x2.b=x1.c and 
     x3.b=x1.d and
     x4.b=x1.d;


# Bug#16198866:
# Additional tests where equality set [t1.d, t3.b, t4.d]
# propagation incorrectly allowed grandparent references
# to be allowed 'through' outer joins (t2):
#
# Test: don't allow t4 to propagate parent deps past 
# outer joined 't2' into t3.
# (As that would implicit cause t3 to depend on t2.)
#
explain
select straight_join * from
 (t1 left join t1 as t2 on t2.a = t1.c and t2.b = t1.d)
 inner join t1 as t3 on t3.a = t1.b and t3.b = t1.c
 left join t1 as t4 on t4.a = t2.c and t4.b = t1.c;

--sorted_result
select straight_join * from
 (t1 left join t1 as t2 on t2.a = t1.c and t2.b = t1.d)
 inner join t1 as t3 on t3.a = t1.b and t3.b = t1.c
 left join t1 as t4 on t4.a = t2.c and t4.b = t1.c;

# However, if t3 already depends on t2 (t3.a = t2.b)
# it *is* allowed as it will not introduce any new 
# t3 dependencies.
#
# Note: Both these two queries are fully pushable, but 
# the later will likely result in a better query plan.
#
explain
select straight_join * from
 (t1 left join t1 as t2 on t2.a = t1.c and t2.b = t1.d)
 inner join t1 as t3 on t3.a = t2.b and t3.b = t1.c
 left join t1 as t4 on t4.a = t2.c and t4.b = t1.c;

--sorted_result
select straight_join * from
 (t1 left join t1 as t2 on t2.a = t1.c and t2.b = t1.d)
 inner join t1 as t3 on t3.a = t2.b and t3.b = t1.c
 left join t1 as t4 on t4.a = t2.c and t4.b = t1.c;

# Test a combination of pushed table scan (x, y)
#  & pushed EQ-bound (indexScan) (z, t1)
# This used to give incorrect results with random result for last table (t1)
set ndb_join_pushdown=true;
explain
  select * from t1 x, t1 y, t1 z, t1 where 
    y.a=x.d and y.b=x.b and 
    z.a=y.d and 
    t1.a = z.d and t1.b=z.b;
--sorted_result
  select * from t1 x, t1 y, t1 z, t1 where 
    y.a=x.d and y.b=x.b and 
    z.a=y.d and 
    t1.a = z.d and t1.b=z.b;

# Pushed scanIndex() with (multi-)range:
explain
  select * from t1 x, t1 y where
    x.a <= 2 and
    y.a=x.d and y.b=x.b;
--sorted_result
  select * from t1 x, t1 y where
    x.a <= 2 and
    y.a=x.d and y.b=x.b;

explain
  select * from t1 x, t1 y where
    (x.a <= 2 or x.a > 3) and
    y.a=x.d and y.b=x.b;
--sorted_result
  select * from t1 x, t1 y where
    (x.a <= 2 or x.a > 3) and
    y.a=x.d and y.b=x.b;

# 'open' range:
--replace_column 10 # 11 #
explain
  select * from t1 x, t1 y where
    (x.a >= 2 or x.a < 3) and
    y.a=x.d and y.b=x.b;
--sorted_result
  select * from t1 x, t1 y where
    (x.a >= 2 or x.a < 3) and
    y.a=x.d and y.b=x.b;

# Combination of range and 'in' list
explain
  select * from t1 x, t1 y where
    (x.a <= 2 or x.a in (0,5,4)) and
    y.a=x.d and y.b=x.b;
--sorted_result
  select * from t1 x, t1 y where
    (x.a <= 2 or x.a in (0,5,4)) and
    y.a=x.d and y.b=x.b;

# Combination of range and 'in' list with exact match
# NOTE: Due to simplification in pushed mrr, exact matches are also
#       executed as range scans
explain
  select * from t1 x, t1 y where
    (x.a <= 2 or (x.a,x.b) in ((0,0),(5,0),(4,3))) and
    y.a=x.d and y.b=x.b;
--sorted_result
  select * from t1 x, t1 y where
    (x.a <= 2 or (x.a,x.b) in ((0,0),(5,0),(4,3))) and
    y.a=x.d and y.b=x.b;

# Test ORDER BY expressions
# If it is a 'simple' order, i.e all order by's are plain column refs
# to the first non-const table, the optimizer will (by heuristic) 
# make the first table 'ordered' beforing joining in the siblings.
# This may involve presorting of the first table into intermediate storage.
#
# This will make this (parent-) table  non-pushable as 
# read of rows to be filesorted will also prefetch rows from pushed child
# operands. These are not cached by the filesort buffer mechanisnm and are
# effectively lost.
#
# Non simple ordering will always writte entire resultset to temp.
# table and filesort that -> No extra push restrictions on these.
#
# Integrating pushed joins we either has to:
#  1)  find a suitable ordered index which we can create an ordered indexscan on 
#  (-> joinType() -> JT_NEXT, or type: 'index' w/ explain)
#  or:
# 2) Reject pushing of the this parent table.
#
# Comment1: As 'order by' correctness is part of what we want to test,
#           '--sorted_result' is *not* specified. Instead we aim at
#           specifying a deterministic sort ordering in order by list.
#
# Comment2: 't1.a, t1.b' is appended where required to the order by spec 
#           to get a deterministic sorting order wo/ '--sorted_result'
#           

## Non-pushed join w/ 'simple order' on non_PK - Presorts parent table.
explain
select *
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
join t1 as t3 on t3.a = t2.a and t3.b = t2.b
order by t1.c,t1.d,
t1.a, t1.b;
#--sorted_result
select *
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
join t1 as t3 on t3.a = t2.a and t3.b = t2.b
order by t1.c,t1.d,
t1.a, t1.b;

## pushed join w/ non-'simple order' on non_PK - Need temp table + filesort
explain
select *
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
join t1 as t3 on t3.a = t2.a and t3.b = t2.b
order by t1.c,t2.d,
t1.a, t1.b;
#--sorted_result
select *
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
join t1 as t3 on t3.a = t2.a and t3.b = t2.b
order by t1.c,t2.d,
t1.a, t1.b;

## pushed join w/ 'simple order' on PK - Should use ordered index scan 
explain
select *
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
join t1 as t3 on t3.a = t2.a and t3.b = t2.b
order by t1.a,t1.b;
#--sorted_result
select *
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
join t1 as t3 on t3.a = t2.a and t3.b = t2.b
order by t1.a,t1.b;

## pushed join w/ non-'simple order' on PK - will need temp table + filesort 
explain
select *
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
join t1 as t3 on t3.a = t2.a and t3.b = t2.b
order by t1.a,t2.b,
t1.a, t1.b;
#--sorted_result
select *
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
join t1 as t3 on t3.a = t2.a and t3.b = t2.b
order by t1.a,t2.b,
t1.a, t1.b;

## Descending ordering on PK - use reversed ordered index scan
explain
select *
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
join t1 as t3 on t3.a = t2.a and t3.b = t2.b
order by t1.a desc,t1.b desc;
#--sorted_result
select *
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
join t1 as t3 on t3.a = t2.a and t3.b = t2.b
order by t1.a desc,t1.b desc;

## Simple-PK column in incorrect order, -> Presort parent table, (no-push)
explain
select *
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
join t1 as t3 on t3.a = t2.a and t3.b = t2.b
order by t1.b,t1.a;
#--sorted_result
select *
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
join t1 as t3 on t3.a = t2.a and t3.b = t2.b
order by t1.b,t1.a;

## Explore other permutations of PK columns in ORDER BY clause
## Don't care about the results here....
# Subset of first part of PK -> ordered index scan
explain
select *
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
join t1 as t3 on t3.a = t2.a and t3.b = t2.b
order by t1.a;

# PK columns not including first part -> Presort parent table, (no-push)
explain
select *
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
join t1 as t3 on t3.a = t2.a and t3.b = t2.b
order by t1.b;


## Similar tests for GROUP BY expression
## PK grouping (or subpart) may be optimized
## by ordered index access.
##
explain
select t1.a, t1.b, count(*)
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
join t1 as t3 on t3.a = t2.a and t3.b = t2.b
group by t1.a, t1.b;
--sorted_result
select t1.a, t1.b, count(*)
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
join t1 as t3 on t3.a = t2.a and t3.b = t2.b
group by t1.a, t1.b;

explain
select t1.a, count(*)
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
join t1 as t3 on t3.a = t2.a and t3.b = t2.b
group by t1.a;
--sorted_result
select t1.a, count(*)
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
join t1 as t3 on t3.a = t2.a and t3.b = t2.b
group by t1.a;

explain
select t1.b, count(*)
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
join t1 as t3 on t3.a = t2.a and t3.b = t2.b
group by t1.b;
--sorted_result
select t1.b, count(*)
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
join t1 as t3 on t3.a = t2.a and t3.b = t2.b
group by t1.b;

--replace_result p0,p1,p2,p3,p4,p5,p6,p7 pXYZ p1 pX p2 pX
explain
select t2.c, count(distinct t2.a)
from t1
join t1 as t2 on t1.a = t2.c and t1.b = t2.d
where t2.a = 4 and t2.b=4
group by t2.c;
--sorted_result
select t2.c, count(distinct t2.a)
from t1
join t1 as t2 on t1.a = t2.c and t1.b = t2.d
where t2.a = 4 and t2.b=4
group by t2.c;

explain
select t2.c, count(distinct t2.a)
from t1
join t1 as t2 on t1.a = t2.c and t1.b = t2.d
where t2.a = 4
group by t2.c;
--sorted_result
select t2.c, count(distinct t2.a)
from t1
join t1 as t2 on t1.a = t2.c and t1.b = t2.d
where t2.a = 4
group by t2.c;

--replace_result p0,p1,p2,p3,p4,p5,p6,p7 pXYZ p1 pX p2 pX
explain
select t2.c, count(distinct t2.a)
from t1
join t1 as t2 on t1.a = t2.c and t1.b = t2.d
where t2.a = 4 and t2.b=4
group by t2.c order by t2.c;
--sorted_result
select t2.c, count(distinct t2.a)
from t1
join t1 as t2 on t1.a = t2.c and t1.b = t2.d
where t2.a = 4 and t2.b=4
group by t2.c order by t2.c;

connection ddl;
create table tx like t1;

connection spj;
insert into tx 
  select x1.a+x2.a*16, x1.b+x2.b*16, x1.c+x2.c*16, x1.d+x2.d*16 
    from t1 as x1 cross join t1 as x2;

# Test of outer join with scan child.
# This query should not be pushed. Doing so would produce lots of extra
# [<x1 row>.NULL] rows, since the x1.d=x2.d predicate cannot be pushed.
explain select count(*) from tx as x1 
  left join tx as x2 on x1.c=x2.a and x1.d=x2.d;
select count(*) from tx as x1 
  left join tx as x2 on x1.c=x2.a and x1.d=x2.d;

connection ddl;
drop table tx;

# Test bushy join with pruned scan.
connection ddl;
alter table t1 partition by key(a);

connection spj;

explain select count(*) from t1 
  join t1 as t2 on t2.a = t1.c 
  join t1 as t3 on t3.a = t1.d;
select count(*) from t1 
  join t1 as t2 on t2.a = t1.c 
  join t1 as t3 on t3.a = t1.d;

# Test bushy join with pruned scan and larger result set.

connection ddl;
CREATE TABLE tx (
  a int NOT NULL,
  PRIMARY KEY (`a`)
) comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
delete from t1;

insert into tx values (0), (1), (2), (3), (4), (5), (6), (7), (8), (9);

insert into t1 select 1, x1.a * 10+x2.a, 1, 1 from tx as x1 cross join tx as x2;

--error 0,1193
set global debug='+d,max_64rows_in_spj_batches';

--replace_column 10 # 11 #
explain select count(*) from t1 as x1
  join t1 as x2 on x2.a = x1.c and x1.b < 2 
  join t1 as x3 on x3.a = x1.d;
select count(*) from t1 as x1
  join t1 as x2 on x2.a = x1.c and x1.b < 2 
  join t1 as x3 on x3.a = x1.d;

--error 0,1193
set global debug=@save_debug;

connection ddl;
drop table t1;
drop table tx;

# Test user defined partition not being pushed
#
# Note: User defined partitions are handled
#       by the SQL layer, and as such are unknown
#       to the NDB datanodes.
# 

connection spj;
create table t1 (
  a int not null,
  b int not null,
  c int not null,
  d int not null,
  primary key (`a`,`b`)
) engine=ndbcluster
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM"
  partition by key(a);

connection spj;
insert into t1 values
(1,1,1,1), (2,2,2,2), (3,3,3,3), (4,4,4,4),
(1,2,5,1), (1,3,1,2), (1,4,2,3),
(2,1,3,4), (2,3,4,5), (2,4,5,1),
(3,1,1,2), (3,2,2,3), (3,4,3,4),
(4,1,4,5), (4,2,5,1), (4,3,1,2);

# Only this query('partition by key') should be pushed
explain
select *
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c;

connection ddl;
set new=on;
alter table t1 partition by hash(a);

explain
select *
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c;

connection ddl;
alter table t1 partition by list(a) (
  partition p1 values in (1),
  partition p2 values in (2),
  partition p3 values in (3),
  partition p4 values in (4)
);

explain
select *
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c;

connection ddl;
alter table t1 partition by range(a) partitions 4 (
  partition p1 values less than (0),
  partition p2 values less than (2),
  partition p3 values less than (4),
  partition p4 values less than (99999)
);

explain
select *
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c;

connection ddl;
drop table t1;
set new=default;


# pushed mrr does not yet handle multiple PK operations in same transaction
# Need 6.0 result handling stuff to simplify result handling
# *** join push is currently dissabled for these ****
#
connection ddl;
create table t1 (a int, b int, primary key(a) using hash) engine = ndb
comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
insert into t1 values (1, 2);
insert into t1 values (2, 3);
insert into t1 values (3, 1);

set ndb_join_pushdown=true;

##
# In ps-protocol, server will adds calls to execute(Commit)
#   (these are optimized away by ndbapi, since there is nothing to commit)
#   and these generates a diff in execute-count
# To not have to investigate problem futher, I simply set autocommit=off
#   (and back further down where we don't track execute-count any longer)
# It would probably be good to changes these tests to instead use frazers new
#   ndbapi counters, and instead measure #round-trips
set autocommit=off;

explain
select *
from t1, t1 as t2
where t1.a in (1,3,5)
  and t2.a = t1.b;
--source suite/ndb/include/ndb_init_execute_count.inc
--sorted_result
select *
from t1, t1 as t2
where t1.a in (1,3,5)
  and t2.a = t1.b;
--source suite/ndb/include/ndb_execute_count.inc
--echo This should yield 3 executes (for now...buh)

set autocommit=on;

connection ddl;
drop table t1;

# Same case when there is an ordered index on PK
connection ddl;
create table t1 (a int, b int, primary key(a)) engine = ndb
comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
insert into t1 values (1, 2);
insert into t1 values (2, 3);
insert into t1 values (3, 1);

set ndb_join_pushdown=true;

##
# In ps-protocol, server will adds calls to execute(Commit)
#   (these are optimized away by ndbapi, since there is nothing to commit)
#   and these generates a diff in execute-count
# To not have to investigate problem futher, I simply set autocommit=off
#   (and back further down where we don't track execute-count any longer)
# It would probably be good to changes these tests to instead use frazers new
#   ndbapi counters, and instead measure #round-trips
set autocommit=off;

explain
select *
from t1, t1 as t2
where t1.a in (1,3,5)
  and t2.a = t1.b;
--source suite/ndb/include/ndb_init_execute_count.inc
--sorted_result
select *
from t1, t1 as t2
where t1.a in (1,3,5)
  and t2.a = t1.b;
--source suite/ndb/include/ndb_execute_count.inc
--echo This should yield 1 execute (but inefficient since it's based on scan)

set autocommit=on;

## Adding and 'order by ... desc' trigger the usage
## of QUICK_SELECT_DESC which somehow prepares a 
## pushed join as indexscan but ends up executing it as 
## primary key access. This (auto-) disables the pushed 
## join execution (EXPLAIN still says 'pushdown') which
## should test handling of this in ha_ndbcluster::index_read_pushed()

explain
select *
from t1, t1 as t2
where t1.a in (1,3,5)
  and t2.a = t1.b
order by t1.a desc;
select *
from t1, t1 as t2
where t1.a in (1,3,5)
  and t2.a = t1.b
order by t1.a desc;


connection ddl;
drop table t1;

set ndb_join_pushdown=true;

connection ddl;
create table t1 (a int, b int, primary key(a)) engine = ndb
comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
create table t2 (c int, d int, primary key(c)) engine = ndb
comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
create table t3 (a3 int, b3 int, c3 int not null, d3 int not null,
 primary key(a3, b3)) engine = ndb
comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
create table t3_hash (a3 int, b3 int, c3 int not null, d3 int not null,
 primary key(a3, b3) using hash) engine = ndb
comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
insert into t1 values (0x1f, 0x2f);
insert into t1 values (0x2f, 0x3f);
insert into t1 values (0x3f, 0x1f);

insert into t2 values (0x1f, 0x2f);
insert into t2 values (0x2f, 0x3f);
insert into t2 values (0x3f, 0x1f);

insert into t3 values (0x1f, 0x2f, 1, 0x1f);
insert into t3 values (0x2f, 0x3f, 2, 0x2f);
insert into t3 values (0x3f, 0x1f, 3, 0x3f);

insert into t3_hash values (0x1f, 0x2f, 1, 0x1f);
insert into t3_hash values (0x2f, 0x3f, 2, 0x2f);
insert into t3_hash values (0x3f, 0x1f, 3, 0x3f);

explain
select * from t3 x, t3 y, t1 where y.a3=x.d3 and y.b3=x.b3 and t1.a = y.d3;
--sorted_result
select * from t3 x, t3 y, t1 where y.a3=x.d3 and y.b3=x.b3 and t1.a = y.d3;

explain
select *
from t3 x, t3 y, t3 z, t3 z2, t1
where y.a3=x.d3 and y.b3=x.b3 and
      z.a3=y.d3 and z.b3=y.b3 and
      z2.a3=z.d3 and z2.b3=z.b3 and
      t1.a = z2.d3;
--sorted_result
select *
from t3 x, t3 y, t3 z, t3 z2, t1
where y.a3=x.d3 and y.b3=x.b3 and
      z.a3=y.d3 and z.b3=y.b3 and
      z2.a3=z.d3 and z2.b3=z.b3 and
      t1.a = z2.d3;

# Table expressions wo/ parent-child linkage should *not* be executes as a pushed join:
--replace_column 10 # 11 #
explain
select straight_join * from t1 x, t1 y where y.a=0x1f and x.b = 0x1f;
select straight_join * from t1 x, t1 y where y.a=0x1f and x.b = 0x1f;

# NOTE: Due to constValue replacement in equality sets, query below are
# effectively the same as the one above. -> Don't push either
--replace_column 10 # 11 #
explain
select straight_join * from t1 x, t1 y where y.a=x.b and x.b = 0x1f;
select straight_join * from t1 x, t1 y where y.a=x.b and x.b = 0x1f;


# Tests usage of unique index
connection ddl;
create unique index t3_d3 on t3(d3); 
create unique index t3_d3 on t3_hash(d3);
commit;

connection spj;
# Use an unique key to lookup root in pushed join:
explain
  select * from t3 x, t3 y where x.d3=31 and y.a3=x.d3 and y.b3=x.b3;
  select * from t3 x, t3 y where x.d3=31 and y.a3=x.d3 and y.b3=x.b3;

# No data-found on unique key lookup root
explain
  select * from t3 x, t3 y where x.d3=0 and y.a3=x.d3 and y.b3=x.b3;
  select * from t3 x, t3 y where x.d3=0 and y.a3=x.d3 and y.b3=x.b3;

# Use an unique key to lookup joined child tables
explain
  select * from t1 x, t3 y where y.d3=x.b;
--sorted_result
  select * from t1 x, t3 y where y.d3=x.b;

# Unique index used both for root lookup and child linkage.
explain
  select * from t3 x, t3 y where x.d3=31 and y.d3=x.b3;
  select * from t3 x, t3 y where x.d3=31 and y.d3=x.b3;

# No data-found on unique key lookup child
explain
  select * from t3 x, t3 y where x.d3=31 and y.d3=x.c3;
  select * from t3 x, t3 y where x.d3=31 and y.d3=x.c3;

# 'index_merge' between PRIMARY and index t3.d3
# NOTE: currently unhandled
explain
  select * from t3 x, t3 y 
  where ((x.a3=0x2f and x.b3=0x3f) or x.d3=0x1f)
    and  (y.a3=x.d3 and y.b3=x.b3);
# No 'sorted_result' required as index merge itself sort on PK
 select * from t3 x, t3 y
 where ((x.a3=0x2f and x.b3=0x3f) or x.d3=0x1f)
   and  (y.a3=x.d3 and y.b3=x.b3);

explain
  select * from t3_hash x, t3_hash y
  where ((x.a3=0x2f and x.b3=0x3f) or x.d3=0x1f)
    and  (y.a3=x.d3 and y.b3=x.b3);
# No 'sorted_result' required as index merge itself sort on PK
  select * from t3_hash x, t3_hash y
  where ((x.a3=0x2f and x.b3=0x3f) or x.d3=0x1f)
    and  (y.a3=x.d3 and y.b3=x.b3);

# Any ordered index may also be used to scan a 'range'
explain
  select * from t3 x, t3 y where x.d3>=31 and y.d3=x.b3;
--sorted_result
  select * from t3 x, t3 y where x.d3>=31 and y.d3=x.b3;


# handle "null" key
insert into t1 values (0x4f, null);
--sorted_result
select * from t1 left join t1 as t2 on t2.a = t1.b;

### Test max number of pushable operations.

insert into t3 values (8,8,8,8);

# Unique lookups only.
explain select count(*) from t3 as x0
 join t3 as x1 on x0.b3=x1.d3 and x0.d3=8
 join t3 as x2 on x1.b3=x2.d3
 join t3 as x3 on x2.b3=x3.d3
 join t3 as x4 on x3.b3=x4.d3
 join t3 as x5 on x4.b3=x5.d3
 join t3 as x6 on x5.b3=x6.d3
 join t3 as x7 on x6.b3=x7.d3
 join t3 as x8 on x7.b3=x8.d3
 join t3 as x9 on x8.b3=x9.d3
 join t3 as x10 on x9.b3=x10.d3
 join t3 as x11 on x10.b3=x11.d3
 join t3 as x12 on x11.b3=x12.d3
 join t3 as x13 on x12.b3=x13.d3
 join t3 as x14 on x13.b3=x14.d3
 join t3 as x15 on x14.b3=x15.d3
 join t3 as x16 on x15.b3=x16.d3
 join t3 as x17 on x16.b3=x17.d3;

select count(*) from t3 as x0
 join t3 as x1 on x0.b3=x1.d3 and x0.d3=8
 join t3 as x2 on x1.b3=x2.d3
 join t3 as x3 on x2.b3=x3.d3
 join t3 as x4 on x3.b3=x4.d3
 join t3 as x5 on x4.b3=x5.d3
 join t3 as x6 on x5.b3=x6.d3
 join t3 as x7 on x6.b3=x7.d3
 join t3 as x8 on x7.b3=x8.d3
 join t3 as x9 on x8.b3=x9.d3
 join t3 as x10 on x9.b3=x10.d3
 join t3 as x11 on x10.b3=x11.d3
 join t3 as x12 on x11.b3=x12.d3
 join t3 as x13 on x12.b3=x13.d3
 join t3 as x14 on x13.b3=x14.d3
 join t3 as x15 on x14.b3=x15.d3
 join t3 as x16 on x15.b3=x16.d3
 join t3 as x17 on x16.b3=x17.d3;


# Max scans
explain select count(*) from t3 as x0
 join t3 as x1 on x0.c3=x1.a3
 join t3 as x2 on x1.c3=x2.a3
 join t3 as x3 on x2.c3=x3.a3
 join t3 as x4 on x3.c3=x4.a3
 join t3 as x5 on x4.c3=x5.a3
 join t3 as x6 on x5.c3=x6.a3
 join t3 as x7 on x6.c3=x7.a3
 join t3 as x8 on x7.c3=x8.a3
 join t3 as x9 on x8.c3=x9.a3
 join t3 as x10 on x9.c3=x10.a3
 join t3 as x11 on x10.c3=x11.a3
 join t3 as x12 on x11.c3=x12.a3
 join t3 as x13 on x12.c3=x13.a3
 join t3 as x14 on x13.c3=x14.a3
 join t3 as x15 on x14.c3=x15.a3
 join t3 as x16 on x15.c3=x16.a3
 join t3 as x17 on x16.c3=x17.a3
 join t3 as x18 on x17.c3=x18.a3
 join t3 as x19 on x18.c3=x19.a3
 join t3 as x20 on x19.c3=x20.a3
 join t3 as x21 on x20.c3=x21.a3
 join t3 as x22 on x21.c3=x22.a3
 join t3 as x23 on x22.c3=x23.a3
 join t3 as x24 on x23.c3=x24.a3
 join t3 as x25 on x24.c3=x25.a3
 join t3 as x26 on x25.c3=x26.a3
 join t3 as x27 on x26.c3=x27.a3
 join t3 as x28 on x27.c3=x28.a3
 join t3 as x29 on x28.c3=x29.a3
 join t3 as x30 on x29.c3=x30.a3
 join t3 as x31 on x30.c3=x31.a3
 join t3 as x32 on x31.c3=x32.a3
 join t3 as x33 on x32.c3=x33.a3;

select count(*) from t3 as x0
 join t3 as x1 on x0.c3=x1.a3
 join t3 as x2 on x1.c3=x2.a3
 join t3 as x3 on x2.c3=x3.a3
 join t3 as x4 on x3.c3=x4.a3
 join t3 as x5 on x4.c3=x5.a3
 join t3 as x6 on x5.c3=x6.a3
 join t3 as x7 on x6.c3=x7.a3
 join t3 as x8 on x7.c3=x8.a3
 join t3 as x9 on x8.c3=x9.a3
 join t3 as x10 on x9.c3=x10.a3
 join t3 as x11 on x10.c3=x11.a3
 join t3 as x12 on x11.c3=x12.a3
 join t3 as x13 on x12.c3=x13.a3
 join t3 as x14 on x13.c3=x14.a3
 join t3 as x15 on x14.c3=x15.a3
 join t3 as x16 on x15.c3=x16.a3
 join t3 as x17 on x16.c3=x17.a3
 join t3 as x18 on x17.c3=x18.a3
 join t3 as x19 on x18.c3=x19.a3
 join t3 as x20 on x19.c3=x20.a3
 join t3 as x21 on x20.c3=x21.a3
 join t3 as x22 on x21.c3=x22.a3
 join t3 as x23 on x22.c3=x23.a3
 join t3 as x24 on x23.c3=x24.a3
 join t3 as x25 on x24.c3=x25.a3
 join t3 as x26 on x25.c3=x26.a3
 join t3 as x27 on x26.c3=x27.a3
 join t3 as x28 on x27.c3=x28.a3
 join t3 as x29 on x28.c3=x29.a3
 join t3 as x30 on x29.c3=x30.a3
 join t3 as x31 on x30.c3=x31.a3
 join t3 as x32 on x31.c3=x32.a3
 join t3 as x33 on x32.c3=x33.a3;

#Mixed join
explain select count(*) from t3 as x0
 join t3 as x1 on x0.b3=x1.d3
 join t3 as x2 on x1.b3=x2.d3
 join t3 as x3 on x2.b3=x3.d3
 join t3 as x4 on x3.b3=x4.d3
 join t3 as x5 on x4.b3=x5.d3
 join t3 as x6 on x5.b3=x6.d3
 join t3 as x7 on x6.b3=x7.d3
 join t3 as x8 on x7.b3=x8.d3
 join t3 as x9 on x8.b3=x9.d3
 join t3 as x10 on x9.b3=x10.d3
 join t3 as x11 on x10.b3=x11.d3
 join t3 as x12 on x11.b3=x12.d3
 join t3 as x13 on x12.b3=x13.d3
 join t3 as x14 on x13.b3=x14.d3
 join t3 as x15 on x14.b3=x15.d3
 join t3 as x16 on x15.b3=x16.d3
 join t3 as x17 on x15.b3=x17.a3
 join t3 as x18 on x16.b3=x18.d3;

select count(*) from t3 as x0
 join t3 as x1 on x0.b3=x1.d3
 join t3 as x2 on x1.b3=x2.d3
 join t3 as x3 on x2.b3=x3.d3
 join t3 as x4 on x3.b3=x4.d3
 join t3 as x5 on x4.b3=x5.d3
 join t3 as x6 on x5.b3=x6.d3
 join t3 as x7 on x6.b3=x7.d3
 join t3 as x8 on x7.b3=x8.d3
 join t3 as x9 on x8.b3=x9.d3
 join t3 as x10 on x9.b3=x10.d3
 join t3 as x11 on x10.b3=x11.d3
 join t3 as x12 on x11.b3=x12.d3
 join t3 as x13 on x12.b3=x13.d3
 join t3 as x14 on x13.b3=x14.d3
 join t3 as x15 on x14.b3=x15.d3
 join t3 as x16 on x15.b3=x16.d3
 join t3 as x17 on x15.b3=x17.a3
 join t3 as x18 on x16.b3=x18.d3;

connection ddl;
drop table t1,t2,t3, t3_hash;

###############################
## Test Primary key and unique key defined 'out of order'
## wrt. the order in which columns was defined in 'create table' 

connection ddl;
create table t3 (a3 int, b3 int, c3 int, d3 int,
 primary key(b3, a3)) engine = ndb
comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

create table t3_hash (a3 int, b3 int, c3 int, d3 int,
 primary key(b3,a3) using hash) engine = ndb
comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

create table t3_unq (pk int, a3 int not null, b3 int not null, c3 int, d3 int,
 primary key(pk) using hash, unique key(b3,a3) using hash) engine = ndb
comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
insert into t3 values (0x1f, 0x2f, 1, 0x1f);
insert into t3 values (0x2f, 0x3f, 2, 0x2f);
insert into t3 values (0x3f, 0x1f, 3, 0x3f);

insert into t3_hash values (0x1f, 0x2f, 1, 0x1f);
insert into t3_hash values (0x2f, 0x3f, 2, 0x2f);
insert into t3_hash values (0x3f, 0x1f, 3, 0x3f);

insert into t3_unq values (1001, 0x1f, 0x2f, 1, 0x1f);
insert into t3_unq values (1002, 0x2f, 0x3f, 2, 0x2f);
insert into t3_unq values (1003, 0x3f, 0x1f, 3, 0x3f);

## Table scans (ALL) as pushed root
explain
select * from t3 x, t3 y where y.a3=x.d3 and y.b3=x.b3;
--sorted_result
select * from t3 x, t3 y where y.a3=x.d3 and y.b3=x.b3;

explain
select * from t3_hash x, t3_hash y where y.a3=x.d3 and y.b3=x.b3;
--sorted_result
select * from t3_hash x, t3_hash y where y.a3=x.d3 and y.b3=x.b3;

explain
select * from t3_unq x, t3_unq y where y.a3=x.d3 and y.b3=x.b3;
--sorted_result
select * from t3_unq x, t3_unq y where y.a3=x.d3 and y.b3=x.b3;

## Lookup (eq_ref/const) as pushed root
explain
select * from t3 x, t3 y where y.a3=x.d3 and y.b3=x.b3
  and x.a3=0x2f and x.b3=0x3f;
select * from t3 x, t3 y where y.a3=x.d3 and y.b3=x.b3
  and x.a3=0x2f and x.b3=0x3f;

explain
select * from t3_hash x, t3_hash y where y.a3=x.d3 and y.b3=x.b3
  and x.a3=0x2f and x.b3=0x3f;
select * from t3_hash x, t3_hash y where y.a3=x.d3 and y.b3=x.b3
  and x.a3=0x2f and x.b3=0x3f;

explain
select * from t3_unq x, t3_unq y where y.a3=x.d3 and y.b3=x.b3
  and x.a3=0x2f and x.b3=0x3f;
select * from t3_unq x, t3_unq y where y.a3=x.d3 and y.b3=x.b3
  and x.a3=0x2f and x.b3=0x3f;

connection ddl;
drop table t3, t3_hash, t3_unq;

###########

connection ddl;
create table t3 (a3 int, b3 int, c3 int, d3 int,
 primary key(a3), unique key(d3)) engine = ndb
comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
insert into t3 values (0x1f, 0x2f, 1, 0x1f);
insert into t3 values (0x2f, 0x3f, 2, 0x2f);
insert into t3 values (0x3f, 0x1f, 3, 0x3f);
insert into t3 values (0x4f, 0,    null, null);

explain
select * from t3 as t1
 left outer join t3 as t2 on t2.d3 = t1.d3
 left outer join t3 as t3 on t3.a3 = t2.d3;
--sorted_result
select * from t3 as t1
 left outer join t3 as t2 on t2.d3 = t1.d3
 left outer join t3 as t3 on t3.a3 = t2.d3;

## Test usage of nullable unique key column in where clause on pushed parent node
explain
select * from t3 as t1
 left outer join t3 as t2 on t2.d3 = t1.a3
 left outer join t3 as t3 on t3.a3 = t2.d3
 where t1.d3 = 47;
--sorted_result
select * from t3 as t1
 left outer join t3 as t2 on t2.d3 = t1.a3
 left outer join t3 as t3 on t3.a3 = t2.d3
 where t1.d3 = 47;

explain
select * from t3 as t1
 left outer join t3 as t2 on t2.d3 = t1.a3
 left outer join t3 as t3 on t3.a3 = t2.d3
 where t1.d3 >= 47;
--sorted_result
select * from t3 as t1
 left outer join t3 as t2 on t2.d3 = t1.a3
 left outer join t3 as t3 on t3.a3 = t2.d3
 where t1.d3 >= 47;

explain
select * from t3 as t1
 left outer join t3 as t2 on t2.d3 = t1.a3
 left outer join t3 as t3 on t3.a3 = t2.d3
 where t1.d3 is null;
--sorted_result
select * from t3 as t1
 left outer join t3 as t2 on t2.d3 = t1.a3
 left outer join t3 as t3 on t3.a3 = t2.d3
 where t1.d3 is null;

explain
select * from t3 as t1
 left outer join t3 as t2 on t2.d3 = t1.a3
 left outer join t3 as t3 on t3.a3 = t2.d3
 where t1.d3 is not null;
--sorted_result
select * from t3 as t1
 left outer join t3 as t2 on t2.d3 = t1.a3
 left outer join t3 as t3 on t3.a3 = t2.d3
 where t1.d3 is not null;


connection ddl;
drop table t3;

####### Composite unique keys, 'const' is part of EQ_REF on child nodes ####

connection ddl;
create table t3 (a3 int not null, b3 int not null, c3 int, d3 int,
 primary key(a3), unique key(b3,d3), unique key(c3,b3), unique key(c3,d3)) engine = ndb
comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
insert into t3 values (0x1f, 0x2f, 1,    0x1f);
insert into t3 values (0x2f, 0x3f, 2,    0x2f);
insert into t3 values (0x3f, 0x1f, 3,    0x3f);
insert into t3 values (0x40, 0,    null, null);
insert into t3 values (0x41, 0,    null, null);
insert into t3 values (0x42, 0,    4,    null);
insert into t3 values (0x43, 0,    null, 0x43);

## Baseline: Not pushed as only one of the columns in unique indexes are REF'ed
explain
select straight_join * 
  from t3 as x join t3 as y on x.b3 = y.b3;
--sorted_result
select straight_join * 
  from t3 as x join t3 as y on x.b3 = y.b3;

## Extend query above with 'where <const cond>'
explain
select straight_join * 
  from t3 as x join t3 as y on x.b3 = y.b3
  where y.d3 = 0x2f;
--sorted_result
select straight_join * 
  from t3 as x join t3 as y on x.b3 = y.b3
  where y.d3 = 0x2f;

--replace_column 10 # 11 #
explain
select straight_join * 
  from t3 as x join t3 as y on x.c3 = y.c3
  where y.d3 = 0x2f;
--sorted_result
select straight_join * 
  from t3 as x join t3 as y on x.c3 = y.c3
  where y.d3 = 0x2f;

explain
select straight_join * 
  from t3 as x join t3 as y on x.d3 = y.d3
  where y.b3 = 0x2f;
--sorted_result
select straight_join * 
  from t3 as x join t3 as y on x.d3 = y.d3
  where y.b3 = 0x2f;

explain
select straight_join * 
  from t3 as x join t3 as y on x.d3 = y.d3
  where y.b3 = 0x20+0x2f;
--sorted_result
select straight_join * 
  from t3 as x join t3 as y on x.d3 = y.d3
  where y.b3 = 0x20+0x2f;


## Not pushable as 'not null' is actually not a single const value
--replace_column 10 # 11 #
explain
select straight_join * 
  from t3 as x join t3 as y on x.b3 = y.b3
  where y.d3 is not null;
--sorted_result
select straight_join * 
  from t3 as x join t3 as y on x.b3 = y.b3
  where y.d3 is not null;

## Neither 'is null' pushable as uniqueness is not defined for null
## ... and null's are not present in the unique index
explain
select straight_join * 
  from t3 as x join t3 as y on x.b3 = y.b3
  where y.d3 is null;
--sorted_result
select straight_join * 
  from t3 as x join t3 as y on x.b3 = y.b3
  where y.d3 is null;

explain
select straight_join * 
  from t3 as x join t3 as y on x.c3 = y.c3
  where y.b3 = 0;
--sorted_result
select straight_join * 
  from t3 as x join t3 as y on x.c3 = y.c3
  where y.b3 = 0;

## As 'b3' is defined as 'not null', this query will optimized as 'Impossible WHERE' (No push)
explain
select straight_join * 
  from t3 as x join t3 as y on x.c3 = y.c3
  where y.b3 is null;
--sorted_result
select straight_join * 
  from t3 as x join t3 as y on x.c3 = y.c3
  where y.b3 is null;

## Will break up query in 2 pushed joins.
## Last join (join t3 as y2) refer x1.c3 which will
## be handled as a constant paramValue wrt. scope of the 
## second pushed join.
explain
select straight_join * from
  t3 as x1
  join t3 as y1 on y1.b3 = x1.b3 and y1.d3 = x1.d3
  join t3 as x2 on x2.b3 = y1.b3+0
  join t3 as y2 on y2.b3 = x2.c3 and y2.d3 = x1.c3;
--sorted_result
select straight_join * from
  t3 as x1
  join t3 as y1 on y1.b3 = x1.b3 and y1.d3 = x1.d3
  join t3 as x2 on x2.b3 = y1.b3+0
  join t3 as y2 on y2.b3 = x2.c3 and y2.d3 = x1.c3;


###########################
### Prepared statments ####
###########################

prepare stmt1 from
'select straight_join * 
  from t3 as x join t3 as y on x.b3 = y.b3
  where y.d3 = 0x2f';

#execute multiple times
execute stmt1;
execute stmt1;

# Execute after drop expected to fail
drop prepare stmt1;
--error 1243
execute stmt1;

# Multiple prepare of same stmt should silently discard prev prepared stmt
prepare stmt1 from
'select straight_join * 
  from t3 as x join t3 as y on x.b3 = y.b3
  where y.d3 = 0x2f';
prepare stmt1 from
'select straight_join * 
  from t3 as x join t3 as y on x.b3 = y.b3
  where y.d3 = 0x2f';
drop prepare stmt1;

#Prepare explain'ed statement and execute it
prepare stmt1 from
'explain select straight_join * 
  from t3 as x join t3 as y on x.b3 = y.b3
  where y.d3 = 0x2f';
execute stmt1;
execute stmt1;

#survives commit;
commit;
execute stmt1;

# Drop index used by query -> Query plan should change to unpushed join
connection ddl;
drop index b3 on t3;

connection spj;
--replace_column 10 # 11 #
execute stmt1;

# Then recreate it -> original query plan
connection ddl;
create unique index b3 on t3(b3,d3);

connection spj;
execute stmt1;
drop prepare stmt1;

### Prepared stmt with dynamic parameters ('?') ###
prepare stmt1 from
'explain select straight_join * 
  from t3 as x join t3 as y on x.b3 = y.b3
  where y.d3 = ?';

set @a=47;
execute stmt1 using @a;
set @a=0;
execute stmt1 using @a;
set @a=null;
execute stmt1 using @a;

prepare stmt1 from
'select straight_join * 
  from t3 as x join t3 as y on x.b3 = y.b3
  where y.d3 = ?';

set @a=47;
execute stmt1 using @a;
set @a=0;
execute stmt1 using @a;
set @a=null;
execute stmt1 using @a;

prepare stmt1 from
'explain select straight_join * 
  from t3 as x join t3 as y on x.b3 = y.b3 and x.d3 = y.d3
  where x.a3 = ?';

set @a=47;
execute stmt1 using @a;
set @a=0;
execute stmt1 using @a;
set @a=null;
execute stmt1 using @a;

prepare stmt1 from
'select straight_join * 
  from t3 as x join t3 as y on x.b3 = y.b3 and x.d3 = y.d3
  where x.a3 = ?';

set @a=47;
execute stmt1 using @a;
set @a=0;
execute stmt1 using @a;
set @a=null;
execute stmt1 using @a;

connection ddl;
drop table t3;

connection spj;
# Execute after table dropped should fail
set @a=47;
--error 1146
execute stmt1 using @a;

####################

# test index scan disguised as JT_ALL
connection ddl;
create table t1 (a int primary key, b int, c int, index(b,c)) engine = ndb
comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
insert into t1 values (1,null, 2);
insert into t1 values (2,1, null);
insert into t1 values (3,2,2);
insert into t1 values (4,null, 2);
insert into t1 values (5,1, null);
insert into t1 values (6,2,2);

set ndb_join_pushdown=false;

--sorted_result
select *
from t1
join t1 as t2 on (t2.b = t1.b or t2.b = t1.a)
join t1 as t3 on t3.a = t2.a
join t1 as t4 on t4.a = t3.b /* index scan disguised as JT_ALL, pushdown=off */;

set ndb_join_pushdown=true;

explain
select *
from t1
join t1 as t2 on (t2.b = t1.b or t2.b = t1.a)
join t1 as t3 on t3.a = t2.a
join t1 as t4 on t4.a = t3.b /* index scan disguised as JT_ALL, pushdown=on */;
--sorted_result
select *
from t1
join t1 as t2 on (t2.b = t1.b or t2.b = t1.a)
join t1 as t3 on t3.a = t2.a
join t1 as t4 on t4.a = t3.b /* index scan disguised as JT_ALL, pushdown=on */;

## Test subquery execution where 'Full scan on null key' strategy requires
## table scan execution in addition to the key lookup which was prepared
## as part of the pushed join NdbQuery
explain
select *
from t1 where b in 
  (select x.a from t1 as x join t1 as y on (y.a = x.b))
xor c > 5;
--sorted_result
select *
from t1 where b in 
  (select x.a from t1 as x join t1 as y on (y.a = x.b))
xor c > 5;

##############
## Subqueries with EQ_REFs in subquery containing an outer referrences
## to 't1.b' should not be pushed as outer referrences are outside
## the scope of our JOIN_TAB's
##############
--replace_column 10 # 11 #
explain
select t1.a, (select straight_join x.a from t1 as x join t1 as y on x.a=y.b where y.a = t1.b) from t1;
--sorted_result
select t1.a, (select straight_join x.a from t1 as x join t1 as y on x.a=y.b where y.a = t1.b) from t1;

connection ddl;
drop table t1;

# mixed engines

connection ddl;
create table t1 (a int primary key, b int) engine = ndb
comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
create table t2 (a int primary key, b int) engine = myisam;

connection spj;
insert into t1 values(1,1), (2,2), (3,3), (4,4);
insert into t2 values(1,1), (2,2), (3,3), (4,4);

explain
select * from t1, t2, t1 as t3
where t2.a = t1.b
  and t3.a = t2.b /* mixed engines */;
--sorted_result
select * from t1, t2, t1 as t3
where t2.a = t1.b
  and t3.a = t2.b /* mixed engines */;

connection ddl;
drop table t1, t2;

# Tables with blob, but not in the selected columns:

connection ddl;
create table t1 (a int primary key, b int, c blob) engine = ndb
comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
create table t2 (a int primary key, b int) engine = ndb
comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
insert into t1 values (1,1, 'kalle');
insert into t1 values (2,1, 'kalle');
insert into t1 values (3,3, 'kalle');
insert into t1 values (4,1, 'kalle');

insert into t2 values (1,1);
insert into t2 values (2,1);
insert into t2 values (3,3);
insert into t2 values (4,1);

set ndb_join_pushdown=true;
explain
select t1.a, t1.b, t2.a, t2.b 
from t1, t2
where t2.a = t1.b;
--sorted_result
select t1.a, t1.b, t2.a, t2.b 
from t1, t2
where t2.a = t1.b;

explain
select t1.a, t1.b, t2.a, t2.b 
from t1, t2
where t2.a = t1.b
  and t1.a = 2;
--sorted_result
select t1.a, t1.b, t2.a, t2.b 
from t1, t2
where t2.a = t1.b
  and t1.a = 2;

explain
select t1.a, t1.b, t2.a, t2.b
from t1, t2
where t1.a = t2.b;
--sorted_result
select t1.a, t1.b, t2.a, t2.b 
from t1, t2
where t2.a = t1.b;

explain
select t1.a, t1.b, t2.a, t2.b
from t1, t2
where t1.a = t2.b
  and t2.a = 3;
--sorted_result
select t1.a, t1.b, t2.a, t2.b 
from t1, t2
where t1.a = t2.b
  and t2.a = 3;

#
# result sets contain blob
#   i.e no push
explain
select *
from t1, t2
where t2.a = t1.b;
--sorted_result
select *
from t1, t2
where t2.a = t1.b;

explain
select *
from t1, t2
where t2.a = t1.b
  and t1.a = 2;
--sorted_result
select *
from t1, t2
where t2.a = t1.b
  and t1.a = 2;

explain
select *
from t1, t2
where t1.a = t2.b;
--sorted_result
select *
from t1, t2
where t2.a = t1.b;

explain
select *
from t1, t2
where t1.a = t2.b
  and t2.a = 3;
--sorted_result
select *
from t1, t2
where t1.a = t2.b
  and t2.a = 3;

connection ddl;
drop table t1, t2;

## Test usage of a constValue() as part of the EQ_REF key relating a child operation
## with its previous parents.
## All datatypes are tested in the section below
##
connection ddl;
  create table t3 (a3 int, b3 tinyint, c3 int not null, d3 int not null, 
    primary key(a3,b3)) engine = ndb
    comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
  insert into t3 values (0x1f, 0x2f, 1, 0x1f);
  insert into t3 values (0x2f, 0x3f, 2, 0x2f);
  insert into t3 values (0x3f, 0x1f, 3, 0x3f);
  explain
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3="63";
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3="63";

connection ddl;
  drop table t3;
  create table t3 (a3 int, b3 tinyint unsigned, c3 int not null, d3 int not null, 
    primary key(a3,b3)) engine = ndb
    comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
connection spj;
  insert into t3 values (0x1f, 0x2f, 1, 0x1f);
  insert into t3 values (0x2f, 0x3f, 2, 0x2f);
  insert into t3 values (0x3f, 0x1f, 3, 0x3f);
  explain
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=(60+3);
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=(60+3);

connection ddl;
  drop table t3;
  create table t3 (a3 int, b3 smallint, c3 int not null, d3 int not null, 
    primary key(a3,b3)) engine = ndb
    comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
connection spj;
  insert into t3 values (0x1f, 0x2f, 1, 0x1f);
  insert into t3 values (0x2f, 0x3f, 2, 0x2f);
  insert into t3 values (0x3f, 0x1f, 3, 0x3f);
  explain
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=(60+3);
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=(60+3);

connection ddl;
  drop table t3;
  create table t3 (a3 int, b3 smallint unsigned, c3 int not null, d3 int not null, 
    primary key(a3,b3)) engine = ndb
    comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
connection spj;
  insert into t3 values (0x1f, 0x2f, 1, 0x1f);
  insert into t3 values (0x2f, 0x3f, 2, 0x2f);
  insert into t3 values (0x3f, 0x1f, 3, 0x3f);
  explain
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=(60+3);
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=(60+3);

connection ddl;
  drop table t3;
  create table t3 (a3 int, b3 mediumint, c3 int not null, d3 int not null, 
    primary key(a3,b3)) engine = ndb
    comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
connection spj;
  insert into t3 values (0x1f, 0x2f, 1, 0x1f);
  insert into t3 values (0x2f, 0x3f, 2, 0x2f);
  insert into t3 values (0x3f, 0x1f, 3, 0x3f);
  explain
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=(60+3);
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=(60+3);

connection ddl;
  drop table t3;
  create table t3 (a3 int, b3 mediumint unsigned, c3 int not null, d3 int not null, 
    primary key(a3,b3)) engine = ndb
    comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
connection spj;
  insert into t3 values (0x1f, 0x2f, 1, 0x1f);
  insert into t3 values (0x2f, 0x3f, 2, 0x2f);
  insert into t3 values (0x3f, 0x1f, 3, 0x3f);
  explain
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=(60+3);
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=(60+3);

connection ddl;
  drop table t3;
  create table t3 (a3 int, b3 int, c3 int not null, d3 int not null, 
    primary key(a3,b3)) engine = ndb
    comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
connection spj;
  insert into t3 values (0x1f, 0x2f, 1, 0x1f);
  insert into t3 values (0x2f, 0x3f, 2, 0x2f);
  insert into t3 values (0x3f, 0x1f, 3, 0x3f);
  explain
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=(60+3);
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=(60+3);

connection ddl;
  drop table t3;
  create table t3 (a3 int, b3 int unsigned, c3 int not null, d3 int not null, 
    primary key(a3,b3)) engine = ndb
    comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
connection spj;
  insert into t3 values (0x1f, 0x2f, 1, 0x1f);
  insert into t3 values (0x2f, 0x3f, 2, 0x2f);
  insert into t3 values (0x3f, 0x1f, 3, 0x3f);
  explain
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=(60+3);
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=(60+3);

connection ddl;
  drop table t3;
  create table t3 (a3 int, b3 bigint, c3 int not null, d3 int not null, 
    primary key(a3,b3)) engine = ndb
    comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
connection spj;
  insert into t3 values (0x1f, 0x2f, 1, 0x1f);
  insert into t3 values (0x2f, 0x3f, 2, 0x2f);
  insert into t3 values (0x3f, 0x1f, 3, 0x3f);
  explain
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=(60+3);
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=(60+3);

connection ddl;
  drop table t3;
  create table t3 (a3 int, b3 bigint unsigned, c3 int not null, d3 int not null, 
    primary key(a3,b3)) engine = ndb
    comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
connection spj;
  insert into t3 values (0x1f, 0x2f, 1, 0x1f);
  insert into t3 values (0x2f, 0x3f, 2, 0x2f);
  insert into t3 values (0x3f, 0x1f, 3, 0x3f);
  explain
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=(60+3);
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=(60+3);

connection ddl;
  drop table t3;
  create table t3 (a3 int, b3 boolean, c3 int not null, d3 int not null, 
    primary key(a3,b3)) engine = ndb
    comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
connection spj;
  insert into t3 values (0x1f, 0, 1, 0x1f);
  insert into t3 values (0x2f, 1, 2, 0x2f);
  insert into t3 values (0x3f, 0, 3, 0x3f);
  explain
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=1;
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=1;

connection ddl;
  drop table t3;
  create table t3 (a3 int, b3 float, c3 int not null, d3 int not null, 
    primary key(a3,b3)) engine = ndb
    comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
connection spj;
  insert into t3 values (0x1f, 2.71, 1, 0x1f);
  insert into t3 values (0x2f, 3.00, 2, 0x2f);
  insert into t3 values (0x3f, 0.50, 3, 0x3f);
  explain
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=3.0;
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=3.0;

connection ddl;
  drop table t3;
  create table t3 (a3 int, b3 float unsigned, c3 int not null, d3 int not null, 
    primary key(a3,b3)) engine = ndb
    comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
connection spj;
  insert into t3 values (0x1f, 2.71, 1, 0x1f);
  insert into t3 values (0x2f, 3.00, 2, 0x2f);
  insert into t3 values (0x3f, 0.50, 3, 0x3f);
  explain
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=3.0;
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=3.0;

connection ddl;
  drop table t3;
  create table t3 (a3 int, b3 double, c3 int not null, d3 int not null, 
    primary key(a3,b3)) engine = ndb
    comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
connection spj;
  insert into t3 values (0x1f, 2.71, 1, 0x1f);
  insert into t3 values (0x2f, 3.14, 2, 0x2f);
  insert into t3 values (0x3f, 0.50, 3, 0x3f);
  explain
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=3.14;
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=3.14;

connection ddl;
  drop table t3;
  create table t3 (a3 int, b3 double unsigned, c3 int not null, d3 int not null, 
    primary key(a3,b3)) engine = ndb
    comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
connection spj;
  insert into t3 values (0x1f, 2.71, 1, 0x1f);
  insert into t3 values (0x2f, 3.14, 2, 0x2f);
  insert into t3 values (0x3f, 0.50, 3, 0x3f);
  explain
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=3.14;
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=3.14;

connection ddl;
  drop table t3;
  create table t3 (a3 int, b3 decimal, c3 int not null, d3 int not null, 
    primary key(a3,b3)) engine = ndb
    comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
connection spj;
  insert into t3 values (0x1f, 0x2f, 1, 0x1f);
  insert into t3 values (0x2f, 0x3f, 2, 0x2f);
  insert into t3 values (0x3f, 0x1f, 3, 0x3f);
  explain
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=63;
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=63;

connection ddl;
  drop table t3;
  create table t3 (a3 int, b3 decimal(12,4), c3 int not null, d3 int not null, 
    primary key(a3,b3)) engine = ndb
    comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
connection spj;
  insert into t3 values (0x1f, 2.71, 1, 0x1f);
  insert into t3 values (0x2f, 3.14, 2, 0x2f);
  insert into t3 values (0x3f, 0.50, 3, 0x3f);
  explain
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=3.14;
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3=3.14;

connection ddl;
  drop table t3;
  create table t3 (a3 int, b3 date, c3 int not null, d3 int not null, 
    primary key(a3,b3)) engine = ndb
    comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
connection spj;
  insert into t3 values (0x1f, '1905-05-17', 1, 0x1f);
  insert into t3 values (0x2f, '2000-02-28', 2, 0x2f);
  insert into t3 values (0x3f, '2000-02-29', 3, 0x3f);
  explain
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3='2000-02-28';
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3='2000-02-28';

## Temp removed due to lack of DATETIME2 support
#connection ddl;
#  drop table t3;
#  create table t3 (a3 int, b3 datetime, c3 int not null, d3 int not null, 
#    primary key(a3,b3)) engine = ndb;
#connection spj;
#  insert into t3 values (0x1f, '1905-05-17 12:30:00', 1, 0x1f);
#  insert into t3 values (0x2f, '2000-02-28 23:59:00', 2, 0x2f);
#  insert into t3 values (0x3f, '2000-02-29 12:59:59', 2, 0x3f);
#  explain
#  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3='2000-02-28 23:59';
#  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3='2000-02-28 23:59';
#
#connection ddl;
#  drop table t3;
#  create table t3 (a3 int, b3 time, c3 int not null, d3 int not null, 
#    primary key(a3,b3)) engine = ndb;
#connection spj;
#  insert into t3 values (0x1f, '12:30:00', 1, 0x1f);
#  insert into t3 values (0x2f, '23:59:00', 2, 0x2f);
#  insert into t3 values (0x3f, '12:59:59', 2, 0x3f);
#  explain
#  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3='23:59';
#  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3='23:59';

connection ddl;
  drop table t3;
  create table t3 (a3 int, b3 char(16), c3 int not null, d3 int not null, 
    primary key(a3,b3)) engine = ndb
    comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
connection spj;
  insert into t3 values (0x1f, 'Ole', 1, 0x1f);
  insert into t3 values (0x2f, 'Dole', 2, 0x2f);
  insert into t3 values (0x3f, 'Doffen', 2, 0x3f);
  explain
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3='Dole';
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3='Dole';

connection ddl;
  drop table t3;
  create table t3 (a3 int, b3 varchar(16), c3 int not null, d3 int not null, 
    primary key(a3,b3)) engine = ndb
    comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
connection spj;
  insert into t3 values (0x1f, 'Ole', 1, 0x1f);
  insert into t3 values (0x2f, 'Dole', 2, 0x2f);
  insert into t3 values (0x3f, 'Doffen', 2, 0x3f);
  explain
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3='Dole';
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3='Dole';

connection ddl;
  drop table t3;
  create table t3 (a3 int, b3 varchar(512), c3 int not null, d3 int not null, 
    primary key(a3,b3)) engine = ndb
    comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
connection spj;
  insert into t3 values (0x1f, 'Ole', 1, 0x1f);
  insert into t3 values (0x2f, 'Dole', 2, 0x2f);
  insert into t3 values (0x3f, 'Doffen', 2, 0x3f);
  explain
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3='Dole';
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3='Dole';

connection ddl;
  drop table t3;
  create table t3 (a3 int, b3 binary(16), c3 int not null, d3 int not null, 
    primary key(a3,b3)) engine = ndb
    comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
connection spj;
  insert into t3 values (0x1f, 'Ole', 1, 0x1f);
  insert into t3 values (0x2f, 'Dole', 2, 0x2f);
  insert into t3 values (0x3f, 'Doffen', 2, 0x3f);
  explain
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3='Dole';
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3='Dole';

connection ddl;
  drop table t3;
    create table t3 (a3 int, b3 varbinary(16), c3 int not null, d3 int not null, 
    primary key(a3,b3)) engine = ndb
    comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
connection spj;
  insert into t3 values (0x1f, 'Ole', 1, 0x1f);
  insert into t3 values (0x2f, 'Dole', 2, 0x2f);
  insert into t3 values (0x3f, 'Doffen', 2, 0x3f);
  explain
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3='Dole';
  select * from t3 x, t3 y where x.a3=0x2f and y.a3=x.d3 and y.b3='Dole';

connection ddl;
  drop table t3;


## Joins where the datatype of the EQ_REF columns are not identical
## should not be pushed
##
connection ddl;
  create table t3 (a3 int, b3 tinyint, c3 int not null, d3 int not null, 
    primary key(a3,b3)) engine = ndb
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
  insert into t3 values (0x1f, 0x2f, 1, 0x1f);
  insert into t3 values (0x2f, 0x3f, 2, 0x2f);
  insert into t3 values (0x3f, 0x1f, 3, 0x3f);
  explain
  select * from t3 x, t3 y where y.a3=x.b3 and y.b3="63";
  select * from t3 x, t3 y where y.a3=x.b3 and y.b3="63";

connection ddl;
  drop table t3;


##
## Testing of varchar datatype as part of lookup key and index bounds.
## Need special attention due to the 'ShrinkVarchar' format used by mysqld.

connection ddl;
create table t3 (a3 varchar(16), b3 int, c3 int not null, d3 int not null, 
  primary key(a3,b3)) engine = ndb
comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
insert into t3 values ('Ole', 0x1f, 1, 0x1f);
insert into t3 values ('Dole', 0x2f, 2, 0x2f);
insert into t3 values ('Doffen', 0x3f, 2, 0x3f);

# Varchar is lookup key
explain
select * from t3 x, t3 y where x.a3='Dole' and x.b3=0x2f and y.a3=x.a3 and y.b3=x.d3;
select * from t3 x, t3 y where x.a3='Dole' and x.b3=0x2f and y.a3=x.a3 and y.b3=x.d3;

# Varchar as hi/low bound
explain
select * from t3 x, t3 y where x.a3='Dole' and y.a3=x.a3 and y.b3=x.d3;
select * from t3 x, t3 y where x.a3='Dole' and y.a3=x.a3 and y.b3=x.d3;

connection ddl;
drop table t3;

connection ddl;
create table t1 (k int primary key, b int) engine = ndb
comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
insert into t1 values (1,1), (2,1), (3,1), (4,1);

## Pushed join driven by a scan, with cached row lookups:
## (Note: We force the Scan to not be pushed as the parent op
## by making the join condition on t2 a non-FIELD_ITEM ('t1.b+0')
## As all column 'b' has the same value (1), the scan will refer 
## the same t2 (parent) row in every access. This will trigger the 
## row caching in join_read_key() where we eliminate redundant lookups
## where 'next row == current row'. In order to work for linked operations,
## the value and status for all linked tables should be kept unaltered.
explain
select *
from t1
  straight_join t1 as t2 on t2.k = t1.b+0
  straight_join t1 as t3 on t3.k = t2.b
  straight_join t1 as t4 on t4.k = t1.b;
--sorted_result
select *
from t1
  straight_join t1 as t2 on t2.k = t1.b+0
  straight_join t1 as t3 on t3.k = t2.b
  straight_join t1 as t4 on t4.k = t1.b;

## Similar example as above, except that access to 't2' is made
## a const table access
explain
select *
from t1
  straight_join t1 as t2 on t2.k = t1.b+0
  straight_join t1 as t3 on t3.k = t2.b
  straight_join t1 as t4 on t4.k = t1.b
where t2.k = 1;
--sorted_result
select *
from t1
  straight_join t1 as t2 on t2.k = t1.b+0
  straight_join t1 as t3 on t3.k = t2.b
  straight_join t1 as t4 on t4.k = t1.b
where t2.k = 1;

connection ddl;
drop table t1;

##
# Try with higher row-count to test batching/flow control
#
--error 0,1193
set global debug='+d,max_64rows_in_spj_batches';

connection ddl;
create table t1 (
  a int not null auto_increment,
  b char(255) not null,
  c int not null,
  d char(255) not null,
  primary key (`a`,`b`)
) engine=ndbcluster
comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
let $1=1000;
disable_query_log;
while ($1)
{
 eval insert into t1(a,b,c,d) values
 ($1, 'a', $1, 'a'),($1, 'b', $1+1, 'b'),($1, 'c', $1-1, 'c');
 dec $1;
}
enable_query_log;

explain
select count(*)
from t1 
join t1 as t2 on t2.a = t1.c and t2.b = t1.d
join t1 as t3 on t3.a = t2.c and t3.b = t2.d;
select count(*)
from t1 
join t1 as t2 on t2.a = t1.c and t2.b = t1.d
join t1 as t3 on t3.a = t2.c and t3.b = t2.d;

explain
select count(*)
from t1
join t1 as t2 on t2.a = t1.c
join t1 as t3 on t3.a = t2.c and t3.b = t2.d;
select count(*)
from t1
join t1 as t2 on t2.a = t1.c
join t1 as t3 on t3.a = t2.c and t3.b = t2.d;

explain
select count(*)
from t1
join t1 as t2 on t2.a = t1.c and t2.b = t1.d
join t1 as t3 on t3.a = t2.c;
select count(*)
from t1
join t1 as t2 on t2.a = t1.c and t2.b = t1.d
join t1 as t3 on t3.a = t2.c;

connection ddl;
alter table t1 partition by key(a);

connection spj;
explain
select count(*)
from t1
join t1 as t2 on t2.a = t1.c
join t1 as t3 on t3.a = t2.c and t3.b = t2.d;
select count(*)
from t1
join t1 as t2 on t2.a = t1.c
join t1 as t3 on t3.a = t2.c and t3.b = t2.d;

connection ddl;
drop table t1;

--error 0,1193
set global debug=@save_debug;

# Pushed join accessing disk data.

connection ddl;
create logfile group lg1
add undofile 'undofile.dat'
initial_size 1m
undo_buffer_size = 1m
engine=ndb;

create tablespace ts1
add datafile 'datafile.dat'
use logfile group lg1
initial_size 6m
engine ndb;

create table t1 (a int not null, 
                 b int not null storage disk, 
       		 c int not null storage memory, 
		 primary key(a)) 
		 tablespace ts1 storage disk engine = ndb
 comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
insert into t1 values (10, 11, 11);
insert into t1 values (11, 12, 12);
insert into t1 values (12, 13, 13);

connection ddl;
create table t2 (a int not null, 
       	     	 b int not null, primary key(a)) engine = ndb
 comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
insert into t2 values (10, 11);
insert into t2 values (11, 12);
insert into t2 values (12, 13);

# Disk data in projection of first op.
explain select * from t1, t2 where t1.c = t2.a;
--sorted_result
select * from t1, t2 where t1.c = t2.a;

explain select * from t1, t2 where t1.a=11 and t1.c = t2.a;
select * from t1, t2 where t1.a=11 and t1.c = t2.a;

# Disk data in projection of second op.
explain select * from t2, t1 where t2.b = t1.a;
--sorted_result
select * from t2, t1 where t2.b = t1.a;

explain select * from t2, t1 where t2.a=11 and t2.b = t1.a;
select * from t2, t1 where t2.a=11 and t2.b = t1.a;

# Disk data in predicate but not in projection
explain select t1.a, t1.c, t2.a, t2.b from t1, t2 where t1.b = t2.a;
--sorted_result
select t1.a, t1.c, t2.a, t2.b from t1, t2 where t1.b = t2.a;

explain select t1.a, t1.c, t2.a, t2.b from t1, t2 where t1.a=11 and t1.b = t2.a;
select t1.a, t1.c, t2.a, t2.b from t1, t2 where t1.a=11 and t1.b = t2.a;

connection ddl;
drop table t1;
drop table t2;

alter tablespace ts1
drop datafile 'datafile.dat';

drop tablespace ts1;

drop logfile group lg1
engine ndb;
connection spj;

# Store old counter values.
connection ddl;
create temporary table old_count 
       select counter_name, sum(val) as val 
       from ndbinfo.counters 
       where block_name='DBSPJ' 
       group by counter_name;

connection ddl;
# Specify number of partition to be independent of configured #LDM's
create table t1 (a int not null, 
                 b int not null,
                 c int not null,
                 primary key(a)) 
                 engine = ndb
                 partition by key() partitions 8;

connection spj;
# We use key values that have the same representation in little and big endian.
# Otherwise, the numbers for local and remote reads may depend on endian-ness,
# since hashing is endian dependent.
insert into t1 values (1, 2, 2);
insert into t1 values (2, 3, 3);
insert into t1 values (3, 4, 4);

# Run some queries that should increment the counters.
select * from t1 t1, t1 t2 where t1.a = 2 and t2.a = t1.b; 

select count(*) from t1 t1, t1 t2 where t2.a = t1.b; 

select count(*) from t1 t1, t1 t2 where t1.a >= 2 and t2.a = t1.b; 


# Get new counter values.
connection ddl;
create temporary table new_count 
       select counter_name, sum(val) as val 
       from ndbinfo.counters 
       where block_name='DBSPJ' 
       group by counter_name;

# Compute the difference.
--sorted_result
select new_count.counter_name, new_count.val - old_count.val 
       from new_count, old_count 
       where new_count.counter_name = old_count.counter_name
       and new_count.counter_name <> 'LOCAL_READS_SENT'
       and new_count.counter_name <> 'REMOTE_READS_SENT';

select 'READS_SENT', sum(new_count.val - old_count.val) 
       from new_count, old_count 
       where new_count.counter_name = old_count.counter_name
       and (new_count.counter_name = 'LOCAL_READS_SENT'
       or new_count.counter_name = 'REMOTE_READS_SENT');

connection ddl;
drop table old_count;
drop table new_count;
drop table t1;

### Test that scan filters are used for pushed operations.

connection ddl;
create table t1 (
       a int primary key, 
       b int,
       c int) engine = ndb
 comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
insert into t1 values (1, 2, 3);
insert into t1 values (2, 3, 4);
insert into t1 values (3, 4, 5);

# Find the total number of lookups issued by the SPJ blocks.
let $spj_lookups = query_get_value(select sum(val) as Value from ndbinfo.counters where block_name='DBSPJ' and (counter_name='LOCAL_READS_SENT' or counter_name='REMOTE_READS_SENT'), Value, 1);


# Root scan should give only one tuple if scan filter is pushed. 
# Therefore only one lookup on 'y'.
--replace_column 10 # 11 #
explain select * from t1 x, t1 y where x.b=y.a and x.c=4;

select * from t1 x, t1 y where x.b=y.a and x.c=4;

--disable_query_log
--eval select sum(val) - $spj_lookups as lookups from ndbinfo.counters where block_name='DBSPJ' and (counter_name='LOCAL_READS_SENT' or counter_name='REMOTE_READS_SENT')
--enable_query_log

# Lookup on y should only give one result tuple if filter is pushed.
# This should give 3 lookups on 'y' and 1 on 'z', 4 in all.
--replace_column 10 # 11 #
explain select * from t1 x, t1 y, t1 z where x.b=y.a and y.c=4 and y.b=z.a;

select * from t1 x, t1 y, t1 z where x.b=y.a and y.c=4 and y.b=z.a;

--disable_query_log
--eval select sum(val) - $spj_lookups as lookups from ndbinfo.counters where block_name='DBSPJ' and (counter_name='LOCAL_READS_SENT' or counter_name='REMOTE_READS_SENT')
--enable_query_log

connection ddl;
drop table t1;

# Test and server status variables (i.e. mysqld counters)

connection ddl;
create table t1(
       a int not null,
       b int not null,
       c int not null,
       primary key(a,b))
engine = ndb
comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM"
partition by key (a);

connection spj;
insert into t1 values (10, 10, 11);
insert into t1 values (11, 11, 12);
insert into t1 values (12, 12, 13);

# First query against a new table causes an extra scan (of a dictionaty table??)
# so adding an extra scan here to make results from the following part easier
# to interpret.
select * from t1 t1, t1 t2 
      where t1.a = 10 and t1.b = 10 and 
      	     t2.a = t1.c and t2.b = t1.c; 

# Save old counter values.
# Save old mysqld counter values.
create temporary table server_counts
       select * from performance_schema.global_status 
       where variable_name in 
       ('Ndb_scan_count',
        'Ndb_pruned_scan_count',
        'Ndb_sorted_scan_count',
        'Ndb_pushed_queries_defined',
        'Ndb_pushed_queries_dropped',
	'Ndb_pushed_reads');

# Run some queries that should increment the counters.
# This query should push a single read.
select * from t1 t1, t1 t2 
      where t1.a = 11 and t1.b = 11 and 
      	     t2.a = t1.c and t2.b = t1.c; 

# This query should push a sorted scan (and three reads).
select * from t1 t1, t1 t2 
       where t2.a = t1.c and t2.b = t1.c
       order by t1.a; 

# This query should push a pruned scan (but pruning must be fixed for 
# pushed scans.)
select count(*) from t1 t1, t1 t2 
       where t1.a = 11 and 
       	     t2.a = t1.c and t2.b = t1.c; 

# Calculate the change in mysqld counters.
select new.variable_name, new.variable_value - old.variable_value
       from server_counts as old,
         performance_schema.global_status as new
       where new.variable_name = old.variable_name
       order by new.variable_name;

drop table server_counts;

connection ddl;
drop table t1;

# Test scan pruning 
connection ddl;
create table t1(
       d int not null,
       c int not null,
       a int not null,
       b int not null,
       primary key using hash (a,b))
engine = ndb
comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM"
partition by key (a);

connection spj;
insert into t1(a,b,c,d) values (10, 10, 11, 11);
insert into t1(a,b,c,d) values (11, 11, 12, 12);
insert into t1(a,b,c,d) values (12, 12, 13, 13);

let $old_pruned_scan_count = query_get_value(show status like 'Ndb_pruned_scan_count', Value, 1);

# Should give pruned scan.

connection ddl;
create index i1 on t1(c,a);

connection spj;

select count(*) from t1 t1, t1 t2 where t1.c = 12 and t1.a = 11 and t2.a = t1.d and t2.b = t1.d; 

connection ddl;
drop index i1 on t1;

connection spj;
--disable_query_log
let $new_pruned_scan_count = query_get_value(show status like 'Ndb_pruned_scan_count', Value, 1);
--eval select $new_pruned_scan_count - $old_pruned_scan_count as pruned_scan_count
let $old_pruned_scan_count = query_get_value(show status like 'Ndb_pruned_scan_count', Value, 1);
--enable_query_log

# Should give pruned scan. There is a one sided limit for t1.b, but this is
# after the partition key prefix.

connection ddl;
create index i2 on t1(a,b);

connection spj;
select count(*) from t1 t1, t1 t2 where t1.a = 11 and t1.b<13 and t2.a = t1.c and t2.b = t1.c; 

--disable_query_log
let $new_pruned_scan_count = query_get_value(show status like 'Ndb_pruned_scan_count', Value, 1);
--eval select $new_pruned_scan_count - $old_pruned_scan_count as pruned_scan_count
let $old_pruned_scan_count = query_get_value(show status like 'Ndb_pruned_scan_count', Value, 1);
--enable_query_log

# Should give pruned scan. Upper and lower bounds for t1.a are the sane. 
select count(*) from t1 t1, t1 t2 where t1.a >= 12 and t1.a<=12 and t2.a = t1.c and t2.b = t1.c; 

--disable_query_log
let $new_pruned_scan_count = query_get_value(show status like 'Ndb_pruned_scan_count', Value, 1);
--eval select $new_pruned_scan_count - $old_pruned_scan_count as pruned_scan_count
let $old_pruned_scan_count = query_get_value(show status like 'Ndb_pruned_scan_count', Value, 1);
--enable_query_log

# Should not give pruned scan. Upper and lower bounds for t1.a are different. 
select count(*) from t1 t1, t1 t2 where t1.a >= 11 and t1.a<=12 and t2.a = t1.c and t2.b = t1.c; 

--disable_query_log
let $new_pruned_scan_count = query_get_value(show status like 'Ndb_pruned_scan_count', Value, 1);
--eval select $new_pruned_scan_count - $old_pruned_scan_count as pruned_scan_count
let $old_pruned_scan_count = query_get_value(show status like 'Ndb_pruned_scan_count', Value, 1);
--enable_query_log

# Should not give pruned scan. There will be two sets of bounds that have
# different distribution keys (t1.a=10 and t1.a=12). 
select count(*) from t1 t1, t1 t2 where (t1.a = 10 or t1.a=12) and t1.b<13 and t2.a = t1.c and t2.b = t1.c; 

--disable_query_log
let $new_pruned_scan_count = query_get_value(show status like 'Ndb_pruned_scan_count', Value, 1);
--eval select $new_pruned_scan_count - $old_pruned_scan_count as pruned_scan_count
let $old_pruned_scan_count = query_get_value(show status like 'Ndb_pruned_scan_count', Value, 1);
--enable_query_log

# Should give pruned scan. There will be two sets of bounds, but they have the
# same distribution key.

select count(*) from t1 t1, t1 t2 where t1.a = 10 and (t1.b<11 or t1.b>11) and t2.a = t1.c and t2.b = t1.c; 

--disable_query_log
let $new_pruned_scan_count = query_get_value(show status like 'Ndb_pruned_scan_count', Value, 1);
--eval select $new_pruned_scan_count - $old_pruned_scan_count as pruned_scan_count
let $old_pruned_scan_count = query_get_value(show status like 'Ndb_pruned_scan_count', Value, 1);
--enable_query_log

connection ddl;
drop table t1;

create table t2(
       d int not null,
       e int not null,
       f int not null,
       a int not null,
       b int not null,
       c int not null,
       primary key using hash (a,b,c))
engine = ndb
comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM"
partition by key (b,a);

connection spj;
insert into t2(a,b,c,d,e,f) values (1, 2, 3, 1, 2, 3); 
insert into t2(a,b,c,d,e,f) values (1, 2, 4, 1, 2, 3); 
insert into t2(a,b,c,d,e,f) values (2, 3, 4, 1, 2, 3); 
insert into t2(a,b,c,d,e,f) values (3, 4, 5, 1, 2, 3); 
insert into t2(a,b,c,d,e,f) values (4, 5, 6, 1, 2, 3); 
insert into t2(a,b,c,d,e,f) values (5, 6, 7, 1, 2, 3); 
insert into t2(a,b,c,d,e,f) values (6, 7, 8, 1, 2, 3); 
insert into t2(a,b,c,d,e,f) values (7, 8, 9, 1, 2, 3); 

connection ddl;
create index i2_1 on t2(d, a, b, e);

connection spj;

# Should give pruned scan. The index prefix containing the distribution key
# has a single possible value.
select count(*) from t2 x, t2 y where x.d=1 and x.a=1 and x.b=2 and y.a=x.d and y.b=x.e and y.c=3;

--disable_query_log
let $new_pruned_scan_count = query_get_value(show status like 'Ndb_pruned_scan_count', Value, 1);
--eval select $new_pruned_scan_count - $old_pruned_scan_count as pruned_scan_count
let $old_pruned_scan_count = query_get_value(show status like 'Ndb_pruned_scan_count', Value, 1);
--enable_query_log

connection ddl;
drop index i2_1 on t2;
create index i2_3 on t2(a, d, b, e);

connection spj;

# Should give pruned scan. The index prefix containing the distribution key
# has a single possible value.

# Turn off condition_fanout_filter to keep "old" QEP
set optimizer_switch='condition_fanout_filter=off';
select count(*) from t2 x, t2 y where x.d=1 and x.a=1 and x.b=2 and y.a=x.d and y.b=x.e and y.c=3;

# Restore condition_fanout_filter to default value
set optimizer_switch='condition_fanout_filter=default';

--disable_query_log
let $new_pruned_scan_count = query_get_value(show status like 'Ndb_pruned_scan_count', Value, 1);
--eval select $new_pruned_scan_count - $old_pruned_scan_count as pruned_scan_count
--enable_query_log

connection ddl;
drop table t2;

connection ddl;
create table t1 (a binary(10) primary key, b binary(10) not null) engine = ndb
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
insert into t1 values ('\0123456789', '1234567890');
insert into t1 values ('1234567890', '\0123456789');

explain
select count(*)
from t1 join t1 as t2 on t2.a = t1.b
where t1.a = '\0123456789';
select count(*)
from t1 join t1 as t2 on t2.a = t1.b
where t1.a = '\0123456789';

connection ddl;
drop table t1;


# Tests for some bugfixes which have been cherry picked from 5.1 main
# Not necessarily test of SPJ functionality, but may test optimizer
# behaviour which we depend on when doing RQG testing
# We can remove these testcases when fixes - and propper MTR testcases -
# Have been merged from 5.1 main branch.

connection ddl;
create table t1 (pk int primary key, a int unique key) engine = ndb
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
insert into t1 values (1,10), (2,20), (3,30);

set ndb_join_pushdown = false;

# Bug#53334:
# Join should be optimized as 'Impossible On condition'
# ... *not* 'Impossible WHERE'

explain
select * from t1 as x right join t1 as y
  on x.pk = y.pk
 and x.pk = y.a
 and x.a = y.pk
where y.pk = 2;
select * from t1 as x right join t1 as y
  on x.pk = y.pk
 and x.pk = y.a
 and x.a = y.pk
where y.pk = 2;


set ndb_join_pushdown = true;

explain
select * from t1 as x right join t1 as y
  on x.pk = y.pk
 and x.pk = y.a
 and x.a = y.pk
where y.pk = 2;
select * from t1 as x right join t1 as y
  on x.pk = y.pk
 and x.pk = y.a
 and x.a = y.pk
where y.pk = 2;

connection ddl;
drop table t1;

#########################################
# Test section for scan-child operations
#########################################

# Test scan-lookup-scan query (see http://lists.mysql.com/commits/115164)

connection ddl;
create table t1 (pk int primary key, u int not null, a int, b int) engine=ndb
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
create index ix1 on t1(b,a);
      
connection spj;
insert into t1 values (0,1,10,20);
insert into t1 values (1,2,20,30);
insert into t1 values (2,3,30,40);

--replace_column 10 # 11 #
explain select * from t1 as x join t1 as y join t1 as z on x.u=y.pk and y.a=z.b;
--sorted_result
select * from t1 as x join t1 as y join t1 as z on x.u=y.pk and y.a=z.b;

connection ddl;
drop table t1;

# Test sorted scan where inner join eliminates all rows (known regression).
connection ddl;
create table t1 (pk int primary key, u int not null) engine=ndb
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
insert into t1 values (0,-1), (1,-1), (2,-1), (3,-1), (4,-1), (5,-1), (6,-1), 
(7,-1), (8,-1), (9,-1), (10,-1), (11,-1), (12,-1), (13,-1), (14,-1), (15,-1), 
(16,-1), (17,-1), (18,-1), (19,-1), (20,-1), (21,-1), (22,-1), (23,-1), 
(24,-1), (25,-1), (26,-1), (27,-1), (28,-1), (29,-1), (30,-1), (31,-1), 
(32,-1), (33,-1), (34,-1), (35,-1), (36,-1), (37,-1), (38,-1), (39,-1), 
(40,-1), (41,-1), (42,-1), (43,-1), (44,-1), (45,-1), (46,-1), (47,-1), 
(48,-1), (49,-1), (50,-1), (51,-1), (52,-1), (53,-1), (54,-1), (55,-1), 
(56,-1), (57,-1), (58,-1), (59,-1), (60,-1), (61,-1), (62,-1), (63,-1), 
(64,-1), (65,-1), (66,-1), (67,-1), (68,-1), (69,-1), (70,-1), (71,-1), 
(72,-1), (73,-1), (74,-1), (75,-1), (76,-1), (77,-1), (78,-1), (79,-1), 
(80,-1), (81,-1), (82,-1), (83,-1), (84,-1), (85,-1), (86,-1), (87,-1), 
(88,-1), (89,-1), (90,-1), (91,-1), (92,-1), (93,-1), (94,-1), (95,-1), 
(96,-1), (97,-1), (98,-1), (99,-1), (100,-1), (101,-1), (102,-1), (103,-1), 
(104,-1), (105,-1), (106,-1), (107,-1), (108,-1), (109,-1), (110,-1), 
(111,-1), (112,-1), (113,-1), (114,-1), (115,-1), (116,-1), (117,-1), 
(118,-1), (119,-1), (120,-1), (121,-1), (122,-1), (123,-1), (124,-1), 
(125,-1), (126,-1), (127,-1), (128,-1), (129,-1), (130,-1), (131,-1), 
(132,-1), (133,-1), (134,-1), (135,-1), (136,-1), (137,-1), (138,-1), (139,-1);

--error 0,1193
set global debug='+d,max_64rows_in_spj_batches';

explain select * from t1 as x join t1 as y on x.u=y.pk order by(x.pk);
select * from t1 as x join t1 as y on x.u=y.pk order by(x.pk);

--error 0,1193
set global debug=@save_debug;

connection ddl;
drop table t1;

# Test query using "scan -> unique index lookup -> index scan".

connection ddl;
create table t1 (pk int primary key, u int not null, a int, b int) engine=ndb
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
create index ix1 on t1(b,a);
create unique index ix2 on t1(u);

connection spj;
insert into t1 values (0,0,10,10);
insert into t1 values (1,1,10,10);
insert into t1 values (2,2,10,10);
insert into t1 values (3,3,10,10);
insert into t1 values (4,4,10,10);
insert into t1 values (5,5,10,10);
insert into t1 values (6,6,10,10);
insert into t1 values (7,7,10,10);
insert into t1 values (8,8,10,10);
insert into t1 values (9,9,10,10);
insert into t1 values (10,10,10,10);
insert into t1 values (11,11,10,10);


explain select count(*) from t1 as x1 join t1 as x2 join t1 as x3
on x1.a=x2.u and x2.a = x3.b;

select count(*) from t1 as x1 join t1 as x2 join t1 as x3 
on x1.a=x2.u and x2.a = x3.b;

explain select count(*) from t1 as x1, t1 as x2, t1 as x3
where x1.u=x2.pk and x1.a=x3.b;

select count(*) from t1 as x1, t1 as x2, t1 as x3 
where x1.u=x2.pk and x1.a=x3.b;

# Regression test for commit http://lists.mysql.com/commits/116372
# (missing rows in left join query with multiple result batches).

insert into t1 values (12,12,20,10);

explain select count(*) from t1 as x1 left join t1 as x2 on x1.a=x2.b;
select count(*) from t1 as x1 left join t1 as x2 on x1.a=x2.b;
set ndb_join_pushdown=off;
select count(*) from t1 as x1 left join t1 as x2 on x1.a=x2.b;
set ndb_join_pushdown=on;

# Test left join with mix of scan and lookup.
explain select count(*) from t1 as x1
left join t1 as x2 on x1.u=x2.pk 
left join t1 as x3 on x2.a=x3.b;

select count(*) from t1 as x1 
left join t1 as x2 on x1.u=x2.pk 
left join t1 as x3 on x2.a=x3.b;
set ndb_join_pushdown=off;
select count(*) from t1 as x1 
left join t1 as x2 on x1.u=x2.pk 
left join t1 as x3 on x2.a=x3.b;
set ndb_join_pushdown=on;

explain select count(*) from t1 as x1
left join t1 as x2 on x1.u=x2.pk
left join t1 as x3 on x2.a=x3.b
left join t1 as x4 on x3.u=x4.pk
left join t1 as x5 on x4.a=x5.b;

select count(*) from t1 as x1
left join t1 as x2 on x1.u=x2.pk
left join t1 as x3 on x2.a=x3.b
left join t1 as x4 on x3.u=x4.pk
left join t1 as x5 on x4.a=x5.b;
set ndb_join_pushdown=off;
select count(*) from t1 as x1
left join t1 as x2 on x1.u=x2.pk
left join t1 as x3 on x2.a=x3.b
left join t1 as x4 on x3.u=x4.pk
left join t1 as x5 on x4.a=x5.b;
set ndb_join_pushdown=on;


############################
# Testcase for 'Got error 20002 'Unknown error code' from NDBCLUSTER'
# Caused by failure to identify AT_MULTI_PRIMARY_KEY as a lookup operation.
# This in turn caused a pushed 'lookup-scan' query to be produced - which we don't support

# Should not be pushed (lookup-scan query)
explain select count(*) from t1 as x1
 join t1 as x2 on x1.a=x2.b
 where x1.pk = 1 or x1.u=1;

select count(*) from t1 as x1
 join t1 as x2 on x1.a=x2.b
 where x1.pk = 1 or x1.u=1;

############################
# Testcase which forced us to ditch using the 'global cursor'
# on the NdbQuery result set from mysqld.
#
# As the global cursor will fool mysqld into handling the resultset
# as a result from a scan - n*lookup query, incorrect cardinality on the
# parent operation was perceived. Which caused extra null-joined outer rows
# to be emitted in this testcase.
#
# Refactored handler interface and SPJ API use subcursor on each operation
# which correctly preserves the dependency between the parent subscans 
# and its child(s).

--error 0,1193
set global debug='+d,max_4rows_in_spj_batches';

set ndb_join_pushdown=on;
explain
select straight_join * from t1 as table1
  left join 
   (t1 as table2  join t1 as table3 on table2.pk = table3.b)
 on table1.pk = table2.b;
--sorted_result
select straight_join * from t1 as table1
  left join 
   (t1 as table2  join t1 as table3 on table2.pk = table3.b)
 on table1.pk = table2.b;

--error 0,1193
set global debug=@save_debug;

#############
# Testcase for 'sledgehammer' fix for scan -> outer join scan:
# Pushing of outer joined has to be dissabled as incomplete child batches
# may cause the parent row to be returned multiple times:

# Push scan-scan when inner joined
explain select straight_join * from t1 as x1
  inner join t1 as x2 on x2.b = x1.a;

# Outer joined scans are not pushed.
explain select straight_join * from t1 as x1
  left join t1 as x2 on x2.b = x1.a;
explain select straight_join * from t1 as x1
  right join t1 as x2 on x2.b = x1.a;

# If there is a lookup operation(s) inbetween the scans
# pushing is disabled if any of these are outer joined

# inner joined lookups, push allowed
explain select straight_join * from
  t1 as x1 inner join
    (t1 as x2 inner join t1 as x3 on x3.b = x2.a)
  on x2.pk = x1.a;

# Even if x3 is inner joined with x2 (lookup)
# push is dissabled as x2 is outer joined with embedding scan operation
# which makes join relation between the scans on x1 & x3 an 'indirect' outer join
explain select straight_join * from
  t1 as x1 left join
    (t1 as x2 inner join t1 as x3 on x3.b = x2.a)
  on x2.pk = x1.a;

#############
# Test bushy-scans:
# These should be allowed to be executed in 'parallel', depending on
# only the root operation
#

explain select straight_join count(*) from t1 as x1
  join t1 as x2 on x2.b = x1.a
  join t1 as x3 on x3.b = x1.b;
set ndb_join_pushdown=off;
select straight_join count(*) from t1 as x1 
  join t1 as x2 on x2.b = x1.a
  join t1 as x3 on x3.b = x1.b;
set ndb_join_pushdown=on;
select straight_join count(*) from t1 as x1 
  join t1 as x2 on x2.b = x1.a
  join t1 as x3 on x3.b = x1.b;

# A really bushy scan - would take almost forever to execute if
# we had to force the child scan to be non-bushy (serialized by
# adding artificial parent dependencies)
# 
--replace_column 10 # 11 #
explain select straight_join count(*) from t1 as x1
  join t1 as x2 on x2.b = x1.a
  join t1 as x3 on x3.b = x1.a
  join t1 as x4 on x4.b = x1.a
  join t1 as x5 on x5.b = x1.a
  join t1 as x6 on x6.b = x1.a
  join t1 as x7 on x7.b = x1.a
where x3.a < x2.pk and x4.a < x3.pk; 

# set '64rows' in order to avoid to small batches which will
# cause all subscans to be repeated... and repeated... and

--error 0,1193
set global debug='+d,max_64rows_in_spj_batches';

select straight_join count(*) from t1 as x1 
  join t1 as x2 on x2.b = x1.a
  join t1 as x3 on x3.b = x1.a
  join t1 as x4 on x4.b = x1.a
  join t1 as x5 on x5.b = x1.a
  join t1 as x6 on x6.b = x1.a
  join t1 as x7 on x7.b = x1.a
where x3.a < x2.pk and x4.a < x3.pk; 

--error 0,1193
set global debug=@save_debug;

#############
# If we have an outer join, we can't create an artificial dep. 'through' the outer join.
# In this case the child scan can't be part of the pushed query.
#
explain select straight_join count(*) from t1 as x1
  left join t1 as x2 on x2.b = x1.a
  join t1 as x3 on x3.b = x1.b;
 
set ndb_join_pushdown=off;
select straight_join count(*) from t1 as x1 
  left join t1 as x2 on x2.b = x1.a
  join t1 as x3 on x3.b = x1.b;
set ndb_join_pushdown=on;
select straight_join count(*) from t1 as x1 
  left join t1 as x2 on x2.b = x1.a
  join t1 as x3 on x3.b = x1.b;

############
# However, When the scanchild itself is an outer join, we *can* push that scan operation
#
explain select straight_join count(*) from t1 as x1
  join t1 as x2 on x2.b = x1.a
  left join t1 as x3 on x3.b = x1.b;
 
set ndb_join_pushdown=off;
select straight_join count(*) from t1 as x1 
  join t1 as x2 on x2.b = x1.a
  left join t1 as x3 on x3.b = x1.b;
set ndb_join_pushdown=on;
select straight_join count(*) from t1 as x1 
  join t1 as x2 on x2.b = x1.a
  left join t1 as x3 on x3.b = x1.b;

##############
# If we have a bushy lookup, with scandescendants depending on these lookups,
# the query is 'scan-bushy' through these lookups.
#
# Bushy execution is expected for these scans (x2 & x4) wrt. root (x1)
#
explain
select straight_join count(*) from t1 as x1
  join t1 as x2 on x2.b = x1.a
  join t1 as x3 on x3.pk = x1.a join t1 as x4 on x4.b = x3.a;

set ndb_join_pushdown=off;
select straight_join count(*) from t1 as x1
  join t1 as x2 on x2.b = x1.a
  join t1 as x3 on x3.pk = x1.a join t1 as x4 on x4.b = x3.a;
set ndb_join_pushdown=on;
select straight_join count(*) from t1 as x1
  join t1 as x2 on x2.b = x1.a
  join t1 as x3 on x3.pk = x1.a join t1 as x4 on x4.b = x3.a;

#############
# Test bushy lookups + 1scan, 
# (Regression test for previous commit: http://lists.mysql.com/commits/117571)
# Repeatable child rangescan with same parent should be allowed to be in 
# 'm_iterState != Iter_finished' if the child row didn't exist (outer join):

explain select straight_join count(*) from t1 as x1
  left join t1 as x3 on x3.b = x1.a
  join t1 as x2 on x2.pk = x1.a;
select straight_join count(*) from t1 as x1 
  left join t1 as x3 on x3.b = x1.a
  join t1 as x2 on x2.pk = x1.a;

# Modify rows to force null rows from outer join
update t1 set b=b+10;

select straight_join count(*) from t1 as x1 
  left join t1 as x3 on x3.b = x1.a
  join t1 as x2 on x2.pk = x1.a;

#Undo update
update t1 set b=b-10;

##############
# Testcase for: http://lists.mysql.com/commits/118917
# There used to be a bug in SPJ API resulthandling of incomplete
# child batches where we tested for incomplete fetch for any 
# childs (in a bushy scan) instead if this particular child batch
# being incomplete.
#
# In this testcase (x inner join y) will have incomplete
# childbatches, while (x left join z) will be complete. 

# Modify rows to force null rows from lookup(z) below
update t1 set u=u+100;

set ndb_join_pushdown=on;

--error 0,1193
set global debug='+d,max_64rows_in_spj_batches';

explain select straight_join count(*) from
  (t1 as x join t1 as y on y.b = x.a)
 left outer join t1 as z on z.u = x.a;
select straight_join count(*) from 
  (t1 as x join t1 as y on y.b = x.a)
 left outer join t1 as z on z.u = x.a;

--error 0,1193
set global debug=@save_debug;

#Undo update
update t1 set u=u-100;

#############
# Additional testcase added as part of WL introducing equi-join
# non-matching row elimination optimizations into the SPJ block
# Test query topology which lacked coverage. (Found to fail
# in manual or RQG testing)

# Modify rows to force null rows from outer joined lookup below
update t1 set a=a+pk;

set ndb_join_pushdown=on;
explain
select straight_join count(*) from t1 as x
  left join t1 as y on y.pk = x.a
  join t1 as z on z.b = x.b;
--sorted_result
select straight_join count(*) from t1 as x
  left join t1 as y on y.pk = x.a
  join t1 as z on z.b = x.b;

explain
select straight_join count(*) from t1 as x
  left join t1 as y1 on y1.pk = x.a
  left join t1 as y2 on y2.pk = x.a
  join t1 as z on z.b = x.b;
--sorted_result
select straight_join count(*) from t1 as x
  left join t1 as y1 on y1.pk = x.a
  left join t1 as y2 on y2.pk = x.a
  join t1 as z on z.b = x.b;

explain
select straight_join count(*) from t1 as x
  join t1 as y1 on y1.pk = x.a
  left join t1 as y2 on y2.pk = x.a
  join t1 as z on z.b = x.b;
--sorted_result
select straight_join count(*) from t1 as x
  join t1 as y1 on y1.pk = x.a
  left join t1 as y2 on y2.pk = x.a
  join t1 as z on z.b = x.b;

explain
select straight_join count(*) from t1 as x
  left join t1 as y1 on y1.pk = x.a
  join t1 as y2 on y2.pk = x.a
  join t1 as z on z.b = x.b;
--sorted_result
select straight_join count(*) from t1 as x
  left join t1 as y1 on y1.pk = x.a
  join t1 as y2 on y2.pk = x.a
  join t1 as z on z.b = x.b;

explain
select straight_join count(*) from t1 as x
  left join (t1 as y join t1 as yy on yy.pk=y.u) on y.pk = x.a
  join t1 as z on z.b = x.b;
--sorted_result
select straight_join count(*) from t1 as x
  left join (t1 as y join t1 as yy on yy.pk=y.u) on y.pk = x.a
  join t1 as z on z.b = x.b;

explain
select straight_join count(*) from t1 as x
  join t1 as z on z.b = x.b
  left join t1 as y on y.pk = x.a;
--sorted_result
select straight_join count(*) from t1 as x
  join t1 as z on z.b = x.b
  left join t1 as y on y.pk = x.a;

explain
select straight_join count(*) from t1 as x
  left join t1 as y1 on y1.pk = x.a
  left join t1 as y2 on y2.pk = x.a
  left join t1 as y3 on y3.pk = x.a;
--sorted_result
select straight_join count(*) from t1 as x
  left join t1 as y1 on y1.pk = x.a
  left join t1 as y2 on y2.pk = x.a
  left join t1 as y3 on y3.pk = x.a;

explain
select straight_join count(*) from t1 as x
  left join t1 as y on y.pk = x.a
  left join t1 as z on z.pk = x.a
    left join t1 as y1 on y1.pk = y.b;
--sorted_result
select straight_join count(*) from t1 as x
  left join t1 as y on y.pk = x.a
  left join t1 as z on z.pk = x.a
    left join t1 as y1 on y1.pk = y.b;

explain
select straight_join count(*) from t1 as x
  join t1 as y on y.pk = x.a
    left join t1 as y1 on y1.pk = y.b
  left join t1 as z on z.pk = x.b;
--sorted_result
select straight_join count(*) from t1 as x
  join t1 as y on y.pk = x.a
    left join t1 as y1 on y1.pk = y.b
  left join t1 as z on z.pk = x.b;

explain
select straight_join count(*) from t1 as x
  left join t1 as y1 on y1.pk = x.a
  join t1 as y2 on y2.pk = x.b
    join t1 as z on z.b = y2.b;
--sorted_result
select straight_join count(*) from t1 as x
  left join t1 as y1 on y1.pk = x.a
  join t1 as y2 on y2.pk = x.b
    join t1 as z on z.b = y2.b;

#undo update
update t1 set a=a-pk;

##############

connection ddl;
drop index ix2 on t1;
create unique index ix2 on t1(a,u);

connection spj;
set ndb_join_pushdown=on;
explain
select straight_join * from
t1 as table1 join 
 (t1 as table2 join t1 as table3 on table3.a = table2.a)
 on table3.u = table1.u
 where table2.pk = 3;

--sorted_result
select straight_join * from
t1 as table1 join 
 (t1 as table2 join t1 as table3 on table3.a = table2.a)
 on table3.u = table1.u
 where table2.pk = 3;


##############
connection ddl;
drop table t1;

##############
# Test that branches of a bushy scan are correctly reset.

connection ddl;
CREATE TABLE t1 (
  a int NOT NULL,
  b int NOT NULL,
  c int NOT NULL,
  d int NOT NULL,
  PRIMARY KEY (`a`,`b`)
) ENGINE=ndbcluster
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
insert into t1 values (1,1,1,1), (1,2,1,1), (1,3,1,1), (1,4,1,2);

connection ddl;
CREATE TABLE t2 (
  a int NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=ndbcluster
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

CREATE TABLE t3 (
  a int NOT NULL,
  b int NOT NULL,
  PRIMARY KEY (`a`,`b`)
) ENGINE=ndbcluster
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection ddl;
insert into t2 values (0), (1), (2), (3), (4), (5), (6), (7), (8), (9);

connection spj;
# Make t3 so big that it takes multiple batches to scan it.
insert into t3 select 1, x1.a * 10+x2.a from t2 as x1 cross join t2 as x2;

--error 0,1193
set global debug='+d,max_64rows_in_spj_batches';

explain select straight_join count(*) from t1 as x0  
   join t3 as x1 on x1.a=x0.c
   join t1 as x2 on x2.a=x0.d
   join t3 as x3 on x3.a=x2.c
   join t1 as x4 on x4.a=x0.d and x4.b=x3.b;

select straight_join count(*) from t1 as x0  
   join t3 as x1 on x1.a=x0.c
   join t1 as x2 on x2.a=x0.d
   join t3 as x3 on x3.a=x2.c
   join t1 as x4 on x4.a=x0.d and x4.b=x3.b;

# If the first batch of an index scan has low parallelism and returns few rows,
# there is a mechanism that will try to query the remaining fragments within
# the same batch. This is done in order to avoid repeating other branches of 
# a bushy scan whenever possible. This is a test of that mechanism. Scan
# of x2 should return only one row. Therefore we should be able to fetch
# x2 in one batch and scan x3 only once.

let $scan_rows = query_get_value(select sum(val) as Value from ndbinfo.counters where block_name='DBSPJ' and counter_name='LOCAL_RANGE_SCANS_SENT', Value, 1);

--replace_column 10 # 11 #
explain select straight_join count(*) from t1 as x1  
   join t1 as x2 on x1.c=x2.a and x2.d=2
   join t3 as x3 on x1.d=x3.a;  

select straight_join count(*) from t1 as x1  
   join t1 as x2 on x1.c=x2.a and x2.d=2
   join t3 as x3 on x1.d=x3.a;

#####
# Bug #18175080 
# INCORRECT 'LOCAL_RANGE_SCAN' REPORTED FROM NDB_JOIN_PUSHDOWN_*.TEST
#
# Reported local_range_scan was not deterministic, so had
# to disable this subtest.
#
#--disable_query_log
#--eval select sum(val) - $scan_rows as Local_range_scans from ndbinfo.counters where block_name='DBSPJ' and counter_name='LOCAL_RANGE_SCANS_SENT';
#--enable_query_log

--error 0,1193
set global debug=@save_debug;

connection ddl;
drop table t1;
drop table t2;
drop table t3;

#############################################
# Test pruned index scan:
connection ddl;
create table t1(
       d int not null,
       e int     null,
       f int     null,
       a int not null,
       b int not null,
       c int not null,
       primary key (a,b,c))
engine = ndb partition by key (b) partitions 8;

connection spj;
insert into t1(a,b,c,d,e,f) values
 (1, 2, 3, 1, 2, 3),
 (1, 2, 4, 1, 2, 3),
 (2, 3, 4, 1, 2, 3),
 (3, 4, 5, 1, 2, 3),
 (4, 5, 6, 1, 2, 3),
 (5, 6, 7, 1, 2, 3),
 (6, 7, 8, 1, 2, 3),
 (7, 8, 9, 1, 2, 3);

# Find the total number of pruned range scans so far
let $pruned_range = query_get_value(select sum(val) as Value from ndbinfo.counters where block_name='DBSPJ' and counter_name='PRUNED_RANGE_SCANS_RECEIVED', Value, 1);

let $const_pruned_range = query_get_value(select sum(val) as Value from ndbinfo.counters where block_name='DBSPJ' and counter_name='CONST_PRUNED_RANGE_SCANS_RECEIVED', Value, 1);


set ndb_join_pushdown=on;

--replace_column 10 # 11 #
explain
select straight_join * from t1 x, t1 y where y.a=x.d and y.b=x.e;

--sorted_result
select straight_join * from t1 x, t1 y where y.a=x.d and y.b=x.e;

connection ddl;
alter table t1 partition by key (a) partitions 8;

connection spj;
--sorted_result
select straight_join * from t1 x, t1 y where y.a=x.d and y.b=x.e;

connection ddl;
alter table t1 partition by key (a,b) partitions 8;

connection spj;
--sorted_result
select straight_join * from t1 x, t1 y where y.a=x.d and y.b=x.e;

connection ddl;
alter table t1 partition by key (b,a) partitions 8;

connection spj;
--sorted_result
select straight_join * from t1 x, t1 y where y.a=x.d and y.b=x.e;

#########
# const pruned testcase 
#########
connection ddl;
alter table t1 partition by key (b) partitions 8;

connection spj;
--sorted_result
select straight_join * from t1 x, t1 y where y.a=x.d and y.b=2;

connection ddl;
alter table t1 partition by key (a) partitions 8;

connection spj;
--sorted_result
select straight_join * from t1 x, t1 y where y.a=1 and y.b=x.e;
select straight_join * from t1 x, t1 y where y.a=0 and y.b=x.e;

# Non-const pruned as both partition keys are not const
connection ddl;
alter table t1 partition by key (a,b) partitions 8;

connection spj;
--sorted_result
select straight_join * from t1 x, t1 y where y.a=1 and y.b=x.e;
--sorted_result
select straight_join * from t1 x, t1 y where y.a=x.d and y.b=2;

##########
# Test pruned scan using an index:
# Declaring PK as 'using hash' will prevent that PK is used as index
# Declare PK / ix1 with mismatching column order will test correct
# usage of NdbRecord::distkey_indexes[]
##########
connection ddl;
alter table t1 drop primary key, add primary key using hash (d,b,a,c);
alter table t1 partition by key (b) partitions 8;
create index ix1 on t1(b,d,a);

connection spj;
--replace_column 10 # 11 #
explain
select straight_join * from t1 x, t1 y where y.a=x.d and y.b=x.e;

###########
# Partition keys may evaluate to null-values:
###########
insert into t1(a,b,c,d,e,f) values
 (8, 9, 0, 1,  null, 3),
 (9, 9, 0, 1,  2,    null);

connection ddl;
alter table t1 partition by key (b) partitions 8;

connection spj;
--sorted_result
select straight_join * from t1 x, t1 y where y.a=x.d and y.b=x.e;

# Verify pruned execution by comparing the NDB$INFO counters
--disable_query_log
--eval select sum(val) - $pruned_range as pruned from ndbinfo.counters where block_name='DBSPJ' and counter_name='PRUNED_RANGE_SCANS_RECEIVED'
--eval select sum(val) - $const_pruned_range as const_pruned from ndbinfo.counters where block_name='DBSPJ' and counter_name='CONST_PRUNED_RANGE_SCANS_RECEIVED'
--enable_query_log

connection ddl;
drop table t1;

###
# Sorted scan with sub scan used to not being pushable.
# However since v7.2.6(?) we support this by enforcing 
# batchRowSize=1 for the parent table in the scan-scan.
# (At the cost of more roundrtrips)
###
connection ddl;
create table t1 (pk int primary key, a int, b int) engine=ndb
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
create index ix1 on t1(b,a);

connection spj;
insert into t1 values (0,10,10);
insert into t1 values (1,10,20);
insert into t1 values (2,20,20);
insert into t1 values (3,10,10);
insert into t1 values (4,10,20);
insert into t1 values (5,10,20);
insert into t1 values (6,10,10);
insert into t1 values (7,10,10);
insert into t1 values (8,10,20);
insert into t1 values (9,10,10);


# This sorted scan-scan is pushed since V7.2.6
explain select x1.pk,x1.a,x1.b from t1 as x1
   join t1 as x2 on x1.a=x2.b 
   join t1 as x3 on x2.a=x3.b 
   order by x1.pk limit 70;
select x1.pk,x1.a,x1.b from t1 as x1 
   join t1 as x2 on x1.a=x2.b 
   join t1 as x3 on x2.a=x3.b 
   order by x1.pk limit 70;

# This query should not be pushed, since mysqld requires sorted
# results for the root scan.
explain select * from t1 as x1, t1 as x2 where x1.a=x2.b and x1.b = 3;
select * from t1 as x1, t1 as x2 where x1.a=x2.b and x1.b = 3;

connection ddl;
drop table t1;

########
# Test correct cleanup of MRR accesses being executed multiple times.
# This used to be bug#57481, and this is a SPJ specific testcase in addition to
# the specific testcase commited together with patch for this bug.
#######
connection ddl;
create table t (pk int primary key, a int) engine=ndb
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
insert into t values 
  (1,1), (2,1),
  (4,3), (6,3),
  (7,4), (8,4);

connection spj;
--replace_column 10 # 11 #
explain
select distinct straight_join table1.pk FROM 
   t as table1  join
    (t as table2  join  
       (t as table3  join t as table4 on table3.pk = table4.a)
     on table2.pk =  table3.pk )
   on table1.a =  table4.pk
   where  table2.pk != 6;

--sorted_result
select distinct straight_join table1.pk FROM 
   t as table1  join
    (t as table2  join  
       (t as table3  join t as table4 on table3.pk = table4.a)
     on table2.pk =  table3.pk )
   on table1.a =  table4.pk
   where  table2.pk != 6;

connection ddl;
drop table t;

########
# SPJ variant of bug#57396
# Test correct format of an 'open bound'
########
connection ddl;
create table t (b int, a int, primary key (a,b)) engine=ndb
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
insert into t values(0,0);

--replace_column 10 # 11 #
explain
select * from t as t1 join t as t2 on t2.a=t1.a where t1.a < 8 or t1.a >= 8;
select * from t as t1 join t as t2 on t2.a=t1.a where t1.a < 8 or t1.a >= 8;

connection ddl;
drop table t;

#######
# Testcase for bug introduced by initial fix for 
# bug#57601 'Optimizer is overly eager to request ordered access.'
# When we turned of 'sorted' for 'descending', we broke QUICK_SELECT_DESC
# which required result to be read as an ordered index access
# Note: Before V7.2.6, and as a result of WL5558, such 'turn of sorting'
# is not expected to happen any more.
#######
connection ddl;
create table t (pk1 int, pk2 int, primary key(pk1,pk2)) engine = ndb
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
insert into t values (1,3), (3,6), (6,9), (9,1);

--replace_column 10 # 11 #
explain
select * from t as t1 join t as t2
  on t1.pk2 = t2.pk1 
  where t1.pk1 != 6
  order by t1.pk1 DESC;

select * from t as t1 join t as t2
  on t1.pk2 = t2.pk1 
  where t1.pk1 != 6
  order by t1.pk1 DESC;

connection ddl;
drop table t;

#######
# Testcase using 'REF_OR_NULL'
# 'ref_or_null' contains elements of left outer join wo/ being identical.
# 
connection ddl;
create table t (k int, uq int, unique key ix1 (uq)) engine = ndb
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
insert into t values (1,3), (3,NULL), (6,9), (9,1);

# Currently we do not handle 'ref_or_null' correctly.
# It is therefore disabled as pushable
explain
select straight_join * from t as a join t as b 
  on a.uq=b.uq or b.uq is null;

--sorted_result
select straight_join * from t as a join t as b 
  on a.uq=b.uq or b.uq is null;

connection ddl;
drop table t;

########
# JT_SYSTEM testcase, 'a.k is null' is known 'false' ->
# Join condition will always fail, and all 'left joins' can be NULL complemented wo/
# even requiring to access left table (b) which becomes 'system' -> No pushed joins !
########
connection ddl;
create table t (k int primary key, uq int) engine = ndb
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
insert into t values (1,3), (3,NULL), (6,9), (9,1);

explain
select * from t as a left join t as b 
  on a.k is null and a.uq=b.uq;

--sorted_result
select * from t as a left join t as b 
  on a.k is null and a.uq=b.uq;

connection ddl;
drop table t;

#######
# Test of varchar query parameteres.
#######

connection ddl;
create table tc(
  a varchar(10) not null,
  b varchar(10),
  c varchar(10),
  primary key (a),
  unique key uk1 (b, c)
) engine=ndbcluster
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
insert into tc values ('aa','bb', 'x'), ('bb','cc', 'x'), ('cc', 'dd', 'x');

explain select * from tc as x1
  right outer join tc as x2 on x1.b=x2.a   
  left outer join tc as x3 on x2.b = x3.b and x1.c=x3.c;

--sorted_result
select * from tc as x1 
  right outer join tc as x2 on x1.b=x2.a   
  left outer join tc as x3 on x2.b = x3.b and x1.c=x3.c;

####
# Test that 'select ... for update' is not pushed, since this requires locking.
####

explain select * from tc as x1, tc as x2 where x1.b=x2.a for update;

explain select * from tc as x1, tc as x2 where x1.b=x2.a;

connection ddl;
drop table tc;

###
# prune with xfrm set incorrect keylen
#
connection ddl;
create table t1 (
  a varchar(16) not null,
  b int not null,
  c varchar(16) not null,
  d int not null,
  primary key (a,b)
) engine ndb
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM"
  partition by key (a);

connection spj;
insert into t1  values ('aaa', 1, 'aaa', 1);
select * from t1 as q1, t1 as q2 where q1.a = 'aaa' and q1.c=q2.a;

connection ddl;
drop table t1;

########################
# Bug#17845161  'CORRUPT KEY IN TC, UNABLE TO XFRM'
# 
# Created an incorrect lookup key when a varchar()
# in the key was NULL

connection ddl;
CREATE TABLE t1 (
  id int NOT NULL AUTO_INCREMENT,
  t2_id int,
  PRIMARY KEY (id)
) ENGINE=ndbcluster DEFAULT CHARSET=latin1
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

CREATE TABLE t2 (
  id int NOT NULL AUTO_INCREMENT,
  t3_id varchar(20) DEFAULT NULL,
  PRIMARY KEY (id)
) ENGINE=ndbcluster DEFAULT CHARSET=latin1
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

CREATE TABLE t3 (
  id varchar(20) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=ndbcluster DEFAULT CHARSET=latin1
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

CREATE TABLE t4 (
  pk int NOT NULL,
  id varchar(20) NOT NULL,
  PRIMARY KEY (pk),
  UNIQUE KEY (`id`)
) ENGINE=ndbcluster DEFAULT CHARSET=latin1
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";


connection spj;
INSERT INTO t1 VALUES (20, NULL);
INSERT INTO t1 VALUES (23, 24);

INSERT INTO t2 VALUES (24, NULL);

EXPLAIN
SELECT *
  FROM t1
       INNER JOIN t2 ON t2.id = t1.t2_id
       LEFT OUTER JOIN t3 ON t3.id = t2.t3_id
 WHERE t1.id = 20;
SELECT *
  FROM t1
       INNER JOIN t2 ON t2.id = t1.t2_id
       LEFT OUTER JOIN t3 ON t3.id = t2.t3_id
 WHERE t1.id = 20;


EXPLAIN
SELECT *
  FROM t1
       INNER JOIN t2 ON t2.id = t1.t2_id
       LEFT OUTER JOIN t3 ON t3.id = t2.t3_id
 WHERE t1.id = 23;
SELECT *
  FROM t1
       INNER JOIN t2 ON t2.id = t1.t2_id
       LEFT OUTER JOIN t3 ON t3.id = t2.t3_id
 WHERE t1.id = 23;

EXPLAIN
SELECT *
  FROM t1
       INNER JOIN t2 ON t2.id = t1.t2_id
       LEFT OUTER JOIN t4 ON t4.id = t2.t3_id
 WHERE t1.id = 23;
SELECT *
  FROM t1
       INNER JOIN t2 ON t2.id = t1.t2_id
       LEFT OUTER JOIN t4 ON t4.id = t2.t3_id
 WHERE t1.id = 23;


EXPLAIN
SELECT *
  FROM t1
       LEFT OUTER JOIN t2 ON t2.id = t1.t2_id
       LEFT OUTER JOIN t2 as t3 ON t3.id = t1.t2_id;
--sorted_result
SELECT *
  FROM t1
       LEFT OUTER JOIN t2 ON t2.id = t1.t2_id
       LEFT OUTER JOIN t2 as t3 ON t3.id = t1.t2_id;

connection ddl;
DROP TABLE t1,t2,t3,t4;

#######################################
# Some tests for nested left joins.

connection ddl;
CREATE TABLE t1 (
  a int NOT NULL,
  b int NOT NULL,
  c int NOT NULL,
  d int,
  PRIMARY KEY (`a`,`b`),
  unique key(c)
) ENGINE=ndbcluster
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
insert into t1 values
(1,1,1,1), 
(1,2,2,1), 
(1,3,3,1), 
(1,4,4,1), 
(1,5,5,2), 
(1,6,6,2), 
(1,7,7,2), 
(1,8,8,2);

explain select count(*) from t1 as x1
   join (t1 as x2 
      left join (t1 as x3 
         cross join t1 as x4) 
      on x2.d=x3.a) 
   on x2.c is null or x1.a=x4.d;
select count(*) from t1 as x1 
   join (t1 as x2 
      left join (t1 as x3 
         cross join t1 as x4) 
      on x2.d=x3.a) 
   on x2.c is null or x1.a=x4.d;

explain select count(*) from t1 as x1
   left join (t1 as x2 
      cross join t1 as x3) 
   on x1.d=x2.a;
select count(*) from t1 as x1 
   left join (t1 as x2 
      cross join t1 as x3) 
   on x1.d=x2.a;


explain select count(*) from t1 as x0
   left join (t1 as x1 
      join (t1 as x2 
         left join (t1 as x3 
            join t1 as x4 on x3.d=x4.a) 
         on x2.d=x3.a) 
      on x2.c is null or x1.a=x4.d) 
   on x0.d=x1.a;
select count(*) from t1 as x0 
   left join (t1 as x1 
      join (t1 as x2 
         left join (t1 as x3 
            join t1 as x4 on x3.d=x4.a) 
         on x2.d=x3.a) 
      on x2.c is null or x1.a=x4.d) 
   on x0.d=x1.a;

connection ddl;
drop table t1;

## Test scan sorted on string field.
connection ddl;
create table t1 (pk char(10) primary key, u int not null) engine=ndb
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
create table t2 (pk int primary key, u int not null) engine=ndb
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
insert into t1 values ('wh',1);
insert into t1 values ('ik',2);
insert into t1 values ('cu',3);
insert into t1 values ('pw',4);
insert into t1 values ('cq',4);

insert into t2 values (1,2), (2,3), (3,4), (4,5);

explain select * from t1 join t2 on t1.u = t2.pk order by t1.pk;
select * from t1 join t2 on t1.u = t2.pk order by t1.pk;

connection ddl;
drop table t1;
drop table t2;

########################################
# Test query with very long records.
connection ddl;
create table t1 (
  a char(10) primary key,
  b char(10) not null,
  c char(10) not null,
  l00 char(255) not null,
  l01 char(255) not null,
  l02 char(255) not null,
  l03 char(255) not null,
  l04 char(255) not null,
  l05 char(255) not null,
  l06 char(255) not null,
  l07 char(255) not null,
  l08 char(255) not null,
  l09 char(255) not null,
  l10 char(255) not null,
  l11 char(255) not null,
  l12 char(255) not null,
  l13 char(255) not null,
  l14 char(255) not null,
  l15 char(255) not null,
  l16 char(255) not null,
  l17 char(255) not null,
  l18 char(255) not null,
  l19 char(255) not null,
  l20 char(255) not null,
  l21 char(255) not null,
  l22 char(255) not null,
  l23 char(255) not null,
  l24 char(255) not null,
  l25 char(255) not null,
  l26 char(255) not null,
  l27 char(255) not null,
  l28 char(255) not null,
  l29 char(255) not null,
  l30 char(255) not null,
  l31 char(255) not null,
  index(c, b)
) engine=ndb character set latin1 partition by key(a) partitions 8;

connection spj;

insert into t1 values ('a','a','a','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x');

insert into t1 values ('b','b','b','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x');

insert into t1 values ('c','c','c','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x','x');

--replace_column 10 # 11 #
explain select count(*) from t1 as x1 join t1 as x2 on x1.b = x2.c;

select count(*) from t1 as x1 join t1 as x2 on x1.b = x2.c;

connection ddl;
drop table t1;

####################
# Test pruned child scans using parameter values (known regression).
####################
create table t1 
       (a int not null,
       b int not null, 
       c int not null,
       d int not null,
       primary key(a,b,c,d)) engine=ndb
       comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM"
       partition by key (b,c);

connection spj;
insert into t1 values (0x4f, 0x4f, 0x4f, 0x4f);

# Prune key depends on parent row.
--replace_column 10 # 11 #
explain select * from t1 as x1 
	join t1 as x2 on x1.c=0x4f and x2.a=0+x1.b and x2.b=x1.b 
	join t1 as x3 on x3.a=x2.d and x3.b=x1.d and x3.c=x2.c;

select * from t1 as x1 
	join t1 as x2 on x1.c=0x4f and x2.a=0+x1.b and x2.b=x1.b 
	join t1 as x3 on x3.a=x2.c and x3.b=x1.d and x3.c=x2.c;

# Prune key is fixed.
--replace_column 10 # 11 #
explain select * from t1 as x1 
	join t1 as x2 on x1.c=0x4f and x2.a=0+x1.b and x2.b=x1.b 
	join t1 as x3 on x3.a=x2.d and x3.b=x1.d and x3.c=0x4f;

select * from t1 as x1 
	join t1 as x2 on x1.c=0x4f and x2.a=0+x1.b and x2.b=x1.b 
	join t1 as x3 on x3.a=x2.c and x3.b=x1.d and x3.c=0x4f;

connection ddl;
drop table t1;

############################################################
# Bug#13990924 / Bug#64865 Large WHERE IN with SPJ leads to
#                          cluster shutdown
#
# Caused by incorrect error handling in SPJ block when
# there was a buffer overflow (>32K)
# (Pushed condition became to large)
#
# API also failed to prevent this from happen as it checked
# for size <= 64K, while actuall buffer size was 32K
# (Has now been increased)
############################################################
create table t1 (
  k1 int primary key,
  i int,
  name varchar(32),
  key (name)
)
default charset = utf8
engine = ndb
comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

insert into t1 values (1, 1, 'Ole');
insert into t1 values (2, 2, 'Dole');
insert into t1 values (3, 3, 'Doffen');
insert into t1 values (4, 4, 'row# 999');

# These queries with large IN clauses exceeds the default memory limit of the
# range optimizer on 64 bit but not on 32 bit, thus cauing different result.
# Fix by setting a fairly low value in order to get consistent behaviour
set @save_range_opt_max = @@session.range_optimizer_max_mem_size;
set range_optimizer_max_mem_size = 64*1024;

echo ------------------------------------------------------------------;
echo;
# Build SPJ query with 2000 IN values
# This used to result in buffer overflow, but will now
# pass as buffer size is incresed from 32K -> 64K
#
let $query = select * from t1 x, t1 y where y.k1=x.i and x.name in ('foo';
let $values = 0;
while ($values < 2000)
{
  let $query = $query , 'row# $values';
  inc $values;
}
let $query = $query );

eval explain $query;
eval $query;

echo ------------------------------------------------------------------;
echo;
# Same SPJ query with 4000 IN values
# This will hit the actuall limit of 64K and will be
# detected by API which will drop this pushed condition.
#   'Warning 4294 Scan filter is too large, discarded'
# (Pushed condition evaluated by ha_ndbcluster handler instead)
#
let $query = select * from t1 x, t1 y where y.k1=x.i and x.name in ('foo';
let $values = 0;
while ($values < 4000)
{
  let $query = $query , 'row# $values';
  inc $values;
}
let $query = $query );

eval explain $query;
eval $query;

echo ------------------------------------------------------------------;
echo;
# Build single table (non-SPJ) query with 4000 IN values
# Should give same 'Warning 4294' as above
let $query = select * from t1 x where x.name in ('foo';
let $values = 0;
while ($values < 4000)
{
  let $query = $query , 'row# $values';
  inc $values;
}
let $query = $query );

eval explain $query;
eval $query;


############################################################
# Bug #27397802 ENTIRE CLUSTER CRASHES WITH "MESSAGE TO BIG
#               IN SENDSIGNAL" WITH BIG IN CLAUSE.
#
# Similar problem as Bug#13990924; This time with a pushed
# condition as part of an EQ_REF child node
#
############################################################

insert into t1 values (5, 4, 'row# 1');
insert into t1 values (6, 4, 'row# 2');
insert into t1 values (7, 4, 'row# 3');
insert into t1 values (8, 4, 'row# 4');

echo ------------------------------------------------------------------;
echo;
# A 'small' 1000 valued IN-list is handled OK
#
let $query = select straight_join * from t1 x, t1 y where y.k1=x.i and y.name in ('foo';
let $values = 0;
while ($values < 1000)
{
  let $query = $query , 'row# $values';
  inc $values;
}
let $query = $query ) order by x.k1;

eval explain $query;
eval $query;

echo ------------------------------------------------------------------;
echo;
# Also a sufficient HUGE IN value list is ok as the pushed
# condition code is > 64K and catched by other 'Too large'
# checks.
#
let $query = select straight_join * from t1 x, t1 y where y.k1=x.i and y.name in ('foo';
let $values = 0;
while ($values < 4000)
{
  let $query = $query , 'row# $values';
  inc $values;
}
let $query = $query ) order by x.k1;

eval explain $query;
eval $query;


echo ------------------------------------------------------------------;
echo;
# A pushed condition with size between 32K - 64K was not not 'Too large',
# but would require fragmented sending in order to work -> Crashed.
#
let $query = select straight_join * from t1 x, t1 y where y.k1=x.i and y.name in ('foo';
let $values = 0;
while ($values < 2000)
{
  let $query = $query , 'row# $values';
  inc $values;
}
let $query = $query ) order by x.k1;

eval explain $query;
eval $query;

set range_optimizer_max_mem_size = @save_range_opt_max;

drop table t1;

############################################################
# Bug#14010406 LARGE PUSHED JOIN HIT ASSERT IN SPJ BLOCK
#
# Buffered rows (caused by PARENT refs) was prematurely
# released when there was no TN_ACTIVE treeNodes childs.
# However, in bushy scans, a scan branch can be 'repeated'
# even if its previous execution was 'complete'. 
#
# Thus we have to use a less eager release strategy where
# we don't release any buffered rows until we prepare for
# a NEXTREQ which will fetch more rows into the treeNode.
############################################################

create table t(
  pk int primary key auto_increment,
  i int, 
  j int,
  k int,
  index(i,j),
  index(i),
  index(j),
  index(k)
) engine = ndb
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

insert into t(i,j,k) values
   (1,1,1), (1,1,1), (1,1,1),
   (2,2,2), (2,2,2), (2,2,2);

--error 0,1193
set global debug='+d,max_4rows_in_spj_batches';

--replace_column 10 # 11 #
explain
select straight_join count(*) from 
  t as t1
  join t as t2 on t2.i = t1.i
  join (t as t3 join t as t4 on t4.k=t3.k join t as t5 on t5.i=t4.i and t5.j=t3.j) on t3.pk=t1.j
  join t as t6 on t6.k = t1.k
  where t1.i < 2;

select straight_join count(*) from 
  t as t1
  join t as t2 on t2.i = t1.i
  join (t as t3 join t as t4 on t4.k=t3.k join t as t5 on t5.i=t4.i and t5.j=t3.j) on t3.pk=t1.j
  join t as t6 on t6.k = t1.k
  where t1.i < 2;

--error 0,1193
set global debug=@save_debug;

drop table t;

####################################################
# Test LQH handling of zero-length key in LQHKEYREQ
# Bug#
create table t1 (a int primary key, b int, c int, index(b,c)) engine = ndb
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
insert into t1 values (4,null, 2);

explain
select x.a from t1 as x join t1 as y on y.a = x.b where x.a=4;
select x.a from t1 as x join t1 as y on y.a = x.b where x.a=4;

drop table t1; 

####################################################
# Regression test for Bug#14644936 "INEFFICIENT AND INCORRECT EXECUTION OF 
# PUSHABLE OUTER JOINS".
####################################################

connection ddl;
CREATE TABLE t1 (
  a int NOT NULL,
  b int DEFAULT NULL,
  c int NOT NULL,
  d int NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=ndbcluster
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

create unique index ix1 on t1(b,c) using hash;

connection spj;

insert into t1 values (1,NULL,1,1);


explain select * from t1 as x1 left join (t1 as x2 join t1 as x3 on x2.d=x3.c) on x1.b=x3.b;

# Record value before query.
create temporary table scan_count
       select * from performance_schema.global_status 
       where variable_name = 'Ndb_scan_count';

# Query gives wrong result on mysql-trunk-cluster, and extra scan on both
# mysql-5.5-cluster-7.2 and mysql-trunk-cluster.
select * from t1 as x1 left join (t1 as x2 join t1 as x3 on x2.d=x3.c) on x1.b=x3.b; 

# Bug causes value to increase by 3 rather than 2.
select scan_count.VARIABLE_NAME,
       old.VARIABLE_VALUE - scan_count.VARIABLE_VALUE  
       from scan_count, performance_schema.global_status as old 
       where old.variable_name = 'Ndb_scan_count';

drop table scan_count;

connection ddl;
drop table t1;

##############
# Bug#16925513: HIT DBUG_ASSERT IN SPJ BLOCK, ~LINE 4406
#
# Testcase extracted from failing RQG test.

connection ddl;
CREATE TABLE table1 (
  col_int_unique int(11), 
  PRIMARY KEY (col_int_unique)
) ENGINE=ndbcluster
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

CREATE TABLE table3 (
  col_int int(11) NOT NULL DEFAULT '0',
  KEY (col_int)
) ENGINE=ndbcluster
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

CREATE TABLE table4 (
  col_int int(11) DEFAULT NULL,
  pk int(11) NOT NULL,
  PRIMARY KEY (pk)
) ENGINE=ndbcluster
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

CREATE TABLE table6 (
  col_int int(11) DEFAULT NULL,
  col_int_unique int(11) DEFAULT NULL
) ENGINE=ndbcluster
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
insert into table6 values
 (2,NULL), 
 (2,NULL), 
 (2,NULL), 
 (2,NULL), 
 (2,NULL), 
 (2,NULL), 
 (2,NULL), 
 (2,NULL);

--error 0,1193
set global debug='+d,max_4rows_in_spj_batches';

EXPLAIN
 SELECT * FROM
 table1 RIGHT JOIN 
   table3 LEFT JOIN table4 ON table3.col_int = table4.col_int 
     JOIN
   table6 ON table4.pk = table6.col_int_unique 
 ON table1.col_int_unique = table6.col_int;

--sorted_result
 SELECT * FROM
 table1 RIGHT JOIN 
   table3 LEFT JOIN table4 ON table3.col_int = table4.col_int 
     JOIN
   table6 ON table4.pk = table6.col_int_unique 
 ON table1.col_int_unique = table6.col_int;


--error 0,1193
set global debug=@save_debug;

connection ddl;
drop table table6;
drop table table4;
drop table table3;
drop table table1;


--echo 5.6 tests
#######################################
# MySQL 5.6 include improved optimizer
# handling of subqueries. Some specific
# test for handling limitations / problems
# specific to these features

#############################################
# Testing of subqueries and join pushdown
#
#  Note that subqueries has limitations very similar
#  to outer joins. 
#############################################

##############
# Bug #16664035
# PUSHED JOIN TOGETHER WITH 'FIRSTMATCH' STRATEGY MAY RETURN TOO MANY ROWS

connection ddl;
create table t1 (
  a int not null,
  b int not null,
  c int not null,
  d int not null,
  primary key (`a`,`b`),
  key(c), key(d)
) engine=ndbcluster
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

connection spj;
insert into t1 values
(1,1,1,1), (2,2,2,2), (3,3,3,3), (4,4,4,4),
(1,2,5,1), (1,3,1,2), (1,4,2,3),
(2,1,3,4), (2,3,4,5), (2,4,5,1),
(3,1,1,2), (3,2,2,3), (3,4,3,4),
(4,1,4,5), (4,2,5,1), (4,3,1,2);

# Use debug hook to enforce only 4 rows in each batch (Two roundtips)
--error 0,1193
set global debug='+d,max_4rows_in_spj_batches';

# Make use of FirstMatch more likely:
set @save_optimizer_switch = @@optimizer_switch;
set optimizer_switch='materialization=off';

# 'subq' should use 'ref, FirstMatch(t1)', preventing join pushdown
explain
select count(*) from t1 where 
  t1.c in (select c from t1 as subq);
select count(*) from t1 where
  t1.c in (select c from t1 as subq);

explain
select count(*) from t1 where 
  t1.c in (select c from t1 as subq1) and
  t1.d in (select d from t1 as subq2);
select count(*) from t1 where 
  t1.c in (select c from t1 as subq1) and
  t1.d in (select d from t1 as subq2);

explain
select count(*) from t1 where 
  t1.c in (select c from t1 as subq1 where 
    subq1.c in (select c from t1 as subq2));
select count(*) from t1 where 
  t1.c in (select c from t1 as subq1 where 
    subq1.c in (select c from t1 as subq2));

explain
select count(*) from t1 where 
  t1.c in (select subq1.c from t1 as subq1 straight_join t1 as subq2 on subq1.a = subq2.c);
select count(*) from t1 where 
  t1.c in (select subq1.c from t1 as subq1 straight_join t1 as subq2 on subq1.a = subq2.c);

set optimizer_switch=@save_optimizer_switch;

--error 0,1193
set global debug=@save_debug;


##################################################
# Bug##26926666
#   'LOOSESCAN' TABLES SHOULD NOT BE CHILD MEMBERS IN A PUSHED JOIN
#
##################################################

######
# Expect a query plan where the subquery table1s becomes
# a child candidate in a pushed join. 'LooseScan' require
# sorted results to be returned from the (child) table, which
# we do not implement.

connection spj;
set @save_optimizer_switch = @@optimizer_switch;
set @@optimizer_switch='semijoin=on';
set @@optimizer_switch='loosescan=on';

explain SELECT count(*)
  FROM (t1 AS table1 JOIN t1 AS table2 USING(a,b))
  WHERE table1.b IN
    (SELECT /*+ SEMIJOIN(LOOSESCAN)*/ table1s.c FROM t1 as table1s);
SELECT count(*)
  FROM (t1 AS table1 JOIN t1 AS table2 USING(a,b))
  WHERE table1.b IN
    (SELECT /*+ SEMIJOIN(LOOSESCAN)*/ table1s.c FROM t1 as table1s);

########
# Same query with 'FirstMatch', suplements FirstMatch (bug#16664035)
# testcase above

set @@optimizer_switch='firstmatch=on';

explain SELECT count(*)
  FROM (t1 AS table1 JOIN t1 AS table2 USING(a,b))
  WHERE table1.b IN
    (SELECT /*+ SEMIJOIN(FIRSTMATCH)*/ table1s.c FROM t1 as table1s);
SELECT count(*)
  FROM (t1 AS table1 JOIN t1 AS table2 USING(a,b))
  WHERE table1.b IN
    (SELECT /*+ SEMIJOIN(FIRSTMATCH)*/ table1s.c FROM t1 as table1s);

set optimizer_switch=@save_optimizer_switch;

##################################################
# Bug#26919289:
#    mysqld crash during join_pushdown analysis
#
# The mysql optimizer introduce syntetic JOIN_TAB
# elements representing where the materialized subq
# should be accessed in the query plan. As these
# are not representing 'real tables', their 'tableno'
# was not set up in ndb_pushed_builder_ctx C'tor.
# It effectively contained uninitialized data, which was
# then later used to access a 'tableno-bitmask'.
###################################################

connection spj;
set @save_optimizer_switch = @@optimizer_switch;
set @@optimizer_switch='semijoin=on';
set @@optimizer_switch='materialization=on';

explain
select count(*)
from t1
join t1 as t2 on t2.a = t1.b
where t1.c IN (select /*+ SEMIJOIN(MATERIALIZATION)*/ c from t1 as t3);

select count(*)
from t1
join t1 as t2 on t2.a = t1.b
where t1.c IN (select /*+ SEMIJOIN(MATERIALIZATION)*/ c from t1 as t3);


explain
select count(*)
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
where t1.c IN (select /*+ SEMIJOIN(MATERIALIZATION)*/ c from t1 as t3);

select count(*)
from t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
where t1.c IN (select /*+ SEMIJOIN(MATERIALIZATION)*/ c from t1 as t3);


explain
select count(*)
from (select * from t1 where t1.c IN (select /*+ SEMIJOIN(MATERIALIZATION)*/ c from t1 as t3)) as t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c;

select count(*)
from (select * from t1 where t1.c IN (select /*+ SEMIJOIN(MATERIALIZATION)*/ c from t1 as t3)) as t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c;


explain
select count(*)
from (select * from t1 where t1.c IN (select /*+ SEMIJOIN(MATERIALIZATION)*/ c from t1 as t3)) as t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
where t1.c IN (select /*+ SEMIJOIN(MATERIALIZATION)*/ c from t1 as t3);

select count(*)
from (select * from t1 where t1.c IN (select /*+ SEMIJOIN(MATERIALIZATION)*/ c from t1 as t3)) as t1
join t1 as t2 on t2.a = t1.b and t2.b = t1.c
where t1.c IN (select /*+ SEMIJOIN(MATERIALIZATION)*/ c from t1 as t3);


set optimizer_switch='block_nested_loop=off';

explain
select count(*)
from t1 cross join t1 as t2
where t2.c IN (select /*+ SEMIJOIN(MATERIALIZATION)*/ t4s.c
  from t1 as t3s join t1 as t4s on t4s.a = t3s.b);

select count(*)
from t1 cross join t1 as t2
where t2.c IN (select /*+ SEMIJOIN(MATERIALIZATION)*/ t4s.c
  from t1 as t3s join t1 as t4s on t4s.a = t3s.b);

set optimizer_switch=@save_optimizer_switch;


###################################################
# Bug #26984919
#   SPJ: 'DUPLICATE WEEDOUT' STRATEGY MAY RETURN INCORRECT RESULTS

###################################################

# Use debug hook to enforce only 4 rows in each batch (multiple roundtips)
--error 0,1193
set global debug='+d,max_4rows_in_spj_batches';

connection spj;
set @save_optimizer_switch = @@optimizer_switch;
set @@optimizer_switch='semijoin=on';

explain
select count(*)
from t1
join t1 as t2 on t2.a = t1.b
where t1.c IN (
  select /*+ SEMIJOIN(DUPSWEEDOUT)*/ t3s.c from t1 as t3s
  join t1 as t4s on t4s.a = t3s.b);

select count(*)
from t1
join t1 as t2 on t2.a = t1.b
where t1.c IN (
  select /*+ SEMIJOIN(DUPSWEEDOUT)*/ t3s.c from t1 as t3s
  join t1 as t4s on t4s.a = t3s.b);


explain
select count(*)
from t1
join t1 as t2 on t2.a = t1.b
where t2.c IN (
  select /*+ SEMIJOIN(DUPSWEEDOUT)*/ t3s.c from t1 as t3s
  join t1 as t4s on t4s.a = t3s.b);

select count(*)
from t1
join t1 as t2 on t2.a = t1.b
where t2.c IN (
  select /*+ SEMIJOIN(DUPSWEEDOUT)*/ t3s.c from t1 as t3s
  join t1 as t4s on t4s.a = t3s.b);


explain
select count(*)
from t1
join t1 as t2 on t2.a = t1.b
join t1 as t3 on t3.a = t2.b
where t1.c IN (
  select /*+ SEMIJOIN(DUPSWEEDOUT)*/ t4s.c from t1 as t4s
  join t1 as t5s on t5s.a = t4s.b
  join t1 as t6s on t6s.a = t5s.b
);

select count(*)
from t1
join t1 as t2 on t2.a = t1.b
join t1 as t3 on t3.a = t2.b
where t1.c IN (
  select /*+ SEMIJOIN(DUPSWEEDOUT)*/ t4s.c from t1 as t4s
  join t1 as t5s on t5s.a = t4s.b
  join t1 as t6s on t6s.a = t5s.b
);


explain
select count(*)
from t1
join t1 as t2 on t2.a = t1.b
join t1 as t3 on t3.a = t2.b
where t2.c IN (
  select /*+ SEMIJOIN(DUPSWEEDOUT)*/ t4s.c from t1 as t4s
  join t1 as t5s on t5s.a = t4s.b
  join t1 as t6s on t6s.a = t5s.b
);

select count(*)
from t1
join t1 as t2 on t2.a = t1.b
join t1 as t3 on t3.a = t2.b
where t2.c IN (
  select /*+ SEMIJOIN(DUPSWEEDOUT)*/ t4s.c from t1 as t4s
  join t1 as t5s on t5s.a = t4s.b
  join t1 as t6s on t6s.a = t5s.b
);


explain
select count(*)
from t1
join t1 as t2 on t2.a = t1.b
join t1 as t3 on t3.a = t2.b
where t3.c IN (
  select /*+ SEMIJOIN(DUPSWEEDOUT)*/ t4s.c from t1 as t4s
  join t1 as t5s on t5s.a = t4s.b
  join t1 as t6s on t6s.a = t5s.b
);

select count(*)
from t1
join t1 as t2 on t2.a = t1.b
join t1 as t3 on t3.a = t2.b
where t3.c IN (
  select /*+ SEMIJOIN(DUPSWEEDOUT)*/ t4s.c from t1 as t4s
  join t1 as t5s on t5s.a = t4s.b
  join t1 as t6s on t6s.a = t5s.b
);


--error 0,1193
set global debug=@save_debug;

set optimizer_switch=@save_optimizer_switch;

###################################################
# Bug#27022925
#   SPJ: 'MATERIALIZED' SEMI JOINS ARE NOT CONSIDDERED FOR JOIN PUSHOWN
#
#   Joins entirely inside a 'MATERIALIZED' semi join
#   not considdered for join pushdown.
#
# Test cases should demonstrate that the 't3s join t4s'
# subqueries can now be pushed down, and materialized.
#
###################################################
explain
select count(*)
from t1
join t1 as t2 on t2.a = t1.b
where t1.c IN (
  select /*+ SEMIJOIN(MATERIALIZATION)*/ t3s.c from t1 as t3s
  join t1 as t4s on t4s.a = t3s.b);

select count(*)
from t1
join t1 as t2 on t2.a = t1.b
where t1.c IN (
  select /*+ SEMIJOIN(MATERIALIZATION)*/ t3s.c from t1 as t3s
  join t1 as t4s on t4s.a = t3s.b);


explain
select count(*)
from t1
join t1 as t2 on t2.a = t1.b
where t1.c IN(
  select /*+ SEMIJOIN(MATERIALIZATION)*/ t3s1.c from t1 as t3s1
  join t1 as t4s1 on t4s1.a = t3s1.b
 )
and t1.d IN (
  select /*+ SEMIJOIN(MATERIALIZATION)*/ t3s2.c from t1 as t3s2
  join t1 as t4s2 on t4s2.a = t3s2.b
 );

select count(*)
from t1
join t1 as t2 on t2.a = t1.b
where t1.c IN(
  select /*+ SEMIJOIN(MATERIALIZATION)*/ t3s1.c from t1 as t3s1
  join t1 as t4s1 on t4s1.a = t3s1.b
 )
and t1.d IN (
  select /*+ SEMIJOIN(MATERIALIZATION)*/ t3s2.c from t1 as t3s2
  join t1 as t4s2 on t4s2.a = t3s2.b
 );


#############################################
# Subqueries-end
#############################################


connection ddl;
drop table t1;

##############
# 16999886 ORDER BY DOESN'T WORK WITH JOIN
# (Limited to pushed join + DESC order only)

connection ddl;
CREATE TABLE ndb_order_test (
  node_id int(10) unsigned NOT NULL,
  user_id int(10) unsigned NOT NULL,
  sort_number int(10) unsigned NOT NULL,
  KEY node_id (node_id,sort_number)
) ENGINE=ndbcluster DEFAULT CHARSET=utf8
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

CREATE TABLE ndb_user_test (
  user_id int(10) unsigned NOT NULL AUTO_INCREMENT,
  name varchar(20) NOT NULL,
  PRIMARY KEY (user_id)
) ENGINE=ndbcluster DEFAULT CHARSET=utf8 AUTO_INCREMENT=2
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";


connection spj;
INSERT INTO ndb_order_test (node_id, user_id, sort_number) VALUES
(68, 1, 1398029),
(68, 1, 549053);

INSERT INTO ndb_user_test (user_id, name) VALUES
(1, 'Shawn');


EXPLAIN 
SELECT *
 FROM ndb_order_test JOIN ndb_user_test USING (user_id)
 WHERE node_id = 68
 ORDER BY sort_number DESC;

SELECT *
 FROM ndb_order_test JOIN ndb_user_test USING (user_id)
 WHERE node_id = 68
 ORDER BY sort_number DESC;

EXPLAIN 
SELECT *
 FROM ndb_order_test JOIN ndb_user_test USING (user_id)
 WHERE node_id = 68
 ORDER BY sort_number ASC;

SELECT *
 FROM ndb_order_test JOIN ndb_user_test USING (user_id)
 WHERE node_id = 68
 ORDER BY sort_number ASC;

connection ddl;
DROP TABLE ndb_order_test, ndb_user_test;

########################################
# Verify counters for entire test:
# Note: These tables are 'temporary' withing 'connection spj'

connection spj;

# Calculate the change in mysqld counters.
select new.variable_name, new.variable_value - old.variable_value
       from server_counts_at_startup as old,
         performance_schema.global_status as new
       where new.variable_name = old.variable_name
       order by new.variable_name;

drop table server_counts_at_startup;

set ndb_join_pushdown = @save_ndb_join_pushdown;

