--source include/no_valgrind_without_big.inc
#
# Run subquery_sj.inc with semijoin and turn off all strategies, but FirstMatch
#

set optimizer_switch='semijoin=on,firstmatch=on';

--disable_query_log
if (`select locate('materialization', @@optimizer_switch) > 0`)
{
  set optimizer_switch='materialization=off';
}
if (`select locate('loosescan', @@optimizer_switch) > 0`)
{
  set optimizer_switch='loosescan=off';
}
if (`select locate('duplicateweedout', @@optimizer_switch) > 0`)
{
  set optimizer_switch='duplicateweedout=off';
}
if (`select locate('index_condition_pushdown', @@optimizer_switch) > 0`)
{
  set optimizer_switch='index_condition_pushdown=off';
}
if (`select locate('mrr', @@optimizer_switch) > 0`)
{
  set optimizer_switch='mrr=off';
}
--enable_query_log

--source include/subquery_sj.inc

--echo #
--echo # Bug#51457 Firstmatch semijoin strategy gives wrong results for
--echo #           certain query plans
--echo #

SET @@default_storage_engine='innodb';
SET @@optimizer_switch='semijoin=on,materialization=off,firstmatch=on,loosescan=off,block_nested_loop=off,batched_key_access=off';

CREATE TABLE t0(a INTEGER);

CREATE TABLE t1(a INTEGER);
INSERT INTO t1 VALUES(1);

CREATE TABLE t2(a INTEGER);
INSERT INTO t2 VALUES(5), (8);

CREATE TABLE t6(a INTEGER);
INSERT INTO t6 VALUES(7), (1), (0), (5), (1), (4);

CREATE TABLE t8(a INTEGER);
INSERT INTO t8 VALUES(1), (3), (5), (7), (9), (7), (3), (1);

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t0;
ANALYZE TABLE t1;
ANALYZE TABLE t2;
ANALYZE TABLE t6;
ANALYZE TABLE t8;
-- enable_result_log
-- enable_query_log

EXPLAIN
SELECT *
FROM t2 AS nt2
WHERE 1 IN (SELECT it1.a
            FROM t1 AS it1 JOIN t6 AS it3 ON it1.a=it3.a);

--sorted_result
SELECT *
FROM t2 AS nt2
WHERE 1 IN (SELECT it1.a
            FROM t1 AS it1 JOIN t6 AS it3 ON it1.a=it3.a);

EXPLAIN
SELECT *
FROM t2 AS nt2, t8 AS nt4
WHERE 1 IN (SELECT it1.a
            FROM t1 AS it1 JOIN t6 AS it3 ON it1.a=it3.a);

--sorted_result
SELECT *
FROM t2 AS nt2, t8 AS nt4
WHERE 1 IN (SELECT it1.a
            FROM t1 AS it1 JOIN t6 AS it3 ON it1.a=it3.a);

EXPLAIN
SELECT *
FROM t0 AS ot1, t2 AS nt3
WHERE ot1.a IN (SELECT it2.a
                FROM t1 AS it2 JOIN t8 AS it4 ON it2.a=it4.a);

SELECT *
FROM t0 as ot1, t2 AS nt3
WHERE ot1.a IN (SELECT it2.a
                FROM t1 AS it2 JOIN t8 AS it4 ON it2.a=it4.a);

DROP TABLE t0, t1, t2, t6, t8;

SET @@default_storage_engine=default;
SET @@optimizer_switch=default;

--echo # End of bug#51457

set optimizer_switch=default;

