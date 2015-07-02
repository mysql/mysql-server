################################################################################
# inc/gcol_keys.inc                                                            #
#                                                                              #
# Purpose:                                                                     #
#  Testing keys, indexes defined upon generated columns.                         #
#                                                                              #
#                                                                              #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: Andrey Zhakov                                               #
# Original Date: 2008-09-02                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################


--echo #            - UNIQUE KEY
--echo #            - INDEX
--echo #            - FULLTEXT INDEX
--echo #            - SPATIAL INDEX (not supported)
--echo #            - FOREIGN INDEX (partially supported)
--echo #            - CHECK (allowed but not used)

--echo # UNIQUE
--error ER_KEY_BASED_ON_GENERATED_COLUMN
create table t1 (a int, b int generated always as (a*2) virtual unique);
create table t1 (a int, b int generated always as (a*2) stored unique);
show create table t1;
describe t1;
drop table t1;

--error ER_KEY_BASED_ON_GENERATED_COLUMN
create table t1 (a int, b int generated always as (a*2) virtual, unique key (b));
create table t1 (a int, b int generated always as (a*2) stored, unique (b));
show create table t1;
describe t1;
drop table t1;

create table t1 (a int, b int generated always as (a*2) virtual);
--error ER_KEY_BASED_ON_GENERATED_COLUMN
alter table t1 add unique key (b);
drop table t1;
create table t1 (a int, b int generated always as (a*2) stored);
alter table t1 add unique key (b);
drop table t1;

--echo # Testing data manipulation operations involving UNIQUE keys 
--echo # on generated columns can be found in:
--echo #  - gcol_ins_upd.inc
--echo #  - gcol_select.inc

--echo # 
--echo # INDEX
--error ER_KEY_BASED_ON_GENERATED_COLUMN
create table t1 (a int, b int generated always as (a*2) virtual, index (b));
--error ER_KEY_BASED_ON_GENERATED_COLUMN
create table t1 (a int, b int generated always as (a*2) virtual, index (a,b));

create table t1 (a int, b int generated always as (a*2) stored, index (b));
show create table t1;
describe t1;
drop table t1;

create table t1 (a int, b int generated always as (a*2) stored, index (a,b));
show create table t1;
describe t1;
drop table t1;

create table t1 (a int, b int generated always as (a*2) virtual);
--error ER_KEY_BASED_ON_GENERATED_COLUMN
alter table t1 add index (b);
--error ER_KEY_BASED_ON_GENERATED_COLUMN
alter table t1 add index (a,b);
drop table t1;

create table t1 (a int, b int generated always as (a*2) stored);
alter table t1 add index (b);
drop table t1;

create table t1 (a int, b int generated always as (a*2) stored);
alter table t1 add index (a,b);
create table t2 like t1;
drop table t2;
drop table t1;

--echo # Testing data manipulation operations involving INDEX
--echo # on generated columns can be found in:
--echo #  - gcol_select.inc

--echo #
--echo # TODO: FULLTEXT INDEX

--echo # SPATIAL INDEX
if (!$skip_spatial_index_check)
{
  --echo # Error "All parts of a SPATIAL index must be geometrical"
  --error 1687
  create table t1 (a int, b int generated always as (a+1) stored, spatial index (b));
  create table t1 (a int, b int generated always as (a+1) stored);
  --error 1687
  alter table t1 add spatial index (b);
  drop table t1;
}

--echo # FOREIGN KEY

--echo # Rejected FK options.
--error ER_WRONG_FK_OPTION_FOR_GENERATED_COLUMN 
create table t1 (a int, b int generated always as (a+1) stored,
                 foreign key (b) references t2(a) on update set null);
--error ER_WRONG_FK_OPTION_FOR_GENERATED_COLUMN 
create table t1 (a int, b int generated always as (a+1) stored,
                 foreign key (b) references t2(a) on update cascade);
--error ER_WRONG_FK_OPTION_FOR_GENERATED_COLUMN 
create table t1 (a int, b int generated always as (a+1) stored,
                 foreign key (b) references t2(a) on delete set null);

create table t1 (a int, b int generated always as (a+1) stored);
--error ER_WRONG_FK_OPTION_FOR_GENERATED_COLUMN 
alter table t1 add foreign key (b) references t2(a) on update set null;
--error ER_WRONG_FK_OPTION_FOR_GENERATED_COLUMN 
alter table t1 add foreign key (b) references t2(a) on update cascade;
--error ER_WRONG_FK_OPTION_FOR_GENERATED_COLUMN 
alter table t1 add foreign key (b) references t2(a) on delete set null;
drop table t1;

--error ER_KEY_BASED_ON_GENERATED_COLUMN
create table t1 (a int, b int generated always as (a+1) virtual,
                 foreign key (b) references t2(a));

create table t1 (a int, b int generated always as (a+1) virtual);
--error ER_KEY_BASED_ON_GENERATED_COLUMN
alter table t1 add foreign key (b) references t2(a);
drop table t1;

--echo # Allowed FK options.
create table t2 (a int primary key, b char(5));
create table t1 (a int, b int generated always as (a % 10) stored,
                 foreign key (b) references t2(a) on update restrict);
drop table t1;
create table t1 (a int, b int generated always as (a % 10) stored,
                 foreign key (b) references t2(a) on update no action);
drop table t1;
create table t1 (a int, b int generated always as (a % 10) stored,
                 foreign key (b) references t2(a) on delete restrict);
drop table t1;
create table t1 (a int, b int generated always as (a % 10) stored,
                 foreign key (b) references t2(a) on delete cascade);
drop table t1;
create table t1 (a int, b int generated always as (a % 10) stored,
                 foreign key (b) references t2(a) on delete no action);
drop table t1;

--echo 
--echo # Testing data manipulation operations involving FOREIGN KEY 
--echo # on generated columns can be found in:
--echo #  - gcol_ins_upd.inc
--echo #  - gcol_select.inc

--echo #
--echo # TODO: CHECK

--echo #
--echo # Test how optimizer picks indexes defined on a GC
--echo #
SET optimizer_trace_max_mem_size=1048576; # 1MB
SET optimizer_trace="enabled=on,one_line=off";
SET end_markers_in_json="on";

CREATE TABLE t1 (f1 int, gc int AS (f1 + 1) STORED PRIMARY KEY);
INSERT INTO t1(f1) VALUES (1),(2),(0),(9),(3),(4),(8),(7),(5),(6);
ANALYZE TABLE t1;
--echo # Should use index
SELECT * FROM t1 WHERE f1 + 1 > 7;
EXPLAIN SELECT * FROM t1 WHERE f1 + 1 > 7;
SELECT * FROM information_schema.OPTIMIZER_TRACE;

SELECT * FROM t1 WHERE f1 + 1 = 7;
EXPLAIN SELECT * FROM t1 WHERE f1 + 1 = 7;
SELECT * FROM t1 WHERE f1 + 1 IN (7,5);
EXPLAIN SELECT * FROM t1 WHERE f1 + 1 IN(7,5);
SELECT * FROM t1 WHERE f1 + 1 BETWEEN 5 AND 7;
EXPLAIN SELECT * FROM t1 WHERE f1 + 1 BETWEEN 5 AND 7;

--echo # Check that expression isn't transformed for a disabled key
SELECT * FROM t1 IGNORE KEY FOR JOIN(PRIMARY) WHERE f1 + 1 BETWEEN 5 AND 7;
EXPLAIN SELECT * FROM t1 IGNORE KEY FOR JOIN(PRIMARY) WHERE f1 + 1 BETWEEN 5 AND 7;

--echo # Check that ORDER BY could be optimized
SELECT * FROM t1 ORDER BY f1 + 1;
EXPLAIN SELECT * FROM t1 ORDER BY f1 + 1;
SELECT * FROM information_schema.OPTIMIZER_TRACE;
EXPLAIN SELECT * FROM t1 IGNORE KEY FOR ORDER BY(PRIMARY) ORDER BY f1 + 1;

--echo # Check that GROUP BY could be optimized
SELECT f1 + 1, MAX(GC) FROM t1 GROUP BY f1 + 1;
EXPLAIN SELECT f1 + 1, MAX(GC) FROM t1 GROUP BY f1 + 1;
SELECT * FROM information_schema.OPTIMIZER_TRACE;
EXPLAIN SELECT f1 + 1, MAX(GC)
  FROM t1 IGNORE KEY FOR GROUP BY(PRIMARY) GROUP BY f1 + 1;

--echo # Shouldn't use index
SELECT * FROM t1 WHERE f1 + 1 > 7.0;
EXPLAIN SELECT * FROM t1 WHERE f1 + 1 > 7.0;

DROP TABLE t1;
--echo # Pick index with proper type
CREATE TABLE t1 (f1 int,
  gc_int int AS (f1 + 1) STORED,
  gc_date DATE AS (f1 + 1) STORED,
  KEY gc_int_idx(gc_int),
  KEY gc_date_idx(gc_date));
INSERT INTO t1(f1) VALUES
 (030303),(040404),
 (050505),(060606),
 (010101),(020202),
 (030303),(040404),
 (050505),(060606),
 (010101),(020202),
 (090909),(101010),
 (010101),(020202),
 (070707),(080808);
ANALYZE TABLE t1;

SELECT * FROM t1 WHERE f1 + 1 > 070707;
--echo # INT column & index should be picked
EXPLAIN SELECT * FROM t1 WHERE f1 + 1 > 070707;
SELECT * FROM t1 WHERE f1 + 1 > CAST(070707 AS DATE);
--echo # DATE column & index should be picked
EXPLAIN SELECT * FROM t1 WHERE f1 + 1 > CAST(070707 AS DATE);

DROP TABLE t1;

--echo #
--echo # BUG#21229846: WL8170: SIGNAL 11 IN JOIN::MAKE_SUM_FUNC_LIST
--echo #
CREATE TABLE t1 (
  pk int primary key auto_increment,
  col_int_key INTEGER ,
  col_int_gc_key INT GENERATED ALWAYS AS (col_int_key + 1) STORED,
  KEY col_int_gc_key(col_int_gc_key)
);

INSERT INTO t1 ( col_int_key) VALUES (7);

ANALYZE TABLE t1;

SELECT  table1.col_int_key + 1 AS field1, table2.col_int_key AS field2
  FROM (t1 AS table1  JOIN t1 AS table2 ON (table2.pk = table1.pk))
  ORDER BY field1, field2;

EXPLAIN SELECT  table1.col_int_key + 1 AS field1, table2.col_int_key AS field2
  FROM (t1 AS table1  JOIN t1 AS table2 ON (table2.pk = table1.pk))
  ORDER BY field1, field2;

SELECT  table1.col_int_key + 1 AS field1, table2.col_int_key AS field2
  FROM (t1 AS table1  JOIN t1 AS table2 ON (table2.pk = table1.pk))
  GROUP BY field1, field2;

EXPLAIN SELECT  table1.col_int_key + 1 AS field1, table2.col_int_key AS field2
  FROM (t1 AS table1  JOIN t1 AS table2 ON (table2.pk = table1.pk))
  GROUP BY field1, field2;

DROP TABLE t1;
--echo #

