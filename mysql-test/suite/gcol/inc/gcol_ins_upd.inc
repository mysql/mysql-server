################################################################################
# inc/gcol_ins_upd.inc                                                         #
#                                                                              #
# Purpose:                                                                     #
#  Testing DDL operations such as INSERT, UPDATE, REPLACE and DELETE.          #
#                                                                              #
#                                                                              #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: Andrey Zhakov                                               #
# Original Date: 2008-09-04                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

let $create1 = create table t1 (a int, 
                                b int generated always as (-a) virtual,
                                c int generated always as (-a) stored);
let $create2 = create table t1 (a int unique, 
                                b int generated always as (-a) virtual,
                                c int generated always as (-a) stored);
let $create3 = create table t1 (a int, 
                                b int generated always as (-a) virtual,
                                c int generated always as (-a) stored unique);
let $create4 = create table t1 (a int, 
                                b int generated always as (-a) virtual,
                                c int generated always as (-a) stored unique,
				d varchar(16));
eval $create1;
set sql_warnings = 1;

--echo #
--echo # *** INSERT ***
--echo #

--echo # INSERT INTO tbl_name VALUES... DEFAULT is specified against gcols
insert into t1 values (1,default,default);
select * from t1;
delete from t1;
select * from t1;

--echo # INSERT INTO tbl_name VALUES... NULL is specified against gcols
--error ER_NON_DEFAULT_VALUE_FOR_GENERATED_COLUMN
insert into t1 values (1,null,null);
select * from t1;
delete from t1;
select * from t1;

--echo # INSERT INTO tbl_name VALUES... a non-NULL value is specified against gcols
--error ER_NON_DEFAULT_VALUE_FOR_GENERATED_COLUMN
insert into t1 values (1,2,3);
select * from t1;
delete from t1;
select * from t1;

--echo # INSERT INTO tbl_name (<non_gcol_list>) VALUES...
insert into t1 (a) values (1), (2);
select * from t1 order by a;
delete from t1;
select * from t1;

--echo # INSERT INTO tbl_name (<normal+gcols>) VALUES... DEFAULT is specified 
--echo # against gcols
insert into t1 (a,b) values (1,default), (2,default);
select * from t1 order by a;
delete from t1;
select * from t1;

--echo # INSERT INTO tbl_name (<normal+gcols>) VALUES... NULL is specified against gcols
--error ER_NON_DEFAULT_VALUE_FOR_GENERATED_COLUMN
insert into t1 (a,b) values (1,null), (2,null);
select * from t1;
delete from t1;
select * from t1;

--echo # INSERT INTO tbl_name (<normal+gcols>) VALUES... a non-NULL value is specified 
--echo # against gcols
--error ER_NON_DEFAULT_VALUE_FOR_GENERATED_COLUMN
insert into t1 (a,b) values (1,3), (2,4);
select * from t1;
delete from t1;
select * from t1;
drop table t1;

--echo # Table with UNIQUE non-gcol field. INSERT INTO tbl_name VALUES... ON DUPLICATE 
--echo # KEY UPDATE <non_gcol>=expr, <gcol>=expr
eval $create2;
insert into t1 values (1,default,default);
insert into t1 values (1,default,default) 
       on duplicate key update a=2, b=default;
select a,b,c from t1;
delete from t1 where b in (1,2);
select * from t1;
drop table t1;

--echo # Table with UNIQUE gcol field. INSERT INTO tbl_name VALUES... ON DUPLICATE 
--echo # KEY UPDATE <non_gcol>=expr, <gcol>=expr
eval $create3;
insert into t1 values (1,default,default);
insert into t1 values (1,default,default) 
       on duplicate key update a=2, b=default;
select a,b,c from t1;

--echo # CREATE new_table ... LIKE old_table
--echo # INSERT INTO new_table SELECT * from old_table
create table t2 like t1;
--error ER_NON_DEFAULT_VALUE_FOR_GENERATED_COLUMN
insert into t2 select * from t1;
insert into t2(a) select a from t1;
select * from t2;
drop table t2;

--echo # CREATE new_table ... LIKE old_table INSERT INTO new_table (<non-gcols>, <gcols>) 
--echo # SELECT <non-gcols>, <gcols> from old_table
insert into t1 values (1,default,default);
select * from t1;
create table t2 like t1;
--error ER_NON_DEFAULT_VALUE_FOR_GENERATED_COLUMN
insert into t2 (a,b) select a,b from t1;
insert into t2 (a) select a from t1;
select * from t2 order by a;
drop table t2;
drop table t1;

--echo #
--echo # *** UPDATE ***
--echo #

--echo # UPDATE tbl_name SET non-gcol=expr WHERE non-gcol=expr
eval $create1;
insert into t1 (a) values (1), (2);
select * from t1 order by a;
update t1 set a=3 where a=2;
select * from t1 order by a;
delete from t1;
select * from t1;

--echo # UPDATE tbl_name SET gcol=expr WHERE non-gcol=expr
insert into t1 (a) values (1), (2);
select * from t1 order by a;
--error ER_NON_DEFAULT_VALUE_FOR_GENERATED_COLUMN
update t1 set c=3 where a=2;
select * from t1 order by a;
delete from t1;
select * from t1;

--echo # UPDATE tbl_name SET non-gcol=expr WHERE gcol=expr
insert into t1 (a) values (1), (2);
select * from t1 order by a;
update t1 set a=3 where b=-2;
select * from t1 order by a;
delete from t1;
select * from t1;

--echo # UPDATE tbl_name SET gcol=expr WHERE gcol=expr
insert into t1 (a) values (1), (2);
select * from t1 order by a;
--error ER_NON_DEFAULT_VALUE_FOR_GENERATED_COLUMN
update t1 set c=3 where b=-2;
select * from t1 order by a;
delete from t1;
select * from t1;
drop table t1;

--echo # INDEX created on gcol 
--echo # UPDATE tbl_name SET non-gcol=expr WHERE gcol=const
eval $create3;
insert into t1 (a) values (1), (2);
select * from t1 order by a;
update t1 set a=3 where c=-2;
select * from t1;
delete from t1;
select * from t1;


--echo # INDEX created on gcol 
--echo # UPDATE tbl_name SET non-gcol=expr WHERE gcol=between const1 and const2
insert into t1 (a) values (1), (2);
select * from t1 order by a;
update t1 set a=3 where c between -3 and -2;
select * from t1 order by a;
delete from t1;
select * from t1;

--echo # No INDEX created on gcol 
--echo # UPDATE tbl_name SET non-gcol=expr WHERE gcol=between const1 and const2
insert into t1 (a) values (1), (2);
select * from t1 order by a;
update t1 set a=3 where b between -3 and -2;
select * from t1 order by a;
delete from t1;
select * from t1;

--echo # INDEX created on gcol 
--echo # UPDATE tbl_name SET non-gcol=expr 
--echo # WHERE gcol=between const1 and const2 ORDER BY gcol
insert into t1 (a) values (1), (2), (3), (4), (5);
select * from t1 order by a;
update t1 set a=6 where c between -1 and 0
          order by c;
select * from t1 order by a;
delete from t1 where c between -6 and 0;
select * from t1;

--echo # INDEX created on gcol 
--echo # UPDATE tbl_name SET non-gcol=expr 
--echo # WHERE gcol=between const1 and const2 ORDER BY gcol LIMIT 2
insert into t1 (a) values (1), (2), (3), (4), (5);
select * from t1 order by a;
update t1 set a=6 where c between -1 and 0
          order by c limit 2;
select * from t1 order by a;
delete from t1 where c between -2 and 0 order by c;
select * from t1 order by a;
delete from t1;

--echo # INDEX created on gcol 
--echo # UPDATE tbl_name SET non-gcol=expr
--echo # WHERE indexed gcol=between const1 and const2 and non-indexed gcol=const3
insert into t1 (a) values (1), (2), (3), (4), (5);
select * from t1 order by a;
update t1 set a=6 where (c between -2 and 0) and (b=-1);
select * from t1 order by a;
delete from t1;

--echo # INDEX created on gcol 
--echo # UPDATE tbl_name SET non-gcol=expr
--echo # WHERE indexed gcol=between const1 and const2 and non-indexed gcol=const3
--echo # ORDER BY indexed gcol
insert into t1 (a) values (1), (2), (3), (4), (5);
select * from t1 order by a;
update t1 set a=6 where (c between -2 and 0) and (b=-1) order by c;
select * from t1 order by a;
delete from t1;
drop table t1;

let $innodb_engine = `SELECT @@session.default_storage_engine='innodb'`;
if ($innodb_engine)
{
  --echo #
  --echo # Verify ON UPDATE/DELETE actions of FOREIGN KEYs
  create table t2 (a int primary key, name varchar(10));
  create table t1 (a int primary key, b int generated always as (a % 10) stored);
  insert into t2 values (1, 'value1'), (2,'value2'), (3,'value3');
  insert into t1 (a) values (1),(2),(3);
  select * from t1 order by a;
  select * from t2 order by a;
  select t1.a, t1.b, t2.name from t1,t2 where t1.b=t2.a order by t1.a;

  --echo #  - ON UPDATE RESTRICT
  alter table t1 add foreign key (b) references t2(a) on update restrict;
  --error 1452
  insert into t1 (a) values (4);
  --error 1451
  update t2 set a=4 where a=3;
  select t1.a, t1.b, t2.name from t1,t2 where t1.b=t2.a;
  alter table t1 drop foreign key t1_ibfk_1;

  --echo #  - ON DELETE RESTRICT
  alter table t1 add foreign key (b) references t2(a) on delete restrict;
  --error 1451
  delete from t2 where a=3;
  select t1.a, t1.b, t2.name from t1,t2 where t1.b=t2.a;
  select t1.a, t1.b, t2.name from t1 left outer join t2 on (t1.b=t2.a);
  alter table t1 drop foreign key t1_ibfk_1;

  --echo #  - ON DELETE CASCADE
  alter table t1 add foreign key (b) references t2(a) on delete cascade;
  delete from t2 where a=3;
  select t1.a, t1.b, t2.name from t1,t2 where t1.b=t2.a;
  select t1.a, t1.b, t2.name from t1 left outer join t2 on (t1.b=t2.a);
  alter table t1 drop foreign key t1_ibfk_1;

  drop table t1;
  drop table t2;
}

--echo #
--echo # *** REPLACE ***
--echo #

--echo # UNIQUE INDEX on gcol
--echo # REPLACE tbl_name (non-gcols) VALUES (non-gcols);
eval $create4;
insert into t1 (a,d) values (1,'a'), (2,'b');
select * from t1 order by a;
replace t1 (a,d) values (1,'c');
select * from t1 order by a;
delete from t1;
select * from t1;


# *** DELETE
# All required tests for DELETE are performed as part of the above testing
# for INSERT, UPDATE and REPLACE.

set sql_warnings = 0;
drop table t1;

--echo Bug#20170778: WL411:FAILING ASSERTION `!TABLE || (!TABLE->WRITE_SET ||
--echo               BITMAP_IS_SET(TABLE->WR
--echo #
CREATE TABLE t1 (col1 INT, col2 INT, col3 INT, col4 INT, col5
INT GENERATED ALWAYS AS (col3 * col2) VIRTUAL, col6 INT GENERATED ALWAYS AS
(col4 * col1) STORED, col7 INT GENERATED ALWAYS AS (col6 + col6) VIRTUAL,
col8 INT GENERATED ALWAYS AS (col6 / col5) STORED, col9 TEXT);

SET @fill_amount = (@@innodb_page_size / 2 ) + 1;

INSERT INTO t1 (col1,col2,col3,col4,col5,col6,col7,col8,col9) VALUES /* 3 */
(3, 3 / 3, 3 + 3, 3 / 3, DEFAULT, DEFAULT, DEFAULT, DEFAULT ,REPEAT(CAST(3 AS
CHAR(1)),@fill_amount)) , (3, 3 * 3, 3 + 3, 3 / 3, DEFAULT, DEFAULT, DEFAULT,
DEFAULT ,REPEAT(CAST(3 AS CHAR(1)),@fill_amount));

UPDATE t1 SET col1 = 2;
UPDATE t1 SET col7 = DEFAULT;
UPDATE t1 SET col8 = DEFAULT;
DROP TABLE t1;

if ($support_virtual_index)
{
--echo Bug#20797344: WL#8149: ALLOCATED SPACE FOR INDEXED BLOB VGC CAN BE
--echo               OVERWRITTEN FOR UPDATE
--echo #
CREATE TABLE t (a varchar(100), b blob,
c blob GENERATED ALWAYS AS (concat(a,b)) VIRTUAL,
d blob GENERATED ALWAYS AS (b) VIRTUAL,
e int(11) GENERATED ALWAYS AS (10) VIRTUAL,
h int(11) NOT NULL, PRIMARY KEY (h), key(c(20)));
INSERT INTO t(a,b,h) VALUES('aaaaaaa','1111111', 11);
INSERT INTO t(a,b,h) VALUES('bbbbbbb','2222222', 22);
SELECT c FROM t;
UPDATE t SET a='ccccccc';
SELECT c FROM t;
DROP TABLE t;
}

--echo # Bug#21081742: ASSERTION !TABLE || (!TABLE->WRITE_SET || 
--echo # BITMAP_IS_SET(TABLE->WRITE_SET
--echo #

CREATE TABLE b ( 
pk INTEGER AUTO_INCREMENT,
col_varchar_nokey VARCHAR(1),
col_varchar_key VARCHAR(2) GENERATED ALWAYS AS
(CONCAT(col_varchar_nokey, col_varchar_nokey)),
PRIMARY KEY (pk)
);

INSERT INTO b (col_varchar_nokey) VALUES ('v'),('v');

CREATE TABLE d (
pk INTEGER AUTO_INCREMENT,
col_varchar_nokey VARCHAR(1),
col_varchar_key VARCHAR(2) GENERATED ALWAYS AS
(CONCAT(col_varchar_nokey, col_varchar_nokey)),
PRIMARY KEY (pk)
) ;

INSERT INTO d (col_varchar_nokey) VALUES ('q'),('g'),('e'),('l'),(NULL),('v'),('c'),('u'),('x');

CREATE TABLE bb (
pk INTEGER AUTO_INCREMENT,
col_varchar_nokey VARCHAR(1) /*! NULL */,
col_varchar_key VARCHAR(2) GENERATED ALWAYS AS
(CONCAT(col_varchar_nokey, col_varchar_nokey)),
PRIMARY KEY (pk)
);

INSERT INTO bb (col_varchar_nokey) VALUES ('j'),('h');

EXPLAIN UPDATE
d AS OUTR1, b AS OUTR2
SET OUTR1.col_varchar_nokey = NULL
WHERE
( 't', 'b' )  IN
(
SELECT
INNR1.col_varchar_nokey AS x,
INNR1.col_varchar_key AS y
FROM bb AS INNR1
WHERE OUTR1.pk = 1
);

DROP TABLE IF EXISTS b,bb,d;


--echo #
--echo # Bug#21216067 ASSERTION FAILED ROW_UPD_SEC_INDEX_ENTRY (INNOBASE/ROW/ROW0UPD.CC:2103)
--echo #

CREATE TABLE t (
x INT, y INT, gc INT GENERATED ALWAYS AS (x+1) STORED
);
INSERT INTO t VALUES ();
UPDATE t t1, t t2 SET t2.y = 1, t1.x = 2;
SELECT * FROM t;
DROP TABLE t;

if ($support_virtual_index)
{
CREATE TABLE t (
x INT, y INT, gc INT GENERATED ALWAYS AS (x+1), KEY (x,gc)
);
INSERT INTO t VALUES ();
UPDATE t t1, t t2 SET t1.x = 1, t2.y = 2;
SELECT * FROM t;
SELECT gc FROM t;
DROP TABLE t;
}

let $query=
UPDATE C AS OUTR1, C AS OUTR2
SET OUTR1.`col_varchar_nokey` = 'f',
OUTR2.`col_varchar_nokey` = "a";

--echo # stored

CREATE TABLE C (
col_varchar_nokey VARCHAR(1),
col_varchar_key VARCHAR(2) GENERATED ALWAYS AS
(CONCAT(col_varchar_nokey, col_varchar_nokey)) STORED
);

INSERT INTO C (col_varchar_nokey) VALUES ('c');
eval EXPLAIN $query;
eval $query;
SELECT * from C;
DROP TABLE C;

--echo # stored, indexed

CREATE TABLE C (
col_varchar_nokey VARCHAR(1),
col_varchar_key VARCHAR(2) GENERATED ALWAYS AS
(CONCAT(col_varchar_nokey, col_varchar_nokey)) STORED,
KEY (col_varchar_key, col_varchar_nokey)
);

INSERT INTO C (col_varchar_nokey) VALUES ('c');
eval EXPLAIN $query;
eval $query;
SELECT * from C;
DROP TABLE C;

--echo # virtual

CREATE TABLE C (
col_varchar_nokey VARCHAR(1),
col_varchar_key VARCHAR(2) GENERATED ALWAYS AS
(CONCAT(col_varchar_nokey, col_varchar_nokey)) VIRTUAL
);

INSERT INTO C (col_varchar_nokey) VALUES ('c');
eval EXPLAIN $query;
eval $query;
SELECT * from C;
DROP TABLE C;

if ($support_virtual_index)
{
--echo # virtual, indexed

CREATE TABLE C (
col_varchar_nokey VARCHAR(1),
col_varchar_key VARCHAR(2) GENERATED ALWAYS AS
(CONCAT(col_varchar_nokey, col_varchar_nokey)) VIRTUAL,
KEY (col_varchar_key, col_varchar_nokey)
);

INSERT INTO C (col_varchar_nokey) VALUES ('c');
eval EXPLAIN $query;
eval $query;
SELECT * from C;
DROP TABLE C;
}

if ($support_virtual_index)
{
--echo #
--echo # Bug #21530366 CRASH/ASSERTION, CORRUPTION WITH INDEXES + 
--echo #               VIRTUAL COLUMNS, BLOB
--echo #

CREATE TABLE t (
  a INTEGER,
  b BLOB GENERATED ALWAYS AS (a) VIRTUAL,
  INDEX (b(57))
);

INSERT INTO t (a) VALUES (9);
UPDATE t SET a = 10;
DELETE FROM t WHERE a = 10;

DROP TABLE t;
}

if ($support_virtual_index)
{
--echo # Bug#21807818: Generated columns not updated with empty insert list

--source include/turn_off_strict_mode.inc

CREATE TABLE t (
a BLOB GENERATED ALWAYS AS ('') VIRTUAL,
b TIMESTAMP(4) GENERATED ALWAYS AS ('') VIRTUAL NOT NULL,
KEY (a(183),b)
);

INSERT INTO t VALUES(), (), ();

DELETE IGNORE FROM t;

DROP TABLE t;

--source include/restore_strict_mode.inc

--echo #
--echo # Bug#22195458:GCOLS: ASSERTION 0 AND CORRUPTION...
--echo #
--disable_warnings
--source include/turn_off_strict_mode.inc
CREATE TABLE t (
  a INT,
  b YEAR GENERATED ALWAYS AS ('a') VIRTUAL,
  c YEAR GENERATED ALWAYS AS ('aaaa') VIRTUAL,
  b1 YEAR GENERATED ALWAYS AS ('a') STORED,
  c1 YEAR GENERATED ALWAYS AS ('aaaa') STORED,
  UNIQUE(b),
  UNIQUE(b1)
);
INSERT INTO t VALUES();
SELECT b from t;
SELECT b1 from t;
SELECT * from t;
DELETE FROM t;
CHECK TABLE t EXTENDED;
DROP TABLE t;
--source include/restore_strict_mode.inc
--enable_warnings

--echo # Bug#22195364:GCOLS: FAILING ASSERTION:
--echo #              DFIELD_IS_NULL(DFIELD2) || DFIELD2->DATA
CREATE TABLE t (
  a INT,
  c BLOB GENERATED ALWAYS AS ('') VIRTUAL NOT NULL,
  UNIQUE KEY(c(1),a)
);
INSERT INTO t(a) VALUES(1) ON DUPLICATE KEY UPDATE a=2;
SELECT * FROM t;
INSERT INTO t(a) VALUES(1) ON DUPLICATE KEY UPDATE a=2;
SELECT * FROM t;
# Test Field_blob::store_to_mem
SELECT GROUP_CONCAT(c ORDER BY c) FROM t;
DROP TABLE t;
}

--echo #Bug#21929967:GCOLS:GCOL VALUE CHANGES WHEN SESSION CHANGES SQL_MODE
CREATE TABLE t(c1 INT GENERATED ALWAYS AS (1) VIRTUAL NOT NULL,
               c2 INT GENERATED ALWAYS AS(2) STORED NOT NULL);
INSERT INTO t VALUES(DEFAULT, DEFAULT);
SELECT * FROM t;
CREATE TABLE t1(c1 INT, c2 INT GENERATED ALWAYS AS(c1 + 1) STORED);
INSERT INTO t1(c2) VALUES(DEFAULT);
SELECT * FROM t1;
CREATE TABLE t2(c1 INT DEFAULT 1, c2 INT GENERATED ALWAYS AS(c1 + 1) STORED NOT NULL);
INSERT INTO t2(c2) VALUES(DEFAULT);
SELECT * FROM t2;
DROP TABLE t, t1, t2;

--echo # Bug#22179637: INSERT INTO TABLE FROM SELECT ACCEPTS TO INSERT INTO
--echo #              GENERATED COLUMNS
CREATE TABLE t1 (
  i1 INTEGER,
  i2 INTEGER GENERATED ALWAYS AS (i1 + i1)
);
--error ER_NON_DEFAULT_VALUE_FOR_GENERATED_COLUMN
INSERT INTO t1 (i1, i2) SELECT 5, 6;
INSERT INTO t1 (i1) SELECT 5;
--error ER_NON_DEFAULT_VALUE_FOR_GENERATED_COLUMN
INSERT INTO t1 (i1) SELECT 5 ON DUPLICATE KEY UPDATE i2= 4;
INSERT INTO t1 (i1) SELECT 5 ON DUPLICATE KEY UPDATE i2= DEFAULT;
SELECT * FROM t1;

CREATE TABLE t2 (
  i1 INTEGER,
  i2 INTEGER GENERATED ALWAYS AS (i1 + i1) STORED
);
--error ER_NON_DEFAULT_VALUE_FOR_GENERATED_COLUMN
INSERT INTO t2 (i1, i2) SELECT 5, 6;
--error ER_NON_DEFAULT_VALUE_FOR_GENERATED_COLUMN
INSERT INTO t2 (i1, i2) SELECT * FROM t1;
INSERT INTO t2 (i1) SELECT 5;
--error ER_NON_DEFAULT_VALUE_FOR_GENERATED_COLUMN
INSERT INTO t2 (i1) SELECT 5 ON DUPLICATE KEY UPDATE i2= 4;
INSERT INTO t2 (i1) SELECT 5 ON DUPLICATE KEY UPDATE i2= DEFAULT;
SELECT * FROM t2;

DROP TABLE t1,t2;

if ($support_virtual_index)
{

--echo #
--echo # Bug#22070021 GCOL:ASSERTION `!TABLE || (!TABLE->WRITE_SET ||
--echo #              BITMAP_IS_SET(TABLE->WRITE_SET,
--echo #

CREATE TABLE t1(
c1 INT,
c2 INT GENERATED ALWAYS AS (c1 + c1) VIRTUAL,
KEY(c2)
);

INSERT INTO t1(c1) VALUES(0);
DELETE O1.* FROM t1 AS O1, t1 AS O2;
SELECT * FROM t1;
DROP TABLE t1;

--echo #
--echo # Bug#21944199 SIMPLE DELETE QUERY CAUSES INNODB: FAILING ASSERTION: 0
--echo #              & DATA CORRUPTION
--echo #

CREATE TEMPORARY TABLE t1 (
  a INTEGER NOT NULL,
  b INTEGER GENERATED ALWAYS AS (a+1) VIRTUAL
);

INSERT INTO t1 (a) VALUES (0), (0), (0);

ALTER TABLE t1 ADD INDEX idx (b);

DELETE FROM t1;

DROP TEMPORARY TABLE t1;

}
