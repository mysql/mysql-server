# include/rowid_order.inc
#
# Test for rowid ordering (and comparison) functions.
# do index_merge select for tables with PK of various types.
#
# The variable
#     $engine_type       -- storage engine to be tested
# has to be set before sourcing this script.
#
# Note: The comments/expections refer to InnoDB.
#       They might be not valid for other storage engines.
#
# Last update:
# 2006-08-02 ML test refactored
#               old name was t/rowid_order.test
#               main code went into include/rowid_order.inc
#

eval SET SESSION DEFAULT_STORAGE_ENGINE = $engine_type;

--disable_warnings
drop table if exists t1, t2, t3,t4;
--enable_warnings

# Signed number as rowid
create table t1 (
  pk1 int not NULL,
  key1 int(11),
  key2 int(11),
  PRIMARY KEY  (pk1),
  KEY key1 (key1),
  KEY key2 (key2)
);
insert into t1 values (-5, 1, 1),
  (-100, 1, 1),
  (3, 1, 1),
  (0, 1, 1),
  (10, 1, 1);
explain select * from t1 force index(key1, key2) where key1 < 3 or key2 < 3;
select * from t1 force index(key1, key2) where key1 < 3 or key2 < 3;
drop table t1;

# Unsigned numbers as rowids
create table t1 (
  pk1 int unsigned not NULL,
  key1 int(11),
  key2 int(11),
  PRIMARY KEY  (pk1),
  KEY key1 (key1),
  KEY key2 (key2)
);
insert into t1 values (0, 1, 1),
  (0xFFFFFFFF, 1, 1),
  (0xFFFFFFFE, 1, 1),
  (1, 1, 1),
  (2, 1, 1);
select * from t1 force index(key1, key2) where key1 < 3 or key2 < 3;
drop table t1;

# Case-insensitive char(N)
create table t1 (
  pk1 char(4) not NULL,
  key1 int(11),
  key2 int(11),
  PRIMARY KEY  (pk1),
  KEY key1 (key1),
  KEY key2 (key2)
) collate latin2_general_ci;
insert into t1 values ('a1', 1, 1),
  ('b2', 1, 1),
  ('A3', 1, 1),
  ('B4', 1, 1);
select * from t1 force index(key1, key2) where key1 < 3 or key2 < 3;
drop table t1;

# Multi-part PK
create table t1 (
  pk1 int not NULL,
  pk2 char(4) not NULL collate latin1_german1_ci,
  pk3 char(4) not NULL collate latin1_bin,
  key1 int(11),
  key2 int(11),
  PRIMARY KEY  (pk1,pk2,pk3),
  KEY key1 (key1),
  KEY key2 (key2)
);
insert into t1 values
  (1, 'u', 'u',        1, 1),
  (1, 'u', char(0xEC), 1, 1),
  (1, 'u', 'x',        1, 1);
insert ignore into t1 select pk1, char(0xEC), pk3, key1, key2  from t1;
insert ignore into t1 select pk1, 'x', pk3, key1, key2  from t1 where pk2='u';
insert ignore into t1 select 2, pk2, pk3, key1, key2  from t1;
select * from t1;
select * from t1 force index(key1, key2) where key1 < 3 or key2 < 3;

# Hidden PK
alter table t1 drop primary key;
select * from t1;
select * from t1 force index(key1, key2) where key1 < 3 or key2 < 3;
drop table t1;

# Variable-length PK
# this is also test for Bug#2688
create table t1  (
  pk1 varchar(8) NOT NULL default '',
  pk2 varchar(4) NOT NULL default '',
  key1 int(11),
  key2 int(11),
  primary key(pk1, pk2),
  KEY key1 (key1),
  KEY key2 (key2)
);
insert into t1 values ('','empt',2,2),
  ('a','a--a',2,2),
  ('bb','b--b',2,2),
  ('ccc','c--c',2,2),
  ('dddd','d--d',2,2);
select * from t1 force index(key1, key2) where key1 < 3 or key2 < 3;

drop table t1;
