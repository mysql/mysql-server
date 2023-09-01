#
# Test of --lower-case-table-names=2
# (User has case insensitive file system and wants to preserve case of
# table names)
#
--source include/have_lowercase2.inc

--disable_warnings
DROP TABLE IF EXISTS t1,t2,t3,t2aA,t1Aa;
DROP DATABASE IF EXISTS `TEST_$1`;
DROP DATABASE IF EXISTS `test_$1`;
DROP DATABASE IF EXISTS mysqltest_LC2;
--enable_warnings

CREATE TABLE T1 (a int) ENGINE=MyISAM;
INSERT INTO T1 VALUES (1);
SHOW TABLES LIKE "T1";
SHOW TABLES LIKE "t1";
SHOW CREATE TABLE T1;
RENAME TABLE T1 TO T2;
SHOW TABLES LIKE "T2";
SELECT * FROM t2;
RENAME TABLE T2 TO t3;
SHOW TABLES LIKE "T3";
RENAME TABLE T3 TO T1;
SHOW TABLES LIKE "T1";
ALTER TABLE T1 add b int;
SHOW TABLES LIKE "T1";
ALTER TABLE T1 RENAME T2;
SHOW TABLES LIKE "T2";

LOCK TABLE T2 WRITE;
ALTER TABLE T2 drop b;
SHOW TABLES LIKE "T2";
UNLOCK TABLES;
RENAME TABLE T2 TO T1;
SHOW TABLES LIKE "T1";
SELECT * from T1;
DROP TABLE T1;

#
# Test database level
#

CREATE DATABASE `TEST_$1`;
SHOW DATABASES LIKE "TEST%";
DROP DATABASE `test_$1`;

#
# Test of innodb tables with lower_case_table_names=2
#

CREATE TABLE T1 (a int) engine=innodb;
INSERT INTO T1 VALUES (1);
SHOW TABLES LIKE "T1";
SHOW TABLES LIKE "t1";
SHOW CREATE TABLE T1;
RENAME TABLE T1 TO T2;
SHOW TABLES LIKE "T2";
SELECT * FROM t2;
RENAME TABLE T2 TO t3;
SHOW TABLES LIKE "T3";
RENAME TABLE T3 TO T1;
SHOW TABLES LIKE "T1";
ALTER TABLE T1 add b int;
SHOW TABLES LIKE "T1";
ALTER TABLE T1 RENAME T2;
SHOW TABLES LIKE "T2";

LOCK TABLE T2 WRITE;
ALTER TABLE T2 drop b;
SHOW TABLES LIKE "T2";
UNLOCK TABLES;
RENAME TABLE T2 TO T1;
SHOW TABLES LIKE "T1";
SELECT * from T1;
DROP TABLE T1;

#
# Test problem with temporary tables (Bug #2858)
#

create table T1 (EVENT_ID int auto_increment primary key,  LOCATION char(20));
insert into T1 values (NULL,"Mic-4"),(NULL,"Mic-5"),(NULL,"Mic-6");
SELECT LOCATION FROM T1 WHERE EVENT_ID=2 UNION ALL  SELECT LOCATION FROM T1 WHERE EVENT_ID=3;
SELECT LOCATION FROM T1 WHERE EVENT_ID=2 UNION ALL  SELECT LOCATION FROM T1 WHERE EVENT_ID=3;
SELECT LOCATION FROM T1 WHERE EVENT_ID=2 UNION ALL  SELECT LOCATION FROM T1 WHERE EVENT_ID=3;
drop table T1;

#
# Test name conversion with ALTER TABLE / CREATE INDEX (Bug #3109)
#

create table T1 (A int);
alter table T1 add index (A);
show tables like 'T1%';
alter table t1 add index (A);
show tables like 't1%';
drop table t1;

#
# Bug #7261: Alter table loses temp table
#

create temporary table T1(a int(11), b varchar(8));
insert into T1 values (1, 'abc');
select * from T1;
alter table T1 add index (a);
select * from T1;
drop table T1;

#
# Bug #8355: Tables not dropped from table cache on drop db
#
create database mysqltest_LC2;
use mysqltest_LC2;
create table myUC (i int);
insert into myUC values (1),(2),(3);
select * from myUC;
use test;
drop database mysqltest_LC2;
create database mysqltest_LC2;
use mysqltest_LC2;
create table myUC (i int);
select * from myUC;
use test;
drop database mysqltest_LC2;

#
# Bug #9500: Problem with WHERE clause
#
create table t2aA (col1 int);
create table t1Aa (col1 int);
select t1Aa.col1 from t1aA,t2Aa where t1Aa.col1 = t2aA.col1;
drop table t2aA, t1Aa;

# End of 4.1 tests

#
# Bug#17661 information_schema.SCHEMATA returns uppercase with lower_case_table_names = 1
#
create database mysqltest_LC2;
use mysqltest_LC2;
create table myUC (i int);
select TABLE_SCHEMA,TABLE_NAME FROM information_schema.TABLES
where TABLE_SCHEMA ='mysqltest_LC2';
use test;
drop database mysqltest_LC2;


--echo #
--echo # Bug #11758687: 50924: object names not resolved correctly 
--echo #   on lctn2 systems
--echo #

CREATE DATABASE BUP_XPFM_COMPAT_DB2;

CREATE TABLE BUP_XPFM_COMPAT_DB2.TABLE2 (c13 INT) DEFAULT CHARSET latin1;
CREATE TABLE BUP_XPFM_COMPAT_DB2.table1 (c13 INT) DEFAULT CHARSET latin1;
CREATE TABLE bup_xpfm_compat_db2.table3 (c13 INT) DEFAULT CHARSET latin1;

delimiter |;
#
CREATE TRIGGER BUP_XPFM_COMPAT_DB2.trigger1 AFTER INSERT
  ON BUP_XPFM_COMPAT_DB2.table1 FOR EACH ROW
  update BUP_XPFM_COMPAT_DB2.table1 set c13=12;
|
CREATE TRIGGER BUP_XPFM_COMPAT_DB2.TRIGGER2 AFTER INSERT
  ON BUP_XPFM_COMPAT_DB2.TABLE2 FOR EACH ROW
  update BUP_XPFM_COMPAT_DB2.table1 set c13=12;
|
CREATE TRIGGER BUP_XPFM_COMPAT_DB2.TrigGer3 AFTER INSERT
  ON BUP_XPFM_COMPAT_DB2.TaBle3 FOR EACH ROW
  update BUP_XPFM_COMPAT_DB2.table1 set c13=12;
|
delimiter ;|

SELECT trigger_schema, trigger_name, event_object_table FROM
INFORMATION_SCHEMA.TRIGGERS
  WHERE trigger_schema COLLATE utf8mb3_bin = 'BUP_XPFM_COMPAT_DB2'
  ORDER BY trigger_schema, trigger_name;

DROP DATABASE BUP_XPFM_COMPAT_DB2;

--echo # End of 5.1 tests


--echo #
--echo # Test for bug #44738 "fill_schema_table_from_frm() opens tables without
--echo # lowercasing table name". Due to not properly normalizing table names
--echo # in lower_case_table_names modes in this function queries to I_S which
--echo # were executed through it left entries with incorrect key in table
--echo # definition cache. As result further queries to I_S that used this
--echo # function produced stale results in cases when table definition was
--echo # changed by a DDL statement. Also combination of this issue and a
--echo # similar problem in CREATE TABLE (it also has peeked into table
--echo # definition cache using non-normalized key) led to spurious
--echo # ER_TABLE_EXISTS_ERROR errors when one tried to create table with the
--echo # same name as a previously existing but dropped table.
--echo #
--disable_warnings
drop database if exists mysqltest_UPPERCASE;
drop table if exists t_bug44738_UPPERCASE;
--enable_warnings
create database mysqltest_UPPERCASE;
use mysqltest_UPPERCASE;
create table t_bug44738_UPPERCASE (i int) comment='Old comment';
create table t_bug44738_lowercase (i int) comment='Old comment';
select table_schema, table_name, table_comment from information_schema.tables
  where table_schema like 'mysqltest_%' and table_name like 't_bug44738_%'
  order by table_name;
alter table t_bug44738_UPPERCASE comment='New comment';
alter table t_bug44738_lowercase comment='New comment';
--echo # There should be no stale entries in TDC for our tables after the
--echo # above ALTER TABLE statements so new version of comments should be
--echo # returned by the below query to I_S.
select table_schema, table_name, table_comment from information_schema.tables
  where table_schema like 'mysqltest_%' and table_name like 't_bug44738_%'
  order by table_name;
drop database mysqltest_UPPERCASE;
use test;

--echo # Let us check that the original test case which led to discovery
--echo # of this problem also works.
create table t_bug44738_UPPERCASE (i int);
select table_schema, table_name, table_comment from information_schema.tables
  where table_schema = 'test' and table_name like 't_bug44738_%';
drop table t_bug44738_UPPERCASE;
--echo # After the above DROP TABLE there are no entries in TDC which correspond
--echo # to our table and therefore the below statement should succeed.
create table t_bug44738_UPPERCASE (i int);
drop table t_bug44738_UPPERCASE;

--echo # BUG#13702397 - 64211: 'CREATE TABLE ... LIKE ...'
--echo #                       FAILS TO KEEP CASE
--echo #

CREATE TABLE TestTable1 (a int);
SHOW TABLES;
CREATE TABLE TestTable2 LIKE TestTable1;
SHOW TABLES;
DROP TABLE TestTable1, TestTable2;

--echo #
--echo # Bug #19894615: CRASH WHEN CREATING A TABLE IN A SCHEMA THAT
--echo #                WAS DROPPED AND CREATED AGAIN
--echo #

CREATE SCHEMA S1;
DROP SCHEMA S1;
CREATE SCHEMA S1;
CREATE TABLE S1.t1(i INT);
DROP TABLE S1.t1;
DROP SCHEMA S1;

--echo #
--echo # Bug #18895960: ASSERT IN OBJECT_CACHE.H LOADING_COMPLETED()
--echo #                IN MYSQL-TRUNK-WL6378
--echo #

CREATE TABLE T1 (i INT);
RENAME TABLE T1 to T2;
RENAME TABLE T2 to T1;
SELECT * FROM T1;
DROP TABLE T1;

CREATE TABLE T1 (i INT) ENGINE= MyISAM;
CREATE TABLE T2 LIKE T1;
ALTER TABLE T2 ENGINE= InnoDB;
DROP TABLE T2;
RENAME TABLE T1 to T2;
SELECT * FROM T2;
DROP TABLE T2;

--echo #
--echo # Bug#24666169: I_S.TABLE_CONSTRAINTS.CONSTRAINT_NAME IS NOT UPDATED
--echo #               AFTER RENAME TABLE
--echo #

# Additional coverage for FK name generation with LCTN=2

CREATE TABLE t1(a INT PRIMARY KEY);

CREATE TABLE T2(a INT, FOREIGN KEY(a) REFERENCES t1(a));
SELECT constraint_name FROM information_schema.referential_constraints
  WHERE table_name = 'T2' ORDER BY constraint_name;
SELECT constraint_name FROM information_schema.table_constraints
  WHERE table_name = 'T2' ORDER BY constraint_name;

RENAME TABLE T2 TO T3;
SELECT constraint_name FROM information_schema.referential_constraints
  WHERE table_name = 'T3' ORDER BY constraint_name;
SELECT constraint_name FROM information_schema.table_constraints
  WHERE table_name = 'T3' ORDER BY constraint_name;

RENAME TABLE t3 TO T4;
SELECT constraint_name FROM information_schema.referential_constraints
  WHERE table_name = 'T4' ORDER BY constraint_name;
SELECT constraint_name FROM information_schema.table_constraints
  WHERE table_name = 'T4' ORDER BY constraint_name;

DROP TABLE T4;

CREATE TABLE T2(a INT, CONSTRAINT T2_ibfk_1 FOREIGN KEY(a) REFERENCES t1(a));
SELECT constraint_name FROM information_schema.referential_constraints
  WHERE table_name = 'T2' ORDER BY constraint_name;
SELECT constraint_name FROM information_schema.table_constraints
  WHERE table_name = 'T2' ORDER BY constraint_name;

RENAME TABLE T2 TO T3;
SELECT constraint_name FROM information_schema.referential_constraints
  WHERE table_name = 'T3' ORDER BY constraint_name;
SELECT constraint_name FROM information_schema.table_constraints
  WHERE table_name = 'T3' ORDER BY constraint_name;

ALTER TABLE T3 DROP FOREIGN KEY t2_ibfk_1;

DROP TABLE T3, t1;

--echo #
--echo # BUG#28351038 - MYSQL WORKBENCH ERROR GETTING DDL FOR OBJECT ON TABLES
--echo # WITH AN UPPERCASE LETTER.
--echo #
CREATE TABLE `T1` (
`id` INT NOT NULL AUTO_INCREMENT,
`creation_utc` DATETIME NOT NULL,
PRIMARY KEY (`id`));

DELIMITER $$;
CREATE TRIGGER `test`.`test_table_t1_before_insert` BEFORE INSERT ON `T1`
FOR EACH ROW
BEGIN
  SET NEW.creation_utc = UTC_TIMESTAMP();
END
$$
DELIMITER ;$$
--echo # The SHOW CREATE TRIGGER command fails without the fix.
--replace_column 7 #
SHOW CREATE TRIGGER `test`.`test_table_t1_before_insert`;
DROP TABLE `T1`;
