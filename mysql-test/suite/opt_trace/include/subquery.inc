# Test for optimizer tracing of subqueries

--source include/have_optimizer_trace.inc
--source include/have_64bit.inc

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
SELECT * FROM information_schema.OPTIMIZER_TRACE;
--echo

--echo # Subselect execute is traced only the first time it is executed
SET @@optimizer_trace_features="greedy_search=off,repeated_subselect=off";
SELECT (SELECT a FROM t1 WHERE t1.a=t2.a), a FROM t2;
--echo
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
SELECT * FROM information_schema.OPTIMIZER_TRACE;
--echo

# evaluate_subselect_cond_steps for remove_eq_conds
SELECT 1 FROM DUAL
WHERE NOT EXISTS
  (SELECT * FROM t2 WHERE a = 50 AND b = 3);
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;
--echo

# Distinct, order and group is removed from subquery. Note: For PS,
# removal happens during prepare so the only visible effect is that
# the subquery does not contain those clauses.
SELECT 1 FROM DUAL WHERE NOT EXISTS (SELECT DISTINCT(a) FROM t2 GROUP BY a ORDER BY b);
--echo
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
INSERT INTO t4 (
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
) ENGINE=MYISAM;
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

select * from information_schema.optimizer_trace;
drop table t1,t2;
