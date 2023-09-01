--source include/force_myisam_default.inc
--source include/have_myisam.inc

SET SQL_WARNINGS=1;

#
# Test with blob + tinyint key
# (Failed for Greg Valure)
#

CREATE TABLE t1 (
  a tinytext NOT NULL,
  b tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY (a(32),b)
) ENGINE=MyISAM;
INSERT INTO t1 VALUES ('a',1),('a',2);
SELECT * FROM t1 WHERE a='a' AND b=2;
SELECT * FROM t1 WHERE a='a' AND b in (2);
SELECT * FROM t1 WHERE a='a' AND b in (1,2);
drop table t1;

#
# Problem with UNIQUE() with NULL parts and auto increment
#

CREATE TABLE t1 (c CHAR(10) NOT NULL,i INT NOT NULL AUTO_INCREMENT,
UNIQUE (c,i)) ENGINE=MYISAM;
INSERT IGNORE INTO t1 (c) VALUES (NULL),(NULL);
SELECT * FROM t1;
INSERT INTO t1 (c) VALUES ('a'),('a');
SELECT * FROM t1;
DROP TABLE IF EXISTS t1;
CREATE TABLE t1 (c CHAR(10) NULL, i INT NOT NULL AUTO_INCREMENT,
UNIQUE (c,i)) ENGINE=MYISAM;
INSERT INTO t1 (c) VALUES (NULL),(NULL);
SELECT * FROM t1;
INSERT INTO t1 (c) VALUES ('a'),('a');
SELECT * FROM t1;
drop table t1;

#
# Test of key read with primary key (Bug #3497)
#

CREATE TABLE t1 (id int unsigned auto_increment, name char(50), primary key (id)) engine=myisam;
insert into t1 (name) values ('a'), ('b'),('c'),('d'),('e'),('f'),('g');
explain select 1 from t1 where id =2;
explain select 1 from t1 where id =2 or id=3;
explain select name from t1 where id =2;
ALTER TABLE t1 DROP PRIMARY KEY, ADD INDEX (id);
explain select 1 from t1 where id =2;
drop table t1;

#
# UNIQUE prefix keys and multi-byte charsets
#

create table t1 (c varchar(30), t text, unique (c(2)), unique (t(3))) charset utf8mb3 engine=myisam;
show create table t1;
insert t1 values ('cccc', 'tttt'),
  (0xD0B1212223D0B1D0B1D0B1D0B1D0B1, 0xD0B1D0B1212223D0B1D0B1D0B1D0B1),
  (0xD0B1222123D0B1D0B1D0B1D0B1D0B1, 0xD0B1D0B1222123D0B1D0B1D0B1D0B1);
--error ER_DUP_ENTRY
insert t1 (c) values ('cc22');
--error ER_DUP_ENTRY
insert t1 (t) values ('ttt22');
--error ER_DUP_ENTRY
insert t1 (c) values (0xD0B1212322D0B1D0B1D0B1D0B1D0B1);
--error ER_DUP_ENTRY
insert t1 (t) values (0xD0B1D0B1212322D0B1D0B1D0B1D0B1);
select c from t1 where c='cccc';
select t from t1 where t='tttt';
select c from t1 where c=0xD0B1212223D0B1D0B1D0B1D0B1D0B1;
select t from t1 where t=0xD0B1D0B1212223D0B1D0B1D0B1D0B1;
drop table t1;

#
# BUG#6151 - myisam index corruption
#
DROP TABLE IF EXISTS t1;
CREATE TABLE t1 (
  c1 int,
  c2 varbinary(240),
  UNIQUE KEY (c1),
  KEY (c2)
) ENGINE=MyISAM;
INSERT INTO t1 VALUES (1,'\Z\Z\Z\Z');
INSERT INTO t1 VALUES (2,'\Z\Z\Z\Z\Z\Z');
INSERT INTO t1 VALUES (3,'\Z\Z\Z\Z');
select c1 from t1 where c2='\Z\Z\Z\Z';
DELETE FROM t1 WHERE (c1 = 1);
check table t1;
select c1 from t1 where c2='\Z\Z\Z\Z';
DELETE FROM t1 WHERE (c1 = 3);
check table t1;
select c1 from t1 where c2='\Z\Z\Z\Z';

#
# test delete of keys in a different order
#
truncate table t1;
insert into t1 values(1,"aaaa"),(2,"aaab"),(3,"aaac"),(4,"aaccc");
delete from t1 where c1=3;
delete from t1 where c1=1;
delete from t1 where c1=4;
check table t1;

drop table t1;

#
# Bug#12565 - ERROR 1034 when running simple UPDATE or DELETE 
#             on large MyISAM table
#
create table t1 (
  c1 int,
  c2 varchar(20) not null,
  primary key (c1),
  key (c2(10))
) engine=myisam;
insert into t1 values (1,'');
insert into t1 values (2,' \t\tTest String');
insert into t1 values (3,' \n\tTest String');
update t1 set c2 = 'New Test String' where c1 = 1;
select * from t1;
drop table t1;

#
# Bug #20604: Test for disabled keys with aggregate functions and FORCE INDEX.
#

CREATE TABLE t1( a TINYINT, KEY(a) ) ENGINE=MyISAM;
INSERT INTO t1 VALUES( 1 );
ALTER TABLE t1 DISABLE KEYS;
EXPLAIN SELECT MAX(a) FROM t1 FORCE INDEX(a);

drop table t1;

#
# Bug #31974: Wrong EXPLAIN output
#

CREATE TABLE t1 (a INT, b INT, INDEX (a,b)) engine=myisam;
INSERT INTO t1 (a, b)
   VALUES
     (1,1), (1,2), (1,3), (1,4), (1,5),
     (2,2), (2,3), (2,1), (3,1), (4,1), (4,2), (4,3), (4,4), (4,5), (4,6),
     (5,1), (5,2), (5,3), (5,4), (5,5);
EXPLAIN SELECT 1 FROM t1 AS t1_outer WHERE
  (SELECT max(b) FROM t1 GROUP BY a HAVING a < 2) > 12;
SELECT 1 as RES FROM t1 AS t1_outer WHERE
  (SELECT max(b) FROM t1 GROUP BY a HAVING a < 2) > 12;

DROP TABLE t1;

--echo #
--echo # Bug#18144: Cost with FORCE/USE index seems incorrect in some cases.
--echo #
--echo # We are interested in showing that the cost for the last plan is higher
--echo # than for the preceding two plans.
--echo #
CREATE TABLE t1( a INT, b INT, KEY( a ) ) engine=myisam;
INSERT INTO t1 values (1, 2), (1, 3), (2, 3), (2, 4), (3, 4), (3, 5);

EXPLAIN SELECT a, SUM( b ) FROM t1 GROUP BY a;
--skip_if_hypergraph  # Depends on the query plan.
SHOW STATUS LIKE 'Last_query_cost';

EXPLAIN SELECT a, SUM( b ) FROM t1 USE INDEX( a ) GROUP BY a;
--skip_if_hypergraph  # Depends on the query plan.
SHOW STATUS LIKE 'Last_query_cost';

EXPLAIN SELECT a, SUM( b ) FROM t1 FORCE INDEX( a ) GROUP BY a;
--skip_if_hypergraph  # Depends on the query plan.
SHOW STATUS LIKE 'Last_query_cost';

DROP TABLE t1;

--echo #
--echo # Additional coverage for handling of key algorithm in index definition.
--echo #

--echo #
--echo # 1) Default case. If key algorithm was not specified explicitly and
--echo #    this is normal key (not spatial or fulltext) storage engine
--echo #    default key algorithm is used.
--echo #  
create table tm (k int, index (k)) charset utf8mb4 engine=myisam;
--echo #
--echo # Key algorithm is not shown in SHOW CREATE TABLE output in this case
--echo #
show create table tm;
--echo #
--echo # But visible in I_S.STATISTICS
--echo #
select table_name, index_type from information_schema.statistics
  where table_schema = 'test' and table_name ='tm' order by table_name;
--echo #
--echo # Same applies when keys are added by ALTER TABLE
--echo #
alter table tm add column l int, add index (l);
show create table tm;
select table_name, index_type from information_schema.statistics
  where table_schema = 'test' and table_name = 'tm' and index_name = 'l'
  order by table_name;
drop tables tm;

--echo #
--echo # 2) For SPATIAL and FULLTEXT keys we always use special key
--echo #    algorithms and there is no way to specify it explicitly.
--echo #
create table tm (pk int primary key, p point not null SRID 0, spatial index (p))
charset utf8mb4 engine=myisam;
--echo #
--echo # Key algorithm is not shown in SHOW CREATE TABLE output in this
--echo # case as well.
--echo #
show create table tm;
--echo #
--echo # But visible in I_S.STATISTICS
--echo #
select table_name, index_type from information_schema.statistics
  where table_schema = 'test' and table_name = 'tm' and index_name = 'p'
  order by table_name;
--echo #
--echo # Same applies when keys are added by ALTER TABLE
--echo #
alter table tm add column q point not null SRID 0, add spatial index (q);
select table_name, index_type from information_schema.statistics
  where table_schema = 'test' and table_name = 'tm' and index_name = 'q'
  order by table_name;
drop tables tm;
--echo #
--echo # Same applies to FULLTEXT indexes.
--echo #
create table tm (pk int primary key, v varchar(255), fulltext index (v))
charset utf8mb4 engine=myisam;
show create table tm;
select table_name, index_type from information_schema.statistics
  where table_schema = 'test' and table_name = 'tm' and index_name = 'v'
  order by table_name;
--echo #
--echo # Same applies when keys are added by ALTER TABLE
--echo #
alter table tm add column w varchar(255), add fulltext index (w);
show create table tm;
select table_name, index_type from information_schema.statistics
  where table_schema = 'test' and table_name = 'tm' and index_name = 'w'
  order by table_name;
drop tables tm;

--echo #
--echo # 3) If key algorithm was specified explicitly and it is supported
--echo #    by the storage engine it will be used.
--echo #
create table tm (k int, index using btree (k)) charset utf8mb4 engine=myisam;
--echo #
--echo # In this case it is shown in SHOW CREATE TABLE output
--echo #
show create table tm;
--echo #
--echo # Also visible in I_S.STATISTICS
--echo #
select table_name, index_type from information_schema.statistics
  where table_schema = 'test' and table_name = 'tm' order by table_name;
--echo #
--echo # Same applies when keys are added by ALTER TABLE
--echo #
--echo # Note that the information that algorithm was provided explicitly is
--echo # preserved by ALTER TABLE.
alter table tm add column l int, add index using btree (l);
show create table tm;
select table_name, index_type from information_schema.statistics
  where table_schema = 'test' and table_name = 'tm' and index_name = 'l'
  order by table_name;
drop tables tm;

--echo #
--echo # 7) Key algorithm and ALTER TABLE ALGORITHM=INPLACE
--echo #
--echo # Changing key algorithm from one used by default to the same one
--echo # but specified explicitly should be inplace/fast operation.
create table t1 (k int, index (k)) charset utf8mb4 engine=myisam;
show create table t1;
alter table t1 drop key k, add index using btree (k), algorithm=inplace;
show create table t1;
alter table t1 drop key k, add index (k), algorithm=inplace;
show create table t1;
--echo # Note that below statement doesn't really change algorithm.
alter table t1 drop key k, add index using hash (k), algorithm=inplace;
drop table t1;
--echo # Changing key algorithm to another one can require COPY algorithm.
create table t1 (k int, index (k)) charset utf8mb4 engine=heap;
show create table t1;
--error ER_ALTER_OPERATION_NOT_SUPPORTED
alter table t1 drop key k, add index using btree (k), algorithm=inplace;
show create table t1;
alter table t1 drop key k, add index using btree (k), algorithm=copy;
show create table t1;
drop table t1;
