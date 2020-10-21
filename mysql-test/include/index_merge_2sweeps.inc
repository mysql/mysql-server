# include/index_merge_2sweeps.inc
#
# 2-sweeps read Index_merge test
#
# The variable
#     $engine_type       -- storage engine to be tested
# has to be set before sourcing this script.
#
# Last update:
# 2006-08-02 ML test refactored
#               old name was index_merge_innodb2.test
#               main code went into include/index_merge_2sweeps.inc
#

--echo #---------------- 2-sweeps read Index merge test 2 -------------------------------
--echo # (Start of test file: index_merge_2sweeps.inc)

eval SET SESSION DEFAULT_STORAGE_ENGINE = $engine_type;

--disable_warnings
drop table if exists t1;
--enable_warnings

create table t1 (
  pk int primary key,
  key1 int,
  key2 int,
  filler char(200),
  filler2 char(200),
  index(key1),
  index(key2)
);


--disable_query_log
let $1=1000;
while ($1)
{
  eval insert into t1 values($1, $1, $1, 'filler-data','filler-data-2');
  dec $1;
}
--enable_query_log

--sorted_result
select * from t1 where (key1 >= 2 and key1 <= 10) or (pk >= 4 and pk <=8 );

set @maxv=1000;

--sorted_result
select * from t1 where
  (pk < 5) or (pk > 10 and pk < 15) or (pk >= 50 and pk < 55 ) or (pk > @maxv-10)
  or key1=18 or key1=60;

--sorted_result
select * from t1 where
  (pk < 5) or (pk > 10 and pk < 15) or (pk >= 50 and pk < 55 ) or (pk > @maxv-10)
  or key1 < 3 or key1 > @maxv-11;

--sorted_result
select * from t1 where
  (pk < 5) or (pk > 10 and pk < 15) or (pk >= 50 and pk < 55 ) or (pk > @maxv-10)
  or
  (key1 < 5) or (key1 > 10 and key1 < 15) or (key1 >= 50 and key1 < 55 ) or (key1 > @maxv-10);

--sorted_result
select * from t1 where
  (pk > 10 and pk < 15) or (pk >= 50 and pk < 55 )
  or
  (key1 < 5) or (key1 > @maxv-10);

drop table t1;
