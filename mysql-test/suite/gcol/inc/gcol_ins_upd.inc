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
select * from t1;
delete from t1;
select * from t1;

--echo # INSERT INTO tbl_name (<normal+gcols>) VALUES... DEFAULT is specified 
--echo # against gcols
insert into t1 (a,b) values (1,default), (2,default);
select * from t1;
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
insert into t2 select * from t1;
select * from t1;
drop table t2;

--echo # CREATE new_table ... LIKE old_table INSERT INTO new_table (<non-gcols>, <gcols>) 
--echo # SELECT <non-gcols>, <gcols> from old_table
insert into t1 values (1,default,default);
select * from t1;
create table t2 like t1;
insert into t2 (a,b) select a,b from t1;
select * from t2;
drop table t2;
drop table t1;

--echo #
--echo # *** UPDATE ***
--echo #

--echo # UPDATE tbl_name SET non-gcol=expr WHERE non-gcol=expr
eval $create1;
insert into t1 (a) values (1), (2);
select * from t1;
update t1 set a=3 where a=2;
select * from t1;
delete from t1;
select * from t1;

--echo # UPDATE tbl_name SET gcol=expr WHERE non-gcol=expr
insert into t1 (a) values (1), (2);
select * from t1;
--error ER_NON_DEFAULT_VALUE_FOR_GENERATED_COLUMN
update t1 set c=3 where a=2;
select * from t1;
delete from t1;
select * from t1;

--echo # UPDATE tbl_name SET non-gcol=expr WHERE gcol=expr
insert into t1 (a) values (1), (2);
select * from t1;
update t1 set a=3 where b=-2;
select * from t1;
delete from t1;
select * from t1;

--echo # UPDATE tbl_name SET gcol=expr WHERE gcol=expr
insert into t1 (a) values (1), (2);
select * from t1;
--error ER_NON_DEFAULT_VALUE_FOR_GENERATED_COLUMN
update t1 set c=3 where b=-2;
select * from t1;
delete from t1;
select * from t1;
drop table t1;

--echo # INDEX created on gcol 
--echo # UPDATE tbl_name SET non-gcol=expr WHERE gcol=const
eval $create3;
insert into t1 (a) values (1), (2);
select * from t1;
update t1 set a=3 where c=-2;
select * from t1;
delete from t1;
select * from t1;


--echo # INDEX created on gcol 
--echo # UPDATE tbl_name SET non-gcol=expr WHERE gcol=between const1 and const2
insert into t1 (a) values (1), (2);
select * from t1;
update t1 set a=3 where c between -3 and -2;
select * from t1;
delete from t1;
select * from t1;

--echo # No INDEX created on gcol 
--echo # UPDATE tbl_name SET non-gcol=expr WHERE gcol=between const1 and const2
insert into t1 (a) values (1), (2);
select * from t1;
update t1 set a=3 where b between -3 and -2;
select * from t1;
delete from t1;
select * from t1;

--echo # INDEX created on gcol 
--echo # UPDATE tbl_name SET non-gcol=expr 
--echo # WHERE gcol=between const1 and const2 ORDER BY gcol
insert into t1 (a) values (1), (2), (3), (4), (5);
select * from t1;
update t1 set a=6 where c between -1 and 0
          order by c;
select * from t1;
delete from t1 where c between -6 and 0;
select * from t1;

--echo # INDEX created on gcol 
--echo # UPDATE tbl_name SET non-gcol=expr 
--echo # WHERE gcol=between const1 and const2 ORDER BY gcol LIMIT 2
insert into t1 (a) values (1), (2), (3), (4), (5);
select * from t1;
update t1 set a=6 where c between -1 and 0
          order by c limit 2;
select * from t1;
delete from t1 where c between -2 and 0 order by c;
select * from t1;
delete from t1;

--echo # INDEX created on gcol 
--echo # UPDATE tbl_name SET non-gcol=expr
--echo # WHERE indexed gcol=between const1 and const2 and non-indexed gcol=const3
insert into t1 (a) values (1), (2), (3), (4), (5);
select * from t1;
update t1 set a=6 where (c between -2 and 0) and (b=-1);
select * from t1;
delete from t1;

--echo # INDEX created on gcol 
--echo # UPDATE tbl_name SET non-gcol=expr
--echo # WHERE indexed gcol=between const1 and const2 and non-indexed gcol=const3
--echo # ORDER BY indexed gcol
insert into t1 (a) values (1), (2), (3), (4), (5);
select * from t1;
update t1 set a=6 where (c between -2 and 0) and (b=-1) order by c;
select * from t1;
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
  select * from t1;
  select * from t2;
  select t1.a, t1.b, t2.name from t1,t2 where t1.b=t2.a;

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
select * from t1;
replace t1 (a,d) values (1,'c');
select * from t1;
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

