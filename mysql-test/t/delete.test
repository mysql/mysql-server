#
# Check for problems with delete
#

--disable_warnings
drop table if exists t1,t2,t3,t11,t12;
--enable_warnings
CREATE TABLE t1 (a tinyint(3), b tinyint(5));
INSERT INTO t1 VALUES (1,1);
INSERT LOW_PRIORITY INTO t1 VALUES (1,2);
INSERT INTO t1 VALUES (1,3);
DELETE from t1 where a=1 limit 1;
DELETE LOW_PRIORITY from t1 where a=1;

INSERT INTO t1 VALUES (1,1);
DELETE from t1;
LOCK TABLE t1 write;
INSERT INTO t1 VALUES (1,2);
DELETE from t1;
UNLOCK TABLES;
INSERT INTO t1 VALUES (1,2);
SET AUTOCOMMIT=0;
DELETE from t1;
SET AUTOCOMMIT=1;
drop table t1;

#
# Test of delete when the delete will cause a node to disappear and reappear
# (This assumes a block size of 1024)
#

create table t1 (
	a bigint not null,
	b bigint not null default 0,
	c bigint not null default 0,
	d bigint not null default 0,
	e bigint not null default 0,
	f bigint not null default 0,
	g bigint not null default 0,
	h bigint not null default 0,
	i bigint not null default 0,
	j bigint not null default 0,
	primary key (a,b,c,d,e,f,g,h,i,j));
insert into t1 (a) values (2),(4),(6),(8),(10),(12),(14),(16),(18),(20),(22),(24),(26),(23);
delete from t1 where a=26;
drop table t1;
create table t1 (
	a bigint not null,
	b bigint not null default 0,
	c bigint not null default 0,
	d bigint not null default 0,
	e bigint not null default 0,
	f bigint not null default 0,
	g bigint not null default 0,
	h bigint not null default 0,
	i bigint not null default 0,
	j bigint not null default 0,
	primary key (a,b,c,d,e,f,g,h,i,j));
insert into t1 (a) values (2),(4),(6),(8),(10),(12),(14),(16),(18),(20),(22),(24),(26),(23),(27);
delete from t1 where a=27;
drop table t1;

CREATE TABLE `t1` (
  `i` int(10) NOT NULL default '0',
  `i2` int(10) NOT NULL default '0',
  PRIMARY KEY  (`i`)
);
-- error 1054
DELETE FROM t1 USING t1 WHERE post='1';
drop table t1;

#
# CHAR(0) bug - not actually DELETE bug, but anyway...
#

CREATE TABLE t1 (
  bool     char(0) default NULL,
  not_null varchar(20) binary NOT NULL default '',
  misc     integer not null,
  PRIMARY KEY  (not_null)
) ;

INSERT INTO t1 VALUES (NULL,'a',4), (NULL,'b',5), (NULL,'c',6), (NULL,'d',7);

select * from t1 where misc > 5 and bool is null;
delete   from t1 where misc > 5 and bool is null;
select * from t1 where misc > 5 and bool is null;

select count(*) from t1;
delete from t1 where 1 > 2;
select count(*) from t1;
delete from t1 where 3 > 2;
select count(*) from t1;

drop table t1;
#
# Bug #5733: Table handler error with self-join multi-table DELETE
#

create table t1 (a int not null auto_increment primary key, b char(32));
insert into t1 (b) values ('apple'), ('apple');
select * from t1;
delete t1 from t1, t1 as t2 where t1.b = t2.b and t1.a > t2.a;
select * from t1;
drop table t1;

#
# IGNORE option
#
create table t11 (a int NOT NULL, b int, primary key (a));
create table t12 (a int NOT NULL, b int, primary key (a));
create table t2 (a int NOT NULL, b int, primary key (a));
insert into t11 values (0, 10),(1, 11),(2, 12);
insert into t12 values (33, 10),(0, 11),(2, 12);
insert into t2 values (1, 21),(2, 12),(3, 23);
analyze table t11,t12,t2;
select * from t11;
select * from t12;
select * from t2;
-- error 1242
delete t11.*, t12.* from t11,t12 where t11.a = t12.a and t11.b <> (select b from t2 where t11.a < t2.a);
select * from t11;
select * from t12;
--error ER_SUBQUERY_NO_1_ROW
delete ignore t11.*, t12.* from t11,t12 where t11.a = t12.a and t11.b <> (select b from t2 where t11.a < t2.a);
DELETE FROM t11 WHERE a = 2;
analyze table t11,t12,t2;
select * from t11;
select * from t12;
insert into t11 values (2, 12);
analyze table t11,t12,t2;
-- error 1242
delete from t11 where t11.b <> (select b from t2 where t11.a < t2.a);
select * from t11;
--error ER_SUBQUERY_NO_1_ROW
delete ignore from t11 where t11.b <> (select b from t2 where t11.a < t2.a);
analyze table t11,t12,t2;
select * from t11;
drop table t11, t12, t2;

--echo # sql_safe_updates mode with single-table DELETE.

CREATE TABLE t (a INTEGER);
INSERT INTO t VALUES (1), (2), (3), (4), (5), (6), (7);

SET sql_safe_updates = ON;
--error ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE
DELETE FROM t;
--error ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE
DELETE FROM t ORDER BY a;
--error ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE
DELETE FROM t WHERE a = 6;
# OK because of LIMIT.
DELETE FROM t ORDER BY a LIMIT 1;

ALTER TABLE t ADD KEY k (a);

--error ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE
DELETE FROM t;
--error ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE
DELETE FROM t ORDER BY a;
# OK because the WHERE clause uses a key.
--skip_if_hypergraph  # Depends on the plan choice.
DELETE FROM t WHERE a = 6;
# OK because of LIMIT.
DELETE FROM t ORDER BY a LIMIT 1;

--skip_if_hypergraph  # Has to be skipped since we are skipping the DELETE above.
SELECT * FROM t ORDER BY a;

SET sql_safe_updates = DEFAULT;
DROP TABLE t;

--echo # sql_safe_updates mode with multi-table DELETE

CREATE TABLE t1(a INTEGER PRIMARY KEY);
INSERT INTO t1 VALUES(10),(20);

CREATE TABLE t2(b INTEGER);
INSERT INTO t2 VALUES(10),(20);

SET SESSION sql_safe_updates=1;

EXPLAIN DELETE t2 FROM t1 JOIN t2 WHERE t1.a = 10;
-- error ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE
DELETE t2 FROM t1 JOIN t2 WHERE t1.a = 10;

SET SESSION sql_safe_updates=default;

DROP TABLE t1, t2;

#
# Bug #4198: deletion and KEYREAD
#

create table t1 (a int, b int, unique key (a), key (b));
insert into t1 values (3, 3), (7, 7);
delete t1 from t1 where a = 3;
check table t1;
select * from t1;
drop table t1;

#
# Bug #8392: delete with ORDER BY containing a direct reference to the table 
#
 
CREATE TABLE t1 ( a int PRIMARY KEY );
DELETE FROM t1 WHERE t1.a > 0 ORDER BY t1.a;
INSERT INTO t1 VALUES (0),(1),(2);
DELETE FROM t1 WHERE t1.a > 0 ORDER BY t1.a LIMIT 1;
SELECT * FROM t1;
DROP TABLE t1;

#
# Bug #21392: multi-table delete with alias table name fails with 
# 1003: Incorrect table name
#

create table t1 (a int);
delete `4.t1` from t1 as `4.t1` where `4.t1`.a = 5;
delete FROM `4.t1` USING t1 as `4.t1` where `4.t1`.a = 5;
drop table t1;

#
# Bug#17711: DELETE doesn't use index when ORDER BY, LIMIT and
#            non-restricting WHERE is present.
#
create table t1(f1 int primary key);
insert into t1 values (4),(3),(1),(2);
delete from t1 where (@a:= f1) order by f1 limit 1;
select @a;
drop table t1;

# BUG#30385 "Server crash when deleting with order by and limit"
CREATE TABLE t1 (
  `date` date ,
  `time` time ,
  `seq` int(10) unsigned NOT NULL auto_increment,
  PRIMARY KEY  (`seq`),
  KEY `seq` (`seq`),
  KEY `time` (`time`),
  KEY `date` (`date`)
);
DELETE FROM t1 ORDER BY date ASC, time ASC LIMIT 1;
drop table t1;

--echo End of 4.1 tests

#
# Test of multi-delete where we are not scanning the first table
#

CREATE TABLE t1 (a int not null,b int not null);
CREATE TABLE t2 (a int not null, b int not null, primary key (a,b));
CREATE TABLE t3 (a int not null, b int not null, primary key (a,b));
insert into t1 values (1,1),(2,1),(1,3);
insert into t2 values (1,1),(2,2),(3,3);
insert into t3 values (1,1),(2,1),(1,3);
select * from t1,t2,t3 where t1.a=t2.a AND t2.b=t3.a and t1.b=t3.b order by t1.a,t1.b;
analyze table t1;
analyze table t2;
analyze table t3;
explain select * from t1,t2,t3 where t1.a=t2.a AND t2.b=t3.a and t1.b=t3.b;
delete t2.*,t3.* from t1,t2,t3 where t1.a=t2.a AND t2.b=t3.a and t1.b=t3.b;
# This should be empty
select * from t3;
drop table t1,t2,t3;

#
# Bug #8143: deleting '0000-00-00' values using IS NULL
#

create table t1(a date not null);
insert ignore into t1 values (0);
select * from t1 where a is null;
delete from t1 where a is null;
select count(*) from t1;
drop table t1;

#
# Bug #26186: delete order by, sometimes accept unknown column
#
CREATE TABLE t1 (a INT); INSERT INTO t1 VALUES (1);

--error ER_BAD_FIELD_ERROR
DELETE FROM t1 ORDER BY x;

# even columns from a table not used in query (and not even existing)
--error ER_BAD_FIELD_ERROR
DELETE FROM t1 ORDER BY t2.x;

# subquery (as long as the subquery from is valid or DUAL)
--error ER_BAD_FIELD_ERROR
DELETE FROM t1 ORDER BY (SELECT x);

DROP TABLE t1;

#
# Bug #30234: Unexpected behavior using DELETE with AS and USING
# '
CREATE TABLE t1 (
  a INT
);

CREATE TABLE t2 (
  a INT
);

CREATE DATABASE db1;
CREATE TABLE db1.t1 (
  a INT
);
INSERT INTO db1.t1 (a) SELECT * FROM t1;

CREATE DATABASE db2;
CREATE TABLE db2.t1 (
  a INT
);
INSERT INTO db2.t1 (a) SELECT * FROM t2;

--error ER_PARSE_ERROR
DELETE FROM t1 alias USING t1, t2 alias WHERE t1.a = alias.a;
DELETE FROM alias USING t1, t2 alias WHERE t1.a = alias.a;
DELETE FROM t1, alias USING t1, t2 alias WHERE t1.a = alias.a;
--error ER_UNKNOWN_TABLE
DELETE FROM t1, t2 USING t1, t2 alias WHERE t1.a = alias.a;
--error ER_PARSE_ERROR
DELETE FROM db1.t1 alias USING db1.t1, db2.t1 alias WHERE db1.t1.a = alias.a;
DELETE FROM alias USING db1.t1, db2.t1 alias WHERE db1.t1.a = alias.a;
--error ER_UNKNOWN_TABLE
DELETE FROM db2.alias USING db1.t1, db2.t1 alias WHERE db1.t1.a = alias.a;
DELETE FROM t1 USING t1 WHERE a = 1;
SELECT * FROM t1;
--error ER_PARSE_ERROR
DELETE FROM t1 alias USING t1 alias WHERE a = 2;
SELECT * FROM t1;

DROP TABLE t1, t2;
DROP DATABASE db1;
DROP DATABASE db2;

#
# Bug 31742: delete from ... order by function call that causes an error, 
#            asserts server
#

CREATE FUNCTION f1() RETURNS INT RETURN 1;
CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (0);
--error 1318
DELETE FROM t1 ORDER BY (f1(10)) LIMIT 1;
DROP TABLE t1;
DROP FUNCTION f1;


--echo #
--echo # Bug #49552 : sql_buffer_result cause crash + not found records 
--echo #   in multitable delete/subquery
--echo #

CREATE TABLE t1(a INT);
INSERT INTO t1 VALUES (1),(2),(3);
SET SESSION SQL_BUFFER_RESULT=1;
DELETE t1 FROM (SELECT SUM(a) a FROM t1) x,t1;

SET SESSION SQL_BUFFER_RESULT=DEFAULT;
SELECT * FROM t1;
DROP TABLE t1;

--echo End of 5.0 tests

#
# Bug#27525: table not found when using multi-table-deletes with aliases over
#            several databas
# Bug#21148: MULTI-DELETE fails to resolve a table by alias if it is from a
#            different database
#

--disable_warnings
DROP DATABASE IF EXISTS db1;
DROP DATABASE IF EXISTS db2;
DROP DATABASE IF EXISTS db3;
DROP DATABASE IF EXISTS db4;
DROP TABLE IF EXISTS t1, t2;
DROP PROCEDURE IF EXISTS count;
--enable_warnings
USE test;
CREATE DATABASE db1;
CREATE DATABASE db2;

CREATE TABLE db1.t1 (a INT, b INT);
INSERT INTO db1.t1 VALUES (1,1),(2,2),(3,3);
CREATE TABLE db1.t2 AS SELECT * FROM db1.t1;
CREATE TABLE db2.t1 AS SELECT * FROM db1.t2;
CREATE TABLE db2.t2 AS SELECT * FROM db2.t1;
CREATE TABLE t1 AS SELECT * FROM db2.t2;
CREATE TABLE t2 AS SELECT * FROM t1;

delimiter |;
CREATE PROCEDURE count_rows()
BEGIN
  SELECT COUNT(*) AS "COUNT(db1.t1)" FROM db1.t1;
  SELECT COUNT(*) AS "COUNT(db1.t2)" FROM db1.t2;
  SELECT COUNT(*) AS "COUNT(db2.t1)" FROM db2.t1;
  SELECT COUNT(*) AS "COUNT(db2.t2)" FROM db2.t2;
  SELECT COUNT(*) AS "COUNT(test.t1)" FROM test.t1;
  SELECT COUNT(*) AS "COUNT(test.t2)" FROM test.t2;
END|
delimiter ;|

#
# Testing without a selected database
#

CREATE DATABASE db3;
USE db3;
DROP DATABASE db3;
--error ER_NO_DB_ERROR
SELECT * FROM t1;

# Detect missing table references

--error ER_NO_DB_ERROR
DELETE a1,a2 FROM db1.t1, db2.t2;
--error ER_NO_DB_ERROR
DELETE a1,a2 FROM db1.t1, db2.t2;
--error ER_NO_DB_ERROR
DELETE a1,a2 FROM db1.t1 AS a1, db2.t2;
--error ER_NO_DB_ERROR
DELETE a1,a2 FROM db1.t1, db2.t2 AS a2;
--error ER_NO_DB_ERROR
DELETE a1,a2 FROM db3.t1 AS a1, db4.t2 AS a2;
--error ER_NO_DB_ERROR
DELETE a1,a2 FROM db3.t1 AS a1, db4.t2 AS a2;

--error ER_NO_DB_ERROR
DELETE FROM a1,a2 USING db1.t1, db2.t2;
--error ER_NO_DB_ERROR
DELETE FROM a1,a2 USING db1.t1, db2.t2;
--error ER_NO_DB_ERROR
DELETE FROM a1,a2 USING db1.t1 AS a1, db2.t2;
--error ER_NO_DB_ERROR
DELETE FROM a1,a2 USING db1.t1, db2.t2 AS a2;
--error ER_NO_DB_ERROR
DELETE FROM a1,a2 USING db3.t1 AS a1, db4.t2 AS a2;
--error ER_NO_DB_ERROR
DELETE FROM a1,a2 USING db3.t1 AS a1, db4.t2 AS a2;

# Ambiguous table references

--error ER_NO_DB_ERROR
DELETE a1 FROM db1.t1 AS a1, db2.t2 AS a1;
--error ER_NO_DB_ERROR
DELETE a1 FROM db1.a1, db2.t2 AS a1;
--error ER_NO_DB_ERROR
DELETE a1 FROM a1, db1.t1 AS a1;
--error ER_NO_DB_ERROR
DELETE t1 FROM db1.t1, db2.t1 AS a1;
--error ER_NO_DB_ERROR
DELETE t1 FROM db1.t1 AS a1, db2.t1 AS a2;
--error ER_NO_DB_ERROR
DELETE t1 FROM db1.t1, db2.t1;

# Test all again, now with a selected database

USE test;

# Detect missing table references

--error ER_UNKNOWN_TABLE
DELETE a1,a2 FROM db1.t1, db2.t2;
--error ER_UNKNOWN_TABLE
DELETE a1,a2 FROM db1.t1, db2.t2;
--error ER_UNKNOWN_TABLE
DELETE a1,a2 FROM db1.t1 AS a1, db2.t2;
--error ER_UNKNOWN_TABLE
DELETE a1,a2 FROM db1.t1, db2.t2 AS a2;
--error ER_BAD_DB_ERROR
DELETE a1,a2 FROM db3.t1 AS a1, db4.t2 AS a2;
--error ER_BAD_DB_ERROR
DELETE a1,a2 FROM db3.t1 AS a1, db4.t2 AS a2;

--error ER_UNKNOWN_TABLE
DELETE FROM a1,a2 USING db1.t1, db2.t2;
--error ER_UNKNOWN_TABLE
DELETE FROM a1,a2 USING db1.t1, db2.t2;
--error ER_UNKNOWN_TABLE
DELETE FROM a1,a2 USING db1.t1 AS a1, db2.t2;
--error ER_UNKNOWN_TABLE
DELETE FROM a1,a2 USING db1.t1, db2.t2 AS a2;
--error ER_BAD_DB_ERROR
DELETE FROM a1,a2 USING db3.t1 AS a1, db4.t2 AS a2;
--error ER_BAD_DB_ERROR
DELETE FROM a1,a2 USING db3.t1 AS a1, db4.t2 AS a2;

# Ambiguous table references

--error ER_NONUNIQ_TABLE
DELETE a1 FROM db1.t1 AS a1, db2.t2 AS a1;
--error ER_NO_SUCH_TABLE
DELETE a1 FROM db1.a1, db2.t2 AS a1;
--error ER_NONUNIQ_TABLE
DELETE a1 FROM a1, db1.t1 AS a1;
--error ER_UNKNOWN_TABLE
DELETE t1 FROM db1.t1, db2.t1 AS a1;
--error ER_UNKNOWN_TABLE
DELETE t1 FROM db1.t1 AS a1, db2.t1 AS a2;
--error ER_UNKNOWN_TABLE
DELETE t1 FROM db1.t1, db2.t1;

# Test multiple-table cross database deletes

DELETE t1 FROM db1.t2 AS t1, db2.t2 AS t2 WHERE t2.a = 1 AND t1.a = t2.a;
SELECT ROW_COUNT();
CALL count_rows();
DELETE a1, a2 FROM db2.t1 AS a1, t2 AS a2 WHERE a1.a = 2 AND a2.a = 2;
SELECT ROW_COUNT();
CALL count_rows();

DROP DATABASE db1;
DROP DATABASE db2;
DROP PROCEDURE count_rows;
DROP TABLE t1, t2;

--echo #
--echo # Bug#46958: Assertion in Diagnostics_area::set_ok_status, trigger, 
--echo # merge table
--echo #
CREATE TABLE t1 ( a INT );
CREATE TABLE t2 ( a INT );
CREATE TABLE t3 ( a INT );

INSERT INTO t1 VALUES (1), (2);
INSERT INTO t2 VALUES (1), (2);
INSERT INTO t3 VALUES (1), (2);

CREATE TRIGGER tr1 BEFORE DELETE ON t2
FOR EACH ROW INSERT INTO no_such_table VALUES (1);

--error ER_NO_SUCH_TABLE
DELETE t1, t2, t3 FROM t1, t2, t3;

SELECT * FROM t1;
SELECT * FROM t2;
SELECT * FROM t3;

DROP TABLE t1, t2, t3;

CREATE TABLE t1 ( a INT );
CREATE TABLE t2 ( a INT );
CREATE TABLE t3 ( a INT );

INSERT INTO t1 VALUES (1), (2);
INSERT INTO t2 VALUES (1), (2);
INSERT INTO t3 VALUES (1), (2);

CREATE TRIGGER tr1 AFTER DELETE ON t2
FOR EACH ROW INSERT INTO no_such_table VALUES (1);

--error ER_NO_SUCH_TABLE
DELETE t1, t2, t3 FROM t1, t2, t3;

SELECT * FROM t1;
SELECT * FROM t2;
SELECT * FROM t3;

DROP TABLE t1, t2, t3;

--echo #
--echo # Bug #46425 crash in Diagnostics_area::set_ok_status, 
--echo #            empty statement, DELETE IGNORE
--echo #

CREATE table t1 (i INTEGER);

INSERT INTO t1 VALUES (1);

--delimiter |

CREATE TRIGGER tr1 AFTER DELETE ON t1 FOR EACH ROW 
BEGIN 
  INSERT INTO t1 SELECT * FROM t1 AS A;
END |

--delimiter ;
--error ER_CANT_UPDATE_USED_TABLE_IN_SF_OR_TRG
DELETE IGNORE FROM t1;

DROP TABLE t1;


--echo #
--echo # Bug #53034: Multiple-table DELETE statements not accepting
--echo #             "Access compatibility" syntax
--echo #

CREATE TABLE t1 (id INT);
CREATE TABLE t2 LIKE t1;
CREATE TABLE t3 LIKE t1;

DELETE FROM t1.*, test.t2.*, a.* USING t1, t2, t3 AS a;

DROP TABLE t1, t2, t3;

--echo End of 5.1 tests


--echo #
--echo # Bug#51099 Assertion in mysql_multi_delete_prepare()
--echo #

--disable_warnings
DROP TABLE IF EXISTS t1, t2;
DROP VIEW IF EXISTS v1, v2;
--enable_warnings

CREATE TABLE t1(a INT);
CREATE TABLE t2(b INT);
CREATE VIEW v1 AS SELECT a, b FROM t1, t2;
CREATE VIEW v2 AS SELECT a FROM v1;

# This is a normal delete
--error ER_VIEW_DELETE_MERGE_VIEW
DELETE FROM v2;
# This is a multi table delete, check that we get the same error
# This caused the assertion.
--error ER_VIEW_DELETE_MERGE_VIEW
DELETE v2 FROM v2;

DROP VIEW v2, v1;
DROP TABLE t1, t2;


--echo #
--echo # Bug#58709 assert in mysql_execute_command
--echo #

--disable_warnings
DROP TABLE IF EXISTS t2, t1;
DROP PROCEDURE IF EXISTS p1;
--enable_warnings

CREATE TABLE t1 (i INT PRIMARY KEY) engine=InnoDB;
CREATE TABLE t2 (i INT, FOREIGN KEY (i) REFERENCES t1 (i) ON DELETE NO ACTION) engine=InnoDB;

INSERT INTO t1 VALUES (1);
INSERT INTO t2 VALUES (1);

DELETE IGNORE FROM t1 WHERE i = 1;

CREATE PROCEDURE p1() DELETE IGNORE FROM t1 WHERE i = 1;
# This triggered the assert
CALL p1();

PREPARE stm FROM 'CALL p1()';
# This also triggered the assert
EXECUTE stm;
DEALLOCATE PREPARE stm;

DROP TABLE t2, t1;
DROP PROCEDURE p1;

--echo #
--echo # Bug#18345346 Assertion failed: table_ref->view && table_ref->table == 0
--echo #

CREATE TABLE b(a INTEGER);
CREATE VIEW y AS SELECT 1 FROM b, b AS e;
CREATE VIEW x AS SELECT 1 FROM y;
CREATE VIEW z AS SELECT 1 FROM x LIMIT 1;

--error ER_NON_UPDATABLE_TABLE
DELETE z FROM (SELECT 1) AS x, z;

DROP VIEW z, x, y;
DROP TABLE b;

--echo # Bug#11752648 : MULTI-DELETE IGNORE DOES NOT REPORT WARNINGS
--echo #
CREATE TABLE t1 (a INT PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 (a) VALUES (1);
CREATE TABLE t2 (a INT PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t2 (a) VALUES (1);
CREATE TABLE t3 (a INT, b INT, CONSTRAINT c_a FOREIGN KEY (a)
REFERENCES t1 (a), CONSTRAINT c_b FOREIGN KEY (b) REFERENCES t2 (a)) ENGINE=InnoDB;
INSERT INTO t3 (a, b) VALUES (1, 1);
DELETE IGNORE FROM t1;
DELETE IGNORE t1,t2 FROM t1,t2;
SELECT * FROM t1;
SELECT * FROM t2;
DROP TABLE t3,t2,t1;

--echo #
--echo # Bug#17550423 : DELETE IGNORE GIVES INCORRECT RESULT WITH FOREIGN KEY
--echo #                   FOR PARENT TABLE
--echo #
CREATE TABLE t1 (a INT PRIMARY KEY) ENGINE=InnoDB;
CREATE TABLE t2 (b INT) ENGINE=InnoDB;
INSERT INTO t1 VALUES (1), (2), (5);
INSERT INTO t2 VALUES (1), (5);
--replace_regex /#sql-(?:[0-9a-f]*_[0-9a-f]*)*/#sql-temporary/
ALTER TABLE t2 ADD CONSTRAINT FOREIGN KEY(b) REFERENCES t1(a);
SELECT * FROM t2;
DELETE IGNORE FROM t1;
SELECT * FROM t1;
DROP TABLE t2,t1;

--echo #
--echo # Bug#20460208: !table || (!table->read_set || bitmap_is_set)
--echo #

CREATE TABLE t1(a INTEGER) engine=innodb;
CREATE TABLE t2(a BLOB) engine=innodb;
--error ER_BAD_FIELD_ERROR
INSERT INTO t1 SET z=1;
INSERT INTO t2 VALUES('a');
DELETE FROM t2 WHERE 1 = a;
DROP TABLE t1, t2;

--echo #
--echo # Bug#20086791 ASSERT `! IS_SET()` IN DIAGNOSTICS_AREA::SET_OK_STATUS
--echo #              ON DELETE (ER_SUBQUERY..)
--echo #

SET sql_mode='';

CREATE TABLE t1 (
  col_int_key int,
  pk integer auto_increment,
  col_datetime_key datetime,
  /*Indices*/
  key (col_int_key ),
  primary key (pk),
  key (col_datetime_key )
) ENGINE=memory;

CREATE TABLE t2 (
  col_varchar_key varchar (1),
  col_date_key date,
  pk integer auto_increment,
  /*Indices*/
  key (col_varchar_key ),
  key (col_date_key ),
  primary key (pk)
) ENGINE=memory;

INSERT INTO t2 VALUES
('v',  '2002-05-01', NULL) ,
('d',  '2001-01-01', NULL) 
;


CREATE TABLE t3 (
  pk integer auto_increment,
  col_int_key int,
  col_varchar_key varchar (1),
  /*Indices*/
  primary key (pk),
  key (col_int_key ),
  key (col_varchar_key )) 
ENGINE=memory;

INSERT INTO t3 VALUES
(NULL, 3, 'n') ,
(NULL, 1, 'r') 
;

CREATE TABLE t4 (
  pk integer auto_increment,
  col_varchar_key varchar (1),
  col_int_key int,
  /*Indices*/
  primary key (pk),
  key (col_varchar_key ),
  key (col_int_key )
) ENGINE=memory;

CREATE TABLE t5 (
  col_datetime_key datetime,
  pk integer auto_increment,
  col_int_key int,
  /*Indices*/
  key (col_datetime_key ),
  primary key (pk),
  key (col_int_key )) 
ENGINE=memory;

INSERT INTO t5 VALUES
('2007-10-01', NULL, 8) ,
('2002-10-01', NULL, 3) 
;

DELETE OUTR1.* FROM t2 AS OUTR1 
JOIN t3 AS OUTR2 
ON ( OUTR1 . col_varchar_key = OUTR2 . col_varchar_key ) 
WHERE OUTR1 . col_varchar_key NOT IN
 ( SELECT  INNR1 . col_varchar_key AS y FROM t5 AS INNR2
   RIGHT JOIN t4 AS INNR1 ON ( INNR2 . pk < INNR1 . col_int_key )
   WHERE INNR1 . col_int_key <= INNR1 . col_int_key
     AND OUTR2 . col_int_key >= 3  );

DELETE QUICK  
FROM OUTR1.* USING t2 AS OUTR1 
LEFT OUTER JOIN t1 AS OUTR2 
ON ( OUTR1 . col_date_key = OUTR2 . col_datetime_key ) 
WHERE OUTR1 . pk NOT IN ( SELECT 2 UNION  SELECT 7 );

--error ER_SUBQUERY_NO_1_ROW
DELETE    OUTR1.* 
FROM t2 AS OUTR1
  LEFT JOIN t1 AS OUTR2 
  ON ( OUTR1 . pk = OUTR2 . col_int_key )
WHERE OUTR1 . pk <> (
  SELECT DISTINCT INNR1 . col_int_key AS y
  FROM t5 AS INNR1 WHERE OUTR1 . pk <= 5
  ORDER BY INNR1 . col_datetime_key
);

DROP TABLE t1, t2, t3, t4, t5;

SET sql_mode=default;

--echo # Bug#20450013 assertion 'select_lex::having_cond() == __null ...

CREATE TABLE t1(a TEXT, FULLTEXT (a));
CREATE VIEW v1 AS SELECT a FROM t1 ORDER BY a;

let $query=
DELETE FROM t1 USING v1,t1;

eval explain $query;
eval $query;

DROP VIEW v1;
DROP TABLE t1;

--echo #
--echo # Bug#25320909 ASSERTION `THD->LEX->CURRENT_SELECT() == UNIT->FIRST_SELECT()' WITH DELETE
--echo #

CREATE TABLE t1 (a INT, b CHAR(8), pk INT AUTO_INCREMENT PRIMARY KEY);
INSERT INTO t1 (a,b) VALUES
(10000,'foobar'),(1,'a'),(2,'b'),(3,'c'),(4,'d'),(5,'e');
INSERT INTO t1 (a,b) SELECT a, b FROM t1;

CREATE TABLE t2 (pk INT AUTO_INCREMENT PRIMARY KEY, c CHAR(8), d INT);
INSERT INTO t2 (c,d) SELECT b, a FROM t1;

DELETE IGNORE FROM t1 WHERE b IS NOT NULL ORDER BY a LIMIT 1;
--sorted_result
SELECT a,b FROM t1;
--error ER_SUBQUERY_NO_1_ROW
DELETE IGNORE t1.*, t2.* FROM t1, t2
  WHERE c < b OR
        a != ( SELECT 1 UNION SELECT 2 );

DROP TABLE t1, t2;

--echo #
--echo # Bug#29153485: ASSERTION IN JOIN_READ_CONST_TABLE()
--echo #
CREATE TABLE d3 (
  pk int(11) NOT NULL AUTO_INCREMENT,
  col_int int(11) DEFAULT NULL,
  col_varchar varchar(1) DEFAULT NULL,
  PRIMARY KEY (pk)
) ;

INSERT INTO d3 VALUES (96,9,'t'),(97,0,'x');

CREATE TABLE e3 (
  col_varchar varchar(1) DEFAULT NULL,
  pk int(11) NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (pk)
) ;

INSERT INTO e3 VALUES ('v',986);
--error ER_SUBQUERY_NO_1_ROW
DELETE OUTR1.* FROM e3 AS OUTR1 INNER JOIN d3 AS OUTR2 ON 
  ( OUTR1.col_varchar = OUTR2.col_varchar )
  LEFT JOIN d3 AS OUTR3 ON ( OUTR1. pk = OUTR3.pk )
  WHERE OUTR1.pk =
    ( SELECT DISTINCT INNR1.col_int FROM d3 AS INNR1 WHERE INNR1.pk <> 8) ;

DROP TABLE d3,e3;

--echo #
--echo # Bug#27455809; DELETE FROM ...WHERE NOT EXISTS WITH TABLE ALIAS CREATES AN ERROR 1064 (42000
--echo #

CREATE TABLE t1 (c1 INT);
INSERT INTO t1 VALUES (1), (2);
SELECT * FROM t1;

DELETE FROM t1 AS a1 WHERE a1.c1 = 2;
SELECT * FROM t1;

CREATE TABLE t2 (c2 INT);
INSERT INTO t2 VALUES (1), (2);
SELECT * FROM t2;

DELETE FROM t2 a2 WHERE NOT EXISTS (SELECT * FROM t1 WHERE t1.c1 = a2.c2);
SELECT * FROM t2;

DROP TABLE t1, t2;

--echo # Bug#31640267: Assertion `trans_safe || updated_rows == 0 || thd->get_transaction()

CREATE TABLE t1(pk INTEGER PRIMARY KEY, a INTEGER);
INSERT INTO t1 VALUES(1, 10), (2, 20);

PREPARE s FROM 'DELETE t1 FROM t1, (SELECT 1 FROM DUAL) AS dt';
EXECUTE s;
SELECT ROW_COUNT();
INSERT INTO t1 VALUES(1, 10), (2, 20);
EXECUTE s;
SELECT ROW_COUNT();
DEALLOCATE PREPARE s;

DROP TABLE t1;

--echo #
--echo # WL#14673: Enable the hypergraph optimizer for DELETE
--echo #

# The hypergraph optimizer can use hash joins in DELETE statements,
# whereas the old optimizer always uses nested loop joins. When using
# hash joins, some extra care must be taken to make sure the row IDs
# are propagated up from the join. (This is not an issue with nested
# loop joins, since the underlying scans are always positioned on the
# row returned from the join.) This test case verifies that the row
# IDs are propagated correctly when we have hash joins inside other
# hash joins. The hypergraph optimizer chooses this plan:
#   (t1 HJ t2) HJ (t3 HJ t4)

CREATE TABLE t1 (a INTEGER);
CREATE TABLE t2 (a INTEGER, b INTEGER);
CREATE TABLE t3 (a INTEGER);
CREATE TABLE t4 (a INTEGER, b INTEGER);

INSERT INTO t1 VALUES (-1), (0), (1), (2), (2), (3), (100), (1000);
INSERT INTO t2 VALUES (-2, 0), (2, 1), (3, 1), (3, 0), (101, 2), (1001, 0);

INSERT INTO t3 VALUES (-1), (0), (1), (2), (2), (3), (100), (1000);
INSERT INTO t4 VALUES (-2, -1), (2, 1), (3, 1), (3, -1), (101, 2), (1001, -1);

DELETE t1, t2, t3, t4
FROM (t1 INNER JOIN t2 USING (a)) LEFT JOIN (t3 INNER JOIN t4 USING (a)) ON t2.b = t4.b;

--sorted_result
SELECT * FROM t1;
--sorted_result
SELECT * FROM t2;
--sorted_result
SELECT * FROM t3;
--sorted_result
SELECT * FROM t4;

DROP TABLE t1, t2, t3, t4;

--echo #
--echo # Bug#33722819: recent regression: crash/corruption with gcols
--echo #               and multitable delete...
--echo #

CREATE TABLE t (
  a INT,
  b INT,
  c TIMESTAMP GENERATED ALWAYS AS (65),
  UNIQUE KEY (a, b),
  UNIQUE KEY (c, a)
);

INSERT IGNORE INTO t(a, b) VALUES (1, 1), (2, 2);

--error ER_TRUNCATED_WRONG_VALUE
DELETE d2 FROM t AS d1, t AS d2 WHERE d1.a > d2.a;
--sorted_result
SELECT * FROM t;
# Different number of warnings with the hypergraph optimizer.
--disable_warnings
DELETE IGNORE d2 FROM t AS d1, t AS d2 WHERE d1.a > d2.a;
--enable_warnings
SELECT * FROM t;
DROP TABLE t;

--echo # Bug#34603748: Assertion !thd->is_error() failed in sql_executor.cc

CREATE TABLE t1 (
  c int,
  k int,
  v varchar(1),
  pk int NOT NULL PRIMARY KEY,
  KEY(k)
);

INSERT INTO t1 VALUES (737324598,-252214477,'k',1);

CREATE TABLE t2 (
  c int,
  k int,
  v varchar(1),
  pk int NOT NULL PRIMARY KEY,
  KEY(k)
);

INSERT INTO t2 VALUES
 (855986294,-882338447,NULL,1), (232637232,731304773,'y',2),
 (1590168493,1015181107,'h',3), (-1009423426,-232667498,'r',4),
 (236379810,1802601033,'H',5), (551188244,-756499064,NULL,6),
 (-1719538815,-448490308,'N',7), (-2122356533,NULL,'y',8),
 (-687608898,199866688,'0',9), (-1901664078,NULL,'B',10),
 (-2108420295,-497498614,'M',11), (1407257546,191555131,'8',12),
 (88536399,-1988379833,'N',13), (351531198,1071194019,'r',14),
 (194539025,1671647783,'Y',15), (-1133948378,1947844472,'e',16),
 (-533037937,1007507258,'E',17), (1268272947,-1875925199,'w',18),
 (2002333920,-764155734,'0',19), (-2017216121,-1780822065,'w',20);

--error ER_TRUNCATED_WRONG_VALUE
DELETE FROM outr1, outr2
       USING t1 AS outr1 RIGHT OUTER JOIN t1 AS outr2
             ON outr1.c = outr2.c
WHERE outr1.c >= (SELECT DISTINCT innr1.k AS y
                  FROM t2 AS innr2 LEFT JOIN t2 AS innr1
                       ON innr2.k = CAST(innr1.v AS DATETIME)
                        WHERE innr1.pk = 3
                 );

DROP TABLE t1, t2;
