--echo #
--echo # Bug#18446462: ASSERT `ROWS >= 0.0' AT
--echo #               COST_MODEL_SERVER::ROW_EVALUATE_COST IN 
--echo #               OPT_COSTMODEL.H
--echo #

# 10 rows
create table t10 (pk int primary key, i int);
insert into t10 values (1,1);
insert into t10 select pk+1, i from t10;
insert into t10 select pk+2, i from t10;
insert into t10 select pk+4, i from t10;
insert into t10 select pk+8, i from t10 limit 2;

# 19 rows
create table t19 (pk int primary key, i int);
insert into t19 values (1,1);
insert into t19 select pk+1, i from t19;
insert into t19 select pk+2, i from t19;
insert into t19 select pk+4, i from t19;
insert into t19 select pk+8, i from t19;
insert into t19 select pk+16, i from t19 limit 3;

# 25 rows
create table t25 (pk int primary key, i int);
insert into t25 values (1,1);
insert into t25 select pk+1, i from t25;
insert into t25 select pk+2, i from t25;
insert into t25 select pk+4, i from t25;
insert into t25 select pk+8, i from t25;
insert into t25 select pk+16, i from t25 limit 9;

set @optimizer_switch_saved=@@optimizer_switch;
set optimizer_switch='derived_merge=off';
--disable_query_log
--disable_result_log
ANALYZE TABLE t10, t19, t25;
--enable_result_log
--enable_query_log

EXPLAIN SELECT * FROM (
SELECT t10_1.*
FROM t10 AS t10_1,
     t10 AS t10_2,
     t10 AS t10_3,
     t10 AS t10_4,
     t10 AS t10_5,
     t10 AS t10_6,
     t25 AS t25_1, 
     t25 AS t25_2,
     t19
) AS d;

set optimizer_switch=@optimizer_switch_saved;

DROP TABLE t10;
DROP TABLE t19;
DROP TABLE t25;
