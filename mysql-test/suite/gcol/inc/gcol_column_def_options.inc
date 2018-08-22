################################################################################
# inc/gcol_column_def_options.inc                                              #
#                                                                              #
# Purpose:                                                                     #
#  Testing different optional parameters specified when defining               #
#  a generated column.                                                         #
#                                                                              #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: Andrey Zhakov                                               #
# Original Date: 2008-09-02                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

--echo #
--echo # Section 1. Wrong column definition options
--echo #            - DEFAULT <value>
--echo #            - AUTO_INCREMENT
--echo #            - SERIAL DEFAULT VALUE
--echo #            - ON UPDATE NOW ...

--echo # NOT NULL
create table t1 (a int, b int generated always as (a+1) virtual not null);
--error 1048
insert into t1(a) values(null);
insert into t1(a) values(1);
select * from t1;
drop table t1;
create table t1 (a int, b int generated always as (a+1) stored not null);
--error 1048
insert into t1(a) values(null);
insert into t1(a) values(1);
select * from t1;
drop table t1;
create table t1 (a int);
alter table t1 add column b int generated always as (a+1) virtual not null;
drop table t1;

--echo # NULL  
create table t1 (a int, b int generated always as (a+1) virtual null);
drop table t1;
create table t1 (a int);
alter table t1 add column b int generated always as (a+1) virtual null;
drop table t1;

--echo # Added columns mixed with virtual GC and other columns
create table t1 (a int);
insert into t1 values(1);
--enable_info
alter table t1 add column (b int generated always as (a+1) virtual, c int);
alter table t1 add column (d int, e int generated always as (a+1) virtual);
alter table t1 add column (f int generated always as (a+1) virtual, g int as(5) stored);
alter table t1 add column (h int generated always as (a+1) virtual, i int as(5) virtual);
--disable_info
drop table t1;

--echo # DEFAULT
--error ER_WRONG_USAGE
create table t1 (a int, b int generated always as (a+1) virtual default 0);
create table t1 (a int);
--error ER_WRONG_USAGE
alter table t1 add column b int generated always as (a+1) virtual default 0;
drop table t1;

--echo # AUTO_INCREMENT
--error ER_WRONG_USAGE
create table t1 (a int, b int generated always as (a+1) virtual AUTO_INCREMENT);
create table t1 (a int);
--error ER_WRONG_USAGE
alter table t1 add column b int generated always as (a+1) virtual AUTO_INCREMENT;
drop table t1;

--echo # SERIAL DEFAULT VALUE
--error ER_WRONG_USAGE
create table t1 (a int, b int generated always as (a+1) virtual SERIAL DEFAULT VALUE);
create table t1 (a int);
--error ER_WRONG_USAGE
alter table t1 add column b int generated always as (a+1) virtual SERIAL DEFAULT VALUE;
drop table t1;

--echo # ON UPDATE
--error ER_WRONG_USAGE
create table t1 (a int, b int generated always as (a+1) virtual ON UPDATE NOW());
create table t1 (a int);
--error ER_WRONG_USAGE
alter table t1 add column b int generated always as (a+1) virtual ON UPDATE NOW();
drop table t1;

--echo # [PRIMARY] KEY
if ($support_virtual_index)
{
--error ER_UNSUPPORTED_ACTION_ON_GENERATED_COLUMN
create table t1 (a int, b int generated always as (a+1) virtual key);
}
create table t1 (a int, b int generated always as (a+1) stored key);
insert into t1 (a) values (3),(1),(2);
--sorted_result
select * from t1;
drop table t1;
if ($support_virtual_index)
{
--error ER_UNSUPPORTED_ACTION_ON_GENERATED_COLUMN
create table t1 (a int, b int generated always as (a+1) virtual primary key);
}
create table t1 (a int, b int generated always as (a+1) stored primary key);
insert into t1 (a) values (3),(1),(2);
select * from t1 order by b;
drop table t1;
create table t1 (a int);
if ($support_virtual_index)
{
--error ER_UNSUPPORTED_ACTION_ON_GENERATED_COLUMN
alter table t1 add column b int generated always as (a+1) virtual key;
}
alter table t1 add column b int generated always as (a+1) stored key;
if ($support_virtual_index)
{
--error ER_MULTIPLE_PRI_KEY
alter table t1 add column c int generated always as (a+2) virtual primary key;
}
show create table t1;
--error ER_MULTIPLE_PRI_KEY
alter table t1 add column c int generated always as (a+2) stored primary key;
drop table t1;

--echo # Section 2. Other column definition options
--echo #            - COMMENT
--echo #            - REFERENCES (only syntax testing here)
--echo #            - STORED (only systax testing here)
create table t1 (a int, b int generated always as (a % 2) virtual comment 'my comment');
show create table t1;
describe t1;
drop table t1;
create table t1 (a int, b int generated always as (a % 2) virtual);
alter table t1 modify b int generated always as (a % 2) virtual comment 'my comment';
show create table t1;
describe t1;
insert into t1 (a) values (1);
select * from t1;
insert into t1 values (2,default);
select a,b from t1 order by a;
create table t2 like t1;
show create table t2;
describe t2;
insert into t2 (a) values (1);
select * from t2;
insert into t2 values (2,default);
select a,b from t2 order by a;
drop table t2;
drop table t1;

create table t1 (a int, b int generated always as (a % 2) stored);
show create table t1;
describe t1;
insert into t1 (a) values (1);
select * from t1;
insert into t1 values (2,default);
select a,b from t1 order by a;
drop table t1;


create table t2 (a int);
create table t1 (a int, b int generated always as (a % 2) stored references t2(a));
show create table t1;
drop table t1;
create table t1 (a int, b int generated always as (a % 2) virtual);
--error 1064
alter table t1 modify b int generated always as (a % 2) stored references t2(a);
show create table t1;
drop table t1;
drop table t2;
--echo FK options
create table t1(a int,  b int as (a % 2), c int as (a) stored);
create table t2 (a int primary key);
--error ER_KEY_COLUMN_DOES_NOT_EXITS
alter table t1 add constraint foreign key fk(d) references t2(a);
if ($support_virtual_index)
{
--error ER_FK_CANNOT_USE_VIRTUAL_COLUMN
alter table t1 add constraint foreign key fk(b) references t2(a);
}
--error ER_WRONG_FK_OPTION_FOR_GENERATED_COLUMN
alter table t1 add constraint foreign key fk(c) references t2(a) on delete set null;
--error ER_WRONG_FK_OPTION_FOR_GENERATED_COLUMN
alter table t1 add constraint foreign key fk(c) references t2(a) on update set null;
--error ER_WRONG_FK_OPTION_FOR_GENERATED_COLUMN
alter table t1 add constraint foreign key fk(c) references t2(a) on update cascade;
drop table t1;
drop table t2;
--echo Generated alwasy is optional
create table t1 (a int, b int as (a % 2) virtual);
show create table t1;
describe t1;
drop table t1;
create table t1 (a int, b int as (a % 2) stored);
show create table t1;
describe t1;
drop table t1;
--echo Default should be non-stored column
create table t1 (a int, b int as (a % 2));
show create table t1;
describe t1;
drop table t1;
--echo Expression can be constant 
create table t1 (a int, b int as (5 * 2));
show create table t1;
describe t1;
drop table t1;
--echo Test generated columns referencing other generated columns
create table t1 (a int unique, b int generated always as(-a) virtual, c int generated always as (b + 1) virtual);
insert into t1 (a) values (1), (2);
--sorted_result
select * from t1;
insert into t1(a) values (1) on duplicate key update a=3;
--sorted_result
select * from t1;
update t1 set a=4 where a=2;
--sorted_result
select * from t1;
drop table t1;

--error ER_GENERATED_COLUMN_NON_PRIOR
create table t1 (a int, b int generated always as(-b) virtual, c int generated always as (b + 1) virtual);
--error ER_GENERATED_COLUMN_NON_PRIOR
create table t1 (a int, b int generated always as(-c) virtual, c int generated always as (b + 1) virtual);
--error ER_GENERATED_COLUMN_REF_AUTO_INC
CREATE TABLE t1 (pk INTEGER AUTO_INCREMENT, col_int_nokey INTEGER GENERATED ALWAYS AS (pk + col_int_key) STORED, col_int_key INTEGER, PRIMARY KEY (pk));

--echo # Bug#20339347: FAIL TO USE CREATE ....SELECT STATEMENT TO CREATE A NEW TABLE
create table t1 (a int, b int generated always as(-a) virtual, c int generated always as (b + 1) stored);
insert into t1(a) values(1),(2);
create table tt as select * from t1;
select * from t1 order by a;
select * from tt order by a;
drop table t1,tt;

if (!$support_virtual_index)
{
--echo # Bug#20769299: INCORRECT KEY ERROR WHEN TRYING TO CREATE INDEX ON
--echo #               VIRTUAL GC FOR MYISAM
--error ER_ILLEGAL_HA_CREATE_OPTION
CREATE TABLE A (
pk INTEGER AUTO_INCREMENT,
col_int_nokey INTEGER,
col_int_key INTEGER GENERATED ALWAYS AS (pk + col_int_nokey) VIRTUAL, KEY
(col_int_key));
}

--echo # Bug#20745142: GENERATED COLUMNS: ASSERTION FAILED:
--echo #   THD->CHANGE_LIST.IS_EMPTY()
--echo #
--error ER_GENERATED_COLUMN_NON_PRIOR
CREATE TABLE t1(a bigint AS (a between 1 and 1));

--echo # Bug#20757211: GENERATED COLUMNS: ALTER TABLE CRASHES
--echo #  IN FIND_FIELD_IN_TABLE
--echo #
CREATE TABLE t1(a int);
--error ER_GENERATED_COLUMN_FUNCTION_IS_NOT_ALLOWED
ALTER TABLE t1 ADD COLUMN z int GENERATED ALWAYS AS
( 1 NOT IN (SELECT 1 FROM t1 WHERE c0006) ) virtual;
DROP TABLE t1;

--echo # Bug#20566243: ERROR WHILE DOING CREATE TABLE T1 SELECT (QUERY ON GC COLUMNS)
CREATE TABLE t1(a int, b int as (a + 1),
            c varchar(12) as ("aaaabb") stored, d blob as (c));
INSERT INTO t1(a) VALUES(1),(3);
SHOW CREATE TABLE t1;
SELECT * FROM t1 order by a;
CREATE TABLE t2 LIKE t1;
SHOW CREATE TABLE t2;
CREATE TABLE t3 AS SELECT * FROM t1;
SHOW CREATE TABLE t3;
SELECT * FROM t3 order by a;
CREATE TABLE t4 AS SELECT b,c,d FROM t1;
SHOW CREATE TABLE t4;
SELECT * FROM t4 order by b;
DROP TABLE t1,t2,t3,t4;

--echo # Bug#21025003:WL8149:ASSERTION `CTX->NUM_TO_DROP_FK
--echo #  == HA_ALTER_INFO->ALTER_INFO-> FAILED
--echo #
CREATE TABLE t1 (
  col1 int(11) DEFAULT NULL,
  col2 int(11) DEFAULT NULL,
  col3 int(11) DEFAULT NULL,
  col4 int(11) DEFAULT NULL,
  col5 int(11) GENERATED ALWAYS AS (col4 / col2) VIRTUAL,
  col6 text
);
INSERT INTO t1(col1,col2,col3,col4,col6) VALUES(NULL,1,4,0,REPEAT(2,1000));
--error ER_CANT_DROP_FIELD_OR_KEY
ALTER TABLE t1 DROP PRIMARY KEY , ADD KEY idx ( col5, col2 ); 
DROP TABLE t1;
--echo # Bug#20949226:i CAN ASSIGN NON-DEFAULT() VALUE TO GENERATED COLUMN
--echo # 
--error ER_NON_DEFAULT_VALUE_FOR_GENERATED_COLUMN
CREATE TABLE t1 (c1 INT, c2 INT AS (c1 * 2)) SELECT 1 AS c1, 5 AS c2;
CREATE TABLE t2 (a int);
INSERT INTO t2 values(1);
--error ER_NON_DEFAULT_VALUE_FOR_GENERATED_COLUMN
CREATE TABLE t1 (c1 INT, c2 INT AS (c1 * 2)) SELECT 1 AS c1, a AS c2 from t2;
CREATE TABLE t1 (c1 INT, c2 INT AS (c1 * 2)) SELECT 1 AS c1, 5;
SELECT * FROM t1;
DROP TABLE t1, t2;

if ($support_virtual_index)
{
--echo # Bug#21074624:i WL8149:SIG 11 INNOBASE_GET_COMPUTED_VALUE |
--echo #  INNOBASE/HANDLER/HA_INNODB.CC:19082
CREATE TABLE t1 (
  col1 int(11) NOT NULL,
  col2 int(11) DEFAULT NULL,
  col3 int(11) NOT NULL,
  col4 int(11) DEFAULT NULL,
  col5 int(11) GENERATED ALWAYS AS (col2 % col4) VIRTUAL,
  col6 int(11) GENERATED ALWAYS AS (col3 + col3) VIRTUAL,
  col7 int(11) GENERATED ALWAYS AS (col5 / col5) VIRTUAL,
  col8 int(11) GENERATED ALWAYS AS (col6 / col5) VIRTUAL,
  col9 text,
  extra int(11) DEFAULT NULL,
  KEY idx (col5)
);
INSERT INTO t1(col1,col2,col3,col4,col9,extra)
VALUES(0,6,3,4,REPEAT(4,1000),0);
ALTER TABLE t1 DROP COLUMN col1;
DROP TABLE t1;

--echo # Bug#21390605:VALGRIND ERROR ON DELETE FROM TABLE CONTAINING
--echo #  AN INDEXED VIRTUAL COLUMN
CREATE TABLE t1 (
  a INTEGER,
  b INTEGER GENERATED ALWAYS AS (a) VIRTUAL,
  c INTEGER GENERATED ALWAYS AS (b) VIRTUAL,
  INDEX idx (b,c)
);
INSERT INTO t1 (a) VALUES (42);
DELETE FROM t1 WHERE c = 42;
DROP TABLE t1;
}

--echo # Bug#20757211: GENERATED COLUMNS: ALTER TABLE CRASHES
--echo #  IN FIND_FIELD_IN_TABLE
--echo # 
CREATE TABLE t1(a int);
--error ER_GENERATED_COLUMN_FUNCTION_IS_NOT_ALLOWED
ALTER TABLE t1 ADD COLUMN z int GENERATED ALWAYS AS
( 1 NOT IN (SELECT 1 FROM t1 WHERE c0006) ) virtual;
--error ER_GENERATED_COLUMN_FUNCTION_IS_NOT_ALLOWED
CREATE TABLE t2(a int, b int as (1 NOT IN (SELECT 1 FROM t1 WHERE not_exist_col)));
--error ER_GENERATED_COLUMN_FUNCTION_IS_NOT_ALLOWED
CREATE TABLE t2(a int, b int as (1 NOT IN (SELECT 1 FROM dual)));
DROP TABLE t1;

if(! $testing_ndb) {
  --echo # Bug#21142905: PARTITIONED GENERATED COLS -
  --echo #  !TABLE || (!TABLE->WRITE_SET || BITMAP_IS_SET
  --echo # 
  if ($support_native_partitioning)
  {
    CREATE TABLE t1 (
    a int,
    b int generated always as (a) virtual,
    c int generated always as (b+a) virtual,
    d int generated always as (b+a) virtual
    ) PARTITION BY LINEAR HASH (b);
    INSERT INTO t1(a) VALUES(0);
    DELETE FROM t1 WHERE c=1;
    DROP TABLE t1;
  }
  if (!$support_native_partitioning)
  {
   --echo "Skipped since this engine does not support native partitioning.
  }
}

--echo # Bug #20709487: COLLATE OPTION NOT ACCEPTED IN GENERATED COLUMN
--echo #                DEFINITION
--echo #

--echo # Check for a valid syntax:

CREATE TABLE t1 (c CHAR(10) CHARACTER SET utf8 COLLATE utf8_bin GENERATED ALWAYS AS ("foo bar"));
SHOW CREATE TABLE t1;
DROP TABLE t1;

CREATE TABLE t1 (i INT);
ALTER TABLE t1 ADD COLUMN c CHAR(10) CHARACTER SET utf8 COLLATE utf8_bin GENERATED ALWAYS AS ("foo bar");
SHOW CREATE TABLE t1;
DROP TABLE t1;

CREATE TABLE t1 (i INT COLLATE utf8_bin, c INT COLLATE utf8_bin GENERATED ALWAYS AS (10));
SHOW CREATE TABLE t1;
DROP TABLE t1;

--echo # Check for a charset mismatch processing:

--error ER_COLLATION_CHARSET_MISMATCH
CREATE TABLE t1 (t CHAR(10) CHARACTER SET utf8 COLLATE ascii_bin GENERATED ALWAYS AS ("foo bar"));


--echo # Check for a sorting order support:

CREATE TABLE t1 (c CHAR(10) CHARSET latin1 COLLATE latin1_bin,
                 c_ci CHAR(10) CHARSET latin1 COLLATE latin1_general_ci GENERATED ALWAYS AS (c),
                 c_cs CHAR(10) CHARSET latin1 COLLATE latin1_general_cs GENERATED ALWAYS AS (c));

INSERT INTO t1 (c) VALUES ('a'), ('A'), ('b');

SELECT * FROM t1 ORDER BY c;
SELECT * FROM t1 ORDER BY c_ci, c;
SELECT * FROM t1 ORDER BY c_cs;

--sorted_result
SELECT c, c_ci REGEXP 'A', c_cs REGEXP 'A' FROM t1;

DROP TABLE t1;

if(! $testing_ndb) {
  --echo # Bug #21469535: VALGRIND ERROR (CONDITIONAL JUMP) WHEN INSERT
  --echo #                ROWS INTO A PARTITIONED TABLE
  --echo #
  if ($support_native_partitioning)
  {
    CREATE TABLE t1 (
        id INT NOT NULL,
        store_id INT NOT NULL,
        x INT GENERATED ALWAYS AS (id + store_id)
    )
    PARTITION BY RANGE (store_id) (
        PARTITION p0 VALUES LESS THAN (6),
        PARTITION p1 VALUES LESS THAN (11),
        PARTITION p2 VALUES LESS THAN (16),
        PARTITION p3 VALUES LESS THAN (21)
    );

    INSERT INTO t1 VALUES(1, 2, default);
    DROP TABLE t1;
  }
  if (!$support_native_partitioning)
  {
    --echo "Skipped since this engine does not support native partitioning.
  }
}

--echo # Bug#21465626:ASSERT/CRASH ON DROPPING/ADDING VIRTUAL COLUMN
CREATE TABLE t (a int(11), b int(11),
 c int(11) GENERATED ALWAYS AS (a+b) VIRTUAL,
 d int(11) GENERATED ALWAYS AS (a+b) VIRTUAL);
INSERT INTO t(a,b) VALUES(1,2);
--enable_info
--echo # Mixed drop/add/rename virtual with non-virtual columns,
--echo # ALGORITHM=INPLACE is not supported for InnoDB
ALTER TABLE t DROP d, ADD e varchar(10);
ALTER TABLE t ADD d int, ADD f char(10) AS ('aaa');
ALTER TABLE t CHANGE d dd int, CHANGE f ff varchar(10) AS ('bbb');
--echo # Only drop/add/change virtual, inplace is supported for Innodb
ALTER TABLE t DROP c, DROP ff;
ALTER TABLE t ADD c int(11) as (a+b), ADD f varchar(10) as ('aaa');
ALTER TABLE t CHANGE c c int(11) as (a), CHANGE f f varchar(10) as('bbb');
--echo # Change order should be ALGORITHM=INPLACE on Innodb
ALTER TABLE t CHANGE c c int(11) as (a) after f;
ALTER TABLE t CHANGE b b int(11) after c;
--echo # TODO: Changing virtual column type should be ALGORITHM=INPLACE on InnoDB, current it goes only with COPY method
ALTER TABLE t CHANGE c c varchar(10) as ('a');
--echo # Changing stored column type is ALGORITHM=COPY
ALTER TABLE t CHANGE dd d varchar(10);
if ($support_virtual_index)
{

ALTER TABLE t ADD INDEX idx(a), ADD INDEX idx1(c);
ALTER TABLE t RENAME INDEX idx TO idx2, RENAME INDEX idx1 TO idx3;
ALTER TABLE t DROP INDEX idx2, DROP INDEX idx3;
ALTER TABLE t ADD INDEX idx(c), ADD INDEX idx1(d);
ALTER TABLE t DROP INDEX idx, DROP INDEX idx1;
}
--disable_info
DROP TABLE t;

--echo #
--echo # Bug#21900170: CAN'T CREATE TABLE WITH NULLABLE GENERATED COLUMNS
--echo #

CREATE TABLE t1 (
  a INT,
  b INT GENERATED ALWAYS AS (-a) VIRTUAL NULL,
  c INT GENERATED ALWAYS AS (a + b) STORED NULL);

--replace_column 8 #
SHOW FULL COLUMNS FROM t1;

INSERT INTO t1 (a) VALUES (NULL), (1);
--sorted_result
SELECT * FROM t1;

DROP TABLE t1;

--echo # Bug#21854004: GCOLS:INNODB: FAILING ASSERTION: I < TABLE->N_DEF
CREATE TABLE t1(
 col1 INTEGER PRIMARY KEY,
 col2 INTEGER,
 col3 INTEGER,
 col4 INTEGER,
 vgc1 INTEGER AS (col2 + col3) VIRTUAL,
 sgc1 INTEGER AS (col2 - col3) STORED
);

INSERT INTO t1(col1, col2, col3) VALUES
 (1, 10, 100), (2, 20, 200);

SELECT * FROM t1 order by col1;

# Change expression of a virtual generated column
ALTER TABLE t1 MODIFY COLUMN vgc1 INTEGER AS (col2 * col3) VIRTUAL;
SELECT * FROM t1 order by col1;

# Change expression of a stored generated column
ALTER TABLE t1 MODIFY COLUMN sgc1 INTEGER AS (col2 / col3) STORED;
SELECT * FROM t1 order by col1;

ALTER TABLE t1 MODIFY COLUMN vgc1 INTEGER AS (col2 + col3) VIRTUAL;
ALTER TABLE t1 MODIFY COLUMN sgc1 INTEGER AS (col2 - col3) STORED;

if ($support_virtual_index)
{
ALTER TABLE t1 ADD INDEX vgc1 (vgc1);
}
ALTER TABLE t1 ADD INDEX sgc1 (sgc1);

if ($support_virtual_index)
{
# Change expression of a virtual generated column, with index
ALTER TABLE t1 MODIFY COLUMN vgc1 INTEGER AS (col2 * col3) VIRTUAL;
SELECT * FROM t1 order by col1;
SELECT vgc1 FROM t1 order by vgc1;
}

# Change expression of a stored generated column, with index
ALTER TABLE t1 MODIFY COLUMN sgc1 INTEGER AS (col2 / col3) STORED;
SELECT * FROM t1 order by col1;
SELECT sgc1 FROM t1 order by sgc1;

if ($support_virtual_index)
{
ALTER TABLE t1 DROP INDEX vgc1;
}
ALTER TABLE t1 DROP INDEX sgc1;

if ($support_virtual_index)
{
ALTER TABLE t1 MODIFY COLUMN vgc1 INTEGER AS (col2 + col3) VIRTUAL;
ALTER TABLE t1 ADD UNIQUE INDEX vgc1 (vgc1);
}
ALTER TABLE t1 MODIFY COLUMN sgc1 INTEGER AS (col2 - col3) STORED;
ALTER TABLE t1 ADD UNIQUE INDEX sgc1 (sgc1);

# Change expression of a virtual generated column, with unique index
if ($support_virtual_index)
{
--error ER_DUP_ENTRY
ALTER TABLE t1 MODIFY COLUMN vgc1 INTEGER AS (col2 / col3) VIRTUAL;
}
--error ER_DUP_ENTRY
ALTER TABLE t1 MODIFY COLUMN sgc1 INTEGER AS (col2 / col3) STORED;

ALTER TABLE t1 MODIFY COLUMN vgc1 INTEGER AS (col2 * col3) VIRTUAL;
SELECT * FROM t1 order by col1;
SELECT vgc1 FROM t1 order by col1;

ALTER TABLE t1 MODIFY COLUMN sgc1 INTEGER AS (col2 * col3) STORED;
SELECT * FROM t1 order by col1;
SELECT sgc1 FROM t1 order by sgc1;

# Change virtual generated column to become stored
--error ER_UNSUPPORTED_ACTION_ON_GENERATED_COLUMN
ALTER TABLE t1 MODIFY COLUMN vgc1 INTEGER AS (col2 * col3) STORED;

# Change stored generated column to become virtual
--error ER_UNSUPPORTED_ACTION_ON_GENERATED_COLUMN
ALTER TABLE t1 MODIFY COLUMN sgc1 INTEGER AS (col2 / col3) VIRTUAL;

# Change base column to become stored generated column:
ALTER TABLE t1 MODIFY COLUMN col4 INTEGER AS (col1 + col2 + col3) STORED;
SELECT * FROM t1 order by col1;

# Change stored generated column to become base column:
ALTER TABLE t1 MODIFY COLUMN col4 INTEGER;
SELECT * FROM t1 order by col1;

DROP TABLE t1;

if ($support_virtual_index)
{
--echo #
--echo # bug#22018979: RECORD NOT FOUND ON UPDATE,
--echo #                VIRTUAL COLUMN, ASSERTION 0
--disable_warnings
SET @sql_mode_save= @@sql_mode;
SET sql_mode= 'ANSI';
CREATE TABLE t1 (
  a INT,
  b VARCHAR(10),
  c CHAR(3) GENERATED ALWAYS AS (substr(b,1,3)) VIRTUAL,
  PRIMARY KEY (a),
  KEY c(c)
);
INSERT INTO t1(a, b) values(1, 'bbbb'), (2, 'cc');
SHOW CREATE TABLE t1;
SELECT * FROM t1 order by a;

SET sql_mode= '';
FLUSH TABLE t1;
SHOW CREATE TABLE t1;
SELECT * FROM t1 order by a;
DELETE FROM t1 where a= 2;

SET sql_mode= @sql_mode_save;
DROP TABLE t1;
--enable_warnings
}

--echo #
--echo # Bug#22680839: DEFAULT IS NOT DETERMINISTIC AND SHOULD NOT BE
--echo #               ALLOWED IN GENERATED COLUMNS
--echo #
if ($support_virtual_index)
{
CREATE TABLE tzz(a INT DEFAULT 5,
                 gc1 INT AS (a+DEFAULT(a)) VIRTUAL,
                 gc2 INT AS (a+DEFAULT(a)) STORED,
                 KEY k1(gc1));
INSERT INTO tzz(A) VALUES (1);
SELECT * FROM tzz;
SELECT gc1 FROM tzz;

ALTER TABLE tzz MODIFY COLUMN a INT DEFAULT 6;
SELECT * FROM tzz;
SELECT gc1 FROM tzz;
DROP TABLE tzz;
}

--echo # Test 1: ALTER DEFAULT
--echo #
CREATE TABLE t1(a INT PRIMARY KEY DEFAULT 5,
                b INT AS (1 + DEFAULT(a)) STORED,
                c INT AS (1 + DEFAULT(a)) VIRTUAL);
INSERT INTO t1 VALUES ();
--error ER_ALTER_OPERATION_NOT_SUPPORTED
ALTER TABLE t1 ALTER COLUMN a SET DEFAULT 7, ALGORITHM = INPLACE;
--disable_warnings
# Check how many rows are accessed: >0 = COPY
--enable_info
ALTER TABLE t1 ALTER COLUMN a SET DEFAULT 7;
ALTER TABLE t1 MODIFY COLUMN a INT DEFAULT 8;
ALTER TABLE t1 CHANGE COLUMN a a DOUBLE DEFAULT 5;
--disable_info
DROP TABLE t1;

--echo # Test 2: ALTER DEFAULT + ADD GCOL
--echo #
CREATE TABLE t1(a INT PRIMARY KEY DEFAULT 5);
INSERT INTO t1 VALUES();
--error ER_ALTER_OPERATION_NOT_SUPPORTED, ER_ALTER_OPERATION_NOT_SUPPORTED_REASON
ALTER TABLE t1 ALTER COLUMN a SET DEFAULT 7,
               ADD COLUMN b INT AS (1 + DEFAULT(a)) STORED,
               ALGORITHM = INPLACE;
# Check how many rows are accessed: >0 = COPY
--enable_info
ALTER TABLE t1 ALTER COLUMN a SET DEFAULT 7,
               ADD COLUMN b INT AS (1 + DEFAULT(a)) STORED,
               ADD COLUMN c INT AS (1 + DEFAULT(a)) VIRTUAL;
--disable_info
DROP TABLE t1;
--enable_warnings
