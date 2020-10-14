
# The include statement below is a temp one for tests that are yet to
#be ported to run with InnoDB,
#but needs to be kept for tests that would need MyISAM in future.
--source include/not_hypergraph.inc # Costs vary
--source include/force_myisam_default.inc
--source include/have_myisam.inc
--source include/not_valgrind.inc
--source include/big_test.inc

#
# A simple test of the greedy query optimization algorithm and the switches that
# control the optimizationprocess.
#

#
# Schema
#
--disable_warnings
drop table if exists t1,t2,t3,t4,t5,t6,t7;
--enable_warnings

create table t1 (
  c11 integer,c12 integer,c13 integer,c14 integer,c15 integer,c16 integer,
  primary key (c11)
);
create table t2 (
  c21 integer,c22 integer,c23 integer,c24 integer,c25 integer,c26 integer
);
create table t3 (
  c31 integer,c32 integer,c33 integer,c34 integer,c35 integer,c36 integer,
  primary key (c31)
);
create table t4 (
  c41 integer,c42 integer,c43 integer,c44 integer,c45 integer,c46 integer
);
create table t5 (
  c51 integer,c52 integer,c53 integer,c54 integer,c55 integer,c56 integer,
  primary key (c51)
);
create table t6 (
  c61 integer,c62 integer,c63 integer,c64 integer,c65 integer,c66 integer
);
create table t7 (
  c71 integer,c72 integer,c73 integer,c74 integer,c75 integer,c76 integer,
  primary key (c71)
);

#
# Data
# cardinality(Ti) = cardinality(T(i-1)) + 3
#
insert into t1 values (1,2,3,4,5,6);
insert into t1 values (2,2,3,4,5,6);
insert into t1 values (3,2,3,4,5,6);

insert into t2 values (1,2,3,4,5,6);
insert into t2 values (2,2,3,4,5,6);
insert into t2 values (3,2,3,4,5,6);
insert into t2 values (4,2,3,4,5,6);
insert into t2 values (5,2,3,4,5,6);
insert into t2 values (6,2,3,4,5,6);

insert into t3 values (1,2,3,4,5,6);
insert into t3 values (2,2,3,4,5,6);
insert into t3 values (3,2,3,4,5,6);
insert into t3 values (4,2,3,4,5,6);
insert into t3 values (5,2,3,4,5,6);
insert into t3 values (6,2,3,4,5,6);
insert into t3 values (7,2,3,4,5,6);
insert into t3 values (8,2,3,4,5,6);
insert into t3 values (9,2,3,4,5,6);

insert into t4 values (1,2,3,4,5,6);
insert into t4 values (2,2,3,4,5,6);
insert into t4 values (3,2,3,4,5,6);
insert into t4 values (4,2,3,4,5,6);
insert into t4 values (5,2,3,4,5,6);
insert into t4 values (6,2,3,4,5,6);
insert into t4 values (7,2,3,4,5,6);
insert into t4 values (8,2,3,4,5,6);
insert into t4 values (9,2,3,4,5,6);
insert into t4 values (10,2,3,4,5,6);
insert into t4 values (11,2,3,4,5,6);
insert into t4 values (12,2,3,4,5,6);

insert into t5 values (1,2,3,4,5,6);
insert into t5 values (2,2,3,4,5,6);
insert into t5 values (3,2,3,4,5,6);
insert into t5 values (4,2,3,4,5,6);
insert into t5 values (5,2,3,4,5,6);
insert into t5 values (6,2,3,4,5,6);
insert into t5 values (7,2,3,4,5,6);
insert into t5 values (8,2,3,4,5,6);
insert into t5 values (9,2,3,4,5,6);
insert into t5 values (10,2,3,4,5,6);
insert into t5 values (11,2,3,4,5,6);
insert into t5 values (12,2,3,4,5,6);
insert into t5 values (13,2,3,4,5,6);
insert into t5 values (14,2,3,4,5,6);
insert into t5 values (15,2,3,4,5,6);

insert into t6 values (1,2,3,4,5,6);
insert into t6 values (2,2,3,4,5,6);
insert into t6 values (3,2,3,4,5,6);
insert into t6 values (4,2,3,4,5,6);
insert into t6 values (5,2,3,4,5,6);
insert into t6 values (6,2,3,4,5,6);
insert into t6 values (7,2,3,4,5,6);
insert into t6 values (8,2,3,4,5,6);
insert into t6 values (9,2,3,4,5,6);
insert into t6 values (10,2,3,4,5,6);
insert into t6 values (11,2,3,4,5,6);
insert into t6 values (12,2,3,4,5,6);
insert into t6 values (13,2,3,4,5,6);
insert into t6 values (14,2,3,4,5,6);
insert into t6 values (15,2,3,4,5,6);
insert into t6 values (16,2,3,4,5,6);
insert into t6 values (17,2,3,4,5,6);
insert into t6 values (18,2,3,4,5,6);

insert into t7 values (1,2,3,4,5,6);
insert into t7 values (2,2,3,4,5,6);
insert into t7 values (3,2,3,4,5,6);
insert into t7 values (4,2,3,4,5,6);
insert into t7 values (5,2,3,4,5,6);
insert into t7 values (6,2,3,4,5,6);
insert into t7 values (7,2,3,4,5,6);
insert into t7 values (8,2,3,4,5,6);
insert into t7 values (9,2,3,4,5,6);
insert into t7 values (10,2,3,4,5,6);
insert into t7 values (11,2,3,4,5,6);
insert into t7 values (12,2,3,4,5,6);
insert into t7 values (13,2,3,4,5,6);
insert into t7 values (14,2,3,4,5,6);
insert into t7 values (15,2,3,4,5,6);
insert into t7 values (16,2,3,4,5,6);
insert into t7 values (17,2,3,4,5,6);
insert into t7 values (18,2,3,4,5,6);
insert into t7 values (19,2,3,4,5,6);
insert into t7 values (20,2,3,4,5,6);
insert into t7 values (21,2,3,4,5,6);

#
# The actual test begins here
#

# Check the default values for the optimizer paramters

select @@optimizer_search_depth;
select @@optimizer_prune_level;

# These are the values for the parameters that control the greedy optimizer
# (total 6 combinations - 3 for optimizer_search_depth, 2 for optimizer_prune_level):
# 3:
# set optimizer_search_depth=0;  - automatic
# set optimizer_search_depth=1;  - min
# set optimizer_search_depth=62; - max (default)
# 2:
# set optimizer_prune_level=0 - exhaustive;
# set optimizer_prune_level=1 - heuristic; # default


#
# Compile several queries with all combinations of the query
# optimizer parameters. Each test query has two variants, where
# in the second variant the tables in the FROM clause are in
# inverse order to the tables in the first variant.
# Due to pre-sorting of tables before compilation, there should
# be no difference in the plans for each two such query variants.
#

# Test the greedy optimization procedures

set optimizer_prune_level=0;
select @@optimizer_prune_level;

set optimizer_search_depth=0;
select @@optimizer_search_depth;

# 6-table join, chain
let $query=
 select t1.c11 from t1, t2, t3, t4, t5, t6, t7 where t1.c12 = t2.c21 and t2.c22 = t3.c31 and t3.c32 = t4.c41 and t4.c42 = t5.c51 and t5.c52 = t6.c61 and t6.c62 = t7.c71;
--source include/execute_with_statistics.inc

let $query=
 select t1.c11 from t7, t6, t5, t4, t3, t2, t1 where t1.c12 = t2.c21 and t2.c22 = t3.c31 and t3.c32 = t4.c41 and t4.c42 = t5.c51 and t5.c52 = t6.c61 and t6.c62 = t7.c71;
--source include/execute_with_statistics.inc

# 6-table join, star
let $query=
 select t1.c11 from t1, t2, t3, t4, t5, t6, t7 where t1.c11 = t2.c21 and t1.c12 = t3.c31 and t1.c13 = t4.c41 and t1.c14 = t5.c51 and t1.c15 = t6.c61 and t1.c16 = t7.c71;
--source include/execute_with_statistics.inc

let $query=
 select t1.c11 from t7, t6, t5, t4, t3, t2, t1 where t1.c11 = t2.c21 and t1.c12 = t3.c31 and t1.c13 = t4.c41 and t1.c14 = t5.c51 and t1.c15 = t6.c61 and t1.c16 = t7.c71;
--source include/execute_with_statistics.inc

# 6-table join, clique
let $query=
 select t1.c11 from t1, t2, t3, t4, t5, t6, t7 where t1.c11 = t2.c21 and t1.c12 = t3.c31 and t1.c13 = t4.c41 and t1.c14 = t5.c51 and t1.c15 = t6.c61 and t1.c16 = t7.c71 and t2.c22 = t3.c32 and t2.c23 = t4.c42 and t2.c24 = t5.c52 and t2.c25 = t6.c62 and t2.c26 = t7.c72 and t3.c33 = t4.c43 and t3.c34 = t5.c53 and t3.c35 = t6.c63 and t3.c36 = t7.c73 and t4.c42 = t5.c54 and t4.c43 = t6.c64 and t4.c44 = t7.c74 and t5.c52 = t6.c65 and t5.c53 = t7.c75 and t6.c62 = t7.c76;
--source include/execute_with_statistics.inc

let $query=
 select t1.c11 from t7, t6, t5, t4, t3, t2, t1 where t1.c11 = t2.c21 and t1.c12 = t3.c31 and t1.c13 = t4.c41 and t1.c14 = t5.c51 and t1.c15 = t6.c61 and t1.c16 = t7.c71 and t2.c22 = t3.c32 and t2.c23 = t4.c42 and t2.c24 = t5.c52 and t2.c25 = t6.c62 and t2.c26 = t7.c72 and t3.c33 = t4.c43 and t3.c34 = t5.c53 and t3.c35 = t6.c63 and t3.c36 = t7.c73 and t4.c42 = t5.c54 and t4.c43 = t6.c64 and t4.c44 = t7.c74 and t5.c52 = t6.c65 and t5.c53 = t7.c75 and t6.c62 = t7.c76;
--source include/execute_with_statistics.inc


set optimizer_search_depth=1;
select @@optimizer_search_depth;

# 6-table join, chain
let $query=
 select t1.c11 from t1, t2, t3, t4, t5, t6, t7 where t1.c12 = t2.c21 and t2.c22 = t3.c31 and t3.c32 = t4.c41 and t4.c42 = t5.c51 and t5.c52 = t6.c61 and t6.c62 = t7.c71;
--source include/execute_with_statistics.inc

let $query=
 select t1.c11 from t7, t6, t5, t4, t3, t2, t1 where t1.c12 = t2.c21 and t2.c22 = t3.c31 and t3.c32 = t4.c41 and t4.c42 = t5.c51 and t5.c52 = t6.c61 and t6.c62 = t7.c71;
--source include/execute_with_statistics.inc

# 6-table join, star
let $query=
 select t1.c11 from t1, t2, t3, t4, t5, t6, t7 where t1.c11 = t2.c21 and t1.c12 = t3.c31 and t1.c13 = t4.c41 and t1.c14 = t5.c51 and t1.c15 = t6.c61 and t1.c16 = t7.c71;
--source include/execute_with_statistics.inc

let $query=
 select t1.c11 from t7, t6, t5, t4, t3, t2, t1 where t1.c11 = t2.c21 and t1.c12 = t3.c31 and t1.c13 = t4.c41 and t1.c14 = t5.c51 and t1.c15 = t6.c61 and t1.c16 = t7.c71;
--source include/execute_with_statistics.inc

# 6-table join, clique
let $query=
 select t1.c11 from t1, t2, t3, t4, t5, t6, t7 where t1.c11 = t2.c21 and t1.c12 = t3.c31 and t1.c13 = t4.c41 and t1.c14 = t5.c51 and t1.c15 = t6.c61 and t1.c16 = t7.c71 and t2.c22 = t3.c32 and t2.c23 = t4.c42 and t2.c24 = t5.c52 and t2.c25 = t6.c62 and t2.c26 = t7.c72 and t3.c33 = t4.c43 and t3.c34 = t5.c53 and t3.c35 = t6.c63 and t3.c36 = t7.c73 and t4.c42 = t5.c54 and t4.c43 = t6.c64 and t4.c44 = t7.c74 and t5.c52 = t6.c65 and t5.c53 = t7.c75 and t6.c62 = t7.c76;
--source include/execute_with_statistics.inc

let $query=
 select t1.c11 from t7, t6, t5, t4, t3, t2, t1 where t1.c11 = t2.c21 and t1.c12 = t3.c31 and t1.c13 = t4.c41 and t1.c14 = t5.c51 and t1.c15 = t6.c61 and t1.c16 = t7.c71 and t2.c22 = t3.c32 and t2.c23 = t4.c42 and t2.c24 = t5.c52 and t2.c25 = t6.c62 and t2.c26 = t7.c72 and t3.c33 = t4.c43 and t3.c34 = t5.c53 and t3.c35 = t6.c63 and t3.c36 = t7.c73 and t4.c42 = t5.c54 and t4.c43 = t6.c64 and t4.c44 = t7.c74 and t5.c52 = t6.c65 and t5.c53 = t7.c75 and t6.c62 = t7.c76;
--source include/execute_with_statistics.inc

set optimizer_search_depth=62;
select @@optimizer_search_depth;

# 6-table join, chain
let $query=
 select t1.c11 from t1, t2, t3, t4, t5, t6, t7 where t1.c12 = t2.c21 and t2.c22 = t3.c31 and t3.c32 = t4.c41 and t4.c42 = t5.c51 and t5.c52 = t6.c61 and t6.c62 = t7.c71;
--source include/execute_with_statistics.inc

let $query=
 select t1.c11 from t7, t6, t5, t4, t3, t2, t1 where t1.c12 = t2.c21 and t2.c22 = t3.c31 and t3.c32 = t4.c41 and t4.c42 = t5.c51 and t5.c52 = t6.c61 and t6.c62 = t7.c71;
--source include/execute_with_statistics.inc

# 6-table join, star
let $query=
 select t1.c11 from t1, t2, t3, t4, t5, t6, t7 where t1.c11 = t2.c21 and t1.c12 = t3.c31 and t1.c13 = t4.c41 and t1.c14 = t5.c51 and t1.c15 = t6.c61 and t1.c16 = t7.c71;
--source include/execute_with_statistics.inc

let $query=
 select t1.c11 from t7, t6, t5, t4, t3, t2, t1 where t1.c11 = t2.c21 and t1.c12 = t3.c31 and t1.c13 = t4.c41 and t1.c14 = t5.c51 and t1.c15 = t6.c61 and t1.c16 = t7.c71;
--source include/execute_with_statistics.inc

# 6-table join, clique
let $query=
 select t1.c11 from t1, t2, t3, t4, t5, t6, t7 where t1.c11 = t2.c21 and t1.c12 = t3.c31 and t1.c13 = t4.c41 and t1.c14 = t5.c51 and t1.c15 = t6.c61 and t1.c16 = t7.c71 and t2.c22 = t3.c32 and t2.c23 = t4.c42 and t2.c24 = t5.c52 and t2.c25 = t6.c62 and t2.c26 = t7.c72 and t3.c33 = t4.c43 and t3.c34 = t5.c53 and t3.c35 = t6.c63 and t3.c36 = t7.c73 and t4.c42 = t5.c54 and t4.c43 = t6.c64 and t4.c44 = t7.c74 and t5.c52 = t6.c65 and t5.c53 = t7.c75 and t6.c62 = t7.c76;
--source include/execute_with_statistics.inc

let $query=
 select t1.c11 from t7, t6, t5, t4, t3, t2, t1 where t1.c11 = t2.c21 and t1.c12 = t3.c31 and t1.c13 = t4.c41 and t1.c14 = t5.c51 and t1.c15 = t6.c61 and t1.c16 = t7.c71 and t2.c22 = t3.c32 and t2.c23 = t4.c42 and t2.c24 = t5.c52 and t2.c25 = t6.c62 and t2.c26 = t7.c72 and t3.c33 = t4.c43 and t3.c34 = t5.c53 and t3.c35 = t6.c63 and t3.c36 = t7.c73 and t4.c42 = t5.c54 and t4.c43 = t6.c64 and t4.c44 = t7.c74 and t5.c52 = t6.c65 and t5.c53 = t7.c75 and t6.c62 = t7.c76;
--source include/execute_with_statistics.inc


set optimizer_prune_level=1;
select @@optimizer_prune_level;

set optimizer_search_depth=0;
select @@optimizer_search_depth;

# 6-table join, chain
let $query=
 select t1.c11 from t1, t2, t3, t4, t5, t6, t7 where t1.c12 = t2.c21 and t2.c22 = t3.c31 and t3.c32 = t4.c41 and t4.c42 = t5.c51 and t5.c52 = t6.c61 and t6.c62 = t7.c71;
--source include/execute_with_statistics.inc

let $query=
 select t1.c11 from t7, t6, t5, t4, t3, t2, t1 where t1.c12 = t2.c21 and t2.c22 = t3.c31 and t3.c32 = t4.c41 and t4.c42 = t5.c51 and t5.c52 = t6.c61 and t6.c62 = t7.c71;
--source include/execute_with_statistics.inc

# 6-table join, star
let $query=
 select t1.c11 from t1, t2, t3, t4, t5, t6, t7 where t1.c11 = t2.c21 and t1.c12 = t3.c31 and t1.c13 = t4.c41 and t1.c14 = t5.c51 and t1.c15 = t6.c61 and t1.c16 = t7.c71;
--source include/execute_with_statistics.inc

let $query=
 select t1.c11 from t7, t6, t5, t4, t3, t2, t1 where t1.c11 = t2.c21 and t1.c12 = t3.c31 and t1.c13 = t4.c41 and t1.c14 = t5.c51 and t1.c15 = t6.c61 and t1.c16 = t7.c71;
--source include/execute_with_statistics.inc

# 6-table join, clique
let $query=
 select t1.c11 from t1, t2, t3, t4, t5, t6, t7 where t1.c11 = t2.c21 and t1.c12 = t3.c31 and t1.c13 = t4.c41 and t1.c14 = t5.c51 and t1.c15 = t6.c61 and t1.c16 = t7.c71 and t2.c22 = t3.c32 and t2.c23 = t4.c42 and t2.c24 = t5.c52 and t2.c25 = t6.c62 and t2.c26 = t7.c72 and t3.c33 = t4.c43 and t3.c34 = t5.c53 and t3.c35 = t6.c63 and t3.c36 = t7.c73 and t4.c42 = t5.c54 and t4.c43 = t6.c64 and t4.c44 = t7.c74 and t5.c52 = t6.c65 and t5.c53 = t7.c75 and t6.c62 = t7.c76;
--source include/execute_with_statistics.inc

let $query=
 select t1.c11 from t7, t6, t5, t4, t3, t2, t1 where t1.c11 = t2.c21 and t1.c12 = t3.c31 and t1.c13 = t4.c41 and t1.c14 = t5.c51 and t1.c15 = t6.c61 and t1.c16 = t7.c71 and t2.c22 = t3.c32 and t2.c23 = t4.c42 and t2.c24 = t5.c52 and t2.c25 = t6.c62 and t2.c26 = t7.c72 and t3.c33 = t4.c43 and t3.c34 = t5.c53 and t3.c35 = t6.c63 and t3.c36 = t7.c73 and t4.c42 = t5.c54 and t4.c43 = t6.c64 and t4.c44 = t7.c74 and t5.c52 = t6.c65 and t5.c53 = t7.c75 and t6.c62 = t7.c76;
--source include/execute_with_statistics.inc

set optimizer_search_depth=1;
select @@optimizer_search_depth;

# 6-table join, chain
let $query=
 select t1.c11 from t1, t2, t3, t4, t5, t6, t7 where t1.c12 = t2.c21 and t2.c22 = t3.c31 and t3.c32 = t4.c41 and t4.c42 = t5.c51 and t5.c52 = t6.c61 and t6.c62 = t7.c71;
--source include/execute_with_statistics.inc

let $query=
 select t1.c11 from t7, t6, t5, t4, t3, t2, t1 where t1.c12 = t2.c21 and t2.c22 = t3.c31 and t3.c32 = t4.c41 and t4.c42 = t5.c51 and t5.c52 = t6.c61 and t6.c62 = t7.c71;
--source include/execute_with_statistics.inc

# 6-table join, star
let $query=
 select t1.c11 from t1, t2, t3, t4, t5, t6, t7 where t1.c11 = t2.c21 and t1.c12 = t3.c31 and t1.c13 = t4.c41 and t1.c14 = t5.c51 and t1.c15 = t6.c61 and t1.c16 = t7.c71;
--source include/execute_with_statistics.inc

let $query=
 select t1.c11 from t7, t6, t5, t4, t3, t2, t1 where t1.c11 = t2.c21 and t1.c12 = t3.c31 and t1.c13 = t4.c41 and t1.c14 = t5.c51 and t1.c15 = t6.c61 and t1.c16 = t7.c71;
--source include/execute_with_statistics.inc

# 6-table join, clique
let $query=
 select t1.c11 from t1, t2, t3, t4, t5, t6, t7 where t1.c11 = t2.c21 and t1.c12 = t3.c31 and t1.c13 = t4.c41 and t1.c14 = t5.c51 and t1.c15 = t6.c61 and t1.c16 = t7.c71 and t2.c22 = t3.c32 and t2.c23 = t4.c42 and t2.c24 = t5.c52 and t2.c25 = t6.c62 and t2.c26 = t7.c72 and t3.c33 = t4.c43 and t3.c34 = t5.c53 and t3.c35 = t6.c63 and t3.c36 = t7.c73 and t4.c42 = t5.c54 and t4.c43 = t6.c64 and t4.c44 = t7.c74 and t5.c52 = t6.c65 and t5.c53 = t7.c75 and t6.c62 = t7.c76;
--source include/execute_with_statistics.inc

let $query=
 select t1.c11 from t7, t6, t5, t4, t3, t2, t1 where t1.c11 = t2.c21 and t1.c12 = t3.c31 and t1.c13 = t4.c41 and t1.c14 = t5.c51 and t1.c15 = t6.c61 and t1.c16 = t7.c71 and t2.c22 = t3.c32 and t2.c23 = t4.c42 and t2.c24 = t5.c52 and t2.c25 = t6.c62 and t2.c26 = t7.c72 and t3.c33 = t4.c43 and t3.c34 = t5.c53 and t3.c35 = t6.c63 and t3.c36 = t7.c73 and t4.c42 = t5.c54 and t4.c43 = t6.c64 and t4.c44 = t7.c74 and t5.c52 = t6.c65 and t5.c53 = t7.c75 and t6.c62 = t7.c76;
--source include/execute_with_statistics.inc

set optimizer_search_depth=62;
select @@optimizer_search_depth;

# 6-table join, chain
let $query=
 select t1.c11 from t1, t2, t3, t4, t5, t6, t7 where t1.c12 = t2.c21 and t2.c22 = t3.c31 and t3.c32 = t4.c41 and t4.c42 = t5.c51 and t5.c52 = t6.c61 and t6.c62 = t7.c71;
--source include/execute_with_statistics.inc

let $query=
 select t1.c11 from t7, t6, t5, t4, t3, t2, t1 where t1.c12 = t2.c21 and t2.c22 = t3.c31 and t3.c32 = t4.c41 and t4.c42 = t5.c51 and t5.c52 = t6.c61 and t6.c62 = t7.c71;
--source include/execute_with_statistics.inc

# 6-table join, star
let $query=
 select t1.c11 from t1, t2, t3, t4, t5, t6, t7 where t1.c11 = t2.c21 and t1.c12 = t3.c31 and t1.c13 = t4.c41 and t1.c14 = t5.c51 and t1.c15 = t6.c61 and t1.c16 = t7.c71;
--source include/execute_with_statistics.inc

let $query=
 select t1.c11 from t7, t6, t5, t4, t3, t2, t1 where t1.c11 = t2.c21 and t1.c12 = t3.c31 and t1.c13 = t4.c41 and t1.c14 = t5.c51 and t1.c15 = t6.c61 and t1.c16 = t7.c71;
--source include/execute_with_statistics.inc

# 6-table join, clique
let $query=
 select t1.c11 from t1, t2, t3, t4, t5, t6, t7 where t1.c11 = t2.c21 and t1.c12 = t3.c31 and t1.c13 = t4.c41 and t1.c14 = t5.c51 and t1.c15 = t6.c61 and t1.c16 = t7.c71 and t2.c22 = t3.c32 and t2.c23 = t4.c42 and t2.c24 = t5.c52 and t2.c25 = t6.c62 and t2.c26 = t7.c72 and t3.c33 = t4.c43 and t3.c34 = t5.c53 and t3.c35 = t6.c63 and t3.c36 = t7.c73 and t4.c42 = t5.c54 and t4.c43 = t6.c64 and t4.c44 = t7.c74 and t5.c52 = t6.c65 and t5.c53 = t7.c75 and t6.c62 = t7.c76;
--source include/execute_with_statistics.inc

let $query=
 select t1.c11 from t7, t6, t5, t4, t3, t2, t1 where t1.c11 = t2.c21 and t1.c12 = t3.c31 and t1.c13 = t4.c41 and t1.c14 = t5.c51 and t1.c15 = t6.c61 and t1.c16 = t7.c71 and t2.c22 = t3.c32 and t2.c23 = t4.c42 and t2.c24 = t5.c52 and t2.c25 = t6.c62 and t2.c26 = t7.c72 and t3.c33 = t4.c43 and t3.c34 = t5.c53 and t3.c35 = t6.c63 and t3.c36 = t7.c73 and t4.c42 = t5.c54 and t4.c43 = t6.c64 and t4.c44 = t7.c74 and t5.c52 = t6.c65 and t5.c53 = t7.c75 and t6.c62 = t7.c76;
--source include/execute_with_statistics.inc

drop table t1,t2,t3,t4,t5,t6,t7;


#
# Bug # 38795: Automatic search depth and nested join's results in server
# crash
#

CREATE TABLE t1 (a int, b int, d int, i int); INSERT INTO t1 VALUES (1,1,1,1);
CREATE TABLE t2 (b int, c int, j int); INSERT INTO t2 VALUES (1,1,1);
CREATE TABLE t2_1 (j int); INSERT INTO t2_1 VALUES (1);
CREATE TABLE t3 (c int, f int); INSERT INTO t3 VALUES (1,1);
CREATE TABLE t3_1 (f int); INSERT INTO t3_1 VALUES (1);
CREATE TABLE t4 (d int, e int, k int); INSERT INTO t4 VALUES (1,1,1);
CREATE TABLE t4_1 (k int); INSERT INTO t4_1 VALUES (1);
CREATE TABLE t5 (g int, d int, h int, l int); INSERT INTO t5 VALUES (1,1,1,1);
CREATE TABLE t5_1 (l int); INSERT INTO t5_1 VALUES (1);

SET optimizer_search_depth = 3;

SELECT 1
FROM t1
LEFT JOIN (
  t2 JOIN t3 ON t3.c = t2.c
) ON t2.b = t1.b
LEFT JOIN (
  t4 JOIN t5 ON t5.d = t4.d
) ON t4.d = t1.d
;

SELECT 1
FROM t1
LEFT JOIN (
  t2 LEFT JOIN (t3 JOIN t3_1 ON t3.f = t3_1.f) ON t3.c = t2.c
) ON t2.b = t1.b
LEFT JOIN (
  t4 JOIN t5 ON t5.d = t4.d
) ON t4.d = t1.d
;

SELECT 1
FROM t1
LEFT JOIN (
 (t2 JOIN t2_1 ON t2.j = t2_1.j) JOIN t3 ON t3.c = t2.c
) ON t2.b = t1.b
LEFT JOIN (
  t4 JOIN t5 ON t5.d = t4.d
) ON t4.d = t1.d
;

SELECT 1
FROM t1
LEFT JOIN (
  t2 JOIN t3 ON t3.c = t2.c
) ON t2.b = t1.b
LEFT JOIN (
  (t4 JOIN t4_1 ON t4.k = t4_1.k) LEFT JOIN t5 ON t5.d = t4.d
) ON t4.d = t1.d
;

SELECT 1
FROM t1
LEFT JOIN (
  t2 JOIN t3 ON t3.c = t2.c
) ON t2.b = t1.b
LEFT JOIN (
  t4 LEFT JOIN (t5 JOIN t5_1 ON t5.l = t5_1.l) ON t5.d = t4.d
) ON t4.d = t1.d
;

SET optimizer_search_depth = DEFAULT;
DROP TABLE t1,t2,t2_1,t3,t3_1,t4,t4_1,t5,t5_1;

--echo End of 5.0 tests



--echo #
--echo # Bug #59326: Greedy optimizer produce stupid query execution plans.
--echo #

CREATE TABLE t10(
  K INT NOT NULL AUTO_INCREMENT,
  I INT,
  PRIMARY KEY(K)
);
INSERT INTO t10(I) VALUES (1),(2),(3),(4),(5),(6),(7),(8),(9),(0);

CREATE TABLE t100 LIKE t10;
INSERT INTO t100(I)
SELECT X.I FROM t10 AS X,t10 AS Y;

CREATE TABLE t10000 LIKE t10;
INSERT INTO t10000(I)
SELECT X.I FROM t100 AS X, t100 AS Y;

--disable_warnings
let $total_handler_reads=
select sum(variable_value) from performance_schema.session_status
 where VARIABLE_NAME like 'Handler_read%';
--enable_warnings


## All crossproducts should be executed in order t10,t100,t10000
EXPLAIN SELECT * FROM t10,t100,t10000;
EXPLAIN SELECT * FROM t10,t10000,t100;
EXPLAIN SELECT * FROM t100,t10,t10000;
EXPLAIN SELECT * FROM t100,t10000,t10;
EXPLAIN SELECT * FROM t10000,t10,t100;
EXPLAIN SELECT * FROM t10000,t100,t10;

######
## Ordering between T100,T10000 EQ-joined T10 will
## normally be with smallest EQ-table joined first
######
let $query=
SELECT STRAIGHT_JOIN COUNT(*) FROM t10,t100,t10000
WHERE t100.K=t10.I
  AND t10000.K=t10.I;
--source include/expect_qep.inc

## However, swapping EQ_REF-joined tables gives the same cost
let $query=
SELECT STRAIGHT_JOIN COUNT(*) FROM t10,t10000,t100
WHERE t100.K=t10.I
  AND t10000.K=t10.I;
--source include/check_qep.inc

#####
# Expect all variants of EQ joining t100 & t10000 with T10
# to have same cost # handler_reads:
let $query=
SELECT COUNT(*) FROM t10,t100,t10000
WHERE t100.K=t10.I
  AND t10000.K=t10.I;
--source include/check_qep.inc

let $query=
SELECT COUNT(*) FROM t10,t10000,t100
WHERE t100.K=t10.I
  AND t10000.K=t10.I;
--source include/check_qep.inc

let $query=
SELECT COUNT(*) FROM t10,t100,t10000
WHERE t100.K=t10.I
  AND t10000.K=t10.K;
--source include/check_qep.inc

let $query=
SELECT COUNT(*) FROM t10,t10000,t100
WHERE t100.K=t10.I
  AND t10000.K=t10.K;
--source include/check_qep.inc

let $query=
SELECT COUNT(*) FROM t100,t10,t10000
WHERE t100.K=t10.I
  AND t10000.K=t10.K;
--source include/check_qep.inc

let $query=
SELECT COUNT(*) FROM t100,t10000,t10
WHERE t100.K=t10.I
  AND t10000.K=t10.K;
--source include/check_qep.inc

let $query=
SELECT COUNT(*) FROM t10000,t10,t100
WHERE t100.K=t10.I
  AND t10000.K=t10.K;
--source include/check_qep.inc

let $query=
SELECT COUNT(*) FROM t10000,t100,t10
WHERE t100.K=t10.I
  AND t10000.K=t10.K;
--source include/check_qep.inc


#####
## EQ_REF Should be executed before table scan(ALL)
## - Independent of #records in table being EQ_REF-joined
#####
#####
# Expect: Join EQ_REF(t100) before ALL(t10000)
let $query=
SELECT STRAIGHT_JOIN COUNT(*) FROM t10,t100,t10000
WHERE t100.K=t10.I
  AND t10000.I=t10.I;
--source include/expect_qep.inc

let $query=
SELECT COUNT(*) FROM t10,t100,t10000
WHERE t100.K=t10.I
  AND t10000.I=t10.I;
--source include/check_qep.inc

let $query=
SELECT COUNT(*) FROM t10,t10000,t100
WHERE t100.K=t10.I
  AND t10000.I=t10.I;
--source include/check_qep.inc

#####
# Expect: Join EQ_REF(t10000) before ALL(t100) (star-join)
let $query=
SELECT STRAIGHT_JOIN COUNT(*) FROM t10,t10000,t100
WHERE t100.I=t10.I
  AND t10000.K=t10.I;
--source include/expect_qep.inc

--echo # See BUG#18352936
let $query=
SELECT COUNT(*) FROM t10,t100,t10000
WHERE t100.I=t10.I
  AND t10000.K=t10.I;
--source include/check_qep.inc

--echo # See BUG#18352936
let $query=
SELECT COUNT(*) FROM t10,t10000,t100
WHERE t100.I=t10.I
  AND t10000.K=t10.I;
--source include/check_qep.inc

#####
# Expect: Join EQ_REF(t10000) before ALL(t100)
let $query=
SELECT STRAIGHT_JOIN COUNT(*) FROM t10,t10000,t100
WHERE t100.I=t10.I
  AND t10000.K=t100.I;
--source include/expect_qep.inc

let $query=
SELECT COUNT(*) FROM t10,t100,t10000
WHERE t100.I=t10.I
  AND t10000.K=t100.I;
--source include/check_qep.inc

let $query=
SELECT COUNT(*) FROM t10,t10000,t100
WHERE t100.I=t10.I
  AND t10000.K=t100.I;
--source include/check_qep.inc


#####
## EQ_REF & ALL join two instances of t10000 with t10:
## Always EQ_REF join first before producing cross product
#####

#####
# Expected QEP: 'join EQ_REF(X) on X.K=t10.I' before 'cross' ALL(Y)
let $query=
SELECT STRAIGHT_JOIN COUNT(*) FROM t10,t10000 x,t10000 y
WHERE x.k=t10.i;
--source include/expect_qep.inc

let $query=
SELECT COUNT(*) FROM t10,t10000 x,t10000 y
WHERE x.k=t10.i;
--source include/check_qep.inc

let $query=
SELECT COUNT(*) FROM t10,t10000 y,t10000 x
WHERE x.k=t10.i;
--source include/check_qep.inc

#####
# Expected QEP: 'join EQ_REF(X) on X.K=t10.I' before ALL(Y)
let $query=
SELECT STRAIGHT_JOIN COUNT(*) FROM t10,t10000 x,t10000 y
WHERE x.k=t10.i
  AND y.i=t10.i;
--source include/expect_qep.inc

let $query=
SELECT COUNT(*) FROM t10,t10000 x,t10000 y
WHERE x.k=t10.i
  AND y.i=t10.i;
--source include/check_qep.inc

let $query=
SELECT COUNT(*) FROM t10,t10000 y,t10000 x
WHERE x.k=t10.i
  AND y.i=t10.i;
--source include/check_qep.inc

#####
# Expected QEP: 'join EQ_REF(X) on X.K=t10.I' before ALL(Y)
let $query=
SELECT STRAIGHT_JOIN COUNT(*) FROM t10,t10000 x,t10000 y
WHERE x.k=t10.i
  AND y.i=x.k;
--source include/expect_qep.inc

let $query=
SELECT COUNT(*) FROM t10,t10000 x,t10000 y
WHERE x.k=t10.i
  AND y.i=x.k;
--source include/check_qep.inc

let $query=
SELECT COUNT(*) FROM t10,t10000 y,t10000 x
WHERE x.k=t10.i
  AND y.i=x.k;
--source include/check_qep.inc



## Create indexes to test REF access
CREATE INDEX IX ON t10(I);
CREATE INDEX IX ON t100(I);
CREATE INDEX IX ON t10000(I);

## Adding SHOW CREATE's so that we cache TABLE_SHARE objects
## for required tables before hand. This helps avoid 'Handler_reads'
## session status variable to get incremented while reading meta data from
## data dictionary tables later while queries is being inspected.
## This enables proper validation of the status variable value.
SHOW CREATE TABLE t10;
SHOW CREATE TABLE t100;
SHOW CREATE TABLE t10000;

########
## EQ_REF Should be executed before 'REF'
## - Independent of #records in table being EQ_REF-joined

####
# Expected QEP: 'join EQ_REF(t100) on t100.K=t10.I' before REF(t10000)
let $query=
SELECT STRAIGHT_JOIN COUNT(*) FROM t10,t100,t10000
WHERE t100.K=t10.I
  AND t10000.I=t10.I;
--source include/expect_qep.inc

let $query=
SELECT COUNT(*) FROM t10,t100,t10000
WHERE t100.K=t10.I
  AND t10000.I=t10.I;
--source include/check_qep.inc

let $query=
SELECT COUNT(*) FROM t10,t10000,t100
WHERE t100.K=t10.I
  AND t10000.I=t10.I;
--source include/check_qep.inc


#####
## EQ_REF & REF join two instances of t10000 with t10:
#####

#####
## Expect this QEP, cost & #handler_read
# Expected QEP: 'join EQ_REF(X) on X.K=t10.I' before 'cross' ALL(Y)
let $query=
SELECT STRAIGHT_JOIN COUNT(*) FROM t10,t10000 x,t10000 y
WHERE x.k=t10.i;
--source include/expect_qep.inc

let $query=
SELECT COUNT(*) FROM t10,t10000 x,t10000 y
WHERE x.k=t10.i;
--source include/check_qep.inc

let $query=
SELECT COUNT(*) FROM t10,t10000 y,t10000 x
WHERE x.k=t10.i;
--source include/check_qep.inc

#####
# Expected QEP: 'join EQ_REF(X) on X.K=t10.I' before REF(Y)
let $query=
SELECT STRAIGHT_JOIN COUNT(*) FROM t10,t10000 x,t10000 y
WHERE x.k=t10.i
  AND y.i=t10.i;
--source include/expect_qep.inc

let $query=
SELECT COUNT(*) FROM t10,t10000 x,t10000 y
WHERE x.k=t10.i
  AND y.i=t10.i;
--source include/check_qep.inc

let $query=
SELECT COUNT(*) FROM t10,t10000 y,t10000 x
WHERE x.k=t10.i
  AND y.i=t10.i;
--source include/check_qep.inc

#####
# Expected QEP: 'join EQ_REF(X) on X.K=t10.I' before REF(Y)
let $query=
SELECT STRAIGHT_JOIN COUNT(*) FROM t10,t10000 x,t10000 y
WHERE x.k=t10.i
  AND y.i=x.k;
--source include/expect_qep.inc

let $query=
SELECT COUNT(*) FROM t10,t10000 x,t10000 y
WHERE x.k=t10.i
  AND y.i=x.k;
--source include/check_qep.inc

let $query=
SELECT COUNT(*) FROM t10,t10000 y,t10000 x
WHERE x.k=t10.i
  AND y.i=x.k;
--source include/check_qep.inc

########



########

--echo #
--echo # Test improved capabilities of analyzing complex query
--echo # plans without restricting 'optimizer_search_depth'.
--echo # Fix problems like those reported as bug#41740 & bug#58225. 
--echo #
--echo # EPLAIN of queries using T1-T62 will timeout/hang wo/ fixes
--echo #

DROP TABLE t10, t10000;

--disable_result_log

let $tabledef=
( K INT NOT NULL AUTO_INCREMENT,
  I INT,
  A INT,
  PRIMARY KEY(K), KEY IX(A)
) engine = InnoDB;

let $analyze = ANALYZE TABLE t100;

let $i= 1;
while ($i < 62)
{
  let $create= CREATE TABLE t$i $tabledef;
  eval $create;

  let $insert =
  INSERT INTO t$i(I,A) SELECT X.K,X.K FROM t100 AS X, t100 AS Y WHERE X.K < 20 AND Y.K <= $i;
  eval $insert;

  let $analyze = $analyze, t$i;
  inc $i;
}
eval $analyze;

set optimizer_prune_level=1;


#################
## The EXPLAIN'ed query itself can't be part of the verified 
## result as the QEP is not 100% predictable due to variation
## in statistics from the engines. This is believed to be
## caused by:
##  - Variations in table fill degree.
##  - 'Fuzzy' statistics provided by engines.
##  - Round errors caused by 'cost' calculation using 
##    'only' 64-bit double precision.
##  - Other bugs...?
##
###############

## Will test with optimizer_search_depth= [0,1,3,62]
let $depth= 0;
while ($depth<4)
{
  if ($depth==0)
  {
    set optimizer_search_depth=0; 
  }
  if ($depth==1)
  {
    set optimizer_search_depth=1;
  }
  if ($depth==2)
  {
    set optimizer_search_depth=3;
  }
  if ($depth==3)
  {
    set optimizer_search_depth=62;
  }
  inc $depth;


  ## Test pruning of joined table scans (ALL) 
  ## Prepare of QEP without timeout is heavily dependent
  ## on maintaining correctly '#rows-sorted' plan
  ##
  let $query= SELECT COUNT(*) FROM t1 AS x;
  let $i= 1;
  while ($i < 61)
  {
    let $query= $query JOIN t$i ON t$i.I=x.I;
    inc $i;

    select @@optimizer_prune_level;
    select @@optimizer_search_depth;
    eval EXPLAIN $query;
  }

  ## Test pruning of joined table scans (ALL) 
  ## with multiple instances of same table.
  ## (All instances being equally expensive)
  let $query= SELECT COUNT(*) FROM t1 AS x;
  let $i= 1;
  while ($i <= 56)
  {
    let $t= t$i;
    let $query= $query JOIN $t as t$i ON t$i.I=x.I;
    inc $i;
    let $query= $query JOIN $t as t$i ON t$i.I=x.I;
    inc $i;
    let $query= $query JOIN $t as t$i ON t$i.I=x.I;
    inc $i;
    let $query= $query JOIN $t as t$i ON t$i.I=x.I;
    inc $i;
    let $query= $query JOIN $t as t$i ON t$i.I=x.I;
    inc $i;
    let $query= $query JOIN $t as t$i ON t$i.I=x.I;
    inc $i;
    let $query= $query JOIN $t as t$i ON t$i.I=x.I;
    inc $i;
    let $query= $query JOIN $t as t$i ON t$i.I=x.I;
    inc $i;

    select @@optimizer_prune_level;
    select @@optimizer_search_depth;
    eval EXPLAIN $query;
  }

  ## A mix of 25% EQ_REF / 75% ALL joins
  ##
  let $query= SELECT COUNT(*) FROM t1 AS x;
  let $i= 1;
  while ($i < 60)
  {
    let $query= $query JOIN t$i ON t$i.I = x.I;
    inc $i;
    let $query= $query JOIN t$i ON t$i.K = x.I;
    inc $i;
    let $query= $query JOIN t$i ON t$i.I = x.I;
    inc $i;
    let $query= $query JOIN t$i ON t$i.I = x.I;
    inc $i;

    eval EXPLAIN $query;
  }

  ## A mix of 50% EQ_REF / 50% ALL joins
  ##
  let $query= SELECT COUNT(*) FROM t1 AS x;
  let $i= 1;
  while ($i < 60)
  {
    let $query= $query JOIN t$i ON t$i.I = x.I;
    inc $i;
    let $query= $query JOIN t$i ON t$i.K = x.I;
    inc $i;
    let $query= $query JOIN t$i ON t$i.I = x.I;
    inc $i;
    let $query= $query JOIN t$i ON t$i.K = x.I;
    inc $i;

    eval EXPLAIN $query;
  }

  ## A mix of 75% EQ_REF / 25% ALL joins
  ##
  let $query= SELECT COUNT(*) FROM t1 AS x;
  let $i= 1;
  while ($i < 60)
  {
    let $query= $query JOIN t$i ON t$i.I = x.I;
    inc $i;
    let $query= $query JOIN t$i ON t$i.K = x.I;
    inc $i;
    let $query= $query JOIN t$i ON t$i.K = x.I;
    inc $i;
    let $query= $query JOIN t$i ON t$i.K = x.I;
    inc $i;

    eval EXPLAIN $query;
  }

  ## 100% EQ_REF joins
  ##
  let $query= SELECT COUNT(*) FROM t1 AS x;
  let $i= 1;
  while ($i < 61)
  {
    let $query= $query JOIN t$i ON t$i.K=x.I;
    inc $i;

    eval EXPLAIN $query;
  }
}

let $drop = DROP TABLE t100;
let $i= 1;
while ($i < 62)
{
  let $drop = $drop, t$i;
  inc $i;
}
eval $drop;

--enable_result_log

SET OPTIMIZER_SEARCH_DEPTH = DEFAULT;

--ECHO END OF 5.6 TESTS
