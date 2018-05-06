################################################################################
# inc/gcol_keys.inc                                                            #
#                                                                              #
# Purpose:                                                                     #
#  Testing keys, indexes defined upon generated columns.                       #
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
if($support_virtual_index)
{
create table t1 (a int, b int generated always as (a*2) virtual unique);
show create table t1;
describe t1;
drop table t1;
}
create table t1 (a int, b int generated always as (a*2) stored unique);
show create table t1;
describe t1;
drop table t1;

if($support_virtual_index)
{
create table t1 (a int, b int generated always as (a*2) virtual, unique key (b));
show create table t1;
describe t1;
drop table t1;
}
create table t1 (a int, b int generated always as (a*2) stored, unique (b));
show create table t1;
describe t1;
drop table t1;

if($support_virtual_index)
{
create table t1 (a int, b int generated always as (a*2) virtual);
alter table t1 add unique key (b);
drop table t1;
}
create table t1 (a int, b int generated always as (a*2) stored);
alter table t1 add unique key (b);
drop table t1;

--echo # Testing data manipulation operations involving UNIQUE keys
--echo # on generated columns can be found in:
--echo #  - gcol_ins_upd.inc
--echo #  - gcol_select.inc

--echo #
--echo # INDEX
if($support_virtual_index)
{
create table t1 (a int, b int generated always as (a*2) virtual, index (b));
show create table t1;
describe t1;
drop table t1;

create table t1 (a int, b int generated always as (a*2) virtual, index (a,b));
drop table t1;
}

create table t1 (a int, b int generated always as (a*2) stored, index (b));
show create table t1;
describe t1;
drop table t1;

create table t1 (a int, b int generated always as (a*2) stored, index (a,b));
show create table t1;
describe t1;
drop table t1;

if($support_virtual_index)
{
create table t1 (a int, b int generated always as (a*2) virtual);
alter table t1 add index (b);

alter table t1 add index (a,b);
drop table t1;
}

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

if($support_virtual_index)
{
--error ER_CANNOT_ADD_FOREIGN
create table t1 (a int, b int generated always as (a+1) virtual,
                 foreign key (b) references t2(a));

create table t1 (a int, b int generated always as (a+1) virtual);
--error ER_CANNOT_ADD_FOREIGN
alter table t1 add foreign key (b) references t2(a);
drop table t1;
}

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
drop table t1,t2;

if($support_virtual_index)
{
--echo #
--echo # Bug#20553262: WL8149: ASSERTION `DELSUM+(INT) Y/4-TEMP >= 0' FAILED
--echo #
CREATE TABLE c (
pk integer AUTO_INCREMENT,
col_datetime_nokey DATETIME /*! NULL */,
col_time_nokey TIME /*! NULL */,
col_datetime_key DATETIME GENERATED ALWAYS AS
(ADDTIME(col_datetime_nokey, col_time_nokey)),
col_time_key TIME GENERATED ALWAYS AS
(ADDTIME(col_datetime_nokey, col_time_nokey)),
col_varchar_nokey VARCHAR(1) /*! NULL */,
PRIMARY KEY (pk),
KEY (col_time_key),
KEY (col_datetime_key));

INSERT INTO c ( col_time_nokey,col_datetime_nokey,col_varchar_nokey) values
('14:03:03.042673','2001-11-28 00:50:27.051028', 'c'),
('01:46:09.016386','2007-10-09 19:53:04.008332', NULL),
('16:21:18.052408','2001-11-08 21:02:12.009395', 'x'),
('18:56:33.027423','2003-04-01 00:00:00', 'i');

--replace_column 10 x 11 x
EXPLAIN SELECT
outr.col_time_key AS x
FROM c as outr
WHERE
outr.col_varchar_nokey in ('c', 'x', 'i')
AND (outr.col_time_key IS NULL OR
 outr.col_datetime_key = '2009-09-27');

SELECT
outr.col_time_key AS x
FROM c AS outr
WHERE
outr.col_varchar_nokey in ('c', 'x', 'i')
AND (outr.col_time_key IS NULL OR
 outr.col_datetime_key = '2009-09-27');

DROP TABLE c;

--echo #
--echo # Bug#20913803: WL8149: SIG 11 IN DFIELD_DUP |
--echo # INNOBASE/INCLUDE/DATA0DATA.IC:253
--echo #
CREATE TABLE A (
col_varchar_nokey TEXT ,
col_varchar_key TEXT GENERATED ALWAYS AS (REPEAT(col_varchar_nokey, 1000)),
KEY (col_varchar_key(50))
);

INSERT INTO A (col_varchar_nokey) VALUES ('');

CREATE TABLE D (
pk INTEGER AUTO_INCREMENT,
col_date_nokey BLOB,
col_date_key BLOB GENERATED ALWAYS AS (REPEAT(col_date_nokey,1000)) VIRTUAL,
col_datetime_nokey LONGBLOB,
col_time_nokey LONGTEXT,

col_datetime_key LONGBLOB GENERATED ALWAYS AS (REPEAT(col_datetime_nokey, 1000)),
col_time_key LONGTEXT GENERATED ALWAYS AS (REPEAT(col_datetime_nokey, 1000)),

col_varchar_nokey TEXT,
col_varchar_key TEXT GENERATED ALWAYS AS (REPEAT(col_varchar_nokey, 1000)),

PRIMARY KEY (pk),
KEY (col_varchar_key(50)),
KEY (col_date_key(20)),
KEY (col_time_key(20)),
KEY (col_datetime_key(20)),
KEY (col_varchar_key(10), col_date_key(10), col_time_key(5), col_datetime_key(5))
);

INSERT INTO D (
col_date_nokey,
col_time_nokey,
col_datetime_nokey,
col_varchar_nokey
) VALUES ('', '', '', ''),('', '', '', '');

DELETE FROM OUTR1.* USING D AS OUTR1 RIGHT JOIN A AS OUTR2 ON 
( OUTR1 . `col_varchar_nokey` = OUTR2 . `col_varchar_nokey` ); 

DROP TABLE IF EXISTS A,D;
--echo #
--echo # Bug#21024896: SIG 11 INNOBASE_ADD_ONE_VIRTUAL |
--echo # INNOBASE/HANDLER/HANDLER0ALTER.CC
--echo #
CREATE TABLE t1 (
  col1 int(11) DEFAULT NULL,
  col2 int(11) DEFAULT NULL,
  col3 int(11) NOT NULL,
  col4 int(11) DEFAULT NULL,
  col5 int(11) GENERATED ALWAYS AS (col2 / col2) VIRTUAL,
  col7 int(11) GENERATED ALWAYS AS (col5 + col5) VIRTUAL,
  col8 int(11) GENERATED ALWAYS AS (col5 * col5) VIRTUAL,
  col9 text,
  col6 int(11) DEFAULT NULL,
  PRIMARY KEY (`col3`),
  UNIQUE KEY uidx (`col2`),
  KEY idx (`col5`)
);

INSERT INTO t1(col1,col2,col3,col4,col9,col6)
VALUES(1,1,0,1,REPEAT(col1,1000),0), (3,2,1,1,REPEAT(col1,1000),NULL);

ALTER TABLE t1 ADD COLUMN extra INT;
DROP TABLE t1;


--echo #
--echo # Bug#21316860: WL8149:INNODB: FAILING ASSERTION:
--echo # TEMPL->CLUST_REC_FIELD_NO != ULINT_UNDEFINED
--echo #
CREATE TABLE t1 (
  pk int(11) NOT NULL,
  col_int_nokey int(11),
  col_int_key int(11) GENERATED ALWAYS AS (col_int_nokey) VIRTUAL NOT NULL,
  col_date_nokey date,
  col_date_key date GENERATED ALWAYS AS (col_date_nokey) VIRTUAL NOT NULL,
  PRIMARY KEY (pk),
  UNIQUE KEY col_int_key (col_int_key)
);

ALTER TABLE t1 DROP COLUMN pk;
DROP TABLE t1;

--echo # Remove the impact on PK choose by index on virtual generated column
CREATE TABLE t1 (
  pk int(11) NOT NULL,
  col_int_nokey int(11) DEFAULT NULL,
  col_int_key int(11) GENERATED ALWAYS AS (col_int_nokey) VIRTUAL NOT NULL,
  UNIQUE KEY col_int_key (col_int_key)
);

ALTER TABLE t1 add unique index idx(pk), algorithm=inplace;
DESC t1;
DROP TABLE t1;

--echo #
--echo # Bug#21346132: WL8149:INNODB: FAILING ASSERTION:
--echo # PRIMARY_KEY_NO == -1 || PRIMARY_KEY_NO == 0
--echo #
CREATE TABLE t1 (
  col_int_nokey int(11) NOT NULL,
  col_int_key int(11) GENERATED ALWAYS AS (col_int_nokey) NOT NULL,
  col_varchar_nokey varchar(1) NOT NULL,
  col_varchar_key varchar(2) GENERATED ALWAYS AS (col_varchar_nokey) NOT NULL,
  UNIQUE KEY col_int_key (col_int_key),
  UNIQUE KEY col_varchar_key (col_varchar_key),
  UNIQUE KEY col_int_key_2 (col_int_key,col_varchar_key),
  UNIQUE KEY col_varchar_key_2 (col_varchar_key,col_varchar_nokey),
  KEY col_int_key_3 (col_int_key,col_int_nokey)
);

ALTER TABLE t1 DROP COLUMN col_varchar_key;
DROP TABLE t1;
--echo #
--echo # Bug#21320151 WL8149: WRONG RESULT WITH INDEX SCAN
--echo #

CREATE TABLE t1 (
  id INTEGER NOT NULL,
  b INTEGER GENERATED ALWAYS AS (id+1) VIRTUAL NOT NULL,
  UNIQUE KEY (b)
);

INSERT INTO t1 (id) VALUES (2),(3),(4),(5),(6),(7),(8),(9),(10);

--disable_query_log
--disable_result_log
ANALYZE TABLE t1;
--enable_result_log
--enable_query_log

# covering index scan
let query= SELECT b FROM t1 FORCE INDEX(b);
eval EXPLAIN $query;
eval $query;

# range scan
let $query= SELECT b FROM t1 FORCE INDEX(b) WHERE b BETWEEN 1 AND 5;
eval EXPLAIN $query;
eval $query;

DROP TABLE t1;

}
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
--sorted_result
SELECT * FROM t1 WHERE f1 + 1 > 7;
EXPLAIN SELECT * FROM t1 WHERE f1 + 1 > 7;
SELECT * FROM information_schema.OPTIMIZER_TRACE;

SELECT * FROM t1 WHERE f1 + 1 = 7;
EXPLAIN SELECT * FROM t1 WHERE f1 + 1 = 7;
--sorted_result
SELECT * FROM t1 WHERE f1 + 1 IN (7,5);
EXPLAIN SELECT * FROM t1 WHERE f1 + 1 IN(7,5);
--sorted_result
SELECT * FROM t1 WHERE f1 + 1 BETWEEN 5 AND 7;
EXPLAIN SELECT * FROM t1 WHERE f1 + 1 BETWEEN 5 AND 7;

--echo # Check that expression isn't transformed for a disabled key
--sorted_result
SELECT * FROM t1 IGNORE KEY FOR JOIN(PRIMARY) WHERE f1 + 1 BETWEEN 5 AND 7;
EXPLAIN SELECT * FROM t1 IGNORE KEY FOR JOIN(PRIMARY) WHERE f1 + 1 BETWEEN 5 AND 7;

--echo # Check that ORDER BY could be optimized
SELECT * FROM t1 ORDER BY f1 + 1;
EXPLAIN SELECT * FROM t1 ORDER BY f1 + 1;
SELECT * FROM information_schema.OPTIMIZER_TRACE;
EXPLAIN SELECT * FROM t1 IGNORE KEY FOR ORDER BY(PRIMARY) ORDER BY f1 + 1;

--echo # Check that GROUP BY could be optimized
--sorted_result
SELECT f1 + 1, MAX(GC) FROM t1 GROUP BY f1 + 1;
EXPLAIN SELECT f1 + 1, MAX(GC) FROM t1 GROUP BY f1 + 1;
SELECT * FROM information_schema.OPTIMIZER_TRACE;
EXPLAIN SELECT f1 + 1, MAX(GC)
  FROM t1 IGNORE KEY FOR GROUP BY(PRIMARY) GROUP BY f1 + 1;

--echo # Shouldn't use index
--sorted_result
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

--sorted_result
SELECT * FROM t1 WHERE f1 + 1 > 070707;
--echo # INT column & index should be picked
EXPLAIN SELECT * FROM t1 WHERE f1 + 1 > 070707;
--sorted_result
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

if($support_virtual_index)
{
--echo #
--echo # Bug#21391781 ASSERT WHEN RUNNING ALTER TABLE ON A TABLE WITH INDEX
--echo #              ON VIRTUAL COLUMN
--echo #

#
# Test 1: column number 2 and 66 are virtual and there is an index
#         on column number 2.
#
CREATE TABLE t1 (
  col1 INTEGER NOT NULL,
  col2 INTEGER NOT NULL,
  gcol1 INTEGER GENERATED ALWAYS AS (col1 + col2) VIRTUAL,
  col3 INTEGER NOT NULL,
  col4 INTEGER NOT NULL,
  col5 INTEGER DEFAULT NULL,
  col6 INTEGER DEFAULT NULL,
  col7 INTEGER DEFAULT NULL,
  col8 INTEGER DEFAULT NULL,
  col9 INTEGER DEFAULT NULL,
  col10 INTEGER DEFAULT NULL,
  col11 INTEGER DEFAULT NULL,
  col12 INTEGER DEFAULT NULL,
  col13 INTEGER DEFAULT NULL,
  col14 INTEGER DEFAULT NULL,
  col15 INTEGER DEFAULT NULL,
  col16 INTEGER DEFAULT NULL,
  col17 INTEGER DEFAULT NULL,
  col18 INTEGER DEFAULT NULL,
  col19 INTEGER DEFAULT NULL,
  col20 INTEGER DEFAULT NULL,
  col21 INTEGER DEFAULT NULL,
  col22 INTEGER DEFAULT NULL,
  col23 INTEGER DEFAULT NULL,
  col24 INTEGER DEFAULT NULL,
  col25 INTEGER DEFAULT NULL,
  col26 INTEGER DEFAULT NULL,
  col27 INTEGER DEFAULT NULL,
  col28 INTEGER DEFAULT NULL,
  col29 INTEGER DEFAULT NULL,
  col30 INTEGER DEFAULT NULL,
  col31 INTEGER DEFAULT NULL,
  col32 INTEGER DEFAULT NULL,
  col33 INTEGER DEFAULT NULL,
  col34 INTEGER DEFAULT NULL,
  col35 INTEGER DEFAULT NULL,
  col36 INTEGER DEFAULT NULL,
  col37 INTEGER DEFAULT NULL,
  col38 INTEGER DEFAULT NULL,
  col39 INTEGER DEFAULT NULL,
  col40 INTEGER DEFAULT NULL,
  col41 INTEGER DEFAULT NULL,
  col42 INTEGER DEFAULT NULL,
  col43 INTEGER DEFAULT NULL,
  col44 INTEGER DEFAULT NULL,
  col45 INTEGER DEFAULT NULL,
  col46 INTEGER DEFAULT NULL,
  col47 INTEGER DEFAULT NULL,
  col48 INTEGER DEFAULT NULL,
  col49 INTEGER DEFAULT NULL,
  col50 INTEGER DEFAULT NULL,
  col51 INTEGER DEFAULT NULL,
  col52 INTEGER DEFAULT NULL,
  col53 INTEGER DEFAULT NULL,
  col54 INTEGER DEFAULT NULL,
  col55 INTEGER DEFAULT NULL,
  col56 INTEGER DEFAULT NULL,
  col57 INTEGER DEFAULT NULL,
  col58 INTEGER DEFAULT NULL,
  col59 INTEGER DEFAULT NULL,
  col60 INTEGER DEFAULT NULL,
  col61 INTEGER DEFAULT NULL,
  col62 INTEGER DEFAULT NULL,
  col63 INTEGER DEFAULT NULL,
  col64 INTEGER DEFAULT NULL,
  col65 INTEGER DEFAULT NULL,
  gcol2 INTEGER GENERATED ALWAYS AS (col3 / col4) VIRTUAL,
  KEY idx1 (gcol1)
);

INSERT INTO t1 (col1, col2, col3, col4)
  VALUES (1,1,1,1), (2,2,2,2), (3,3,3,3), (4,4,4,4), (5,5,5,5);

# This will call my_eval_gcolumn_expr to compute the indexed column value
ALTER TABLE t1 ADD COLUMN extra INTEGER;

SELECT gcol1 FROM t1 FORCE INDEX(idx1);

DROP TABLE t1;

#
# Test 2: column number 2 and 66 are virtual and there is an index
#         on column number 66.
#
CREATE TABLE t1 (
  col1 INTEGER NOT NULL,
  col2 INTEGER NOT NULL,
  gcol1 INTEGER GENERATED ALWAYS AS (col1 + col2) VIRTUAL,
  col3 INTEGER NOT NULL,
  col4 INTEGER NOT NULL,
  col5 INTEGER DEFAULT NULL,
  col6 INTEGER DEFAULT NULL,
  col7 INTEGER DEFAULT NULL,
  col8 INTEGER DEFAULT NULL,
  col9 INTEGER DEFAULT NULL,
  col10 INTEGER DEFAULT NULL,
  col11 INTEGER DEFAULT NULL,
  col12 INTEGER DEFAULT NULL,
  col13 INTEGER DEFAULT NULL,
  col14 INTEGER DEFAULT NULL,
  col15 INTEGER DEFAULT NULL,
  col16 INTEGER DEFAULT NULL,
  col17 INTEGER DEFAULT NULL,
  col18 INTEGER DEFAULT NULL,
  col19 INTEGER DEFAULT NULL,
  col20 INTEGER DEFAULT NULL,
  col21 INTEGER DEFAULT NULL,
  col22 INTEGER DEFAULT NULL,
  col23 INTEGER DEFAULT NULL,
  col24 INTEGER DEFAULT NULL,
  col25 INTEGER DEFAULT NULL,
  col26 INTEGER DEFAULT NULL,
  col27 INTEGER DEFAULT NULL,
  col28 INTEGER DEFAULT NULL,
  col29 INTEGER DEFAULT NULL,
  col30 INTEGER DEFAULT NULL,
  col31 INTEGER DEFAULT NULL,
  col32 INTEGER DEFAULT NULL,
  col33 INTEGER DEFAULT NULL,
  col34 INTEGER DEFAULT NULL,
  col35 INTEGER DEFAULT NULL,
  col36 INTEGER DEFAULT NULL,
  col37 INTEGER DEFAULT NULL,
  col38 INTEGER DEFAULT NULL,
  col39 INTEGER DEFAULT NULL,
  col40 INTEGER DEFAULT NULL,
  col41 INTEGER DEFAULT NULL,
  col42 INTEGER DEFAULT NULL,
  col43 INTEGER DEFAULT NULL,
  col44 INTEGER DEFAULT NULL,
  col45 INTEGER DEFAULT NULL,
  col46 INTEGER DEFAULT NULL,
  col47 INTEGER DEFAULT NULL,
  col48 INTEGER DEFAULT NULL,
  col49 INTEGER DEFAULT NULL,
  col50 INTEGER DEFAULT NULL,
  col51 INTEGER DEFAULT NULL,
  col52 INTEGER DEFAULT NULL,
  col53 INTEGER DEFAULT NULL,
  col54 INTEGER DEFAULT NULL,
  col55 INTEGER DEFAULT NULL,
  col56 INTEGER DEFAULT NULL,
  col57 INTEGER DEFAULT NULL,
  col58 INTEGER DEFAULT NULL,
  col59 INTEGER DEFAULT NULL,
  col60 INTEGER DEFAULT NULL,
  col61 INTEGER DEFAULT NULL,
  col62 INTEGER DEFAULT NULL,
  col63 INTEGER DEFAULT NULL,
  col64 INTEGER DEFAULT NULL,
  col65 INTEGER DEFAULT NULL,
  gcol2 INTEGER GENERATED ALWAYS AS (col3 / col4) VIRTUAL,
  KEY idx1 (gcol2)
);

INSERT INTO t1 (col1, col2, col3, col4)
  VALUES (1,1,1,1), (2,2,2,2), (3,3,3,3), (4,4,4,4), (5,5,5,5);

# This will call my_eval_gcolumn_expr to compute the indexed column value
ALTER TABLE t1 ADD COLUMN extra INTEGER;

SELECT gcol2 FROM t1 FORCE INDEX(idx1);

DROP TABLE t1;
}

if($support_virtual_index)
{
--echo #
--echo # Bug#21628161 CRASH/MEMORY CORRUPTION ADDING INDEXES TO VIRTUAL COLUMN
--echo #
# When generating the value of column b, an out-of-range warning is
# raised. A warning is required in order to reproduce the bug, but it
# is promoted to an error on insertion unless we turn off strict mode.
--source include/turn_off_strict_mode.inc
CREATE TABLE t (a INT,
                b BOOLEAN GENERATED ALWAYS AS (a+10000) VIRTUAL,
                c BLOB GENERATED ALWAYS AS (b=2) VIRTUAL);
INSERT INTO t(a) VALUES (1);
# Before index was created, this query returned the expected one match.
SELECT * FROM t WHERE c = '0';
# Adding an index sometimes crashed, other times populated it with garbage ...
ALTER TABLE t ADD UNIQUE INDEX (c(1));
# ... so that this query found no match in the index.
SELECT * FROM t WHERE c = '0';
DROP TABLE t;
--source include/restore_strict_mode.inc

--echo #
--echo # Bug#21688115 VIRTUAL COLUMN COMPUTATION SAVE_IN_FIELD()
--echo #              DID NOT RETURN TRUE WITH DIVIDE 0
--echo #
CREATE TABLE t (a INT, b INT, h VARCHAR(10));
INSERT INTO t VALUES (12, 3, "ss");
INSERT INTO t VALUES (13, 4, "ss");
INSERT INTO t VALUES (14, 0, "ss");
ALTER TABLE t ADD c INT GENERATED ALWAYS AS (a/b) VIRTUAL;
--error ER_DIVISION_BY_ZERO
CREATE INDEX idx ON t(c);
CALL mtr.add_suppression("\\[Warning\\] \\[[^]]*\\] InnoDB: Compute virtual column values failed");
DROP TABLE t;
}

--echo #
--echo # Bug#21770798 OPTIMIZER DOES NOT USE INDEX FOR GENERATED EXPRESSIONS
--echo #              WITH LOGICAL OPERATORS
--echo #
CREATE TABLE t (a INT, b INT,
                gc_and INT GENERATED ALWAYS AS (a AND b) STORED,
                gc_or INT GENERATED ALWAYS AS (a OR b) STORED,
                gc_xor INT GENERATED ALWAYS AS (a XOR b) STORED,
                gc_not INT GENERATED ALWAYS AS (NOT a) STORED,
                gc_case INT GENERATED ALWAYS AS
                (CASE WHEN (a AND b) THEN a ELSE b END) STORED,
                INDEX(gc_and), INDEX(gc_or), INDEX(gc_xor), INDEX(gc_not),
                INDEX(gc_case));
INSERT INTO t (a, b) VALUES (0, 0), (0, 1), (1, 0), (1, 1);
ANALYZE TABLE t;
EXPLAIN SELECT a, b FROM t WHERE (a AND b) = 1;
SELECT a, b FROM t WHERE (a AND b) = 1;
EXPLAIN SELECT a, b FROM t WHERE 1 = (a AND b);
SELECT a, b FROM t WHERE 1 = (a AND b);
EXPLAIN SELECT a, b FROM t WHERE (a AND b) IN (1, 2, 3);
SELECT a, b FROM t WHERE (a AND b) IN (1, 2, 3);
EXPLAIN SELECT a, b FROM t WHERE (a OR b) = 1;
--sorted_result
SELECT a, b FROM t WHERE (a OR b) = 1;
EXPLAIN SELECT a, b FROM t WHERE (a OR b) BETWEEN 1 AND 10;
--sorted_result
SELECT a, b FROM t WHERE (a OR b) BETWEEN 1 AND 10;
# XOR and NOT worked even before the bug fix, but we test all logical
# operators here for completeness.
EXPLAIN SELECT a, b FROM t WHERE (a XOR b) = 1;
--sorted_result
SELECT a, b FROM t WHERE (a XOR b) = 1;
EXPLAIN SELECT a FROM t WHERE (NOT a) = 1;
SELECT a FROM t WHERE (NOT a) = 1;
# Also verify that a logical expression nested inside another
# expression doesn't prevent substitution.
EXPLAIN SELECT a FROM t WHERE (CASE WHEN (a AND b) THEN a ELSE b END) = 1;
--sorted_result
SELECT a FROM t WHERE (CASE WHEN (a AND b) THEN a ELSE b END) = 1;
# The expression must be exactly the same as the generated expression.
# (b AND a) is not recognized as equivalent to (a AND b).
EXPLAIN SELECT a, b FROM t WHERE 1 = (b AND a);
SELECT a, b FROM t WHERE 1 = (b AND a);
--sorted_result
EXPLAIN SELECT a, b FROM t WHERE 1 = (b OR a);
--sorted_result
SELECT a, b FROM t WHERE 1 = (b OR a);
DROP TABLE t;

--echo #
--echo # Bug#21854241: QUERY USING JSON_EXTRACT() RETURNS WRONG RESULT
--echo #               AFTER ADDING VIRTUAL INDEX
--echo #
CREATE TABLE employees (
  data JSON,
  name1 VARCHAR(30) AS (JSON_EXTRACT(data, "$.name")) STORED,
  name2 VARCHAR(30) AS (JSON_UNQUOTE(JSON_EXTRACT(data, "$.name"))) STORED
);
INSERT INTO employees (data) VALUES('{"id": 1, "name": "Jane"}');
INSERT INTO employees (data) VALUES('{"id": 2, "name": "Joe"}');
let $query= SELECT * FROM employees WHERE JSON_EXTRACT(data, '\$.name') = 'Jane';
ANALYZE TABLE employees;
--eval EXPLAIN $query
--sorted_result
--eval $query
# The index on name1 should not be used, since it contains
# double-quoted values which don't match the string literal in the
# predicate. The query used to return zero rows.
ALTER TABLE employees ADD INDEX name_idx1(name1);
ANALYZE TABLE employees;
--eval EXPLAIN $query
--sorted_result
--eval $query
# The index on name2 contains unquoted strings and can be used.
ALTER TABLE employees ADD INDEX name_idx2(name2);
ANALYZE TABLE employees;
--eval EXPLAIN $query
--sorted_result
--eval $query
DROP TABLE employees;
--echo #
--echo # Bug#22077611 UPDATE .. WHERE JSON_EXTRACT(..) = '..' NOT USING
--echo #              VIRTUAL COL INDEX
--echo #
CREATE TABLE t(a INT, b INT, gc INT GENERATED ALWAYS AS (a+1) STORED, KEY(gc));
INSERT INTO t(a) VALUES (1), (2), (3), (4), (5), (1), (2), (3), (4), (5);
ANALYZE TABLE t;
--let $query= UPDATE t SET b = 10 WHERE (a+1) = 3
--eval EXPLAIN $query
--eval $query
SELECT * FROM t ORDER BY a, b;
ANALYZE TABLE t;
--let $query= UPDATE t SET b = 9 ORDER BY (a+1) LIMIT 1
--eval EXPLAIN $query
--eval $query
SELECT * FROM t ORDER BY a, b;
ANALYZE TABLE t;
--let $query= DELETE FROM t WHERE (a+1) = 2
--eval EXPLAIN $query
--eval $query
SELECT * FROM t ORDER BY a, b;
ANALYZE TABLE t;
--let $query= DELETE FROM t ORDER BY (a+1) LIMIT 1
--eval EXPLAIN $query
--eval $query
SELECT * FROM t ORDER BY a, b;
DROP TABLE t;

if($support_virtual_index)
{
--echo #
--echo # Bug#22095783: ALTER TABLE ADD COLUMN FAILS WITH OUT
--echo #               OF RANGE VALUE ERROR
CREATE TABLE c1(doc JSON,_id VARCHAR(32) GENERATED ALWAYS
AS (JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))) STORED NOT NULL UNIQUE);
INSERT INTO c1(doc) VALUES('{"X":10,"_id":9}');
--error ER_WARN_DATA_OUT_OF_RANGE
ALTER TABLE c1 ADD COLUMN vc1 DOUBLE(4, 3) GENERATED
ALWAYS AS (JSON_EXTRACT(doc, '$.X')) VIRTUAL , ADD INDEX IX1 (vc1);
ALTER TABLE c1 ADD vc2 INT(11);
ALTER TABLE c1 ADD COLUMN vc3 INT(11) GENERATED ALWAYS AS
(JSON_EXTRACT(doc, '$.X')) VIRTUAL , ADD UNIQUE INDEX IX2 (vc3);
ALTER TABLE c1 drop vc2;
ALTER TABLE c1 ADD vc4 INT(11);
DROP TABLE c1;
} 

--echo #
--echo # Bug#22810883: ASSERTION FAILED:
--echo #               !(USED_TABS & (~READ_TABLES & ~FILTER_FOR_TABLE))
--echo #
CREATE TABLE t1 (a1 INTEGER GENERATED ALWAYS AS (1 AND 0) STORED,
                 a2 INTEGER, KEY (a1));
INSERT INTO t1 VALUES ();
CREATE TABLE t2 (b INTEGER);
INSERT INTO t2 VALUES (1);
ANALYZE TABLE t1, t2;
--echo # Used to choose the index on a1 and get wrong results.
--let $query= SELECT * FROM t1 WHERE (a2 AND a2) = 0
--eval EXPLAIN $query
--eval $query
--echo # Used to get assertion or wrong results.
--let $query= SELECT * FROM t1 STRAIGHT_JOIN t2 ON b WHERE (b AND b) = 1
--eval EXPLAIN $query
--eval $query
DROP TABLE t1, t2;

if ($support_virtual_index) {
--echo #
--echo # Bug#24345509: WRONG RESULTS WHEN GCOL INDEX IS USED
--echo #
CREATE TABLE t1 (
  pk INT PRIMARY KEY AUTO_INCREMENT,
  i INT,
  vc VARCHAR(7),
  gc1 VARCHAR(14) GENERATED ALWAYS AS (concat(vc, vc)) VIRTUAL NOT NULL,
  gc2 VARCHAR(14) GENERATED ALWAYS AS (concat(gc1, 'x')) VIRTUAL NOT NULL,
  KEY gc2_key (gc2),
  KEY gc2_key_prefix (gc2(5))
);

INSERT INTO t1 (i, vc) VALUES
(7, 'xcek'), (3, 'ceksat'), (3, 'eksate'), (3, 'ksatef'), (6, 's');

ANALYZE TABLE t1;

# Missing rows in the result when using a non-covering secondary index
# and the end-range condition referenced a virtual column.

let $query = SELECT i FROM t1 WHERE gc2 <= 'ksatefksatefx';
--eval EXPLAIN $query
--eval $query
let $query = SELECT i FROM t1 FORCE INDEX (gc2_key_prefix)
             WHERE gc2 <= 'ksatefksatefx';
--eval EXPLAIN $query
--eval $query

# Similar test case with covering index. Used to miss rows when using
# a prefix index.

let $query = SELECT COUNT(*) FROM t1 WHERE gc2 <= 'ksatefksatefx';
--eval EXPLAIN $query
--eval $query
let $query = SELECT COUNT(*) FROM t1 FORCE INDEX (gc2_key_prefix)
             WHERE gc2 <= 'ksatefksatefx';
--eval EXPLAIN $query
--eval $query

# After bug#23481444 InnoDB also checks the end-range condition on
# page boundaries if many not yet committed records have been scanned.
# Fill up a page with uncommitted rows at the beginning of the
# secondary index to exercise this code.
CREATE TABLE t2 (
  i INT NOT NULL,
  gc INT GENERATED ALWAYS AS (i) VIRTUAL NOT NULL,
  KEY (gc)
);

INSERT INTO t2 (i) VALUES (-1), (0), (1), (2);

BEGIN;
--disable_query_log
--let $i = 2000
while ($i > 0)
{
  INSERT INTO t2 (i) VALUES (-10);
  dec $i;
}
--enable_query_log

CONNECT (con1,localhost,root,,);
# This query used to return zero rows.
SELECT i FROM t2 FORCE INDEX (gc) WHERE gc <= -1;
# Similar query using covering index was correct even before.
SELECT gc FROM t2 FORCE INDEX (gc) WHERE gc <= -1;
DISCONNECT con1;
CONNECTION default;

COMMIT;

DROP TABLE t1, t2;

--echo #
--echo # Bug#27010089: ASSERTION FAILURE:
--echo # ROW0SEL.CC:2846:TEMPL->MYSQL_COL_LEN == LEN
--echo #

CREATE TABLE t (
pk INTEGER AUTO_INCREMENT PRIMARY KEY,
col_int_key INTEGER GENERATED ALWAYS AS (1) VIRTUAL,
col_varchar VARCHAR(7) ,
col_varchar_key VARCHAR(14) GENERATED ALWAYS AS ('abc') VIRTUAL,
KEY (col_varchar_key(5), col_int_key),
KEY (col_int_key, col_varchar_key));

INSERT INTO t VALUES (), ();

SELECT 1 FROM t AS alias1
WHERE EXISTS
(SELECT SQ2_alias1.col_int_key AS SQ2_field1
 FROM t AS SQ2_alias1 JOIN t AS SQ2_alias2
 ON (SQ2_alias2.col_varchar_key = SQ2_alias1.col_varchar_key)
 WHERE SQ2_alias1.col_varchar <> alias1.col_varchar AND
       SQ2_alias1.col_int_key >= SQ2_alias2.pk);

DROP TABLE t;
}

--echo #
--echo # Bug#27403367: GENERATED COLUMN EXPRESSIONS IGNORED WITH PREFIX INDEX
--echo #

CREATE TABLE t (vc VARCHAR(100),
                gc VARCHAR(100) GENERATED ALWAYS AS (REVERSE(vc)) STORED,
                KEY (gc(3)));

INSERT INTO t(vc) VALUES ('a'), ('abc'), ('abcabc'), ('abcdef');

ANALYZE TABLE t;

EXPLAIN SELECT * FROM t WHERE REVERSE(vc) = 'cba';
SELECT * FROM t WHERE REVERSE(vc) = 'cba';

EXPLAIN SELECT * FROM t WHERE REVERSE(vc) = 'cbacba';
SELECT * FROM t WHERE REVERSE(vc) = 'cbacba';

EXPLAIN SELECT * FROM t WHERE REVERSE(vc) BETWEEN 'c' AND 'e';
SELECT * FROM t WHERE REVERSE(vc) BETWEEN 'c' AND 'e';

DROP TABLE t;

--echo #
