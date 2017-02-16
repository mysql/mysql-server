################################################################################
# inc/vcol_ins_upd.inc                                                         #
#                                                                              #
# Purpose:                                                                     #
#  Testing DDL operations such as INSERT, UPDATE, REPLACE and DELETE.          #
#                                                                              #
#                                                                              #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: Andrey Zhakov                                               #
# Original Date: 2008-09-04                                                    #
# Change Author: Oleksandr Byelkin (Monty program Ab)
# Date: 2009-03-24 
# Change: Syntax changed
################################################################################

let $create1 = create table t1 (a int, 
                                b int as (-a),
				c int as (-a) persistent);
let $create2 = create table t1 (a int unique, 
                                b int as (-a),
				c int as (-a) persistent);
let $create3 = create table t1 (a int, 
                                b int as (-a),
				c int as (-a) persistent unique);
let $create4 = create table t1 (a int, 
                                b int as (-a),
				c int as (-a) persistent unique,
				d varchar(16));
eval $create1;
set sql_warnings = 1;

--echo #
--echo # *** INSERT ***
--echo #

--echo # INSERT INTO tbl_name VALUES... DEFAULT is specified against vcols
insert into t1 values (1,default,default);
select * from t1;
delete from t1;
select * from t1;

--echo # INSERT INTO tbl_name VALUES... NULL is specified against vcols
insert into t1 values (1,null,null);
select * from t1;
delete from t1;
select * from t1;

--echo # INSERT INTO tbl_name VALUES... a non-NULL value is specified against vcols
insert into t1 values (1,2,3);
select * from t1;
delete from t1;
select * from t1;

--echo # INSERT INTO tbl_name (<non_vcol_list>) VALUES...
insert into t1 (a) values (1), (2);
select * from t1;
delete from t1;
select * from t1;

--echo # INSERT INTO tbl_name (<normal+vcols>) VALUES... DEFAULT is specified 
--echo # against vcols
insert into t1 (a,b) values (1,default), (2,default);
select * from t1;
delete from t1;
select * from t1;

--echo # INSERT INTO tbl_name (<normal+vcols>) VALUES... NULL is specified against vcols
insert into t1 (a,b) values (1,null), (2,null);
select * from t1;
delete from t1;
select * from t1;

--echo # INSERT INTO tbl_name (<normal+vcols>) VALUES... a non-NULL value is specified 
--echo # against vcols
insert into t1 (a,b) values (1,3), (2,4);
select * from t1;
delete from t1;
select * from t1;
drop table t1;

--echo # Table with UNIQUE non-vcol field. INSERT INTO tbl_name VALUES... ON DUPLICATE 
--echo # KEY UPDATE <non_vcol>=expr, <vcol>=expr
eval $create2;
insert into t1 values (1,default,default);
insert into t1 values (1,default,default) 
       on duplicate key update a=2, b=default;
select a,b,c from t1;
delete from t1 where b in (1,2);
select * from t1;
drop table t1;

--echo # Table with UNIQUE vcol field. INSERT INTO tbl_name VALUES... ON DUPLICATE 
--echo # KEY UPDATE <non_vcol>=expr, <vcol>=expr
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

--echo # CREATE new_table ... LIKE old_table INSERT INTO new_table (<non-vcols>, <vcols>) 
--echo # SELECT <non-vcols>, <vcols> from old_table
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

--echo # UPDATE tbl_name SET non-vcol=expr WHERE non-vcol=expr
eval $create1;
insert into t1 (a) values (1), (2);
select * from t1;
update t1 set a=3 where a=2;
select * from t1;
delete from t1;
select * from t1;

--echo # UPDATE tbl_name SET vcol=expr WHERE non-vcol=expr
insert into t1 (a) values (1), (2);
select * from t1;
update t1 set c=3 where a=2;
select * from t1;
delete from t1;
select * from t1;

--echo # UPDATE tbl_name SET non-vcol=expr WHERE vcol=expr
insert into t1 (a) values (1), (2);
select * from t1;
update t1 set a=3 where b=-2;
select * from t1;
delete from t1;
select * from t1;

--echo # UPDATE tbl_name SET vcol=expr WHERE vcol=expr
insert into t1 (a) values (1), (2);
select * from t1;
update t1 set c=3 where b=-2;
select * from t1;
delete from t1;
select * from t1;
drop table t1;

--echo # INDEX created on vcol 
--echo # UPDATE tbl_name SET non-vcol=expr WHERE vcol=const
eval $create3;
insert into t1 (a) values (1), (2);
select * from t1;
update t1 set a=3 where c=-2;
select * from t1;
delete from t1;
select * from t1;


--echo # INDEX created on vcol 
--echo # UPDATE tbl_name SET non-vcol=expr WHERE vcol=between const1 and const2
insert into t1 (a) values (1), (2);
select * from t1;
update t1 set a=3 where c between -3 and -2;
select * from t1;
delete from t1;
select * from t1;

--echo # No INDEX created on vcol 
--echo # UPDATE tbl_name SET non-vcol=expr WHERE vcol=between const1 and const2
insert into t1 (a) values (1), (2);
select * from t1;
update t1 set a=3 where b between -3 and -2;
select * from t1;
delete from t1;
select * from t1;

--echo # INDEX created on vcol 
--echo # UPDATE tbl_name SET non-vcol=expr 
--echo # WHERE vcol=between const1 and const2 ORDER BY vcol
insert into t1 (a) values (1), (2), (3), (4), (5);
select * from t1;
update t1 set a=6 where c between -1 and 0
          order by c;
select * from t1;
delete from t1 where c between -6 and 0;
select * from t1;

--echo # INDEX created on vcol 
--echo # UPDATE tbl_name SET non-vcol=expr 
--echo # WHERE vcol=between const1 and const2 ORDER BY vcol LIMIT 2
insert into t1 (a) values (1), (2), (3), (4), (5);
select * from t1;
update t1 set a=6 where c between -1 and 0
          order by c limit 2;
select * from t1;
delete from t1 where c between -2 and 0 order by c;
select * from t1;
delete from t1;

--echo # INDEX created on vcol 
--echo # UPDATE tbl_name SET non-vcol=expr
--echo # WHERE indexed vcol=between const1 and const2 and non-indexed vcol=const3
insert into t1 (a) values (1), (2), (3), (4), (5);
select * from t1;
update t1 set a=6 where (c between -2 and 0) and (b=-1);
select * from t1;
delete from t1;

--echo # INDEX created on vcol 
--echo # UPDATE tbl_name SET non-vcol=expr
--echo # WHERE indexed vcol=between const1 and const2 and non-indexed vcol=const3
--echo # ORDER BY indexed vcol
insert into t1 (a) values (1), (2), (3), (4), (5);
select * from t1;
update t1 set a=6 where (c between -2 and 0) and (b=-1) order by c;
select * from t1;
delete from t1;
drop table t1;

let $innodb_engine = `SELECT @@session.storage_engine='innodb'`;
if ($innodb_engine)
{
  --echo #
  --echo # Verify ON UPDATE/DELETE actions of FOREIGN KEYs
  create table t2 (a int primary key, name varchar(10));
  create table t1 (a int primary key, b int as (a % 10) persistent);
  insert into t2 values (1, 'value1'), (2,'value2'), (3,'value3');
  insert into t1 (a) values (1),(2),(3);
  select * from t1;
  select * from t2;
  select t1.a, t1.b, t2.name from t1,t2 where t1.b=t2.a;

  --echo #  - ON UPDATE RESTRICT
  alter table t1 add foreign key (b) references t2(a) on update restrict;
  --error ER_NO_REFERENCED_ROW_2
  insert into t1 (a) values (4);
  --error ER_ROW_IS_REFERENCED_2
  update t2 set a=4 where a=3;
  select t1.a, t1.b, t2.name from t1,t2 where t1.b=t2.a;
  alter table t1 drop foreign key t1_ibfk_1;

  --echo #  - ON DELETE RESTRICT
  alter table t1 add foreign key (b) references t2(a) on delete restrict;
  --error ER_ROW_IS_REFERENCED_2
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

--echo # UNIQUE INDEX on vcol
--echo # REPLACE tbl_name (non-vcols) VALUES (non-vcols);
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

--echo #
--echo # MDEV-9093: Persistent computed column is not updated when
--echo # update query contains join
--echo #

CREATE TABLE `t1` (
  `id` bigint(20) NOT NULL,
  `name` varchar(254) DEFAULT NULL,
  `name_hash` varchar(64) AS (sha1(name)) PERSISTENT,
  PRIMARY KEY (`id`)
);

insert into t1(id,name) values (2050, 'name1'),(2051, 'name2'),(2041, 'name3');

create table t2 (id bigint);
insert into t2 values (2050),(2051),(2041);

select * from t1;

update t1 join t2 using(id) set name = concat(name,
'+1') where t1.id in (2051,2041);

select * from t1;

drop table t1,t2;

