# Continuation of tests for optimizer trace

--source include/have_optimizer_trace.inc

let $DEFAULT_TRACE_MEM_SIZE=1048576; # 1MB
eval set optimizer_trace_max_mem_size=$DEFAULT_TRACE_MEM_SIZE;

set end_markers_in_json=on;
set optimizer_trace="enabled=on";

--echo # check that if a sub-statement should not be traced,
--echo # it is not traced even if inside a traced top statement
--echo
set optimizer_trace_offset=0, optimizer_trace_limit=100;
delimiter |;
create function f1(arg char(1)) returns int
begin
  declare res int;
  declare dummy varchar(1);
  select 1 into res from dual;
  select TRACE+NULL into dummy from information_schema.OPTIMIZER_TRACE limit 1;
  select 2 into res from dual;
  return 3;
end|
# ps-protocol specific note: as we asked to retain all traces,
# we see the one of PREPARE too.
select f1("c")|
--echo
# we should not see the trace of "select TRACE+NULL..."
# because tracing is disabled when OPTIMIZER_TRACE table is used.
select * from information_schema.OPTIMIZER_TRACE|
delimiter ;|
set optimizer_trace_offset=default, optimizer_trace_limit=default;
drop function f1;

--echo # check that if a tracing gets disabled in a routine's  body,
--echo # substatements are not traced
--echo
set optimizer_trace_offset=0, optimizer_trace_limit=100;
delimiter |;
create function f1(arg char(1)) returns int
begin
  declare res int;
  declare dummy varchar(1);
  set optimizer_trace="enabled=off";
  select 1 into res from dual;
  select TRACE+NULL into dummy from information_schema.OPTIMIZER_TRACE limit 1;
  select 2 into res from dual;
  return 3;
end|
select f1("c")|
--echo
select * from information_schema.OPTIMIZER_TRACE|
delimiter ;|
set optimizer_trace_offset=default, optimizer_trace_limit=default;
select @@optimizer_trace;
set optimizer_trace="enabled=on";
drop function f1;

--echo
--echo # Check that if a sub-statement reads OPTIMIZER_TRACE,
--echo # thus reading the unfinished trace of its caller statement,
--echo # there is no crash.
--echo

create temporary table optt
(id int primary key auto_increment,
QUERY varchar(200),
TRACE text);
create table t1 (a int, key(a));
insert into t1 values(2);
set optimizer_trace_offset=0, optimizer_trace_limit=100;
delimiter |;
create function f1(arg char(1)) returns int
begin
  declare res int;
  insert into optt select NULL, QUERY, TRACE from information_schema.OPTIMIZER_TRACE;
  return 3;
end|
select * from t1 where a in (select f1("c") from t1)|
--echo
delimiter ;|
set optimizer_trace="enabled=off";
--echo this should find unfinished traces
select count(*) from optt where TRACE NOT LIKE "%] /* steps */\n}";
select count(*)<>0 from optt;
--echo this should not
select count(*) from information_schema.OPTIMIZER_TRACE where TRACE NOT LIKE "%] /* steps */\n}";
select count(*)<>0 from information_schema.OPTIMIZER_TRACE;

set optimizer_trace_offset=default, optimizer_trace_limit=default;
drop temporary table optt;
drop function f1;
drop table t1;
set optimizer_trace="enabled=on";

--echo
--echo # check of crash with I_S.VIEWS (TABLE_LIST::alias==NULL)
--echo
create table t1(a int, b int);
create view v1 as select a from t1;
select VIEW_DEFINITION from information_schema.VIEWS
where TABLE_SCHEMA="test" and TABLE_NAME="v1";
drop table t1;
drop view v1;

--echo
--echo # check for readable display of BIT values
--echo
create table t1 (a bit(5), key(a));
insert into t1 values(b'00000'),(b'01101');
select cast(a as unsigned) from t1 where a > b'01100';
# Note that in the trace we get either 0x0c or 12
select TRACE from information_schema.OPTIMIZER_TRACE;
drop table t1;

--echo
--echo # check that trace lists all pushed down ON conditions
--echo
create table t1 (i int not null);
insert into t1 values (0),    (2),(3),(4);
create table t2 (i int not null);
insert into t2 values (0),(1),    (3),(4);
create table t3 (i int not null);
insert into t3 values (0),(1),(2),    (4);
select * from
 t1 LEFT JOIN
 ( t2 LEFT JOIN
   ( t3 
   )
   ON t3.i = t2.i
 )
 ON t2.i = t1.i
 WHERE t3.i IS NULL
 ;
select TRACE from information_schema.OPTIMIZER_TRACE;
drop table t1,t2,t3;

--echo
--echo # test of tracing a query with an HAVING condition, in
--echo # ps-protocol, does not crash
--echo
# Comes from having.test

CREATE TABLE t1 (f1 INT, f2 VARCHAR(1)) CHARSET utf8mb4;
INSERT INTO t1 VALUES (16,'f');
INSERT INTO t1 VALUES (16,'f');
CREATE TABLE t2 (f1 INT, f2 VARCHAR(1)) CHARSET utf8mb4;
INSERT INTO t2 VALUES (13,'f');
INSERT INTO t2 VALUES (20,'f');
CREATE TABLE t3 (f1 INT, f2 VARCHAR(1)) CHARSET utf8mb4;
INSERT INTO t3 VALUES (7,'f');
--source include/turn_off_only_full_group_by.inc

EXPLAIN SELECT t1.f2 FROM t1
STRAIGHT_JOIN (t2 JOIN t3 ON t3.f2  = t2.f2  ) ON t3 .f2  = t2 .f2
HAVING ('v', 'i') NOT IN (SELECT f2, MIN(f2) FROM t1)
ORDER BY f2;

SELECT t1.f2 FROM t1
STRAIGHT_JOIN (t2 JOIN t3 ON t3.f2  = t2.f2  ) ON t3 .f2  = t2 .f2
HAVING ('v', 'i') NOT IN (SELECT f2, MIN(f2) FROM t1)
ORDER BY f2;
--replace_regex /("sort_buffer_size":) [0-9]+/\1 "NNN"/ /("key_size":) [0-9]+/\1 "XXX"/ /("row_size":) [0-9]+/\1 "XXX"/
select TRACE from information_schema.OPTIMIZER_TRACE;

--source include/restore_sql_mode_after_turn_off_only_full_group_by.inc
DROP TABLES t1,t2,t3;

--echo
--echo # Test that tracing a query with a materialized FROM-clause
--echo # derived table using a GROUP BY, does not crash
--echo
# Comes from profiling.test
create table t1 (a int, b int);
insert into t1 values (1,1), (2,null), (3, 4);
select max(x) from (select sum(a) as x from t1 group by b) as teeone;
--replace_regex /("sort_buffer_size":) [0-9]+/\1 "NNN"/
select TRACE from information_schema.OPTIMIZER_TRACE;
drop table t1;

--echo
--echo # To have no crash above, we had to restore the ref_array at
--echo # end of JOIN::exec(). This impacts how the query looks like,
--echo # but not too much, as seen in the error message below.
--echo # Comes from func_gconcat.test.
--echo
CREATE TABLE t1(f1 int);
INSERT INTO t1 values (0),(0);
set optimizer_trace="enabled=off";
--disable_ps_protocol
--error ER_ILLEGAL_VALUE_FOR_TYPE
SELECT POLYGON((SELECT 1 FROM (SELECT 1 IN (GROUP_CONCAT(t1.f1)) FROM t1, t1 t GROUP BY t.f1 ) d));
--enable_ps_protocol
set optimizer_trace="enabled=on";
--disable_ps_protocol
--error ER_ILLEGAL_VALUE_FOR_TYPE
SELECT POLYGON((SELECT 1 FROM (SELECT 1 IN (GROUP_CONCAT(t1.f1)) FROM t1, t1 t GROUP BY t.f1 ) d));
--enable_ps_protocol
DROP TABLE t1;

--echo
--echo # Check that SQL PREPARE and SQL EXECUTE each produce one trace.
--echo
set optimizer_trace_offset=0, optimizer_trace_limit=100;
prepare stmt from "select 1";
select * from information_schema.OPTIMIZER_TRACE;
set optimizer_trace_offset=0, optimizer_trace_limit=100;
execute stmt;
select * from information_schema.OPTIMIZER_TRACE;
deallocate prepare stmt;
set optimizer_trace_offset=default, optimizer_trace_limit=default;

--echo
--echo # Test of SELECTs in IF in stored routine.
--echo # Same test for CASE WHEN.
--echo
create table t1 (a int);
delimiter |;
create procedure p1()
begin
  if exists(select 1) then
    insert into t1 values(1);
  end if;
  if exists(select 2) then
    insert into t1 values(2);
  end if;
  if (select count(*) from t1) then
    insert into t1 values(3);
  end if;
  set @a=(select count(a) from t1 where a>0);
  case (select count(a) from t1 where a>1)
    when 2 then set @b=2;
    else set @b=3;
  end case;
end|
delimiter ;|
set optimizer_trace_offset=0, optimizer_trace_limit=100;
set @old_max=@@optimizer_trace_max_mem_size;
set optimizer_trace_max_mem_size=40000;
call p1();
# SET @a=(SELECT) is not traced because part of SET
# which is a real command and not traced.
select * from information_schema.OPTIMIZER_TRACE;
select * from t1;
select @a,@b;
set optimizer_trace_max_mem_size=@old_max;
drop procedure p1;
drop table t1;

--echo
--echo # Test of tracing of DO.
--echo

set optimizer_trace_offset=0, optimizer_trace_limit=100;
do (select 42);
select * from information_schema.OPTIMIZER_TRACE;

--echo
--echo # Test of tracing of subquery used in parameter of routine call
--echo
create table t1(a int);
insert into t1 values(1),(2);
delimiter |;
create procedure p1(x int)
begin
  declare b int;
  set b=(select 2+x from dual);
end|
delimiter ;|
set optimizer_trace_offset=0, optimizer_trace_limit=100;
call p1((select a from t1 limit 1));
select * from information_schema.OPTIMIZER_TRACE;
drop procedure p1;
drop table t1;
set optimizer_trace_offset=default, optimizer_trace_limit=default;

--echo
--echo # Test that printing expanded query does not alter query's
--echo # results.
--echo # Comes from ctype_utf8mb4_heap.test
--echo
create table t1 (f1 varchar(1) not null) default charset utf8mb4;
insert into t1 values (''), ('');
select concat(concat(_latin1'->',f1),_latin1'<-') from t1;
select * from information_schema.optimizer_trace;
drop table t1;

--echo
--echo # Bug#12546331 - SEGFAULT IN SUBSELECT_INDEXSUBQUERY_ENGINE::PRINT WITH OPTIMIZER TRACE
--echo

CREATE TABLE t1 (  
  col_int_nokey INT,  
  col_int_key INT,  
  col_varchar_key varchar(1),
  KEY col_int_key (col_int_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
);

INSERT INTO t1 VALUES
  (NULL,8,'x'),
  (8,7,'d'),
  (1,1,'r'),
  (9,7,'f'),
  (4,9,'y'),
  (3,NULL,'u'),
  (2,1,'m'),
  (NULL,9,NULL),
  (2,2,'o'),
  (NULL,9,'w'),
  (6,2,'m'),
  (7,4,'q'),
  (2,0,NULL),
  (5,4,'d'),
  (7,8,'g'),
  (6,NULL,'x'),
  (6,NULL,'f'),
  (2,0,'p'),
  (9,NULL,'j'),
  (6,8,'c')
;

CREATE TABLE t2 (
  col_int_nokey INT,
  col_int_key INT,
  col_varchar_key varchar(1),
  KEY col_int_key (col_int_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
);

INSERT INTO t2 VALUES
  (2,4,'v'),
  (150,62,'v'),
  (NULL,7,'c'),
  (2,1,NULL),
  (5,0,'x'),
  (3,7,'i'),
  (1,7,'e'),
  (4,1,'p'),
  (NULL,7,'s'),
  (2,1,'j'),
  (6,5,'z'),
  (6,2,'c'),
  (8,0,'a'),
  (2,1,'q'),
  (6,8,'y'),
  (8,1,NULL),
  (3,1,'r'),
  (3,9,'v'),
  (9,1,NULL),
  (6,5,'r')
;

SELECT col_int_nokey
FROM (
  SELECT *
  FROM t2
  WHERE col_varchar_key > 'a'
    OR ( 7 , 5 ) NOT IN (
      SELECT col_int_nokey , col_int_key
      FROM t1 )
  ) AS alias1;

DROP TABLE t1;
DROP TABLE t2;

--echo
--echo BUG#12552262 - INVALID JSON WITH TWO CALLS TO TEST_QUICK_SELECT
--echo

CREATE TABLE t1 (
  col_varchar_10_latin1_key varchar(10) CHARACTER SET latin1 DEFAULT NULL,
  col_int_key INT,
  KEY col_int_key (col_int_key)
);

CREATE TABLE t2 (
  col_varchar_10_latin1_key varchar(10) CHARACTER SET latin1 DEFAULT NULL,
  col_varchar_10_utf8_key varchar(10) CHARACTER SET utf8 DEFAULT NULL,
  col_int_key INT,
  KEY col_varchar_10_utf8_key (col_varchar_10_utf8_key),
  KEY col_int_key (col_int_key)
);

INSERT INTO t2 VALUES ('qykbaqfyhz','l',NULL);

CREATE TABLE t3 (
  col_int_key INT,
  col_varchar_10_utf8_key varchar(10) CHARACTER SET utf8 DEFAULT NULL,
  col_varchar_10_latin1_key varchar(10) CHARACTER SET latin1 DEFAULT NULL,
  KEY col_varchar_10_utf8_key (col_varchar_10_utf8_key),
  KEY col_varchar_10_latin1_key (col_varchar_10_latin1_key)
);

INSERT INTO t3 VALUES (0,'s','it');
INSERT INTO t3 VALUES (9,'IQTHK','JCAQM');

SELECT table2.col_int_key
FROM t3 AS table1
  LEFT JOIN t1 AS table2 ON table1.col_int_key < table2.col_int_key
  LEFT JOIN t2 AS table3 ON table2.col_varchar_10_latin1_key >=
table3.col_varchar_10_utf8_key
;

select * from information_schema.optimizer_trace;

DROP TABLE t1,t2,t3;

--echo
--echo Tests of tracing of the "eq_ref optimization" of plan search
--echo

# test for trace point "chosen:true","pruned_by_cost:true" and
# "added_to_eq_ref_extension:true" (from main.subquery_sj_none_jcl7)

create table t0 (a int);
insert into t0 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);
create table t1 (a int, b int, key(a));
create table t2 (a int, b int, key(a));
create table t3 (a int, b int, key(a));
insert into t1 select a,a from t0;
insert into t2 select a,a from t0;
insert into t3 select a,a from t0;

set @old_opt_switch=@@optimizer_switch;
# The SET below must not be output, because only servers supporting
# semijoin will execute it (would make varying output).
if (`select locate('semijoin', @@optimizer_switch) > 0`)
{
--disable_query_log
  set optimizer_switch="semijoin=off,materialization=off";
--enable_query_log
}
explain select * 
from t0 where a in
(select t2.a+t3.a from t1 left join (t2 join t3) on t2.a=t1.a and t3.a=t1.a);

select * from information_schema.optimizer_trace;
set optimizer_switch=@old_opt_switch;
drop table t0,t1,t2,t3;

# test for trace point "added_to_eq_ref_extension:false" (from main.derived)

CREATE TABLE t1 (
OBJECTID int(11) NOT NULL default '0',
SORTORDER int(11) NOT NULL auto_increment,
KEY t1_SortIndex (SORTORDER),
KEY t1_IdIndex (OBJECTID)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
CREATE TABLE t2 (
ID int(11) default NULL,
PARID int(11) default NULL,
UNIQUE KEY t2_ID_IDX (ID),
KEY t2_PARID_IDX (PARID)
) engine=MyISAM DEFAULT CHARSET=latin1;
INSERT INTO t2 VALUES (1000,0),(1001,0),(1002,0),(1003,0),(1008,1),(1009,1),(1010,1),(1011,1),(1016,2);
CREATE TABLE t3 (
ID int(11) default NULL,
DATA decimal(10,2) default NULL,
UNIQUE KEY t3_ID_IDX (ID)
) engine=MyISAM DEFAULT CHARSET=latin1;
INSERT INTO t3 VALUES (1000,0.00),(1001,0.25),(1002,0.50),(1003,0.75),(1008,1.00),(1009,1.25),(1010,1.50),(1011,1.75);

select 497, tmp.ID, NULL from (select 497 as ID, MAX(t3.DATA) as DATA      from t1 join t2 on (t1.ObjectID = t2.ID) join t3 on (t1.ObjectID = t3.ID) group by t2.ParID order by DATA DESC) as tmp;
select * from information_schema.optimizer_trace;
drop table t1,t2,t3;

# test of multiple nested trace points "added_to_eq_ref_extension:true"
# (eq_ref optimization finding a sequence of eq_ref-joined tables) (from
# main.type_blob)

CREATE TABLE t1 (
t1_id bigint(21) NOT NULL auto_increment,
_field_72 varchar(128) DEFAULT '' NOT NULL,
_field_95 varchar(32),
_field_115 tinyint(4) DEFAULT '0' NOT NULL,
_field_122 tinyint(4) DEFAULT '0' NOT NULL,
_field_126 tinyint(4),
_field_134 tinyint(4),
PRIMARY KEY (t1_id),
UNIQUE _field_72 (_field_72),
KEY _field_115 (_field_115),
KEY _field_122 (_field_122)
);
INSERT INTO t1 VALUES (1,'admin','21232f297a57a5a743894a0e4a801fc3',0,1,NULL,NULL);
INSERT INTO t1 VALUES (2,'hroberts','7415275a8c95952901e42b13a6b78566',0,1,NULL,NULL);
INSERT INTO t1 VALUES (3,'guest','d41d8cd98f00b204e9800998ecf8427e',1,0,NULL,NULL);

CREATE TABLE t2 (
seq_0_id bigint(21) DEFAULT '0' NOT NULL,
seq_1_id bigint(21) DEFAULT '0' NOT NULL,
PRIMARY KEY (seq_0_id,seq_1_id)
);
INSERT INTO t2 VALUES (1,1);
INSERT INTO t2 VALUES (2,1);
INSERT INTO t2 VALUES (2,2);
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
CREATE TABLE t3 (
t3_id bigint(21) NOT NULL auto_increment,
_field_131 varchar(128),
_field_133 tinyint(4) DEFAULT '0' NOT NULL,
_field_135 datetime DEFAULT '0000-00-00 00:00:00' NOT NULL,
_field_137 tinyint(4),
_field_139 datetime DEFAULT '0000-00-00 00:00:00' NOT NULL,
_field_140 blob,
_field_142 tinyint(4) DEFAULT '0' NOT NULL,
_field_145 tinyint(4) DEFAULT '0' NOT NULL,
_field_148 tinyint(4) DEFAULT '0' NOT NULL,
PRIMARY KEY (t3_id),
KEY _field_133 (_field_133),
KEY _field_135 (_field_135),
KEY _field_139 (_field_139),
KEY _field_142 (_field_142),
KEY _field_145 (_field_145),
KEY _field_148 (_field_148)
);
INSERT INTO t3 VALUES (1,'test job 1',0,'0000-00-00 00:00:00',0,'1999-02-25 22:43:32','test\r\njob\r\n1',0,0,0);
INSERT INTO t3 VALUES (2,'test job 2',0,'0000-00-00 00:00:00',0,'1999-02-26 21:08:04','',0,0,0);
SET sql_mode = default;
CREATE TABLE t4 (
seq_0_id bigint(21) DEFAULT '0' NOT NULL,
seq_1_id bigint(21) DEFAULT '0' NOT NULL,
PRIMARY KEY (seq_0_id,seq_1_id)
);
INSERT INTO t4 VALUES (1,1);
INSERT INTO t4 VALUES (2,1);

CREATE TABLE t5 (
t5_id bigint(21) NOT NULL auto_increment,
_field_149 tinyint(4),
_field_156 varchar(128) DEFAULT '' NOT NULL,
_field_157 varchar(128) DEFAULT '' NOT NULL,
_field_158 varchar(128) DEFAULT '' NOT NULL,
_field_159 varchar(128) DEFAULT '' NOT NULL,
_field_160 varchar(128) DEFAULT '' NOT NULL,
_field_161 varchar(128) DEFAULT '' NOT NULL,
PRIMARY KEY (t5_id),
KEY _field_156 (_field_156),
KEY _field_157 (_field_157),
KEY _field_158 (_field_158),
KEY _field_159 (_field_159),
KEY _field_160 (_field_160),
KEY _field_161 (_field_161)
);
INSERT INTO t5 VALUES (1,0,'tomato','','','','','');
INSERT INTO t5 VALUES (2,0,'cilantro','','','','','');

CREATE TABLE t6 (
seq_0_id bigint(21) DEFAULT '0' NOT NULL,
seq_1_id bigint(21) DEFAULT '0' NOT NULL,
PRIMARY KEY (seq_0_id,seq_1_id)
);
INSERT INTO t6 VALUES (1,1);
INSERT INTO t6 VALUES (1,2);
INSERT INTO t6 VALUES (2,2);

CREATE TABLE t7 (
t7_id bigint(21) NOT NULL auto_increment,
_field_143 tinyint(4),
_field_165 varchar(32),
_field_166 smallint(6) DEFAULT '0' NOT NULL,
PRIMARY KEY (t7_id),
KEY _field_166 (_field_166)
);
INSERT INTO t7 VALUES (1,0,'High',1);
INSERT INTO t7 VALUES (2,0,'Medium',2);
INSERT INTO t7 VALUES (3,0,'Low',3);
--source include/turn_off_only_full_group_by.inc

select
replace(t3._field_140, "\r","^M"),
 t3_id,
 min(t3._field_131),
 min(t3._field_135),
 min(t3._field_139),
 min(t3._field_137),
 min(link_alias_142._field_165),
 min(link_alias_133._field_72),
 min(t3._field_145),
 min(link_alias_148._field_156),
 replace(min(t3._field_140), "\r","^M"),
 t3.t3_id
from
 t3 left join t4 on t4.seq_0_id = t3.t3_id
 left join t7 link_alias_142 on t4.seq_1_id = link_alias_142.t7_id
 left join t6 on t6.seq_0_id = t3.t3_id
 left join t1 link_alias_133 on t6.seq_1_id = link_alias_133.t1_id
 left join t2 on t2.seq_0_id = t3.t3_id
 left join t5 link_alias_148 on t2.seq_1_id = link_alias_148.t5_id
where
 t3.t3_id in (1)
group by
 t3.t3_id
order by
 link_alias_142._field_166,
 _field_139,
 link_alias_133._field_72,
 _field_135,
  link_alias_148._field_156
;
select * from information_schema.optimizer_trace;

--source include/restore_sql_mode_after_turn_off_only_full_group_by.inc
drop table t1,t2,t3,t4,t5,t6,t7;

--echo #
--echo # Tracing of ORDER BY & GROUP BY simplification.
--echo #

# this is originally the testcase for
# Bug#12699645 SELECT SUM() + STRAIGHT_JOIN QUERY MISSES ROWS
CREATE TABLE t1 (
pk INT, col_int_key INT,
col_varchar_key VARCHAR(1), col_varchar_nokey VARCHAR(1)
);
INSERT INTO t1 VALUES
(10,7,'v','v'),(11,0,'s','s'),(12,9,'l','l'),(13,3,'y','y'),(14,4,'c','c'),
(15,2,'i','i'),(16,5,'h','h'),(17,3,'q','q'),(18,1,'a','a'),(19,3,'v','v'),
(20,6,'u','u'),(21,7,'s','s'),(22,5,'y','y'),(23,1,'z','z'),(24,204,'h','h'),
(25,224,'p','p'),(26,9,'e','e'),(27,5,'i','i'),(28,0,'y','y'),(29,3,'w','w');

CREATE TABLE t2 (
pk INT, col_int_key INT,
col_varchar_key VARCHAR(1), col_varchar_nokey VARCHAR(1),
PRIMARY KEY (pk)
);
INSERT INTO t2 VALUES
(1,4,'b','b'),(2,8,'y','y'),(3,0,'p','p'),(4,0,'f','f'),(5,0,'p','p'),
(6,7,'d','d'),(7,7,'f','f'),(8,5,'j','j'),(9,3,'e','e'),(10,188,'u','u'),
(11,4,'v','v'),(12,9,'u','u'),(13,6,'i','i'),(14,1,'x','x'),(15,5,'l','l'),
(16,6,'q','q'),(17,2,'n','n'),(18,4,'r','r'),(19,231,'c','c'),(20,4,'h','h'),
(21,3,'k','k'),(22,3,'t','t'),(23,7,'t','t'),(24,6,'k','k'),(25,7,'g','g'),
(26,9,'z','z'),(27,4,'n','n'),(28,4,'j','j'),(29,2,'l','l'),(30,1,'d','d'),
(31,2,'t','t'),(32,194,'y','y'),(33,2,'i','i'),(34,3,'j','j'),(35,8,'r','r'),
(36,4,'b','b'),(37,9,'o','o'),(38,4,'k','k'),(39,5,'a','a'),(40,5,'f','f'),
(41,9,'t','t'),(42,3,'c','c'),(43,8,'c','c'),(44,0,'r','r'),(45,98,'k','k'),
(46,3,'l','l'),(47,1,'o','o'),(48,0,'t','t'),(49,189,'v','v'),(50,8,'x','x'),
(51,3,'j','j'),(52,3,'x','x'),(53,9,'k','k'),(54,6,'o','o'),(55,8,'z','z'),
(56,3,'n','n'),(57,9,'c','c'),(58,5,'d','d'),(59,9,'s','s'),(60,2,'j','j'),
(61,2,'w','w'),(62,5,'f','f'),(63,8,'p','p'),(64,6,'o','o'),(65,9,'f','f'),
(66,0,'x','x'),(67,3,'q','q'),(68,6,'g','g'),(69,5,'x','x'),(70,8,'p','p'),
(71,2,'q','q'),(72,120,'q','q'),(73,25,'v','v'),(74,1,'g','g'),(75,3,'l','l'),
(76,1,'w','w'),(77,3,'h','h'),(78,153,'c','c'),(79,5,'o','o'),(80,9,'o','o'),
(81,1,'v','v'),(82,8,'y','y'),(83,7,'d','d'),(84,6,'p','p'),(85,2,'z','z'),
(86,4,'t','t'),(87,7,'b','b'),(88,3,'y','y'),(89,8,'k','k'),(90,4,'c','c'),
(91,6,'z','z'),(92,1,'t','t'),(93,7,'o','o'),(94,1,'u','u'),(95,0,'t','t'),
(96,2,'k','k'),(97,7,'u','u'),(98,2,'b','b'),(99,1,'m','m'),(100,5,'o','o');

# We see the functional dependency implied by ON

let $query=
SELECT SUM(alias2.col_varchar_nokey) , alias2.pk AS field2 FROM t1 AS alias1
STRAIGHT_JOIN t2 AS alias2 ON alias2.pk = alias1.col_int_key WHERE alias1.pk
GROUP BY field2 ORDER BY alias1.col_int_key,alias2.pk ;
eval $query;
--replace_regex /("sort_buffer_size":) [0-9]+/\1 "NNN"/ /("key_size":) [0-9]+/\1 "XXX"/ /("row_size":) [0-9]+/\1 "XXX"/
SELECT * FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;

DROP TABLE t1,t2;

--echo #
--echo # Trace of "condition on constant tables"
--echo #
create table t1(a int) engine=myisam;
insert into t1 values(26);
create table t2(b int primary key, c int) engine=myisam;
insert into t2 values(1,100),(2,200),(3,300);
select * from t1,t2 where t1.a+t2.c=cos(10) and t2.b=2;
SELECT * FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;
drop table t1,t2;

--echo #
--echo # Trace of non-default db
--echo #
create table t1(a int);
insert into t1 values(1),(2),(3);
create database mysqltest2;
create table mysqltest2.t2(a int);
insert into mysqltest2.t2 values(1),(2);
select * from t1,mysqltest2.t2;
SELECT * FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;
drop table t1;
drop database mysqltest2;
