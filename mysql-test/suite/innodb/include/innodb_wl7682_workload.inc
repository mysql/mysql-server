################################################################################
#
# 1. Create/Drop sequence.
# 2. Basic DML operation on intrinsic table.
# 3. Run different types of queries on intrinsic table.
# 4. Multiple key DML
# 5. Large column (blob/text) column DML
#
################################################################################

#-----------------------------------------------------------------------------
#
# 1. Create/Drop sequence.
#
create temporary table t1 (i int, j int, primary key pk(i)) engine = innodb;
create temporary table t2 (i int, j int, primary key pk(i)) engine = innodb;
create temporary table t3 (i int, j int, primary key pk(i)) engine = innodb;
create temporary table t4 (i int, j int, primary key pk(i)) engine = innodb;
drop table t1,t2,t3,t4;
#
create temporary table t1 (i int, j int, primary key pk(i)) engine = innodb;
drop table t1;

#-----------------------------------------------------------------------------
#
# 2. Basic DML operation on intrinsic table.
#
use test;
create temporary table t1
        (keyc int, c1 char(100), c2 char(100),
         primary key(keyc)) engine = innodb;
delimiter |;
CREATE PROCEDURE populate_t1()
BEGIN
	DECLARE i INT DEFAULT 1;
	while (i <= 2000) DO
		insert into t1 values (i, 'a', 'b');
		SET i = i + 1;
	END WHILE;
END|
delimiter ;|
#
begin; call populate_t1(); commit; select count(*) from t1;
--error ER_ILLEGAL_HA
truncate t1;
delete from t1 where keyc < 1000; select count(*) from t1;
update t1 set c1 = 'aefafdf' where keyc < 1500; select count(*) from t1;
update t1 set c2 = '141434' where keyc < 2000; select count(*) from t1;
delete from t1; select count(*) from t1;
#
alter table t1 add index sk(c1);
#
begin; call populate_t1(); commit; select count(*) from t1;
--error ER_ILLEGAL_HA
truncate t1;
delete from t1 where keyc < 1000; select count(*) from t1;
update t1 set c1 = 'aefafdf' where keyc < 1500; select count(*) from t1;
update t1 set c2 = '141434' where keyc < 2000; select count(*) from t1;
#
alter table t1 add column (t char(10));
select * from t1 limit 5;
delete from t1; select count(*) from t1;
#
#
drop procedure populate_t1;
drop table t1;

#-----------------------------------------------------------------------------
#
# 3. Run different types of queries on intrinsic table.
#
--disable_query_log
let $wl6737_temp = temporary;
--source suite/innodb/include/create_workload_itt.inc
--enable_query_log
--source suite/innodb/include/query_workload_itt.inc
--source suite/innodb/include/drop_workload_itt.inc

#-----------------------------------------------------------------------------
#
# 4. Multiple key DML
#
create temporary table t1 (
	a int, b int, c int,
	primary key pk(a), index sk(b), index sk2(c)) engine=innodb;
insert into t1 values (1, 1, 1), (2, 2, 2), (3, 3, 3);
update t1 set c = 4 where c = 2;
update t1 set b = 6 where b = 2;
update t1 set a = 100 where a = 1;
--error ER_DUP_ENTRY
update t1 set a = 3 where a = 2;
#
select * from t1;
begin;
insert into t1 values (4, 4, 4), (5, 5, 5);
rollback;
select * from t1;
--error ER_DUP_ENTRY
insert into t1 values (6, 6, 6), (7, 7, 7), (100, 100, 100);
select * from t1;
#
insert into t1 values (8, 8, 8), (100, 100, 100)
	on duplicate key update c = 100 + c;
select * from t1;
#
delete from t1;
insert into t1 values (8, 8, 8), (100, 100, 100)
	on duplicate key update c = 100 + c;
insert into t1 values (8, 8, 8), (100, 100, 100)
	on duplicate key update c = 100 + c;
select * from t1;
#
drop table t1;

#-----------------------------------------------------------------------------
#
# 5. Large column (blob/text) column DML
#
use test;
create temporary table t1
        (keyc int, c1 blob, c2 char(100),
         primary key(keyc), index sk(c1(10)), index sk2(c2)) engine = innodb;
delimiter |;
CREATE PROCEDURE populate_t1()
BEGIN
	DECLARE i INT DEFAULT 1;
	while (i <= 20) DO
		insert into t1 values (i, repeat('a', 10000), 'b');
		SET i = i + 1;
	END WHILE;
END|
delimiter ;|
#
begin; call populate_t1(); commit; select count(*) from t1;
--error ER_ILLEGAL_HA
truncate t1;
delete from t1 where keyc < 10; select count(*) from t1;
update t1 set c1 = 'aefafdf' where keyc < 15; select count(*) from t1;
update t1 set c2 = '141434' where keyc < 20; select count(*) from t1;
delete from t1; select count(*) from t1;
drop procedure populate_t1;
drop table t1;


