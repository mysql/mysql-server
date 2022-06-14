#1. Check Redo logging syntax and status
SHOW STATUS LIKE 'Innodb_redo_log_enabled';
Variable_name	Value
Innodb_redo_log_enabled	ON

SELECT * FROM performance_schema.global_status WHERE variable_name like 'Innodb_redo_log_enabled';
VARIABLE_NAME	VARIABLE_VALUE
Innodb_redo_log_enabled	ON

ALTER INSTANCE DISABLE INNODB REDO_LOG;

SHOW STATUS LIKE 'Innodb_redo_log_enabled';
Variable_name	Value
Innodb_redo_log_enabled	OFF

SELECT * FROM performance_schema.global_status WHERE variable_name like 'Innodb_redo_log_enabled';
VARIABLE_NAME	VARIABLE_VALUE
Innodb_redo_log_enabled	OFF

ALTER INSTANCE ENABLE INNODB REDO_LOG;

SHOW STATUS LIKE 'Innodb_redo_log_enabled';
Variable_name	Value
Innodb_redo_log_enabled	ON

SELECT * FROM performance_schema.global_status WHERE variable_name like 'Innodb_redo_log_enabled';
VARIABLE_NAME	VARIABLE_VALUE
Innodb_redo_log_enabled	ON

#2. Check privilege
CREATE USER 'user_redo'@'localhost' IDENTIFIED BY '123';

GRANT SELECT ON performance_schema.* to 'user_redo'@'localhost';

ALTER INSTANCE DISABLE INNODB REDO_LOG;
ERROR 42000: Access denied; you need (at least one of) the INNODB_REDO_LOG_ENABLE privilege(s) for this operation

SELECT * FROM performance_schema.global_status WHERE variable_name like 'Innodb_redo_log_enabled';
VARIABLE_NAME	VARIABLE_VALUE
Innodb_redo_log_enabled	ON

ALTER INSTANCE ENABLE INNODB REDO_LOG;
ERROR 42000: Access denied; you need (at least one of) the INNODB_REDO_LOG_ENABLE privilege(s) for this operation

GRANT INNODB_REDO_LOG_ENABLE ON *.* TO 'user_redo'@'localhost';

ALTER INSTANCE DISABLE INNODB REDO_LOG;

SELECT * FROM performance_schema.global_status WHERE variable_name like 'Innodb_redo_log_enabled';
VARIABLE_NAME	VARIABLE_VALUE
Innodb_redo_log_enabled	OFF

ALTER INSTANCE ENABLE INNODB REDO_LOG;

SELECT * FROM performance_schema.global_status WHERE variable_name like 'Innodb_redo_log_enabled';
VARIABLE_NAME	VARIABLE_VALUE
Innodb_redo_log_enabled	ON

DROP USER 'user_redo'@'localhost';

#3. Check basic DDL and DML when redo logging is disabled
ALTER INSTANCE DISABLE INNODB REDO_LOG;
SELECT * FROM performance_schema.global_status WHERE variable_name like 'Innodb_redo_log_enabled';
VARIABLE_NAME	VARIABLE_VALUE
Innodb_redo_log_enabled	OFF
Creating partitioned tables
---------------------------
CREATE TABLE table_part_1(
id int primary key, value int, name varchar(32), data BLOB,
key k1(value)) partition by hash (id) partitions 5;

CREATE TABLE table_part_2(
id int primary key, value int, name varchar(32), data BLOB,
key k1(value)) 
partition by hash (id) partitions 5;

CREATE TABLE table_part_sub_1(
id int, value int, name varchar(32), data BLOB, primary key pk(id, value),
key k1(value)) partition by range (value) subpartition by hash (id)
subpartitions 2 ( partition Part_1 values less than (101),
partition pArt_2 values less than (401),
partition part_3 values less than (701),
partition PART_4 values less than MAXVALUE);

CREATE TABLE table_part_sub_2(
id int, value int, name varchar(32), data BLOB, primary key pk(id, value),
key k1(value)) partition by range (value) subpartition by hash (id)
( partition Part_1 values less than (101)(
subpartition Sub_1,
subpartition sUb_2),
partition pArt_2 values less than (501)(
subpartition suB_3 ,
subpartition SUB_4),
partition PART_3 values less than MAXVALUE(
subpartition SUB_5,
subpartition sub_6));

CREATE TABLE `table_part_\_special_/_sub_?_3`(
id int, value int, name varchar(32), data BLOB, primary key pk(id, value),
key k1(value)) partition by range (value) subpartition by hash (id)
( partition `Part_?_1` values less than (201)(
subpartition `Sub_?_1`,
subpartition `sUb_/_2`),
partition `pArt_\_2` values less than (501)(
subpartition `suB_\_3`,
subpartition `SUB_?\_4`),
partition `PART_/_3` values less than MAXVALUE(
subpartition `SUB_?/_5`,
subpartition `sub_\/?_6`));

Inserting 10 rows to table_part_1
---------------------------------------
insert into table_part_1 VALUES(0, 55, "Row - 0", REPEAT('Large Column Data - 0' , 256));
insert into table_part_1 VALUES(1, 155, "Row - 1", REPEAT('Large Column Data - 1' , 256));
insert into table_part_1 VALUES(2, 255, "Row - 2", REPEAT('Large Column Data - 2' , 256));
insert into table_part_1 VALUES(3, 355, "Row - 3", REPEAT('Large Column Data - 3' , 256));
insert into table_part_1 VALUES(4, 455, "Row - 4", REPEAT('Large Column Data - 4' , 256));
insert into table_part_1 VALUES(5, 555, "Row - 5", REPEAT('Large Column Data - 5' , 256));
insert into table_part_1 VALUES(6, 655, "Row - 6", REPEAT('Large Column Data - 6' , 256));
insert into table_part_1 VALUES(7, 755, "Row - 7", REPEAT('Large Column Data - 7' , 256));
insert into table_part_1 VALUES(8, 855, "Row - 8", REPEAT('Large Column Data - 8' , 256));
insert into table_part_1 VALUES(9, 955, "Row - 9", REPEAT('Large Column Data - 9' , 256));

Inserting 10 rows to table_part_sub_1
---------------------------------------
insert into table_part_sub_1 VALUES(0, 55, "Row - 0", REPEAT('Large Column Data - 0' , 256));
insert into table_part_sub_1 VALUES(1, 155, "Row - 1", REPEAT('Large Column Data - 1' , 256));
insert into table_part_sub_1 VALUES(2, 255, "Row - 2", REPEAT('Large Column Data - 2' , 256));
insert into table_part_sub_1 VALUES(3, 355, "Row - 3", REPEAT('Large Column Data - 3' , 256));
insert into table_part_sub_1 VALUES(4, 455, "Row - 4", REPEAT('Large Column Data - 4' , 256));
insert into table_part_sub_1 VALUES(5, 555, "Row - 5", REPEAT('Large Column Data - 5' , 256));
insert into table_part_sub_1 VALUES(6, 655, "Row - 6", REPEAT('Large Column Data - 6' , 256));
insert into table_part_sub_1 VALUES(7, 755, "Row - 7", REPEAT('Large Column Data - 7' , 256));
insert into table_part_sub_1 VALUES(8, 855, "Row - 8", REPEAT('Large Column Data - 8' , 256));
insert into table_part_sub_1 VALUES(9, 955, "Row - 9", REPEAT('Large Column Data - 9' , 256));

Inserting 10 rows to table_part_sub_2
---------------------------------------
insert into table_part_sub_2 VALUES(0, 55, "Row - 0", REPEAT('Large Column Data - 0' , 256));
insert into table_part_sub_2 VALUES(1, 155, "Row - 1", REPEAT('Large Column Data - 1' , 256));
insert into table_part_sub_2 VALUES(2, 255, "Row - 2", REPEAT('Large Column Data - 2' , 256));
insert into table_part_sub_2 VALUES(3, 355, "Row - 3", REPEAT('Large Column Data - 3' , 256));
insert into table_part_sub_2 VALUES(4, 455, "Row - 4", REPEAT('Large Column Data - 4' , 256));
insert into table_part_sub_2 VALUES(5, 555, "Row - 5", REPEAT('Large Column Data - 5' , 256));
insert into table_part_sub_2 VALUES(6, 655, "Row - 6", REPEAT('Large Column Data - 6' , 256));
insert into table_part_sub_2 VALUES(7, 755, "Row - 7", REPEAT('Large Column Data - 7' , 256));
insert into table_part_sub_2 VALUES(8, 855, "Row - 8", REPEAT('Large Column Data - 8' , 256));
insert into table_part_sub_2 VALUES(9, 955, "Row - 9", REPEAT('Large Column Data - 9' , 256));

Inserting 10 rows to `table_part_\_special_/_sub_?_3`
---------------------------------------
insert into `table_part_\_special_/_sub_?_3` VALUES(0, 55, "Row - 0", REPEAT('Large Column Data - 0' , 256));
insert into `table_part_\_special_/_sub_?_3` VALUES(1, 155, "Row - 1", REPEAT('Large Column Data - 1' , 256));
insert into `table_part_\_special_/_sub_?_3` VALUES(2, 255, "Row - 2", REPEAT('Large Column Data - 2' , 256));
insert into `table_part_\_special_/_sub_?_3` VALUES(3, 355, "Row - 3", REPEAT('Large Column Data - 3' , 256));
insert into `table_part_\_special_/_sub_?_3` VALUES(4, 455, "Row - 4", REPEAT('Large Column Data - 4' , 256));
insert into `table_part_\_special_/_sub_?_3` VALUES(5, 555, "Row - 5", REPEAT('Large Column Data - 5' , 256));
insert into `table_part_\_special_/_sub_?_3` VALUES(6, 655, "Row - 6", REPEAT('Large Column Data - 6' , 256));
insert into `table_part_\_special_/_sub_?_3` VALUES(7, 755, "Row - 7", REPEAT('Large Column Data - 7' , 256));
insert into `table_part_\_special_/_sub_?_3` VALUES(8, 855, "Row - 8", REPEAT('Large Column Data - 8' , 256));
insert into `table_part_\_special_/_sub_?_3` VALUES(9, 955, "Row - 9", REPEAT('Large Column Data - 9' , 256));

Inserting 10 rows to table_part_2
---------------------------------------
insert into table_part_2 VALUES(0, 55, "Row - 0", REPEAT('Large Column Data - 0' , 256));
insert into table_part_2 VALUES(1, 155, "Row - 1", REPEAT('Large Column Data - 1' , 256));
insert into table_part_2 VALUES(2, 255, "Row - 2", REPEAT('Large Column Data - 2' , 256));
insert into table_part_2 VALUES(3, 355, "Row - 3", REPEAT('Large Column Data - 3' , 256));
insert into table_part_2 VALUES(4, 455, "Row - 4", REPEAT('Large Column Data - 4' , 256));
insert into table_part_2 VALUES(5, 555, "Row - 5", REPEAT('Large Column Data - 5' , 256));
insert into table_part_2 VALUES(6, 655, "Row - 6", REPEAT('Large Column Data - 6' , 256));
insert into table_part_2 VALUES(7, 755, "Row - 7", REPEAT('Large Column Data - 7' , 256));
insert into table_part_2 VALUES(8, 855, "Row - 8", REPEAT('Large Column Data - 8' , 256));
insert into table_part_2 VALUES(9, 955, "Row - 9", REPEAT('Large Column Data - 9' , 256));
select id, value, name from table_part_sub_2 partition (sub_1) order by id;
id	value	name
0	55	Row - 0
select id, value, name from table_part_sub_2 partition (sub_2) order by id;
id	value	name
select id, value, name from table_part_sub_2 partition (sub_3) order by id;
id	value	name
2	255	Row - 2
4	455	Row - 4
select id, value, name from table_part_sub_2 partition (sub_4) order by id;
id	value	name
1	155	Row - 1
3	355	Row - 3
select id, value, name from table_part_sub_2 partition (sub_5) order by id;
id	value	name
6	655	Row - 6
8	855	Row - 8
select id, value, name from table_part_sub_2 partition (sub_6) order by id;
id	value	name
5	555	Row - 5
7	755	Row - 7
9	955	Row - 9
ALTER INSTANCE ENABLE INNODB REDO_LOG;
SELECT * FROM performance_schema.global_status WHERE variable_name like 'Innodb_redo_log_enabled';
VARIABLE_NAME	VARIABLE_VALUE
Innodb_redo_log_enabled	ON
DML on partitioned tables
-------------------------

insert into table_part_1 values(15, 1555, "Row - 15", REPEAT('Large Column Data - 15' , 256));

update table_part_1 set name = 'Row - updated' where value > 500;

delete from table_part_1 where id = 7;

insert into table_part_sub_1 values(15, 1555, "Row - 15", REPEAT('Large Column Data - 15' , 256));

update table_part_sub_1 set name = 'Row - updated' where value > 500;

delete from table_part_sub_1 where id = 7;

insert into table_part_sub_2 values(15, 1555, "Row - 15", REPEAT('Large Column Data - 15' , 256));

update table_part_sub_2 set name = 'Row - updated' where value > 500;

delete from table_part_sub_2 where id = 7;

insert into `table_part_\_special_/_sub_?_3` values(15, 1555, "Row - 15", REPEAT('Large Column Data - 15' , 256));

update `table_part_\_special_/_sub_?_3` set name = 'Row - updated' where value > 500;

delete from `table_part_\_special_/_sub_?_3` where id = 7;

insert into table_part_2 values(15, 1555, "Row - 15", REPEAT('Large Column Data - 15' , 256));

update table_part_2 set name = 'Row - updated' where value > 500;

delete from table_part_2 where id = 7;
ALTER INSTANCE DISABLE INNODB REDO_LOG;
SELECT * FROM performance_schema.global_status WHERE variable_name like 'Innodb_redo_log_enabled';
VARIABLE_NAME	VARIABLE_VALUE
Innodb_redo_log_enabled	OFF
Dropping partitioned tables
---------------------------
DROP TABLE table_part_1;
DROP TABLE table_part_sub_1;
DROP TABLE table_part_sub_2;
DROP TABLE `table_part_\_special_/_sub_?_3`;
DROP TABLE table_part_2;
Creating partitioned tables
---------------------------
CREATE TABLE table_part_1(
id int primary key, value int, name varchar(32), data BLOB,
key k1(value)) partition by hash (id) partitions 5;

CREATE TABLE table_part_2(
id int primary key, value int, name varchar(32), data BLOB,
key k1(value)) 
partition by hash (id) partitions 5;

CREATE TABLE table_part_sub_1(
id int, value int, name varchar(32), data BLOB, primary key pk(id, value),
key k1(value)) partition by range (value) subpartition by hash (id)
subpartitions 2 ( partition Part_1 values less than (101),
partition pArt_2 values less than (401),
partition part_3 values less than (701),
partition PART_4 values less than MAXVALUE);

CREATE TABLE table_part_sub_2(
id int, value int, name varchar(32), data BLOB, primary key pk(id, value),
key k1(value)) partition by range (value) subpartition by hash (id)
( partition Part_1 values less than (101)(
subpartition Sub_1,
subpartition sUb_2),
partition pArt_2 values less than (501)(
subpartition suB_3 ,
subpartition SUB_4),
partition PART_3 values less than MAXVALUE(
subpartition SUB_5,
subpartition sub_6));

CREATE TABLE `table_part_\_special_/_sub_?_3`(
id int, value int, name varchar(32), data BLOB, primary key pk(id, value),
key k1(value)) partition by range (value) subpartition by hash (id)
( partition `Part_?_1` values less than (201)(
subpartition `Sub_?_1`,
subpartition `sUb_/_2`),
partition `pArt_\_2` values less than (501)(
subpartition `suB_\_3`,
subpartition `SUB_?\_4`),
partition `PART_/_3` values less than MAXVALUE(
subpartition `SUB_?/_5`,
subpartition `sub_\/?_6`));

Inserting 10 rows to table_part_1
---------------------------------------
insert into table_part_1 VALUES(0, 55, "Row - 0", REPEAT('Large Column Data - 0' , 256));
insert into table_part_1 VALUES(1, 155, "Row - 1", REPEAT('Large Column Data - 1' , 256));
insert into table_part_1 VALUES(2, 255, "Row - 2", REPEAT('Large Column Data - 2' , 256));
insert into table_part_1 VALUES(3, 355, "Row - 3", REPEAT('Large Column Data - 3' , 256));
insert into table_part_1 VALUES(4, 455, "Row - 4", REPEAT('Large Column Data - 4' , 256));
insert into table_part_1 VALUES(5, 555, "Row - 5", REPEAT('Large Column Data - 5' , 256));
insert into table_part_1 VALUES(6, 655, "Row - 6", REPEAT('Large Column Data - 6' , 256));
insert into table_part_1 VALUES(7, 755, "Row - 7", REPEAT('Large Column Data - 7' , 256));
insert into table_part_1 VALUES(8, 855, "Row - 8", REPEAT('Large Column Data - 8' , 256));
insert into table_part_1 VALUES(9, 955, "Row - 9", REPEAT('Large Column Data - 9' , 256));

Inserting 10 rows to table_part_sub_1
---------------------------------------
insert into table_part_sub_1 VALUES(0, 55, "Row - 0", REPEAT('Large Column Data - 0' , 256));
insert into table_part_sub_1 VALUES(1, 155, "Row - 1", REPEAT('Large Column Data - 1' , 256));
insert into table_part_sub_1 VALUES(2, 255, "Row - 2", REPEAT('Large Column Data - 2' , 256));
insert into table_part_sub_1 VALUES(3, 355, "Row - 3", REPEAT('Large Column Data - 3' , 256));
insert into table_part_sub_1 VALUES(4, 455, "Row - 4", REPEAT('Large Column Data - 4' , 256));
insert into table_part_sub_1 VALUES(5, 555, "Row - 5", REPEAT('Large Column Data - 5' , 256));
insert into table_part_sub_1 VALUES(6, 655, "Row - 6", REPEAT('Large Column Data - 6' , 256));
insert into table_part_sub_1 VALUES(7, 755, "Row - 7", REPEAT('Large Column Data - 7' , 256));
insert into table_part_sub_1 VALUES(8, 855, "Row - 8", REPEAT('Large Column Data - 8' , 256));
insert into table_part_sub_1 VALUES(9, 955, "Row - 9", REPEAT('Large Column Data - 9' , 256));

Inserting 10 rows to table_part_sub_2
---------------------------------------
insert into table_part_sub_2 VALUES(0, 55, "Row - 0", REPEAT('Large Column Data - 0' , 256));
insert into table_part_sub_2 VALUES(1, 155, "Row - 1", REPEAT('Large Column Data - 1' , 256));
insert into table_part_sub_2 VALUES(2, 255, "Row - 2", REPEAT('Large Column Data - 2' , 256));
insert into table_part_sub_2 VALUES(3, 355, "Row - 3", REPEAT('Large Column Data - 3' , 256));
insert into table_part_sub_2 VALUES(4, 455, "Row - 4", REPEAT('Large Column Data - 4' , 256));
insert into table_part_sub_2 VALUES(5, 555, "Row - 5", REPEAT('Large Column Data - 5' , 256));
insert into table_part_sub_2 VALUES(6, 655, "Row - 6", REPEAT('Large Column Data - 6' , 256));
insert into table_part_sub_2 VALUES(7, 755, "Row - 7", REPEAT('Large Column Data - 7' , 256));
insert into table_part_sub_2 VALUES(8, 855, "Row - 8", REPEAT('Large Column Data - 8' , 256));
insert into table_part_sub_2 VALUES(9, 955, "Row - 9", REPEAT('Large Column Data - 9' , 256));

Inserting 10 rows to `table_part_\_special_/_sub_?_3`
---------------------------------------
insert into `table_part_\_special_/_sub_?_3` VALUES(0, 55, "Row - 0", REPEAT('Large Column Data - 0' , 256));
insert into `table_part_\_special_/_sub_?_3` VALUES(1, 155, "Row - 1", REPEAT('Large Column Data - 1' , 256));
insert into `table_part_\_special_/_sub_?_3` VALUES(2, 255, "Row - 2", REPEAT('Large Column Data - 2' , 256));
insert into `table_part_\_special_/_sub_?_3` VALUES(3, 355, "Row - 3", REPEAT('Large Column Data - 3' , 256));
insert into `table_part_\_special_/_sub_?_3` VALUES(4, 455, "Row - 4", REPEAT('Large Column Data - 4' , 256));
insert into `table_part_\_special_/_sub_?_3` VALUES(5, 555, "Row - 5", REPEAT('Large Column Data - 5' , 256));
insert into `table_part_\_special_/_sub_?_3` VALUES(6, 655, "Row - 6", REPEAT('Large Column Data - 6' , 256));
insert into `table_part_\_special_/_sub_?_3` VALUES(7, 755, "Row - 7", REPEAT('Large Column Data - 7' , 256));
insert into `table_part_\_special_/_sub_?_3` VALUES(8, 855, "Row - 8", REPEAT('Large Column Data - 8' , 256));
insert into `table_part_\_special_/_sub_?_3` VALUES(9, 955, "Row - 9", REPEAT('Large Column Data - 9' , 256));

Inserting 10 rows to table_part_2
---------------------------------------
insert into table_part_2 VALUES(0, 55, "Row - 0", REPEAT('Large Column Data - 0' , 256));
insert into table_part_2 VALUES(1, 155, "Row - 1", REPEAT('Large Column Data - 1' , 256));
insert into table_part_2 VALUES(2, 255, "Row - 2", REPEAT('Large Column Data - 2' , 256));
insert into table_part_2 VALUES(3, 355, "Row - 3", REPEAT('Large Column Data - 3' , 256));
insert into table_part_2 VALUES(4, 455, "Row - 4", REPEAT('Large Column Data - 4' , 256));
insert into table_part_2 VALUES(5, 555, "Row - 5", REPEAT('Large Column Data - 5' , 256));
insert into table_part_2 VALUES(6, 655, "Row - 6", REPEAT('Large Column Data - 6' , 256));
insert into table_part_2 VALUES(7, 755, "Row - 7", REPEAT('Large Column Data - 7' , 256));
insert into table_part_2 VALUES(8, 855, "Row - 8", REPEAT('Large Column Data - 8' , 256));
insert into table_part_2 VALUES(9, 955, "Row - 9", REPEAT('Large Column Data - 9' , 256));
select id, value, name from table_part_sub_2 partition (sub_1) order by id;
id	value	name
0	55	Row - 0
select id, value, name from table_part_sub_2 partition (sub_2) order by id;
id	value	name
select id, value, name from table_part_sub_2 partition (sub_3) order by id;
id	value	name
2	255	Row - 2
4	455	Row - 4
select id, value, name from table_part_sub_2 partition (sub_4) order by id;
id	value	name
1	155	Row - 1
3	355	Row - 3
select id, value, name from table_part_sub_2 partition (sub_5) order by id;
id	value	name
6	655	Row - 6
8	855	Row - 8
select id, value, name from table_part_sub_2 partition (sub_6) order by id;
id	value	name
5	555	Row - 5
7	755	Row - 7
9	955	Row - 9
DML on partitioned tables
-------------------------

insert into table_part_1 values(15, 1555, "Row - 15", REPEAT('Large Column Data - 15' , 256));

update table_part_1 set name = 'Row - updated' where value > 500;

delete from table_part_1 where id = 7;

insert into table_part_sub_1 values(15, 1555, "Row - 15", REPEAT('Large Column Data - 15' , 256));

update table_part_sub_1 set name = 'Row - updated' where value > 500;

delete from table_part_sub_1 where id = 7;

insert into table_part_sub_2 values(15, 1555, "Row - 15", REPEAT('Large Column Data - 15' , 256));

update table_part_sub_2 set name = 'Row - updated' where value > 500;

delete from table_part_sub_2 where id = 7;

insert into `table_part_\_special_/_sub_?_3` values(15, 1555, "Row - 15", REPEAT('Large Column Data - 15' , 256));

update `table_part_\_special_/_sub_?_3` set name = 'Row - updated' where value > 500;

delete from `table_part_\_special_/_sub_?_3` where id = 7;

insert into table_part_2 values(15, 1555, "Row - 15", REPEAT('Large Column Data - 15' , 256));

update table_part_2 set name = 'Row - updated' where value > 500;

delete from table_part_2 where id = 7;
#3A. Check shutdown and restart
# Restart the server with --log-error
# restart: --log-error=SEARCH_FILE --no-console
SELECT * FROM performance_schema.global_status WHERE variable_name like 'Innodb_redo_log_enabled';
VARIABLE_NAME	VARIABLE_VALUE
Innodb_redo_log_enabled	OFF
Dropping partitioned tables
---------------------------
DROP TABLE table_part_1;
DROP TABLE table_part_sub_1;
DROP TABLE table_part_sub_2;
DROP TABLE `table_part_\_special_/_sub_?_3`;
DROP TABLE table_part_2;
Creating partitioned tables
---------------------------
CREATE TABLE table_part_1(
id int primary key, value int, name varchar(32), data BLOB,
key k1(value)) partition by hash (id) partitions 5;

CREATE TABLE table_part_2(
id int primary key, value int, name varchar(32), data BLOB,
key k1(value)) 
partition by hash (id) partitions 5;

CREATE TABLE table_part_sub_1(
id int, value int, name varchar(32), data BLOB, primary key pk(id, value),
key k1(value)) partition by range (value) subpartition by hash (id)
subpartitions 2 ( partition Part_1 values less than (101),
partition pArt_2 values less than (401),
partition part_3 values less than (701),
partition PART_4 values less than MAXVALUE);

CREATE TABLE table_part_sub_2(
id int, value int, name varchar(32), data BLOB, primary key pk(id, value),
key k1(value)) partition by range (value) subpartition by hash (id)
( partition Part_1 values less than (101)(
subpartition Sub_1,
subpartition sUb_2),
partition pArt_2 values less than (501)(
subpartition suB_3 ,
subpartition SUB_4),
partition PART_3 values less than MAXVALUE(
subpartition SUB_5,
subpartition sub_6));

CREATE TABLE `table_part_\_special_/_sub_?_3`(
id int, value int, name varchar(32), data BLOB, primary key pk(id, value),
key k1(value)) partition by range (value) subpartition by hash (id)
( partition `Part_?_1` values less than (201)(
subpartition `Sub_?_1`,
subpartition `sUb_/_2`),
partition `pArt_\_2` values less than (501)(
subpartition `suB_\_3`,
subpartition `SUB_?\_4`),
partition `PART_/_3` values less than MAXVALUE(
subpartition `SUB_?/_5`,
subpartition `sub_\/?_6`));

Inserting 10 rows to table_part_1
---------------------------------------
insert into table_part_1 VALUES(0, 55, "Row - 0", REPEAT('Large Column Data - 0' , 256));
insert into table_part_1 VALUES(1, 155, "Row - 1", REPEAT('Large Column Data - 1' , 256));
insert into table_part_1 VALUES(2, 255, "Row - 2", REPEAT('Large Column Data - 2' , 256));
insert into table_part_1 VALUES(3, 355, "Row - 3", REPEAT('Large Column Data - 3' , 256));
insert into table_part_1 VALUES(4, 455, "Row - 4", REPEAT('Large Column Data - 4' , 256));
insert into table_part_1 VALUES(5, 555, "Row - 5", REPEAT('Large Column Data - 5' , 256));
insert into table_part_1 VALUES(6, 655, "Row - 6", REPEAT('Large Column Data - 6' , 256));
insert into table_part_1 VALUES(7, 755, "Row - 7", REPEAT('Large Column Data - 7' , 256));
insert into table_part_1 VALUES(8, 855, "Row - 8", REPEAT('Large Column Data - 8' , 256));
insert into table_part_1 VALUES(9, 955, "Row - 9", REPEAT('Large Column Data - 9' , 256));

Inserting 10 rows to table_part_sub_1
---------------------------------------
insert into table_part_sub_1 VALUES(0, 55, "Row - 0", REPEAT('Large Column Data - 0' , 256));
insert into table_part_sub_1 VALUES(1, 155, "Row - 1", REPEAT('Large Column Data - 1' , 256));
insert into table_part_sub_1 VALUES(2, 255, "Row - 2", REPEAT('Large Column Data - 2' , 256));
insert into table_part_sub_1 VALUES(3, 355, "Row - 3", REPEAT('Large Column Data - 3' , 256));
insert into table_part_sub_1 VALUES(4, 455, "Row - 4", REPEAT('Large Column Data - 4' , 256));
insert into table_part_sub_1 VALUES(5, 555, "Row - 5", REPEAT('Large Column Data - 5' , 256));
insert into table_part_sub_1 VALUES(6, 655, "Row - 6", REPEAT('Large Column Data - 6' , 256));
insert into table_part_sub_1 VALUES(7, 755, "Row - 7", REPEAT('Large Column Data - 7' , 256));
insert into table_part_sub_1 VALUES(8, 855, "Row - 8", REPEAT('Large Column Data - 8' , 256));
insert into table_part_sub_1 VALUES(9, 955, "Row - 9", REPEAT('Large Column Data - 9' , 256));

Inserting 10 rows to table_part_sub_2
---------------------------------------
insert into table_part_sub_2 VALUES(0, 55, "Row - 0", REPEAT('Large Column Data - 0' , 256));
insert into table_part_sub_2 VALUES(1, 155, "Row - 1", REPEAT('Large Column Data - 1' , 256));
insert into table_part_sub_2 VALUES(2, 255, "Row - 2", REPEAT('Large Column Data - 2' , 256));
insert into table_part_sub_2 VALUES(3, 355, "Row - 3", REPEAT('Large Column Data - 3' , 256));
insert into table_part_sub_2 VALUES(4, 455, "Row - 4", REPEAT('Large Column Data - 4' , 256));
insert into table_part_sub_2 VALUES(5, 555, "Row - 5", REPEAT('Large Column Data - 5' , 256));
insert into table_part_sub_2 VALUES(6, 655, "Row - 6", REPEAT('Large Column Data - 6' , 256));
insert into table_part_sub_2 VALUES(7, 755, "Row - 7", REPEAT('Large Column Data - 7' , 256));
insert into table_part_sub_2 VALUES(8, 855, "Row - 8", REPEAT('Large Column Data - 8' , 256));
insert into table_part_sub_2 VALUES(9, 955, "Row - 9", REPEAT('Large Column Data - 9' , 256));

Inserting 10 rows to `table_part_\_special_/_sub_?_3`
---------------------------------------
insert into `table_part_\_special_/_sub_?_3` VALUES(0, 55, "Row - 0", REPEAT('Large Column Data - 0' , 256));
insert into `table_part_\_special_/_sub_?_3` VALUES(1, 155, "Row - 1", REPEAT('Large Column Data - 1' , 256));
insert into `table_part_\_special_/_sub_?_3` VALUES(2, 255, "Row - 2", REPEAT('Large Column Data - 2' , 256));
insert into `table_part_\_special_/_sub_?_3` VALUES(3, 355, "Row - 3", REPEAT('Large Column Data - 3' , 256));
insert into `table_part_\_special_/_sub_?_3` VALUES(4, 455, "Row - 4", REPEAT('Large Column Data - 4' , 256));
insert into `table_part_\_special_/_sub_?_3` VALUES(5, 555, "Row - 5", REPEAT('Large Column Data - 5' , 256));
insert into `table_part_\_special_/_sub_?_3` VALUES(6, 655, "Row - 6", REPEAT('Large Column Data - 6' , 256));
insert into `table_part_\_special_/_sub_?_3` VALUES(7, 755, "Row - 7", REPEAT('Large Column Data - 7' , 256));
insert into `table_part_\_special_/_sub_?_3` VALUES(8, 855, "Row - 8", REPEAT('Large Column Data - 8' , 256));
insert into `table_part_\_special_/_sub_?_3` VALUES(9, 955, "Row - 9", REPEAT('Large Column Data - 9' , 256));

Inserting 10 rows to table_part_2
---------------------------------------
insert into table_part_2 VALUES(0, 55, "Row - 0", REPEAT('Large Column Data - 0' , 256));
insert into table_part_2 VALUES(1, 155, "Row - 1", REPEAT('Large Column Data - 1' , 256));
insert into table_part_2 VALUES(2, 255, "Row - 2", REPEAT('Large Column Data - 2' , 256));
insert into table_part_2 VALUES(3, 355, "Row - 3", REPEAT('Large Column Data - 3' , 256));
insert into table_part_2 VALUES(4, 455, "Row - 4", REPEAT('Large Column Data - 4' , 256));
insert into table_part_2 VALUES(5, 555, "Row - 5", REPEAT('Large Column Data - 5' , 256));
insert into table_part_2 VALUES(6, 655, "Row - 6", REPEAT('Large Column Data - 6' , 256));
insert into table_part_2 VALUES(7, 755, "Row - 7", REPEAT('Large Column Data - 7' , 256));
insert into table_part_2 VALUES(8, 855, "Row - 8", REPEAT('Large Column Data - 8' , 256));
insert into table_part_2 VALUES(9, 955, "Row - 9", REPEAT('Large Column Data - 9' , 256));
select id, value, name from table_part_sub_2 partition (sub_1) order by id;
id	value	name
0	55	Row - 0
select id, value, name from table_part_sub_2 partition (sub_2) order by id;
id	value	name
select id, value, name from table_part_sub_2 partition (sub_3) order by id;
id	value	name
2	255	Row - 2
4	455	Row - 4
select id, value, name from table_part_sub_2 partition (sub_4) order by id;
id	value	name
1	155	Row - 1
3	355	Row - 3
select id, value, name from table_part_sub_2 partition (sub_5) order by id;
id	value	name
6	655	Row - 6
8	855	Row - 8
select id, value, name from table_part_sub_2 partition (sub_6) order by id;
id	value	name
5	555	Row - 5
7	755	Row - 7
9	955	Row - 9
DML on partitioned tables
-------------------------

insert into table_part_1 values(15, 1555, "Row - 15", REPEAT('Large Column Data - 15' , 256));

update table_part_1 set name = 'Row - updated' where value > 500;

delete from table_part_1 where id = 7;

insert into table_part_sub_1 values(15, 1555, "Row - 15", REPEAT('Large Column Data - 15' , 256));

update table_part_sub_1 set name = 'Row - updated' where value > 500;

delete from table_part_sub_1 where id = 7;

insert into table_part_sub_2 values(15, 1555, "Row - 15", REPEAT('Large Column Data - 15' , 256));

update table_part_sub_2 set name = 'Row - updated' where value > 500;

delete from table_part_sub_2 where id = 7;

insert into `table_part_\_special_/_sub_?_3` values(15, 1555, "Row - 15", REPEAT('Large Column Data - 15' , 256));

update `table_part_\_special_/_sub_?_3` set name = 'Row - updated' where value > 500;

delete from `table_part_\_special_/_sub_?_3` where id = 7;

insert into table_part_2 values(15, 1555, "Row - 15", REPEAT('Large Column Data - 15' , 256));

update table_part_2 set name = 'Row - updated' where value > 500;

delete from table_part_2 where id = 7;
DDL on partitioned tables
-------------------------
ALTER TABLE table_part_1 TRUNCATE PARTITION p0;

ALTER TABLE table_part_1 COALESCE PARTITION 2;

SHOW CREATE TABLE table_part_1;
Table	Create Table
table_part_1	CREATE TABLE `table_part_1` (
  `id` int NOT NULL,
  `value` int DEFAULT NULL,
  `name` varchar(32) DEFAULT NULL,
  `data` blob,
  PRIMARY KEY (`id`),
  KEY `k1` (`value`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (`id`)
PARTITIONS 3 */

ALTER TABLE table_part_sub_1 TRUNCATE PARTITION part_1;

ALTER TABLE table_part_sub_2 TRUNCATE PARTITION part_3;

CREATE TABLE test_table LIKE table_part_sub_2;

ALTER TABLE test_table REMOVE PARTITIONING;
insert into test_table values(2, 265, "Row - 2", REPEAT('Large Column Data - 2' , 256));

SELECT id, value, name FROM table_part_sub_2 PARTITION (sub_3) order by id;
id	value	name
2	255	Row - 2
4	455	Row - 4

SELECT id, value, name FROM test_table order by id;
id	value	name
2	265	Row - 2

ALTER TABLE table_part_sub_2 EXCHANGE PARTITION sub_3 WITH TABLE test_table;

SELECT id, value, name FROM table_part_sub_2 PARTITION (sub_3) order by id;
id	value	name
2	265	Row - 2

SELECT id, value, name FROM test_table order by id;
id	value	name
2	255	Row - 2
4	455	Row - 4

DROP TABLE test_table;

ALTER TABLE table_part_sub_2 RENAME renamed_table;;
SHOW CREATE TABLE renamed_table;
Table	Create Table
renamed_table	CREATE TABLE `renamed_table` (
  `id` int NOT NULL,
  `value` int NOT NULL,
  `name` varchar(32) DEFAULT NULL,
  `data` blob,
  PRIMARY KEY (`id`,`value`),
  KEY `k1` (`value`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (`value`)
SUBPARTITION BY HASH (`id`)
(PARTITION Part_1 VALUES LESS THAN (101)
 (SUBPARTITION Sub_1 ENGINE = InnoDB,
  SUBPARTITION sUb_2 ENGINE = InnoDB),
 PARTITION pArt_2 VALUES LESS THAN (501)
 (SUBPARTITION suB_3 ENGINE = InnoDB,
  SUBPARTITION SUB_4 ENGINE = InnoDB),
 PARTITION PART_3 VALUES LESS THAN MAXVALUE
 (SUBPARTITION SUB_5 ENGINE = InnoDB,
  SUBPARTITION sub_6 ENGINE = InnoDB)) */

ALTER TABLE renamed_table RENAME table_part_sub_2;;

ALTER TABLE table_part_1 ENGINE = InnoDB;;

ALTER TABLE table_part_1 ADD COLUMN new_id int AFTER id;

ALTER TABLE table_part_1 REMOVE PARTITIONING;

ALTER TABLE table_part_sub_1 ENGINE = InnoDB;;

ALTER TABLE table_part_sub_1 ADD COLUMN new_id int AFTER id;

ALTER TABLE table_part_sub_1 REMOVE PARTITIONING;

ALTER TABLE table_part_sub_2 ENGINE = InnoDB;;

ALTER TABLE table_part_sub_2 ADD COLUMN new_id int AFTER id;

ALTER TABLE table_part_sub_2 REMOVE PARTITIONING;

ALTER TABLE `table_part_\_special_/_sub_?_3` ENGINE = InnoDB;;

ALTER TABLE `table_part_\_special_/_sub_?_3` ADD COLUMN new_id int AFTER id;

ALTER TABLE `table_part_\_special_/_sub_?_3` REMOVE PARTITIONING;

ALTER TABLE table_part_2 ENGINE = InnoDB;;

ALTER TABLE table_part_2 ADD COLUMN new_id int AFTER id;

ALTER TABLE table_part_2 REMOVE PARTITIONING;
#3B. Check shutdown and restart with innodb_fast_shutdown = 2
SET GLOBAL innodb_fast_shutdown = 2;
# restart:
Pattern "\[Warning\] .*MY-\d+.* InnoDB cannot do cold shutdown 'innodb_fast_shutdown = 2' and is forcing 'innodb_fast_shutdown = 1' as redo logging is disabled. InnoDB would flush all dirty pages to ensure physical data consistency." found
SELECT * FROM performance_schema.global_status WHERE variable_name like 'Innodb_redo_log_enabled';
VARIABLE_NAME	VARIABLE_VALUE
Innodb_redo_log_enabled	OFF
#3C. Check shutdown and restart with innodb-read-only
# restart: --innodb-read-only
SELECT * FROM performance_schema.global_status WHERE variable_name like 'Innodb_redo_log_enabled';
VARIABLE_NAME	VARIABLE_VALUE
Innodb_redo_log_enabled	OFF
# restart:
Checking partitioned tables
---------------------------
SET NAMES utf8mb4 COLLATE utf8mb4_0900_as_cs;

CHECK TABLE table_part_1;
Table	Op	Msg_type	Msg_text
test.table_part_1	check	status	OK

SHOW CREATE TABLE table_part_1;
Table	Create Table
table_part_1	CREATE TABLE `table_part_1` (
  `id` int NOT NULL,
  `new_id` int DEFAULT NULL,
  `value` int DEFAULT NULL,
  `name` varchar(32) DEFAULT NULL,
  `data` blob,
  PRIMARY KEY (`id`),
  KEY `k1` (`value`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci

SELECT id, value, name, SUBSTRING(data, 1024, 32) FROM table_part_1 order by id;
id	value	name	SUBSTRING(data, 1024, 32)
1	155	Row - 1	ta - 1Large Column Data - 1Large
2	255	Row - 2	ta - 2Large Column Data - 2Large
3	355	Row - 3	ta - 3Large Column Data - 3Large
4	455	Row - 4	ta - 4Large Column Data - 4Large
6	655	Row - updated	ta - 6Large Column Data - 6Large
8	855	Row - updated	ta - 8Large Column Data - 8Large
9	955	Row - updated	ta - 9Large Column Data - 9Large

CHECK TABLE table_part_sub_1;
Table	Op	Msg_type	Msg_text
test.table_part_sub_1	check	status	OK

SHOW CREATE TABLE table_part_sub_1;
Table	Create Table
table_part_sub_1	CREATE TABLE `table_part_sub_1` (
  `id` int NOT NULL,
  `new_id` int DEFAULT NULL,
  `value` int NOT NULL,
  `name` varchar(32) DEFAULT NULL,
  `data` blob,
  PRIMARY KEY (`id`,`value`),
  KEY `k1` (`value`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci

SELECT id, value, name, SUBSTRING(data, 1024, 32) FROM table_part_sub_1 order by id;
id	value	name	SUBSTRING(data, 1024, 32)
1	155	Row - 1	ta - 1Large Column Data - 1Large
2	255	Row - 2	ta - 2Large Column Data - 2Large
3	355	Row - 3	ta - 3Large Column Data - 3Large
4	455	Row - 4	ta - 4Large Column Data - 4Large
5	555	Row - updated	ta - 5Large Column Data - 5Large
6	655	Row - updated	ta - 6Large Column Data - 6Large
8	855	Row - updated	ta - 8Large Column Data - 8Large
9	955	Row - updated	ta - 9Large Column Data - 9Large
15	1555	Row - updated	n Data - 15Large Column Data - 1

CHECK TABLE table_part_sub_2;
Table	Op	Msg_type	Msg_text
test.table_part_sub_2	check	status	OK

SHOW CREATE TABLE table_part_sub_2;
Table	Create Table
table_part_sub_2	CREATE TABLE `table_part_sub_2` (
  `id` int NOT NULL,
  `new_id` int DEFAULT NULL,
  `value` int NOT NULL,
  `name` varchar(32) DEFAULT NULL,
  `data` blob,
  PRIMARY KEY (`id`,`value`),
  KEY `k1` (`value`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci

SELECT id, value, name, SUBSTRING(data, 1024, 32) FROM table_part_sub_2 order by id;
id	value	name	SUBSTRING(data, 1024, 32)
0	55	Row - 0	ta - 0Large Column Data - 0Large
1	155	Row - 1	ta - 1Large Column Data - 1Large
2	265	Row - 2	ta - 2Large Column Data - 2Large
3	355	Row - 3	ta - 3Large Column Data - 3Large

CHECK TABLE `table_part_\_special_/_sub_?_3`;
Table	Op	Msg_type	Msg_text
test.table_part_\_special_/_sub_?_3	check	status	OK

SHOW CREATE TABLE `table_part_\_special_/_sub_?_3`;
Table	Create Table
table_part_\_special_/_sub_?_3	CREATE TABLE `table_part_\_special_/_sub_?_3` (
  `id` int NOT NULL,
  `new_id` int DEFAULT NULL,
  `value` int NOT NULL,
  `name` varchar(32) DEFAULT NULL,
  `data` blob,
  PRIMARY KEY (`id`,`value`),
  KEY `k1` (`value`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci

SELECT id, value, name, SUBSTRING(data, 1024, 32) FROM `table_part_\_special_/_sub_?_3` order by id;
id	value	name	SUBSTRING(data, 1024, 32)
0	55	Row - 0	ta - 0Large Column Data - 0Large
1	155	Row - 1	ta - 1Large Column Data - 1Large
2	255	Row - 2	ta - 2Large Column Data - 2Large
3	355	Row - 3	ta - 3Large Column Data - 3Large
4	455	Row - 4	ta - 4Large Column Data - 4Large
5	555	Row - updated	ta - 5Large Column Data - 5Large
6	655	Row - updated	ta - 6Large Column Data - 6Large
8	855	Row - updated	ta - 8Large Column Data - 8Large
9	955	Row - updated	ta - 9Large Column Data - 9Large
15	1555	Row - updated	n Data - 15Large Column Data - 1

CHECK TABLE table_part_2;
Table	Op	Msg_type	Msg_text
test.table_part_2	check	status	OK

SHOW CREATE TABLE table_part_2;
Table	Create Table
table_part_2	CREATE TABLE `table_part_2` (
  `id` int NOT NULL,
  `new_id` int DEFAULT NULL,
  `value` int DEFAULT NULL,
  `name` varchar(32) DEFAULT NULL,
  `data` blob,
  PRIMARY KEY (`id`),
  KEY `k1` (`value`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci

SELECT id, value, name, SUBSTRING(data, 1024, 32) FROM table_part_2 order by id;
id	value	name	SUBSTRING(data, 1024, 32)
0	55	Row - 0	ta - 0Large Column Data - 0Large
1	155	Row - 1	ta - 1Large Column Data - 1Large
2	255	Row - 2	ta - 2Large Column Data - 2Large
3	355	Row - 3	ta - 3Large Column Data - 3Large
4	455	Row - 4	ta - 4Large Column Data - 4Large
5	555	Row - updated	ta - 5Large Column Data - 5Large
6	655	Row - updated	ta - 6Large Column Data - 6Large
8	855	Row - updated	ta - 8Large Column Data - 8Large
9	955	Row - updated	ta - 9Large Column Data - 9Large
15	1555	Row - updated	n Data - 15Large Column Data - 1

Checking IFS views
------------------
select table_schema, table_name from information_schema.tables
where table_name LIKE '%table_part%' order by table_schema, table_name;
TABLE_SCHEMA	TABLE_NAME
test	table_part_1
test	table_part_2
test	table_part_\_special_/_sub_?_3
test	table_part_sub_1
test	table_part_sub_2

select name, space_type from information_schema.innodb_tables
where name LIKE '%table_part%' order by name, space_type;
name	space_type
test/table_part_1	Single
test/table_part_2	Single
test/table_part_@005c_special_@002f_sub_@003f_3	Single
test/table_part_sub_1	Single
test/table_part_sub_2	Single

select table_schema, table_name, partition_name, subpartition_name
from information_schema.partitions
where table_name LIKE '%table_part%'
  order by table_schema, table_name, partition_name, subpartition_name;
TABLE_SCHEMA	TABLE_NAME	PARTITION_NAME	SUBPARTITION_NAME
test	table_part_1	NULL	NULL
test	table_part_2	NULL	NULL
test	table_part_\_special_/_sub_?_3	NULL	NULL
test	table_part_sub_1	NULL	NULL
test	table_part_sub_2	NULL	NULL

select name from information_schema.innodb_tablespaces
where name LIKE '%table_part%' order by name;
name
test/table_part_1
test/table_part_2
test/table_part_sub_1
test/table_part_sub_2
test/table_part_\_special_/_sub_?_3

select file_type, status, file_name, tablespace_name from information_schema.files
where file_name LIKE '%table_part%'
  order by file_type, status, file_name, tablespace_name;
FILE_TYPE	STATUS	FILE_NAME	TABLESPACE_NAME
TABLESPACE	NORMAL	./test/table_part_1.ibd	test/table_part_1
TABLESPACE	NORMAL	./test/table_part_2.ibd	test/table_part_2
TABLESPACE	NORMAL	./test/table_part_@005c_special_@002f_sub_@003f_3.ibd	test/table_part_\_special_/_sub_?_3
TABLESPACE	NORMAL	./test/table_part_sub_1.ibd	test/table_part_sub_1
TABLESPACE	NORMAL	./test/table_part_sub_2.ibd	test/table_part_sub_2

Checking Innodb stat tables
---------------------------
select database_name, table_name from mysql.innodb_table_stats
where table_name like '%table_part%' order by database_name, table_name;
database_name	table_name
test	table_part_1
test	table_part_2
test	table_part_\_special_/_sub_?_3
test	table_part_sub_1
test	table_part_sub_2

select database_name, table_name, index_name, stat_name from mysql.innodb_index_stats
where table_name like '%table_part%'
  order by database_name, table_name, index_name, stat_name;
database_name	table_name	index_name	stat_name
test	table_part_1	PRIMARY	n_diff_pfx01
test	table_part_1	PRIMARY	n_leaf_pages
test	table_part_1	PRIMARY	size
test	table_part_1	k1	n_diff_pfx01
test	table_part_1	k1	n_diff_pfx02
test	table_part_1	k1	n_leaf_pages
test	table_part_1	k1	size
test	table_part_2	PRIMARY	n_diff_pfx01
test	table_part_2	PRIMARY	n_leaf_pages
test	table_part_2	PRIMARY	size
test	table_part_2	k1	n_diff_pfx01
test	table_part_2	k1	n_diff_pfx02
test	table_part_2	k1	n_leaf_pages
test	table_part_2	k1	size
test	table_part_\_special_/_sub_?_3	PRIMARY	n_diff_pfx01
test	table_part_\_special_/_sub_?_3	PRIMARY	n_diff_pfx02
test	table_part_\_special_/_sub_?_3	PRIMARY	n_leaf_pages
test	table_part_\_special_/_sub_?_3	PRIMARY	size
test	table_part_\_special_/_sub_?_3	k1	n_diff_pfx01
test	table_part_\_special_/_sub_?_3	k1	n_diff_pfx02
test	table_part_\_special_/_sub_?_3	k1	n_leaf_pages
test	table_part_\_special_/_sub_?_3	k1	size
test	table_part_sub_1	PRIMARY	n_diff_pfx01
test	table_part_sub_1	PRIMARY	n_diff_pfx02
test	table_part_sub_1	PRIMARY	n_leaf_pages
test	table_part_sub_1	PRIMARY	size
test	table_part_sub_1	k1	n_diff_pfx01
test	table_part_sub_1	k1	n_diff_pfx02
test	table_part_sub_1	k1	n_leaf_pages
test	table_part_sub_1	k1	size
test	table_part_sub_2	PRIMARY	n_diff_pfx01
test	table_part_sub_2	PRIMARY	n_diff_pfx02
test	table_part_sub_2	PRIMARY	n_leaf_pages
test	table_part_sub_2	PRIMARY	size
test	table_part_sub_2	k1	n_diff_pfx01
test	table_part_sub_2	k1	n_diff_pfx02
test	table_part_sub_2	k1	n_leaf_pages
test	table_part_sub_2	k1	size
SET NAMES DEFAULT;
Dropping partitioned tables
---------------------------
DROP TABLE table_part_1;
DROP TABLE table_part_sub_1;
DROP TABLE table_part_sub_2;
DROP TABLE `table_part_\_special_/_sub_?_3`;
DROP TABLE table_part_2;
ALTER INSTANCE ENABLE INNODB REDO_LOG;
SELECT * FROM performance_schema.global_status WHERE variable_name like 'Innodb_redo_log_enabled';
VARIABLE_NAME	VARIABLE_VALUE
Innodb_redo_log_enabled	ON
