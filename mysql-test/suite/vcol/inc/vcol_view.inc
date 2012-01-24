################################################################################
# inc/vcol_view.inc                                                            #
#                                                                              #
# Purpose:                                                                     #
#  Testing views defined on tables with virtual columns.                       #
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



create table t1 (a int not null,
                 b int as (-a), 
		 c int as (-a) persistent);
insert into t1 (a) values (1), (1), (2), (2), (3);

# simple view
create view v1 (d,e) as select abs(b), abs(c) from t1;
select d,e from v1;
select is_updatable from information_schema.views where table_name='v1';

# view with different algorithms (explain output differs)
explain extended select d,e from v1;
create algorithm=temptable view v2 (d,e) as select abs(b), abs(c) from t1;
show create view v2;
select d,e from v2;
explain extended select d,e from v2;

# VIEW on VIEW test
create view v3 (d,e) as select d*2, e*2 from v1;
select * from v3;
explain extended select * from v3;

drop view v1,v2,v3;
drop table t1;

#
# DISTINCT option for VIEW
#
create table t1 (a int not null,
                 b int as (-a), 
		 c int as (-a) persistent);
insert into t1 (a) values (1), (2), (3), (1), (2), (3);
create view v1 as select distinct b from t1;
select * from v1;
explain select * from v1;
select * from t1;
drop view v1;
create view v1 as select distinct c from t1;
select * from v1;
explain select * from v1;
select * from t1;
drop view v1;
drop table t1;

#
# LIMIT clause test
#
create table t1 (a int not null,
                 b int as (-a), 
		 c int as (-a) persistent);
insert into t1 (a) values (1), (2), (3), (4);
create view v1 as select b+1 from t1 order by 1 desc limit 2;
select * from v1;
--echo MariaDB-5.3: the following EXPLAIN produces incorrect #rows for table t1.
--echo MariaDB-5.3: this is expected to go away when FROM subquery optimizations are pushed
explain select * from v1;
drop view v1;
create view v1 as select c+1 from t1 order by 1 desc limit 2;
select * from v1;
--echo MariaDB-5.3: the following EXPLAIN produces incorrect #rows for table t1.
--echo MariaDB-5.3: this is expected to go away when FROM subquery optimizations are pushed
explain select * from v1;
drop view v1;
drop table t1;

#
# simple view + simple update, insert and delete
#
create table t1 (a int,
                 b int,
		 c int as (-a),
		 d int as (-a) persistent,
                 primary key(a));
insert into t1 (a,b) values (10,2), (20,3), (30,4), (40,5), (50,10);
create view v1 (a,e,f,g) as select a, b+1,c+1,d+1 from t1;
# updatable field of updateable view
update v1 set a=a+e;
select * from v1;
select * from t1;
delete from v1;
select * from v1;
select * from t1;
--error ER_NON_INSERTABLE_TABLE
insert into v1 (a,e) values (60,15);
drop table t1;
drop view v1;

#
# outer join based on VIEW with WHERE clause
#

SET @save_optimizer_switch=@@optimizer_switch;
SET optimizer_switch='outer_join_with_cache=off';

create table t1 (a int, 
		 b int as (-a),
		 c int as (-a) persistent,
                 primary key(a));
insert into t1 (a) values (1), (2), (3);
create view v1 (x,y,z) as select a,b,c from t1 where b < -1;
select t1.a, v1.x, v1.y, v1.z from t1 left join v1 on (t1.b= v1.y);
drop view v1;
create view v1 (x,y,z) as select a,b,c from t1 where c < -1;
select t1.a, v1.x, v1.y, v1.z from t1 left join v1 on (t1.c= v1.z);
drop view v1;
drop table t1;

SET optimizer_switch=@save_optimizer_switch;

#
# VIEW built over UNION
#
create table t1 (a1 int,
                 b1 int as (-a1),
		 c1 int as (-a1) persistent);
create table t2 (a2 int,
                 b2 int as (-a2),
		 c2 int as (-a2) persistent);
insert into t1 (a1) values (1), (2);
insert into t2 (a2) values (2), (3);
create view v1 as select * from t1,t2 union all select * from t1,t2;
select * from v1;
drop view v1;
drop table t1, t2;

#
# Showing VIEW with VIEWs in subquery
#
create table t1 (a int,
                 b int as (-a),
		 c int as (-a) persistent);
create table t2 like t1;
create view v1 as select a,b,c from t1;
create view v2 as select a,b,c from t2 where b in (select b from v1);
show create view v2;
drop view v2, v1;
drop table t1, t2;

#
# TODO: VIEW with full text
#
#CREATE TABLE t1 (c1 int not null auto_increment primary key, c2 varchar(20), fulltext(c2));
#insert into t1 (c2) VALUES ('real Beer'),('Water'),('Kossu'),('Coca-Cola'),('Vodka'),('Wine'),('almost real Beer');
#select * from t1 WHERE match (c2) against ('Beer');
#CREATE VIEW v1 AS SELECT  * from t1 WHERE match (c2) against ('Beer');
#select * from v1;
#drop view v1;
#drop table t1;

#
# distinct in temporary table with a VIEW
#
create table t1 (a int,
                 b int as (-a),
		 c int as (-a) persistent);
insert into t1 (a) values (1),(1),(2),(2),(3),(3);
create view v1 as select b from t1;
select distinct b from v1;
select distinct b from v1 limit 2;
select distinct b from t1 limit 2;
prepare stmt1 from "select distinct b from v1 limit 2";
execute stmt1;
execute stmt1;
deallocate prepare stmt1;
drop view v1;
create view v1 as select c from t1;
select distinct c from v1;
select distinct c from v1 limit 2;
select distinct c from t1 limit 2;
prepare stmt1 from "select distinct c from v1 limit 2";
execute stmt1;
execute stmt1;
deallocate prepare stmt1;
drop view v1;
drop table t1;

#
# WITH CHECK OPTION insert/update test
#
create table t1 (a int,
                 b int as (-a),
		 c int as (-a) persistent);
create view v1 as select * from t1 where b > -2 && c >-2 with check option;
# simple insert
insert into v1 (a) values (1);
-- error 1369
insert into v1 (a) values (3);
# simple insert with ignore
insert ignore into v1 (a) values (2),(3),(0);
select * from t1;
drop view v1;
drop table t1;

