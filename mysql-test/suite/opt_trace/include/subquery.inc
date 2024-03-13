# Test for optimizer tracing of subqueries

--source include/have_optimizer_trace.inc

SET optimizer_trace_max_mem_size=1048576; # 1MB
SET end_markers_in_json=on;
SET optimizer_trace="enabled=on,one_line=off";

CREATE TABLE t1 (a INT);
CREATE TABLE t2 (a INT, b INT);
INSERT INTO t1 VALUES (2);
INSERT INTO t2 VALUES (1,7),(2,7);

--echo # Subselect execute is traced every time it is executed
SET @@optimizer_trace_features="greedy_search=off,repeated_subselect=on";
SELECT (SELECT a FROM t1 WHERE t1.a=t2.a), a FROM t2;
--echo
--skip_if_hypergraph  # Does not support the same optimizer trace.
SELECT * FROM information_schema.OPTIMIZER_TRACE;
--echo

--echo # Subselect execute is traced only the first time it is executed
SET @@optimizer_trace_features="greedy_search=off,repeated_subselect=off";
SELECT (SELECT a FROM t1 WHERE t1.a=t2.a), a FROM t2;
--echo
--skip_if_hypergraph  # Does not support the same optimizer trace.
SELECT * FROM information_schema.OPTIMIZER_TRACE;
--echo

DROP TABLE t1,t2;
SET @@optimizer_trace_features="default";


CREATE TABLE t1 (a FLOAT(5,4) zerofill);
CREATE TABLE t2 (a FLOAT(5,4),b FLOAT(2,0));

# evaluate_subselect_cond_steps for build_equal_item()
SELECT t1.a
FROM t1
WHERE t1.a= (SELECT b FROM t2 LIMIT 1) AND NOT
      t1.a= (SELECT a FROM t2 LIMIT 1) ;
--echo
--skip_if_hypergraph  # Does not support the same optimizer trace.
SELECT * FROM information_schema.OPTIMIZER_TRACE;
--echo

# evaluate_subselect_cond_steps for remove_eq_conds
SELECT 1 FROM DUAL
WHERE NOT EXISTS
  (SELECT * FROM t2 WHERE a = 50 AND b = 3);
--echo
--skip_if_hypergraph  # Does not support the same optimizer trace.
SELECT * FROM information_schema.OPTIMIZER_TRACE;
--echo

# Distinct, order and group is removed from subquery. Note: For PS,
# removal happens during prepare so the only visible effect is that
# the subquery does not contain those clauses.
SELECT 1 FROM DUAL WHERE NOT EXISTS (SELECT DISTINCT(a) FROM t2 GROUP BY a ORDER BY b);
--echo
--skip_if_hypergraph  # Does not support the same optimizer trace.
SELECT * FROM information_schema.OPTIMIZER_TRACE;
--echo

DROP TABLE t1,t2;

--echo #
--echo # BUG#12905521 - ASSERT IN OPT_TRACE_STMT::SYNTAX_ERROR ON SELECT
--echo # DISTINCT/MIN/JOIN/SUBQ QUERY
--echo #

CREATE TABLE t1 (
pk INTEGER,
col_int_nokey INTEGER,
col_int_key INTEGER,
col_varchar_key VARCHAR(1),
col_varchar_nokey VARCHAR(1),
PRIMARY KEY (pk),
KEY (col_varchar_key,col_int_key)
) ENGINE=MYISAM;
CREATE TABLE t2 (
pk INTEGER,
col_int_nokey INTEGER,
col_int_key INTEGER,
col_varchar_key VARCHAR(1),
col_varchar_nokey VARCHAR(1),
PRIMARY KEY (pk),
KEY (col_varchar_key,col_int_key)
) ENGINE=MYISAM;
CREATE TABLE t3 (
pk INTEGER,
col_int_nokey INTEGER,
col_int_key INTEGER,
col_time_key TIME,
col_datetime_nokey DATETIME,
col_varchar_key VARCHAR(1),
col_varchar_nokey VARCHAR(1),
PRIMARY KEY (pk),
KEY (col_time_key),
KEY (col_varchar_key,col_int_key)
) ENGINE=MYISAM;
CREATE TABLE t4 (
pk INTEGER,
col_int_nokey INTEGER,
col_int_key INTEGER,
col_date_key DATE,
col_date_nokey DATE,
col_time_key TIME,
col_time_nokey TIME,
col_datetime_key DATETIME,
col_datetime_nokey DATETIME,
col_varchar_key VARCHAR(1),
col_varchar_nokey VARCHAR(1),
PRIMARY KEY (pk),
KEY (col_varchar_key,col_int_key)
) ENGINE=MYISAM;
INSERT IGNORE INTO t4 (
col_int_key,col_int_nokey,
col_date_key,col_date_nokey,
col_time_key,col_time_nokey,
col_datetime_key,col_datetime_nokey,
col_varchar_key,col_varchar_nokey
) VALUES
(8,7,'2008-10-02','2008-10-02','04:07:22.028954','04:07:22.028954','2001-10-08 00:00:00','2001-10-08 00:00:00','g','g');
CREATE TABLE t5 (
pk INTEGER AUTO_INCREMENT,
col_int_nokey INTEGER,
col_int_key INTEGER,
col_date_key DATE,
col_date_nokey DATE,
col_time_key TIME,
col_time_nokey TIME,
col_datetime_key DATETIME,
col_datetime_nokey DATETIME,
col_varchar_key VARCHAR(1),
col_varchar_nokey VARCHAR(1),
PRIMARY KEY (pk),
KEY (col_int_key),
KEY (col_varchar_key,col_int_key)
) ENGINE=MYISAM;
INSERT INTO t5 (
col_int_key,col_int_nokey,
col_date_key,col_date_nokey,
col_time_key,col_time_nokey,
col_datetime_key,col_datetime_nokey,
col_varchar_key,col_varchar_nokey
) VALUES
(8,NULL,'2000-12-03','2000-12-03','22:55:23.019225','22:55:23.019225','2005-07-20 00:00:00','2005-07-20 00:00:00','x','x'),
(7,8,'2008-05-03','2008-05-03','10:19:31.050677','10:19:31.050677','2007-10-06 17:56:40.056051','2007-10-06 17:56:40.056051','d','d'),
(8,6,'2000-09-20','2000-09-20','14:11:27.044095','14:11:27.044095','2003-06-13 23:19:49.018300','2003-06-13 23:19:49.018300','c','c');

set @old_opt_switch=@@optimizer_switch;
if (`select locate('semijoin', @@optimizer_switch) > 0`)
{
--disable_query_log
  set optimizer_switch="semijoin=off";
--enable_query_log
}

select distinct
alias1.`col_varchar_key` as field1 ,alias1.`col_date_key` as
field2 ,( select min( sq1_alias1.`col_varchar_nokey` ) as sq1_field1 from ( t1
as sq1_alias1 inner join ( t5 as sq1_alias2 left join t5 as sq1_alias3 on
(sq1_alias3.`col_varchar_nokey` = sq1_alias2.`col_varchar_key` ) ) on
(sq1_alias3.`col_varchar_nokey` = sq1_alias2.`col_varchar_key` ) ) where
exists ( select distinct c_sq1_alias2.`col_int_nokey` as c_sq1_field1 from (
t3 as c_sq1_alias1 right join t4 as c_sq1_alias2 on (c_sq1_alias2.`col_int_nokey` = c_sq1_alias1.`pk` ) ) where
c_sq1_alias2.`col_varchar_key` = sq1_alias2.`col_varchar_nokey` ) ) as field3
,( select max( sq2_alias1.`pk` ) as sq2_field1 from t5 as sq2_alias1 ) as
field4 ,alias2.`col_varchar_nokey` as field5 ,alias2.`col_varchar_nokey` as
field6 from ( t5 as alias1 right outer join ( ( ( select sq3_alias2.* from ( t5 as sq3_alias1 ,t4 as sq3_alias2 ) ) as alias2 right join t4
as alias3 on (alias3.`col_varchar_key` = alias2.`col_varchar_key` ) ) ) on
(alias3.`col_int_key` = alias2.`pk` ) ) where ( alias1.`col_varchar_nokey` in
( select sq4_alias1.`col_varchar_key` as sq4_field1 from ( t3 as sq4_alias1
inner join ( t2 as sq4_alias2 right outer join t3 as sq4_alias3 on
(sq4_alias3.`pk` = sq4_alias2.`col_int_key` ) ) on
(sq4_alias3.`col_varchar_nokey` = sq4_alias2.`col_varchar_key` ) ) where
sq4_alias2.`col_int_key` < alias1.`col_int_nokey` and
sq4_alias3.`col_varchar_nokey` <> alias1.`col_varchar_key` ) ) and
alias1.`col_int_key` not in (214) group by field1,field2,field3,
field4,field5,field6; 

--replace_regex /("sort_buffer_size":) [0-9]+/\1 "NNN"/
--skip_if_hypergraph  # Does not support the same optimizer trace.
select * from information_schema.optimizer_trace;
set optimizer_switch=@old_opt_switch;
drop table t1,t2,t3,t4,t5;

--echo #
--echo # BUG#12905758 - ASSERT IN OPT_TRACE_STMT::SYNTAX_ERROR ON
--echo # SELECT/SUBQ/SUM QUERY
--echo #

CREATE TABLE t1 (
pk INTEGER AUTO_INCREMENT,
col_int_nokey INTEGER,
col_int_key INTEGER,
col_date_key DATE,
col_date_nokey DATE,
col_time_key TIME,
col_time_nokey TIME,
col_datetime_key DATETIME,
col_datetime_nokey DATETIME,
col_varchar_key VARCHAR(1),
col_varchar_nokey VARCHAR(1),
PRIMARY KEY (pk),
KEY (col_varchar_key,col_int_key)
) ENGINE=MYISAM, CHARSET utf8mb4;
INSERT INTO t1 (
col_int_key,col_int_nokey,
col_date_key,col_date_nokey,
col_time_key,col_time_nokey,
col_datetime_key,col_datetime_nokey,
col_varchar_key,col_varchar_nokey
) VALUES
(8,NULL,'2000-12-03','2000-12-03','22:55:23.019225','22:55:23.019225','2005-07-20 00:00:00','2005-07-20 00:00:00','x','x'),
(8,6,'2000-09-20','2000-09-20','14:11:27.044095','14:11:27.044095','2003-06-13 23:19:49.018300','2003-06-13 23:19:49.018300','c','c');
CREATE TABLE t2 (I INTEGER);

select ( select sum( subquery1_t1.`col_int_nokey` ) as subquery1_field1 from
t1 as subquery1_t1 ) as field1 from ( t1 as table1 straight_join t1 as table2
on (table2.`col_varchar_key` = table1.`col_varchar_key` ) ) where (
table2.`col_int_nokey` <> any ( select 5 from t2 ) ) and table1.`pk` in
(192,18) order by field1 desc; 

--skip_if_hypergraph  # Does not support the same optimizer trace.
select * from information_schema.optimizer_trace;
drop table t1,t2;

--echo
--echo #
--echo # Tracing of semijoin loosescan
--echo #

--source include/show_json_object.inc

create table t0 (a int);
insert into t0 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);
create table t1 (a int, b int, filler char(100), key(a,b)) charset utf8mb4;
insert into t1 select A.a, B.a, 'filler' from t0 A, t0 B;
create table t2 as select * from t1;
set @old_opt_switch=@@optimizer_switch;
set optimizer_switch="firstmatch=off,materialization=off,duplicateweedout=off";
set @old_opt_prune_level=@@optimizer_prune_level;
set optimizer_prune_level=0;

explain select * from t2 where a in (select b from t1 where a=3);

--echo # Equality-propagation involving inner field => 1st sj equality is bound
explain select * from t2 where (b+0,a+0) in (select a,b from t1 where a=3);

--echo # Equality-propagation involving outer field => 3rd sj equality is bound.
explain select * from t2 where (b,a,filler) in (select a,b,a*3 from t1) and filler='abc';
# Show that LS is possible (even though too costly in the end):
--skip_if_hypergraph  # Does not support the same optimizer trace.
SELECT show_json_object('"recalculate_access_paths_and_cost": {', TRACE)
FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;

--echo # Remove the condition on 'filler' => 3rd sj equality is not bound.
explain select * from t2 where (b,a,filler) in (select a,b,a*3 from t1);
# Show that LS is impossible:
--skip_if_hypergraph  # Does not support the same optimizer trace.
SELECT show_json_object('"searching_loose_scan_index": {', TRACE)
FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;

--echo # Equality-propagation involving outer field => 3rd sj equality is bound.
explain select * from t2 as t3, t2
where t2.filler=t3.filler and
      (t2.b,t2.a,t2.filler) in (select a,b,a*3 from t1);
--skip_if_hypergraph  # Does not support the same optimizer trace.
SELECT show_json_object('"recalculate_access_paths_and_cost": {', TRACE)
FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;

--echo # In plan t3-t1-t2, 3rd outer expression is dependent only on
--echo # previous tables => 3rd sj equality is bound.
--echo # If t1 is before t3, 3rd sj equality is not bound.
explain select * from t2 as t3 left join t2 on t2.filler+10=t3.filler+20
where (t2.b,t2.a,t3.filler+2) in (select a,b,a*3 from t1);
# Show all LS plans (possible and impossible):
--skip_if_hypergraph  # Does not support the same optimizer trace.
SELECT show_json_object('"considered_execution_plans": [', TRACE)
FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;

drop table t0,t1,t2;

--echo #
--echo # Discover bound equality thanks to equality propagation
--echo # specific of ON clause.
--echo #

CREATE TABLE t1 (
  a int(11) DEFAULT NULL,
  b varchar(100) DEFAULT NULL,
  c int(11) DEFAULT NULL,
  KEY b_c_a (b,c,a)
) ENGINE=InnoDB, CHARSET utf8mb4;

explain select *
from t1 left join t1 as t2
     on (t2.a= t1.a and (t2.a,t2.b) in (select a,b from t1 as t3))
where t1.a < 5;

drop table t1;

--echo #
--echo # Show that loosescan planning is not dependent on order of
--echo # creation of indexes anymore.
--echo #

create table it(a int, b int, index a_b (a,b), index a (a))
engine=InnoDB;
insert into it values(1,1),(2,3),(4,3);

let $query=
select * from it as ot
 where (ot.a,ot.b) in (select it.a,it.b from it where it.b=3);
eval $query;

# The index on "A" is able to handle needed semi-join equalities,
# because it handles the first one, and the second one is bound due to
# b=3.

let $read_trace=
select TRACE LIKE '%"index_handles_needed_semijoin_equalities": false%'
from information_schema.optimizer_trace;
--skip_if_hypergraph  # Does not support the same optimizer trace.
eval $read_trace;

# Switch order of indexes:
drop table it;
create table it(a int, b int, index a (a),index a_b (a,b))
engine=InnoDB;
insert into it values(1,1),(2,3),(4,3);

eval $query;
eval $read_trace;

drop table it;

--echo #
--echo # Show that we reject LooseScan if no handled key parts
--echo #

CREATE TABLE t1 (a INT, b INT, KEY (a)) ENGINE=INNODB;
CREATE TABLE t2 (a INT, b INT) ENGINE=INNODB;
EXPLAIN SELECT * FROM t2 AS t3, t2
  WHERE t2.b=t3.b AND  
        (t2.b) IN (SELECT b*3 FROM t1 WHERE a=10);

--skip_if_hypergraph  # Does not support the same optimizer trace.
SELECT TRACE LIKE '%"some_index_part_used": false%'
FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;

DROP TABLE t1,t2;

--echo #
--echo # Show that we detect a hole in sequence of key parts
--echo #

CREATE TABLE ot1 (a INTEGER);
INSERT INTO ot1 VALUES (0),(1),(3),(7);

CREATE TABLE it1 (a VARCHAR(1), b INTEGER, KEY (a,b)) CHARSET utf8mb4;
INSERT INTO it1 VALUES ('a',7), ('b',7);

CREATE TABLE it2 (a VARCHAR(1), b INTEGER, KEY (a,b)) CHARSET utf8mb4;
INSERT INTO it2 VALUES ('a',7), ('b',7);

let $query=
SELECT * FROM ot1
WHERE a IN (
        SELECT it1.b
        FROM it1 JOIN it2
             ON it1.a = it2.a
);

eval explain $query;

--skip_if_hypergraph  # Does not support the same optimizer trace.
SELECT TRACE LIKE '%"index_can_remove_duplicates": false%'
FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;

DROP TABLE ot1, it1, it2;

--echo #
--echo # Show that handled keyparts cannot be on prefix
--echo #

create table t1 (a int, b varchar(100), key a_b (a,b)) charset utf8mb4;
insert into t1 values(25,'111111'),(25,'1111112');
let $query=
select * from t1 as t2 where t2.b in (select b from t1 where a=25);
eval explain $query;
eval $query;

alter table t1 drop key a_b, add key a_b_prefix (a,b(2));
eval explain $query;
# If we had not detected that key is on prefix, result would be only 1 row:
eval $query;
--skip_if_hypergraph  # Does not support the same optimizer trace.
select TRACE LIKE '%"index_handles_needed_semijoin_equalities": false%'
from information_schema.optimizer_trace;

drop table t1;

set optimizer_switch=@old_opt_switch;
set optimizer_prune_level=@old_opt_prune_level;

--echo # Tracing of CTEs using the same tmp table: observe
--echo # "reusing_tmp_table" and only one "creating_tmp_table".
with q(a) as (select 1 union all select 2)
select * from q, q q1 where q.a=1 and q1.a=2;
--skip_if_hypergraph  # Does not support the same optimizer trace.
select TRACE from information_schema.optimizer_trace;

--echo # Tracing of repetitions of the recursive member
--echo # can be silenced with repeated_subselect=off

let $query=
with recursive qn(n) as
  (select 1 union all select n+1 from qn where n<4)
select count(*) from qn;
eval $query;
--skip_if_hypergraph  # Does not support the same optimizer trace.
SELECT show_json_object('"join_execution": {', TRACE)
FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;
SET @@optimizer_trace_features="repeated_subselect=off";
eval $query;
--skip_if_hypergraph  # Does not support the same optimizer trace.
SELECT show_json_object('"join_execution": {', TRACE)
FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;

--echo # Test removal of LIMIT 1 when no OFFSET in scalar subquery
--echo # with implicit grouping
CREATE TABLE t1(a INT, b INT);

SELECT 1 FROM t1 WHERE 1 LIKE
 (SELECT MIN(b) FROM t1 WHERE a = 1 ORDER BY MIN(b) LIMIT 1);
--skip_if_hypergraph  # Does not support the same optimizer trace.
SELECT * FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;

SELECT 1 FROM t1 WHERE 1 LIKE
 (SELECT MIN(b) FROM t1 WHERE a = 1 LIMIT 1);
--skip_if_hypergraph  # Does not support the same optimizer trace.
SELECT * FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;

DROP TABLE t1;


SET @@optimizer_trace_features=default;

drop function show_json_object;
