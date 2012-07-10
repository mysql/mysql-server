#
# DuplicateElimination strategy test
#

-- disable_query_log
-- disable_result_log
SET GLOBAL innodb_stats_persistent=0;
-- enable_result_log
-- enable_query_log

create table t0 (a int);
insert into t0 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);

# First test simple cases: I20 order, no join buffering.

create table t1 (
  a int, 
  b int
) engine=innodb;
insert into t1 values (1,1),(1,1),(2,2);

create table t2 (
  a int,
  b int,
  key(b)
) engine=innodb;
insert into t2 select a, a/2 from t0;

select * from t1;
select * from t2;
explain select * from t2 where b in (select a from t1);
select * from t2 where b in (select a from t1);

# Try an InnoDB table with very long rowid
create table t3 (
   a int, 
   b int,
   key(b),
   pk1 char(200), pk2 char(200), pk3 char(200),
   primary key(pk1, pk2, pk3)
) engine=innodb;
insert into t3 select a,a, a,a,a from t0;

explain select * from t3 where b in (select a from t1);
select * from t3 where b in (select a from t1);

# Test overflow to MyISAM:
set @save_max_heap_table_size= @@max_heap_table_size;
set max_heap_table_size=16384;
set @save_join_buffer_size = @@join_buffer_size;
set join_buffer_size= 8192;

drop table t3;
create table t3 (
   a int, 
   b int,
   key(b),
   pk1 char(200), pk2 char(200),
   primary key(pk1, pk2)
) engine=innodb;
insert into t3 select 
  A.a + 10*B.a, A.a + 10*B.a, A.a + 10*B.a, A.a + 10*B.a 
from t0 A, t0 B where B.a <5;

--replace_column 9 #
explain select * from t3 where b in (select a from t0);
--sorted_result
select * from t3 where b in (select A.a+B.a from t0 A, t0 B where B.a<5);
 
set join_buffer_size= @save_join_buffer_size;
set max_heap_table_size= @save_max_heap_table_size;

# O2I join orders, with shortcutting: 
explain select * from t1 where a in (select b from t2);
select * from t1;
select * from t1 where a in (select b from t2);

drop table t0, t1, t2, t3;
# (no need for anything in range/index_merge/DS-MRR) 

#
# BUG#34799: crash or/and memory overrun with dependant subquery and some joins
#
create table t1 (a int);
insert into t1 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);

create table t2 (a char(200), b char(200), c char(200), primary key (a,b,c)) engine=innodb;
insert into t2 select concat(a, repeat('X',198)),repeat('B',200),repeat('B',200) from t1;
insert into t2 select concat(a, repeat('Y',198)),repeat('B',200),repeat('B',200) from t1;
alter table t2 add filler1 int;

insert into t1 select A.a + 10*(B.a + 10*C.a) from t1 A, t1 B, t1 C;

set @save_join_buffer_size=@@join_buffer_size; 
--disable_warnings
set join_buffer_size=1;
--enable_warnings

select * from t2 where filler1 in ( select a from t1);
set join_buffer_size=default;

drop table t1, t2; 


--echo
--echo BUG#42740: crash in optimize_semijoin_nests
--echo
create table t1 (c6 timestamp,key (c6)) engine=innodb;
create table t2 (c2 double) engine=innodb;
explain select 1 from t2 where c2 = any (select log10(null) from t1 where c6 <null)  ;
drop table t1, t2;

--echo # 
--echo # BUG#42742: crash in setup_sj_materialization, Copy_field::set
--echo # 
create table t3 ( c1 year) engine=innodb;
insert into t3 values (2135),(2142);
create table t2 (c1 tinytext,c2 text,c6 timestamp) engine=innodb;
-- echo # The following must not crash, EXPLAIN should show one SJ strategy, not a mix:
explain select 1 from t2 where 
  c2 in (select 1 from t3, t2) and
  c1 in (select convert(c6,char(1)) from t2);
drop table t2, t3;
--echo # 
--echo # BUG#57431: subquery returns wrong result (semijoin=on) with pred AND
--echo # 
CREATE TABLE t1 (
   i INT
) ENGINE=InnoDB;
INSERT INTO t1 VALUES (2),(4);

CREATE TABLE t2 (
   i INT,
   vc VARCHAR(1)
) ENGINE=InnoDB;
INSERT INTO t2 VALUES (8,NULL);

SELECT i
FROM t1
WHERE i IN (SELECT innr.i
             FROM t2 LEFT JOIN t2 innr ON innr.vc)
   AND i = 2;

DROP TABLE t1, t2;

-- disable_query_log
-- disable_result_log
SET GLOBAL innodb_stats_persistent=default;
-- enable_result_log
-- enable_query_log
