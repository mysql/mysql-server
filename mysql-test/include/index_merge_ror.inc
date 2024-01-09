# include/index_merge_ror.inc
#
#  ROR-index_merge tests.
#
# The variable
#     $engine_type       -- storage engine to be tested
# has to be set before sourcing this script.
#
# Note: The comments/expectations refer to MyISAM.
#       They might be not valid for other storage engines.
#
# Last update:
# 2006-08-02 ML test refactored
#               old name was t/index_merge_ror.test
#               main code went into include/index_merge_ror.inc
#

--source include/elide_costs.inc

--echo #---------------- ROR-index_merge tests -----------------------
--echo # (Start of test file: index_merge_ror.inc)

eval SET SESSION DEFAULT_STORAGE_ENGINE = $engine_type;
eval SET SESSION EXPLAIN_FORMAT = tree;

--disable_warnings
drop table if exists  t0,t1,t2;
--enable_warnings
create table t1
(
  /* Field names reflect value(rowid) distribution, st=STairs, swt= SaWTooth */
  st_a int not null default 0,
  swt1a int not null default 0,
  swt2a int not null default 0,

  st_b int not null default 0,
  swt1b int not null default 0,
  swt2b int not null default 0,

  /* fields/keys for row retrieval tests */
  key1 int,
  key2 int,
  key3 int,
  key4 int,

  /* make rows much bigger then keys */
  filler1 char (200),
  filler2 char (200),
  filler3 char (200),
  filler4 char (200),
  filler5 char (200),
  filler6 char (200),

  /* order of keys is important */
  key sta_swt12a(st_a,swt1a,swt2a),
  key sta_swt1a(st_a,swt1a),
  key sta_swt2a(st_a,swt2a),
  key sta_swt21a(st_a,swt2a,swt1a),

  key st_a(st_a),
  key stb_swt1a_2b(st_b,swt1b,swt2a),
  key stb_swt1b(st_b,swt1b),
  key st_b(st_b),

  key(key1),
  key(key2),
  key(key3),
  key(key4)
) ;

# Fill table
create table t0 as select * from t1;
--disable_query_log
--echo # Printing of many insert into t0 values (....) disabled.
let $cnt=1000;
while ($cnt)
{
  eval insert into t0 values (1, 2, 3, 1, 2, 3, 0, 0, 0, 0, 'data1', 'data2', 'data3', 'data4', 'data5', 'data6');
  dec $cnt;
}
--enable_query_log

alter table t1 disable keys;
--disable_query_log
--echo # Printing of many insert into t1 select .... from t0 disabled.
let $1=4;
while ($1)
{
  let $2=4;
  while ($2)
  {
    let $3=4;
    while ($3)
    {
      eval insert into t1 select $1, $2, $3, $1 ,$2, $3, key1, key2, key3, key4, filler1, filler2, filler3, filler4, filler5, filler6 from t0;
      dec $3;
    }
    dec $2;
  }
 dec $1;
}

--echo # Printing of many insert into t1 (...) values (....) disabled.
# Row retrieval tests
# -1 is used for values 'out of any range we are using'
# insert enough rows for index intersection to be used for (key1,key2)
insert into t1 (key1, key2, key3, key4, filler1) values (100, 100, 100, 100,'key1-key2-key3-key4');
let $cnt=400;
while ($cnt)
{
  eval insert into t1 (key1, key2, key3, key4, filler1) values (100, -1, 100, -1,'key1-key3');
  dec $cnt;
}
let $cnt=400;
while ($cnt)
{
  eval insert into t1 (key1, key2, key3, key4, filler1) values (-1, 100, -1, 100,'key2-key4');
  dec $cnt;
}
--enable_query_log
alter table t1 enable keys;
select count(*) from t1;

-- disable_query_log
-- disable_result_log
analyze table t0;
analyze table t1;
-- enable_result_log
-- enable_query_log

--echo # One row results tests for cases where a single row matches all conditions
--replace_result key2 key1
--replace_regex $elide_costs_and_rows
explain select key1,key2 from t1 where key1=100 and key2=100;
select key1,key2 from t1 where key1=100 and key2=100;
--replace_result key2 key1 key4 key3
--replace_regex $elide_costs_and_rows
explain select key1,key2,key3,key4,filler1 from t1 where key1=100 and key2=100 or key3=100 and key4=100;
# Mask cost values since it varies between runs how much of the indexes
# are cached in InnoDB's buffer pool
--replace_result key2 key1 key4 key3
--replace_regex /(_rows": )[0-9.]+/\1"#"/ /(_cost": )[\"]*[0-9.]+[\"]*/\1"#"/
explain format=json select key1,key2,key3,key4,filler1 from t1 where key1=100 and key2=100 or key3=100 and key4=100;
select key1,key2,key3,key4,filler1 from t1 where key1=100 and key2=100 or key3=100 and key4=100;

--echo # Several-rows results
insert into t1 (key1, key2, key3, key4, filler1) values (100, 100, -1, -1, 'key1-key2');
insert into t1 (key1, key2, key3, key4, filler1) values (-1, -1, 100, 100, 'key4-key3');

-- disable_query_log
-- disable_result_log
analyze table t1;
-- enable_result_log
-- enable_query_log

--echo #  ROR-intersection, not covering
--replace_result key2 key1
--replace_regex $elide_costs_and_rows
explain select key1,key2,filler1 from t1 where key1=100 and key2=100;
select key1,key2,filler1 from t1 where key1=100 and key2=100;

--echo #  ROR-intersection, covering
--replace_result key2 key1
--replace_regex $elide_costs_and_rows
explain select key1,key2 from t1 where key1=100 and key2=100;
select key1,key2 from t1 where key1=100 and key2=100;

--echo #  ROR-union of ROR-intersections
--replace_result key2 key1 key4 key3
--replace_regex $elide_costs_and_rows
explain select key1,key2,key3,key4 from t1 where key1=100 and key2=100 or key3=100 and key4=100;
select key1,key2,key3,key4 from t1 where key1=100 and key2=100 or key3=100 and key4=100;
--replace_result key2 key1 key4 key3
--replace_regex $elide_costs_and_rows
explain select key1,key2,key3,key4,filler1 from t1 where key1=100 and key2=100 or key3=100 and key4=100;
select key1,key2,key3,key4,filler1 from t1 where key1=100 and key2=100 or key3=100 and key4=100;

--echo #  3-way ROR-intersection
--replace_result key2 key1 key3 key1
--replace_regex $elide_costs_and_rows
explain select key1,key2,key3 from t1 where key1=100 and key2=100 and key3=100;
select key1,key2,key3 from t1 where key1=100 and key2=100 and key3=100;

--echo #  ROR-union(ROR-intersection, ROR-range)
insert into t1 (key1,key2,key3,key4,filler1) values (101,101,101,101, 'key1234-101');
--replace_result key2 key1
--replace_regex $elide_costs_and_rows
explain select key1,key2,key3,key4,filler1 from t1 where key1=100 and key2=100 or key3=101;
select key1,key2,key3,key4,filler1 from t1 where key1=100 and key2=100 or key3=101;

--echo # Run some ROR updates/deletes
select key1,key2, filler1 from t1 where key1=100 and key2=100;
update t1 set filler1='to be deleted' where key1=100 and key2=100;
update t1 set key1=200,key2=200 where key1=100 and key2=100;
delete from t1 where key1=200 and key2=200;
-- disable_query_log
-- disable_result_log
analyze table t1;
-- enable_result_log
-- enable_query_log
select key1,key2,filler1 from t1 where key2=100 and key2=200;

--echo # ROR-union(ROR-intersection) with one of ROR-intersection giving empty
--echo # results
--replace_result key2 key1 key4 key3
--replace_regex $elide_costs_and_rows
explain select key1,key2,key3,key4,filler1 from t1 where key1=100 and key2=100 or key3=100 and key4=100;
select key1,key2,key3,key4,filler1 from t1 where key1=100 and key2=100 or key3=100 and key4=100;

delete from t1 where key3=100 and key4=100;

-- disable_query_log
-- disable_result_log
analyze table t1;
-- enable_result_log
-- enable_query_log

--echo # ROR-union with all ROR-intersections giving empty results
# The relative order of keys inside 
# "Using union(intersect(keyx,keyx),...)" doesn't matter
--replace_result key2 key1 key4 key3
--replace_regex $elide_costs_and_rows
explain select key1,key2,key3,key4,filler1 from t1 where key1=100 and key2=100 or key3=100 and key4=100;
select key1,key2,key3,key4,filler1 from t1 where key1=100 and key2=100 or key3=100 and key4=100;

--echo # ROR-intersection with empty result
--replace_result key2 key1 key4 key3
--replace_regex $elide_costs_and_rows
explain select key1,key2 from t1 where key1=100 and key2=100;
select key1,key2 from t1 where key1=100 and key2=100;

--echo # ROR-union tests with various cases.
--echo #  All scans returning duplicate rows:
insert into t1 (key1, key2, key3, key4, filler1) values (100, 100, 200, 200,'key1-key2-key3-key4-1');
insert into t1 (key1, key2, key3, key4, filler1) values (100, 100, 200, 200,'key1-key2-key3-key4-2');
insert into t1 (key1, key2, key3, key4, filler1) values (100, 100, 200, 200,'key1-key2-key3-key4-3');

-- disable_query_log
-- disable_result_log
analyze table t1;
-- enable_result_log
-- enable_query_log

--replace_result key2 key1 key4 key3
--replace_regex $elide_costs_and_rows
explain select key1,key2,key3,key4,filler1 from t1 where key3=200 or (key1=100 and key2=100) or key4=200;
select key1,key2,key3,key4,filler1 from t1 where key3=200 or (key1=100 and key2=100) or key4=200;

insert into t1 (key1, key2, key3, key4, filler1) values (-1, -1, -1, 200,'key4');

-- disable_query_log
-- disable_result_log
analyze table t1;
-- enable_result_log
-- enable_query_log

--replace_result key2 key1 key4 key3
--replace_regex $elide_costs_and_rows
explain select key1,key2,key3,key4,filler1 from t1 where key3=200 or (key1=100 and key2=100) or key4=200;
select key1,key2,key3,key4,filler1 from t1 where key3=200 or (key1=100 and key2=100) or key4=200;

insert into t1 (key1, key2, key3, key4, filler1) values (-1, -1, 200, -1,'key3');

-- disable_query_log
-- disable_result_log
analyze table t1;
-- enable_result_log
-- enable_query_log

--replace_result key2 key1 key4 key3
--replace_regex $elide_costs_and_rows
explain select key1,key2,key3,key4,filler1 from t1 where key3=200 or (key1=100 and key2=100) or key4=200;
select key1,key2,key3,key4,filler1 from t1 where key3=200 or (key1=100 and key2=100) or key4=200;

--echo ##
--echo ## Optimizer tests
--echo ##

--echo # Check that the shortest key is used for ROR-intersection, covering and non-covering.
if ($engine_type != "InnoDB" )
{
  # Too unstable on InnoDB
  explain select * from t1 where st_a=1 and st_b=1;
  explain select st_a,st_b from t1 where st_a=1 and st_b=1;
  explain select st_a from t1 ignore index (st_a) where st_a=1 and st_b=1;
}

--echo # Do many tests
--echo # Check that keys that don't improve selectivity are skipped.
#

# Different value on 32 and 64 bit
--replace_regex $elide_costs_and_rows
--replace_result sta_swt12a sta_swt21a sta_swt12a, sta_swt12a,
explain select * from t1 where st_a=1 and swt1a=1 and swt2a=1;

--replace_regex $elide_costs_and_rows
explain select * from t1 where st_b=1 and swt1b=1 and swt2b=1;

--replace_regex $elide_costs_and_rows
explain select * from t1 where st_a=1 and swt1a=1 and swt2a=1 and st_b=1 and swt1b=1 and swt2b=1;

--replace_regex $elide_costs_and_rows
explain select * from t1 ignore index (sta_swt21a, stb_swt1a_2b)
  where st_a=1 and swt1a=1 and swt2a=1 and st_b=1 and swt1b=1 and swt2b=1;

--replace_regex $elide_costs_and_rows
explain select * from t1 ignore index (sta_swt21a, sta_swt12a, stb_swt1a_2b)
  where st_a=1 and swt1a=1 and swt2a=1 and st_b=1 and swt1b=1 and swt2b=1;

--replace_regex $elide_costs_and_rows
explain select * from t1 ignore index (sta_swt21a, sta_swt12a, stb_swt1a_2b, stb_swt1b)
  where st_a=1 and swt1a=1 and swt2a=1 and st_b=1 and swt1b=1 and swt2b=1;

--replace_regex $elide_costs_and_rows
explain select * from t1
  where st_a=1 and swt1a=1 and swt2a=1 and st_b=1 and swt1b=1;

--replace_regex $elide_costs_and_rows
explain select * from t1
  where st_a=1 and swt1a=1 and st_b=1 and swt1b=1 and swt1b=1;

--replace_regex $elide_costs_and_rows
explain select st_a from t1
  where st_a=1 and swt1a=1 and st_b=1 and swt1b=1 and swt1b=1;

--replace_regex $elide_costs_and_rows
explain select st_a from t1
  where st_a=1 and swt1a=1 and st_b=1 and swt1b=1 and swt1b=1;

drop table t0,t1;

--echo # 'Partially' covered fields test

create table t2 (
  a char(10) collate utf8mb4_0900_bin,
  b char(10) collate utf8mb4_0900_bin,
  filler1 char(255),
  filler2 char(255),
  key(a(5)),
  key(b(5))
);

--disable_query_log
let $1=8;
while ($1)
{
  eval insert into t2 values (repeat(char($1+64), 8),repeat(char($1+64), 8),'filler1', 'filler2');
  dec $1;
}
insert into t2 select * from t2;
insert into t2 select * from t2;
--enable_query_log

# The table row buffer is reused. Fill it with rows that don't match.
select count(a) from t2 where a='BBBBBBBB';
select count(a) from t2 where b='BBBBBBBB';

-- disable_query_log
-- disable_result_log
analyze table t2;
-- enable_result_log
-- enable_query_log

--echo # BUG#1:
--replace_regex /\b[ab]\b/a_or_b/ / *\(cost=.*//
explain select count(a) from t2 where a='AAAAAAAA' and b='AAAAAAAA';
select count(a) from t2 where a='AAAAAAAA' and b='AAAAAAAA';
select count(a) from t2 ignore index(a,b) where a='AAAAAAAA' and b='AAAAAAAA';

insert into t2 values ('ab', 'ab', 'uh', 'oh');
-- disable_query_log
-- disable_result_log
analyze table t2;
-- enable_result_log
-- enable_query_log
--replace_regex $elide_costs_and_rows
explain select a from t2 where a='ab';
drop table t2;

--echo #
--echo # BUG#25048 - ERROR 126 : Incorrect key file for table '.XXXX.MYI';
--echo #             try to repair it
--echo #
CREATE TABLE t1(c1 INT, c2 INT DEFAULT 0, c3 CHAR(255) DEFAULT '',
KEY(c1), KEY(c2), KEY(c3)) charset latin1;
INSERT INTO t1(c1) VALUES(0),(0),(0),(0),(0),(0),(0),(0),(0),(0),(0),(0),(0),
(0),(0),(0),(0),(0),(0),(0),(0),(0),(0),(0),(0),(0),(0),(0),(0);
INSERT INTO t1 VALUES(0,0,0);
CREATE TABLE t2(c1 int);
INSERT INTO t2 VALUES(1);
DELETE t1 FROM t1,t2 WHERE t1.c1=0 AND t1.c2=0;
SELECT * FROM t1;
DROP TABLE t1,t2;
