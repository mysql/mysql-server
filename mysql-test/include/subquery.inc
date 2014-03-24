# Basic subquery tests 
#
# NOTE. Please do not switch connection inside this test.
#       subquery.inc is included from several other test cases which set
#       explicit session properties that must be preserved throughout the test.
#       If you need to use a dedicated connection for a test case,
#       close the new connection and switch back to "default" as soon
#       as possible.
#

# This portion of the file vas developed when subquery materialization
# was rule-based; to preserve the intended test scenarios, we switch
# off cost-based choice for them.
set @old_opt_switch=@@optimizer_switch;
set optimizer_switch='subquery_materialization_cost_based=off';

# Initialise
--disable_warnings
drop table if exists t1,t2,t3,t4,t5,t6,t7,t8,t11,t12;
--enable_warnings
select (select 2);
explain extended select (select 2);
SELECT (SELECT 1) UNION SELECT (SELECT 2);
explain extended SELECT (SELECT 1) UNION SELECT (SELECT 2);
SELECT (SELECT (SELECT 0 UNION SELECT 0));
explain extended SELECT (SELECT (SELECT 0 UNION SELECT 0));
-- error ER_ILLEGAL_REFERENCE
SELECT (SELECT 1 FROM (SELECT 1) as b HAVING a=1) as a;
-- error ER_ILLEGAL_REFERENCE
SELECT (SELECT 1 FROM (SELECT 1) as b HAVING b=1) as a,(SELECT 1 FROM (SELECT 1) as c HAVING a=1) as b;
SELECT (SELECT 1),MAX(1) FROM (SELECT 1) as a;
-- error ER_ILLEGAL_REFERENCE
SELECT (SELECT a) as a;
EXPLAIN EXTENDED SELECT 1 FROM (SELECT 1 as a) as b  HAVING (SELECT a)=1;
SELECT 1 FROM (SELECT 1 as a) as b HAVING (SELECT a)=1;
-- error ER_BAD_FIELD_ERROR
SELECT (SELECT 1), a;
SELECT 1 as a FROM (SELECT 1) as b HAVING (SELECT a)=1;
-- error ER_BAD_FIELD_ERROR
SELECT 1 FROM (SELECT (SELECT a) b) c;
SELECT * FROM (SELECT 1 as id) b WHERE id IN (SELECT * FROM (SELECT 1 as id) c ORDER BY id);
-- error ER_OPERAND_COLUMNS
SELECT * FROM (SELECT 1) a  WHERE 1 IN (SELECT 1,1);
SELECT 1 IN (SELECT 1);
SELECT 1 FROM (SELECT 1 as a) b WHERE 1 IN (SELECT (SELECT a));
-- error ER_WRONG_USAGE
select (SELECT 1 FROM (SELECT 1) a PROCEDURE ANALYSE(1));
-- error ER_PARSE_ERROR
SELECT 1 FROM (SELECT 1) a PROCEDURE ANALYSE((SELECT 1));
-- error ER_BAD_FIELD_ERROR
SELECT (SELECT 1) as a FROM (SELECT 1) b WHERE (SELECT a) IS NULL;
-- error ER_BAD_FIELD_ERROR
SELECT (SELECT 1) as a FROM (SELECT 1) b WHERE (SELECT a) IS NOT NULL;
SELECT (SELECT 1,2,3) = ROW(1,2,3);
SELECT (SELECT 1,2,3) = ROW(1,2,1);
SELECT (SELECT 1,2,3) < ROW(1,2,1);
SELECT (SELECT 1,2,3) > ROW(1,2,1);
SELECT (SELECT 1,2,3) = ROW(1,2,NULL);
SELECT ROW(1,2,3) = (SELECT 1,2,3);
SELECT ROW(1,2,3) = (SELECT 1,2,1);
SELECT ROW(1,2,3) < (SELECT 1,2,1);
SELECT ROW(1,2,3) > (SELECT 1,2,1);
SELECT ROW(1,2,3) = (SELECT 1,2,NULL);
SELECT (SELECT 1.5,2,'a') = ROW(1.5,2,'a');
SELECT (SELECT 1.5,2,'a') = ROW(1.5,2,'b');
SELECT (SELECT 1.5,2,'a') = ROW('1.5b',2,'b');
SELECT (SELECT 'b',2,'a') = ROW(1.5,2,'a');
SELECT (SELECT 1.5,2,'a') = ROW(1.5,'2','a');
SELECT (SELECT 1.5,'c','a') = ROW(1.5,2,'a');

-- error ER_OPERAND_COLUMNS
SELECT (SELECT * FROM (SELECT 'test' a,'test' b) a);

SELECT 1 as a,(SELECT a+a) b,(SELECT b);

create table t1 (a int);
create table t2 (a int, b int);
create table t3 (a int);
create table t4 (a int not null, b int not null);
insert into t1 values (2);
insert into t2 values (1,7),(2,7);
insert into t4 values (4,8),(3,8),(5,9);
-- error ER_ILLEGAL_REFERENCE
select (select a from t1 where t1.a = a1) as a2, (select b from t2 where t2.b=a2) as a1;
select (select a from t1 where t1.a=t2.a), a from t2;
select (select a from t1 where t1.a=t2.b), a from t2;
select (select a from t1), a, (select 1 union select 2 limit 1) from t2;
select (select a from t3), a from t2;
select * from t2 where t2.a=(select a from t1);
insert into t3 values (6),(7),(3);
select * from t2 where t2.b=(select a from t3 order by 1 desc limit 1);
(select * from t2 where t2.b=(select a from t3 order by 1 desc limit 1)) union (select * from t4 order by a limit 2) limit 3;
(select * from t2 where t2.b=(select a from t3 order by 1 desc limit 1)) union (select * from t4 where t4.b=(select max(t2.a)*4 from t2) order by a);
explain extended (select * from t2 where t2.b=(select a from t3 order by 1 desc limit 1)) union (select * from t4 where t4.b=(select max(t2.a)*4 from t2) order by a);
select (select a from t3 where a<t2.a*4 order by 1 desc limit 1), a from t2;
select (select t3.a from t3 where a<8 order by 1 desc limit 1), a from
(select * from t2 where a>1) as tt;
explain extended select (select t3.a from t3 where a<8 order by 1 desc limit 1), a from
(select * from t2 where a>1) as tt;
select * from t1 where t1.a=(select t2.a from t2 where t2.b=(select max(a) from t3) order by 1 desc limit 1);
select * from t1 where t1.a=(select t2.a from t2 where t2.b=(select max(a) from t3 where t3.a > t1.a) order by 1 desc limit 1);
select * from t1 where t1.a=(select t2.a from t2 where t2.b=(select max(a) from t3 where t3.a < t1.a) order by 1 desc limit 1);
select b,(select avg(t2.a+(select min(t3.a) from t3 where t3.a >= t4.a)) from t2) from t4;
explain extended select b,(select avg(t2.a+(select min(t3.a) from t3 where t3.a >= t4.a)) from t2) from t4;
select * from t3 where exists (select * from t2 where t2.b=t3.a);
select * from t3 where not exists (select * from t2 where t2.b=t3.a);
select * from t3 where a in (select b from t2);
select * from t3 where a not in (select b from t2);
select * from t3 where a = some (select b from t2);
select * from t3 where a <> any (select b from t2);

# Rewrite: select * from t3 where not exists (select b from t2 where a <> b);
select * from t3 where a = all (select b from t2);

select * from t3 where a <> all (select b from t2);
insert into t2 values (100, 5);
select * from t3 where a < any (select b from t2);
select * from t3 where a < all (select b from t2);
select * from t3 where a >= any (select b from t2);
explain extended select * from t3 where a >= any (select b from t2);
select * from t3 where a >= all (select b from t2);
delete from t2 where a=100;
-- error ER_OPERAND_COLUMNS
select * from t3 where a in (select a,b from t2);
-- error ER_OPERAND_COLUMNS
select * from t3 where a in (select * from t2);
insert into t4 values (12,7),(1,7),(10,9),(9,6),(7,6),(3,9),(1,10);
# empty set
select b,max(a) as ma from t4 group by b having b < (select max(t2.a) from t2 where t2.b=t4.b);
insert into t2 values (2,10);
select b,max(a) as ma from t4 group by b having ma < (select max(t2.a) from t2 where t2.b=t4.b);
delete from t2 where a=2 and b=10;
select b,max(a) as ma from t4 group by b having b >= (select max(t2.a) from t2 where t2.b=t4.b);
create table t5 (a int);
select (select a from t1 where t1.a=t2.a union select a from t5 where t5.a=t2.a), a from t2;
insert into t5 values (5);
select (select a from t1 where t1.a=t2.a union select a from t5 where t5.a=t2.a), a from t2;
insert into t5 values (2);
select (select a from t1 where t1.a=t2.a union select a from t5 where t5.a=t2.a), a from t2;
explain extended select (select a from t1 where t1.a=t2.a union select a from t5 where t5.a=t2.a), a from t2;
-- error ER_SUBQUERY_NO_1_ROW
select (select a from t1 where t1.a=t2.a union all select a from t5 where t5.a=t2.a), a from t2;
create table t6 (patient_uq int, clinic_uq int, index i1 (clinic_uq));
create table t7( uq int primary key, name char(25));
insert into t7 values(1,"Oblastnaia bolnitsa"),(2,"Bolnitsa Krasnogo Kresta");
insert into t6 values (1,1),(1,2),(2,2),(1,3);
select * from t6 where exists (select * from t7 where uq = clinic_uq);
explain extended select * from t6 where exists (select * from t7 where uq = clinic_uq);

# not unique fields
-- error ER_NON_UNIQ_ERROR
select * from t1 where a= (select a from t2,t4 where t2.b=t4.b);

# different tipes & group functions
drop table t1,t2,t3;

CREATE TABLE t3 (a varchar(20),b char(1) NOT NULL default '0');
INSERT INTO t3 VALUES ('W','a'),('A','c'),('J','b');
CREATE TABLE t2 (a varchar(20),b int NOT NULL default '0');
INSERT INTO t2 VALUES ('W','1'),('A','3'),('J','2');
CREATE TABLE t1 (a varchar(20),b date NOT NULL default '0000-00-00');
INSERT INTO t1 VALUES ('W','1732-02-22'),('A','1735-10-30'),('J','1743-04-13');
SELECT * FROM t1 WHERE b = (SELECT MIN(b) FROM t1);
SELECT * FROM t2 WHERE b = (SELECT MIN(b) FROM t2);
SELECT * FROM t3 WHERE b = (SELECT MIN(b) FROM t3);

CREATE TABLE `t8` (
  `pseudo` varchar(35) character set latin1 NOT NULL default '',
  `email` varchar(60) character set latin1 NOT NULL default '',
  PRIMARY KEY  (`pseudo`),
  UNIQUE KEY `email` (`email`)
) ENGINE=MyISAM CHARSET=latin1 ROW_FORMAT=DYNAMIC;

INSERT INTO t8 (pseudo,email) VALUES ('joce','test');
INSERT INTO t8 (pseudo,email) VALUES ('joce1','test1');
INSERT INTO t8 (pseudo,email) VALUES ('2joce1','2test1');
EXPLAIN EXTENDED SELECT pseudo,(SELECT email FROM t8 WHERE pseudo=(SELECT pseudo FROM t8 WHERE pseudo='joce')) FROM t8 WHERE pseudo=(SELECT pseudo FROM t8 WHERE pseudo='joce');
-- error ER_OPERAND_COLUMNS
SELECT pseudo FROM t8 WHERE pseudo=(SELECT pseudo,email FROM
t8 WHERE pseudo='joce');
-- error ER_OPERAND_COLUMNS
SELECT pseudo FROM t8 WHERE pseudo=(SELECT * FROM t8 WHERE
pseudo='joce');
SELECT pseudo FROM t8 WHERE pseudo=(SELECT pseudo FROM t8 WHERE pseudo='joce');
-- error ER_SUBQUERY_NO_1_ROW
SELECT pseudo FROM t8 WHERE pseudo=(SELECT pseudo FROM t8 WHERE pseudo LIKE '%joce%');

drop table if exists t1,t2,t3,t4,t5,t6,t7,t8;

#searchconthardwarefr3 forumconthardwarefr7
CREATE TABLE `t1` (
  `topic` mediumint(8) unsigned NOT NULL default '0',
  `date` date NOT NULL default '0000-00-00',
  `pseudo` varchar(35) character set latin1 NOT NULL default '',
  PRIMARY KEY  (`pseudo`,`date`,`topic`),
  KEY `topic` (`topic`)
) ENGINE=MyISAM ROW_FORMAT=DYNAMIC;
INSERT INTO t1 (topic,date,pseudo) VALUES
('43506','2002-10-02','joce'),('40143','2002-08-03','joce');
EXPLAIN EXTENDED SELECT DISTINCT date FROM t1 WHERE date='2002-08-03';
EXPLAIN EXTENDED SELECT (SELECT DISTINCT date FROM t1 WHERE date='2002-08-03');
SELECT DISTINCT date FROM t1 WHERE date='2002-08-03';
SELECT (SELECT DISTINCT date FROM t1 WHERE date='2002-08-03');
SELECT 1 FROM t1 WHERE 1=(SELECT 1 UNION SELECT 1) UNION ALL SELECT 1;
-- error ER_SUBQUERY_NO_1_ROW
SELECT 1 FROM t1 WHERE 1=(SELECT 1 UNION ALL SELECT 1) UNION SELECT 1;
EXPLAIN EXTENDED SELECT 1 FROM t1 WHERE 1=(SELECT 1 UNION SELECT 1);
drop table t1;

#forumconthardwarefr7 searchconthardwarefr7
CREATE TABLE `t1` (
  `numeropost` mediumint(8) unsigned NOT NULL auto_increment,
  `maxnumrep` int(10) unsigned NOT NULL default '0',
  PRIMARY KEY  (`numeropost`),
  UNIQUE KEY `maxnumrep` (`maxnumrep`)
) ENGINE=MyISAM ROW_FORMAT=FIXED;

INSERT INTO t1 (numeropost,maxnumrep) VALUES (40143,1),(43506,2);

CREATE TABLE `t2` (
      `mot` varchar(30) NOT NULL default '',
      `topic` mediumint(8) unsigned NOT NULL default '0',
      `date` date NOT NULL default '0000-00-00',
      `pseudo` varchar(35) NOT NULL default '',
       PRIMARY KEY  (`mot`,`pseudo`,`date`,`topic`)
    ) ENGINE=MyISAM ROW_FORMAT=DYNAMIC;

INSERT INTO t2 (mot,topic,date,pseudo) VALUES ('joce','40143','2002-10-22','joce'), ('joce','43506','2002-10-22','joce');
select numeropost as a FROM t1 GROUP BY (SELECT 1 FROM t1 HAVING a=1);
SELECT numeropost,maxnumrep FROM t1 WHERE exists (SELECT 1 FROM t2 WHERE (mot='joce') AND date >= '2002-10-21' AND t1.numeropost = t2.topic) ORDER BY maxnumrep DESC LIMIT 0, 20;
-- error ER_BAD_FIELD_ERROR
SELECT (SELECT 1) as a FROM (SELECT 1 FROM t1 HAVING a=1) b;
-- error ER_BAD_FIELD_ERROR
SELECT 1 IN (SELECT 1 FROM t2 HAVING a);

SELECT * from t2 where topic IN (SELECT topic FROM t2 GROUP BY topic);
SELECT * from t2 where topic IN (SELECT topic FROM t2 GROUP BY topic HAVING topic < 4100);
SELECT * from t2 where topic IN (SELECT SUM(topic) FROM t1);
SELECT * from t2 where topic = any (SELECT topic FROM t2 GROUP BY topic);
SELECT * from t2 where topic = any (SELECT topic FROM t2 GROUP BY topic HAVING topic < 4100);
SELECT * from t2 where topic = any (SELECT SUM(topic) FROM t1);
SELECT * from t2 where topic = all (SELECT topic FROM t2 GROUP BY topic);
SELECT * from t2 where topic = all (SELECT topic FROM t2 GROUP BY topic HAVING topic < 4100);
SELECT *, topic = all (SELECT topic FROM t2 GROUP BY topic HAVING topic < 4100) from t2;
SELECT * from t2 where topic = all (SELECT SUM(topic) FROM t2);
SELECT * from t2 where topic <> any (SELECT SUM(topic) FROM t2);
SELECT * from t2 where topic IN (SELECT topic FROM t2 GROUP BY topic HAVING topic < 41000);
SELECT * from t2 where topic = any (SELECT topic FROM t2 GROUP BY topic HAVING topic < 41000);
SELECT * from t2 where topic = all (SELECT topic FROM t2 GROUP BY topic HAVING topic < 41000);
SELECT *, topic = all (SELECT topic FROM t2 GROUP BY topic HAVING topic < 41000) from t2;
drop table t1,t2;

#forumconthardwarefr7
CREATE TABLE `t1` (
  `numeropost` mediumint(8) unsigned NOT NULL auto_increment,
  `maxnumrep` int(10) unsigned NOT NULL default '0',
  PRIMARY KEY  (`numeropost`),
  UNIQUE KEY `maxnumrep` (`maxnumrep`)
) ENGINE=MyISAM ROW_FORMAT=FIXED;

INSERT INTO t1 (numeropost,maxnumrep) VALUES (1,0),(2,1);
-- error ER_SUBQUERY_NO_1_ROW
select numeropost as a FROM t1 GROUP BY (SELECT 1 FROM t1 HAVING a=1);
-- error ER_SUBQUERY_NO_1_ROW
select numeropost as a FROM t1 ORDER BY (SELECT 1 FROM t1 HAVING a=1);
drop table t1;

create table t1 (a int);
insert into t1 values (1),(2),(3);
(select * from t1) union (select * from t1) order by (select a from t1 limit 1);
drop table t1;

#iftest
CREATE TABLE t1 (field char(1) NOT NULL DEFAULT 'b');
INSERT INTO t1 VALUES ();
-- error ER_SUBQUERY_NO_1_ROW
SELECT field FROM t1 WHERE 1=(SELECT 1 UNION ALL SELECT 1 FROM (SELECT 1) a HAVING field='b');
drop table t1;

# threadhardwarefr7
CREATE TABLE `t1` (
  `numeropost` mediumint(8) unsigned NOT NULL default '0',
  `numreponse` int(10) unsigned NOT NULL auto_increment,
  `pseudo` varchar(35) NOT NULL default '',
  PRIMARY KEY  (`numeropost`,`numreponse`),
  UNIQUE KEY `numreponse` (`numreponse`),
  KEY `pseudo` (`pseudo`,`numeropost`)
) ENGINE=MyISAM;
-- error ER_ILLEGAL_REFERENCE
SELECT (SELECT numeropost FROM t1 HAVING numreponse=a),numreponse FROM (SELECT * FROM t1) as a;
-- error ER_BAD_FIELD_ERROR
SELECT numreponse, (SELECT numeropost FROM t1 HAVING numreponse=a) FROM (SELECT * FROM t1) as a;
SELECT numreponse, (SELECT numeropost FROM t1 HAVING numreponse=1) FROM (SELECT * FROM t1) as a;
INSERT INTO t1 (numeropost,numreponse,pseudo) VALUES (1,1,'joce'),(1,2,'joce'),(1,3,'test');
-- error ER_SUBQUERY_NO_1_ROW
EXPLAIN EXTENDED SELECT numreponse FROM t1 WHERE numeropost='1' AND numreponse=(SELECT 1 FROM t1 WHERE numeropost='1');
EXPLAIN EXTENDED SELECT MAX(numreponse) FROM t1 WHERE numeropost='1';
EXPLAIN EXTENDED SELECT numreponse FROM t1 WHERE numeropost='1' AND numreponse=(SELECT MAX(numreponse) FROM t1 WHERE numeropost='1');
drop table t1;

CREATE TABLE t1 (a int(1));
INSERT INTO t1 VALUES (1);
SELECT 1 FROM (SELECT a FROM t1) b HAVING (SELECT b.a)=1;
drop table t1;

#update with subselects
create table t1 (a int NOT NULL, b int, primary key (a));
create table t2 (a int NOT NULL, b int, primary key (a));
insert into t1 values (0, 10),(1, 11),(2, 12);
insert into t2 values (1, 21),(2, 22),(3, 23);
select * from t1;
-- error ER_UPDATE_TABLE_USED
update t1 set b= (select b from t1);
-- error ER_SUBQUERY_NO_1_ROW
update t1 set b= (select b from t2);
update t1 set b= (select b from t2 where t1.a = t2.a);
select * from t1;
drop table t1, t2;

#delete with subselects
create table t1 (a int NOT NULL, b int, primary key (a));
create table t2 (a int NOT NULL, b int, primary key (a));
insert into t1 values (0, 10),(1, 11),(2, 12);
insert into t2 values (1, 21),(2, 12),(3, 23);
select * from t1;
select * from t1 where b = (select b from t2 where t1.a = t2.a);
-- error ER_UPDATE_TABLE_USED
delete from t1 where b = (select b from t1);
-- error ER_SUBQUERY_NO_1_ROW
delete from t1 where b = (select b from t2);
delete from t1 where b = (select b from t2 where t1.a = t2.a);
select * from t1;
drop table t1, t2;

#multi-delete with subselects

create table t11 (a int NOT NULL, b int, primary key (a));
create table t12 (a int NOT NULL, b int, primary key (a));
create table t2 (a int NOT NULL, b int, primary key (a));
insert into t11 values (0, 10),(1, 11),(2, 12);
insert into t12 values (33, 10),(22, 11),(2, 12);
insert into t2 values (1, 21),(2, 12),(3, 23);
select * from t11;
select * from t12;
-- error ER_UPDATE_TABLE_USED
delete t11.*, t12.* from t11,t12 where t11.a = t12.a and t11.b = (select b from t12 where t11.a = t12.a);
-- error ER_SUBQUERY_NO_1_ROW
delete t11.*, t12.* from t11,t12 where t11.a = t12.a and t11.b = (select b from t2);
delete t11.*, t12.* from t11,t12 where t11.a = t12.a and t11.b = (select b from t2 where t11.a = t2.a);
select * from t11;
select * from t12;
drop table t11, t12, t2;

#insert with subselects
CREATE TABLE t1 (x int);
create table t2 (a int);
create table t3 (b int);
insert into t2 values (1);
insert into t3 values (1),(2);
-- error ER_UPDATE_TABLE_USED
INSERT INTO t1 (x) VALUES ((SELECT x FROM t1));
-- error ER_SUBQUERY_NO_1_ROW
INSERT INTO t1 (x) VALUES ((SELECT b FROM t3));
INSERT INTO t1 (x) VALUES ((SELECT a FROM t2));
select * from t1;
insert into t2 values (1);
let $row_count_before= `SELECT COUNT(*) FROM t1`;
INSERT DELAYED INTO t1 (x) VALUES ((SELECT SUM(a) FROM t2));
let $wait_condition= SELECT COUNT(*) <> $row_count_before FROM t1;
--source include/wait_condition.inc
select * from t1;
INSERT INTO t1 (x) select (SELECT SUM(a)+1 FROM t2) FROM t2;
select * from t1;
# After this, only data based on old t1 records should have been added.
INSERT INTO t1 (x) select (SELECT SUM(x)+2 FROM t1) FROM t2;
select * from t1;
-- error ER_BAD_FIELD_ERROR
INSERT DELAYED INTO t1 (x) VALUES ((SELECT SUM(x) FROM t2));
let $row_count_before= `SELECT COUNT(*) FROM t1`;
INSERT DELAYED INTO t1 (x) VALUES ((SELECT SUM(a) FROM t2));
let $wait_condition= SELECT COUNT(*) <> $row_count_before FROM t1;
--source include/wait_condition.inc
select * from t1;
#
#TODO: should be uncommented after Bug#380 fix pushed
#INSERT INTO t1 (x) SELECT (SELECT SUM(a)+b FROM t2) from t3;
#select * from t1;
drop table t1, t2, t3;

#replace with subselects
CREATE TABLE t1 (x int not null, y int, primary key (x));
create table t2 (a int);
create table t3 (a int);
insert into t2 values (1);
insert into t3 values (1),(2);
select * from t1;
-- error ER_UPDATE_TABLE_USED
replace into t1 (x, y) VALUES ((SELECT x FROM t1), (SELECT a+1 FROM t2));
-- error ER_SUBQUERY_NO_1_ROW
replace into t1 (x, y) VALUES ((SELECT a FROM t3), (SELECT a+1 FROM t2));
replace into t1 (x, y) VALUES ((SELECT a FROM t2), (SELECT a+1 FROM t2));
select * from t1;
replace into t1 (x, y) VALUES ((SELECT a FROM t2), (SELECT a+2 FROM t2));
select * from t1;
let $row_count_before= `SELECT COUNT(*) FROM t1`;
replace DELAYED into t1 (x, y) VALUES ((SELECT a+3 FROM t2), (SELECT a FROM t2));
# We get one additional row
let $wait_condition= SELECT COUNT(*) <> $row_count_before FROM t1;
--source include/wait_condition.inc
select * from t1;
let $row_count_before= `SELECT COUNT(*) FROM t1 WHERE y = 2`;
replace DELAYED into t1 (x, y) VALUES ((SELECT a+3 FROM t2), (SELECT a+1 FROM t2));
let $wait_condition= SELECT COUNT(*) <> $row_count_before FROM t1 WHERE y = 2;
--source include/wait_condition.inc
select * from t1;
replace LOW_PRIORITY into t1 (x, y) VALUES ((SELECT a+1 FROM t2), (SELECT a FROM t2));
select * from t1;
drop table t1, t2, t3;

-- error ER_NO_TABLES_USED
SELECT * FROM (SELECT 1) b WHERE 1 IN (SELECT *);

CREATE TABLE t2 (id int(11) default NULL, KEY id (id)) ENGINE=MyISAM CHARSET=latin1;
INSERT INTO t2 VALUES (1),(2);
SELECT * FROM t2 WHERE id IN (SELECT 1);
EXPLAIN EXTENDED SELECT * FROM t2 WHERE id IN (SELECT 1);
SELECT * FROM t2 WHERE id IN (SELECT 1 UNION SELECT 3);
SELECT * FROM t2 WHERE id IN (SELECT 1+(select 1));
EXPLAIN EXTENDED SELECT * FROM t2 WHERE id IN (SELECT 1+(select 1));
EXPLAIN EXTENDED SELECT * FROM t2 WHERE id IN (SELECT 1 UNION SELECT 3);
SELECT * FROM t2 WHERE id IN (SELECT 5 UNION SELECT 3);
SELECT * FROM t2 WHERE id IN (SELECT 5 UNION SELECT 2);
-- error ER_UPDATE_TABLE_USED
INSERT INTO t2 VALUES ((SELECT * FROM t2));
-- error ER_UPDATE_TABLE_USED
INSERT INTO t2 VALUES ((SELECT id FROM t2));
SELECT * FROM t2;
CREATE TABLE t1 (id int(11) default NULL, KEY id (id)) ENGINE=MyISAM CHARSET=latin1;
INSERT INTO t1 values (1),(1);
-- error ER_SUBQUERY_NO_1_ROW
UPDATE t2 SET id=(SELECT * FROM t1);
drop table t2, t1;

#NULL test
create table t1 (a int);
insert into t1 values (1),(2),(3);
select 1 IN (SELECT * from t1);
select 10 IN (SELECT * from t1);
select NULL IN (SELECT * from t1);
update t1 set a=NULL where a=2;
select 1 IN (SELECT * from t1);
select 3 IN (SELECT * from t1);
select 10 IN (SELECT * from t1);
select 1 > ALL (SELECT * from t1);
select 10 > ALL (SELECT * from t1);
select 1 > ANY (SELECT * from t1);
select 10 > ANY (SELECT * from t1);
drop table t1;
create table t1 (a varchar(20));
insert into t1 values ('A'),('BC'),('DEF');
select 'A' IN (SELECT * from t1);
select 'XYZS' IN (SELECT * from t1);
select NULL IN (SELECT * from t1);
update t1 set a=NULL where a='BC';
select 'A' IN (SELECT * from t1);
select 'DEF' IN (SELECT * from t1);
select 'XYZS' IN (SELECT * from t1);
select 'A' > ALL (SELECT * from t1);
select 'XYZS' > ALL (SELECT * from t1);
select 'A' > ANY (SELECT * from t1);
select 'XYZS' > ANY (SELECT * from t1);
drop table t1;
create table t1 (a float);
insert into t1 values (1.5),(2.5),(3.5);
select 1.5 IN (SELECT * from t1);
select 10.5 IN (SELECT * from t1);
select NULL IN (SELECT * from t1);
update t1 set a=NULL where a=2.5;
select 1.5 IN (SELECT * from t1);
select 3.5 IN (SELECT * from t1);
select 10.5 IN (SELECT * from t1);
select 1.5 > ALL (SELECT * from t1);
select 10.5 > ALL (SELECT * from t1);
select 1.5 > ANY (SELECT * from t1);
select 10.5 > ANY (SELECT * from t1);
explain extended select (select a+1) from t1;
select (select a+1) from t1;
drop table t1;

#
# Null with keys
#

CREATE TABLE t1 (a int(11) NOT NULL default '0', PRIMARY KEY  (a));
CREATE TABLE t2 (a int(11) default '0', INDEX (a));
INSERT INTO t1 VALUES (1),(2),(3),(4);
INSERT INTO t2 VALUES (1),(2),(3);
SELECT t1.a, t1.a in (select t2.a from t2) FROM t1;
explain extended SELECT t1.a, t1.a in (select t2.a from t2) FROM t1;
CREATE TABLE t3 (a int(11) default '0');
INSERT INTO t3 VALUES (1),(2),(3);
SELECT t1.a, t1.a in (select t2.a from t2,t3 where t3.a=t2.a) FROM t1;
explain extended SELECT t1.a, t1.a in (select t2.a from t2,t3 where t3.a=t2.a) FROM t1;
drop table t1,t2,t3;

#LIMIT is not supported now
create table t1 (a float);
-- error ER_NOT_SUPPORTED_YET
select 10.5 IN (SELECT * from t1 LIMIT 1);
-- error ER_NOT_SUPPORTED_YET
select 10.5 IN (SELECT * from t1 LIMIT 1 UNION SELECT 1.5);
drop table t1;

create table t1 (a int, b int, c varchar(10));
create table t2 (a int);
insert into t1 values (1,2,'a'),(2,3,'b'),(3,4,'c');
insert into t2 values (1),(2),(NULL);
select a, (select a,b,c from t1 where t1.a=t2.a) = ROW(a,2,'a'),(select c from t1 where a=t2.a)  from t2;
select a, (select a,b,c from t1 where t1.a=t2.a) = ROW(a,3,'b'),(select c from t1 where a=t2.a) from t2;
select a, (select a,b,c from t1 where t1.a=t2.a) = ROW(a,4,'c'),(select c from t1 where a=t2.a) from t2;
drop table t1,t2;

create table t1 (a int, b real, c varchar(10));
insert into t1 values (1, 1, 'a'), (2,2,'b'), (NULL, 2, 'b');
select ROW(1, 1, 'a') IN (select a,b,c from t1);
select ROW(1, 2, 'a') IN (select a,b,c from t1);
select ROW(1, 1, 'a') IN (select b,a,c from t1);
select ROW(1, 1, 'a') IN (select a,b,c from t1 where a is not null);
select ROW(1, 2, 'a') IN (select a,b,c from t1 where a is not null);
select ROW(1, 1, 'a') IN (select b,a,c from t1 where a is not null);
select ROW(1, 1, 'a') IN (select a,b,c from t1 where c='b' or c='a');
select ROW(1, 2, 'a') IN (select a,b,c from t1 where c='b' or c='a');
select ROW(1, 1, 'a') IN (select b,a,c from t1 where c='b' or c='a');
-- error ER_NOT_SUPPORTED_YET
select ROW(1, 1, 'a') IN (select b,a,c from t1 limit 2);
drop table t1;

#
# DO & SET
#
create table t1 (a int);
insert into t1 values (1);
do @a:=(SELECT a from t1);
select @a;
set @a:=2;
set @a:=(SELECT a from t1);
select @a;
drop table t1;
-- error ER_NO_SUCH_TABLE
do (SELECT a from t1);
-- error ER_NO_SUCH_TABLE
set @a:=(SELECT a from t1);

CREATE TABLE t1 (a int, KEY(a));
HANDLER t1 OPEN;
-- error ER_PARSE_ERROR
HANDLER t1 READ a=((SELECT 1));
HANDLER t1 CLOSE;
drop table t1;

create table t1 (a int);
create table t2 (b int);
insert into t1 values (1),(2);
insert into t2 values (1);
select a from t1 where a in (select a from t1 where a in (select b from t2));
drop table t1, t2;

create table t1 (a int, b int);
create table t2 like t1;
insert into t1 values (1,2),(1,3),(1,4),(1,5);
insert into t2 values (1,2),(1,3);
select * from t1 where row(a,b) in (select a,b from t2);
drop table t1, t2;

CREATE TABLE `t1` (`i` int(11) NOT NULL default '0',PRIMARY KEY  (`i`)) ENGINE=MyISAM CHARSET=latin1;
INSERT INTO t1 VALUES (1);
UPDATE t1 SET i=i+1 WHERE i=(SELECT MAX(i));
select * from t1;
drop table t1;

#test of uncacheable subqueries
CREATE TABLE t1 (a int(1));
EXPLAIN EXTENDED SELECT (SELECT RAND() FROM t1) FROM t1;
EXPLAIN EXTENDED SELECT (SELECT ENCRYPT('test') FROM t1) FROM t1;
EXPLAIN EXTENDED SELECT (SELECT BENCHMARK(1,1) FROM t1) FROM t1;
drop table t1;


CREATE TABLE `t1` (
  `mot` varchar(30) character set latin1 NOT NULL default '',
  `topic` mediumint(8) unsigned NOT NULL default '0',
  `date` date NOT NULL default '0000-00-00',
  `pseudo` varchar(35) character set latin1 NOT NULL default '',
  PRIMARY KEY  (`mot`,`pseudo`,`date`,`topic`),
  KEY `pseudo` (`pseudo`,`date`,`topic`),
  KEY `topic` (`topic`)
) ENGINE=MyISAM CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE `t2` (
  `mot` varchar(30) character set latin1 NOT NULL default '',
  `topic` mediumint(8) unsigned NOT NULL default '0',
  `date` date NOT NULL default '0000-00-00',
  `pseudo` varchar(35) character set latin1 NOT NULL default '',
  PRIMARY KEY  (`mot`,`pseudo`,`date`,`topic`),
  KEY `pseudo` (`pseudo`,`date`,`topic`),
  KEY `topic` (`topic`)
) ENGINE=MyISAM CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE `t3` (
  `numeropost` mediumint(8) unsigned NOT NULL auto_increment,
  `maxnumrep` int(10) unsigned NOT NULL default '0',
  PRIMARY KEY  (`numeropost`),
  UNIQUE KEY `maxnumrep` (`maxnumrep`)
) ENGINE=MyISAM CHARSET=latin1;
INSERT INTO t1 VALUES ('joce','1','','joce'),('test','2','','test');

INSERT INTO t2 VALUES ('joce','1','','joce'),('test','2','','test');

INSERT INTO t3 VALUES (1,1);

SELECT DISTINCT topic FROM t2 WHERE NOT EXISTS(SELECT * FROM t3 WHERE
numeropost=topic);
select * from t1;
DELETE FROM t1 WHERE topic IN (SELECT DISTINCT topic FROM t2 WHERE NOT
EXISTS(SELECT * FROM t3 WHERE numeropost=topic));
select * from t1;

drop table t1, t2, t3;

SELECT * FROM (SELECT 1 as a,(SELECT a)) a;
CREATE TABLE t1 SELECT * FROM (SELECT 1 as a,(SELECT 1)) a;
SHOW CREATE TABLE t1;
drop table t1;
CREATE TABLE t1 SELECT * FROM (SELECT 1 as a,(SELECT a)) a;
SHOW CREATE TABLE t1;
drop table t1;
CREATE TABLE t1 SELECT * FROM (SELECT 1 as a,(SELECT a+0)) a;
SHOW CREATE TABLE t1;
drop table t1;
CREATE TABLE t1 SELECT (SELECT 1 as a UNION SELECT 1+1 limit 1,1) as a;
select * from t1;
SHOW CREATE TABLE t1;
drop table t1;

create table t1 (a int);
insert into t1 values (1), (2), (3);
explain extended select a,(select (select rand() from t1 limit 1)  from t1 limit 1)
from t1;
drop table t1;

#
# error in IN
#
-- error ER_NO_SUCH_TABLE
select t1.Continent, t2.Name, t2.Population from t1 LEFT JOIN t2 ON t1.Code = t2.Country  where t2.Population IN (select max(t2.Population) AS Population from t2, t1 where t2.Country = t1.Code group by Continent);

#
# complex subquery
#

CREATE TABLE t1 (
  ID int(11) NOT NULL auto_increment,
  name char(35) NOT NULL default '',
  t2 char(3) NOT NULL default '',
  District char(20) NOT NULL default '',
  Population int(11) NOT NULL default '0',
  PRIMARY KEY  (ID)
) ENGINE=MyISAM;

INSERT INTO t1 VALUES (130,'Sydney','AUS','New South Wales',3276207);
INSERT INTO t1 VALUES (131,'Melbourne','AUS','Victoria',2865329);
INSERT INTO t1 VALUES (132,'Brisbane','AUS','Queensland',1291117);

CREATE TABLE t2 (
  Code char(3) NOT NULL default '',
  Name char(52) NOT NULL default '',
  Continent enum('Asia','Europe','North America','Africa','Oceania','Antarctica','South America') NOT NULL default 'Asia',
  Region char(26) NOT NULL default '',
  SurfaceArea float(10,2) NOT NULL default '0.00',
  IndepYear smallint(6) default NULL,
  Population int(11) NOT NULL default '0',
  LifeExpectancy float(3,1) default NULL,
  GNP float(10,2) default NULL,
  GNPOld float(10,2) default NULL,
  LocalName char(45) NOT NULL default '',
  GovernmentForm char(45) NOT NULL default '',
  HeadOfState char(60) default NULL,
  Capital int(11) default NULL,
  Code2 char(2) NOT NULL default '',
  PRIMARY KEY  (Code)
) ENGINE=MyISAM;

INSERT INTO t2 VALUES ('AUS','Australia','Oceania','Australia and New Zealand',7741220.00,1901,18886000,79.8,351182.00,392911.00,'Australia','Constitutional Monarchy, Federation','Elisabeth II',135,'AU');
INSERT INTO t2 VALUES ('AZE','Azerbaijan','Asia','Middle East',86600.00,1991,7734000,62.9,4127.00,4100.00,'Azärbaycan','Federal Republic','Heydär Äliyev',144,'AZ');

select t2.Continent, t1.Name, t1.Population from t2 LEFT JOIN t1 ON t2.Code = t1.t2  where t1.Population IN (select max(t1.Population) AS Population from t1, t2 where t1.t2 = t2.Code group by Continent);

drop table t1, t2;

#
# constants in IN
#
CREATE TABLE `t1` (
  `id` mediumint(8) unsigned NOT NULL auto_increment,
  `pseudo` varchar(35) character set latin1 NOT NULL default '',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `pseudo` (`pseudo`)
) ENGINE=MyISAM PACK_KEYS=1 ROW_FORMAT=DYNAMIC;
INSERT INTO t1 (pseudo) VALUES ('test');
SELECT 0 IN (SELECT 1 FROM t1 a);
EXPLAIN EXTENDED SELECT 0 IN (SELECT 1 FROM t1 a);
INSERT INTO t1 (pseudo) VALUES ('test1');
SELECT 0 IN (SELECT 1 FROM t1 a);
EXPLAIN EXTENDED SELECT 0 IN (SELECT 1 FROM t1 a);
drop table t1;

CREATE TABLE `t1` (
  `i` int(11) NOT NULL default '0',
  PRIMARY KEY  (`i`)
) ENGINE=MyISAM CHARSET=latin1;

INSERT INTO t1 VALUES (1);
UPDATE t1 SET i=i+(SELECT MAX(i) FROM (SELECT 1) t) WHERE i=(SELECT MAX(i));
UPDATE t1 SET i=i+1 WHERE i=(SELECT MAX(i));
-- error ER_BAD_FIELD_ERROR
UPDATE t1 SET t.i=i+(SELECT MAX(i) FROM (SELECT 1) t);
select * from t1;
drop table t1;

#
# Multi update test
#
CREATE TABLE t1 (
  id int(11) default NULL
) ENGINE=MyISAM CHARSET=latin1;
INSERT INTO t1 VALUES (1),(1),(2),(2),(1),(3);
CREATE TABLE t2 (
  id int(11) default NULL,
  name varchar(15) default NULL
) ENGINE=MyISAM CHARSET=latin1;

INSERT INTO t2 VALUES (4,'vita'), (1,'vita'), (2,'vita'), (1,'vita');
update t1, t2 set t2.name='lenka' where t2.id in (select id from t1);
select * from t2;
drop table t1,t2;

#
# correct NULL in <CONSTANT> IN (SELECT ...)
#
create table t1 (a int, unique index indexa (a));
insert into t1 values (-1), (-4), (-2), (NULL);
select -10 IN (select a from t1 FORCE INDEX (indexa));
drop table t1;

#
# Test optimization for sub selects
#
create table t1 (id int not null auto_increment primary key, salary int, key(salary));
insert into t1 (salary) values (100),(1000),(10000),(10),(500),(5000),(50000);
explain extended SELECT id FROM t1 where salary = (SELECT MAX(salary) FROM t1);
drop table t1;

CREATE TABLE t1 (
  ID int(10) unsigned NOT NULL auto_increment,
  SUB_ID int(3) unsigned NOT NULL default '0',
  REF_ID int(10) unsigned default NULL,
  REF_SUB int(3) unsigned default '0',
  PRIMARY KEY (ID,SUB_ID),
  UNIQUE KEY t1_PK (ID,SUB_ID),
  KEY t1_FK (REF_ID,REF_SUB),
  KEY t1_REFID (REF_ID)
) ENGINE=MyISAM CHARSET=cp1251;
INSERT INTO t1 VALUES (1,0,NULL,NULL),(2,0,NULL,NULL);
SELECT DISTINCT REF_ID FROM t1 WHERE ID= (SELECT DISTINCT REF_ID FROM t1 WHERE ID=2);
DROP TABLE t1;

#
# uninterruptable update
#
create table t1 (a int, b int);
create table t2 (a int, b int);

insert into t1 values (1,0), (2,0), (3,0);
insert into t2 values (1,1), (2,1), (3,1), (2,2);

update ignore t1 set b=(select b from t2 where t1.a=t2.a);
select * from t1;

drop table t1, t2;

#
# reduced subselect in ORDER BY & GROUP BY clauses
#

CREATE TABLE `t1` (
  `id` mediumint(8) unsigned NOT NULL auto_increment,
  `pseudo` varchar(35) NOT NULL default '',
  `email` varchar(60) NOT NULL default '',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `email` (`email`),
  UNIQUE KEY `pseudo` (`pseudo`)
) ENGINE=MyISAM CHARSET=latin1 PACK_KEYS=1 ROW_FORMAT=DYNAMIC;
INSERT INTO t1 (id,pseudo,email) VALUES (1,'test','test'),(2,'test1','test1');
SELECT pseudo as a, pseudo as b FROM t1 GROUP BY (SELECT a) ORDER BY (SELECT id*1);
drop table if exists t1;

(SELECT 1 as a) UNION (SELECT 1) ORDER BY (SELECT a+0);

#
# alloc_group_fields() working
#
create table t1 (a int, b int);
create table t2 (a int, b int);
create table t3 (a int, b int);
insert into t1 values (0,100),(1,2), (1,3), (2,2), (2,7), (2,-1), (3,10);
insert into t2 values (0,0), (1,1), (2,1), (3,1), (4,1);
insert into t3 values (3,3), (2,2), (1,1);
select a,(select count(distinct t1.b) as sum from t1,t2 where t1.a=t2.a and t2.b > 0 and t1.a <= t3.b group by t1.a order by sum limit 1) from t3;
drop table t1,t2,t3;

#
# aggregate functions in HAVING test
#
create table t1 (s1 int);
create table t2 (s1 int);
insert into t1 values (1);
insert into t2 values (1);
select * from t1 where exists (select s1 from t2 having max(t2.s1)=t1.s1);
drop table t1,t2;

#
# update subquery with wrong field (to force name resolving
# in UPDATE name space)
#
create table t1 (s1 int);
create table t2 (s1 int);
insert into t1 values (1);
insert into t2 values (1);
-- error ER_BAD_FIELD_ERROR
update t1 set  s1 = s1 + 1 where 1 = (select x.s1 as A from t2 WHERE t2.s1 > t1.s1 order by A);
DROP TABLE t1, t2;

#
# collation test
#
CREATE TABLE t1 (s1 CHAR(5) COLLATE latin1_german1_ci,
                 s2 CHAR(5) COLLATE latin1_swedish_ci);
INSERT INTO t1 VALUES ('z','?');
-- error ER_CANT_AGGREGATE_2COLLATIONS
select * from t1 where s1 > (select max(s2) from t1);
-- error ER_CANT_AGGREGATE_2COLLATIONS
select * from t1 where s1 > any (select max(s2) from t1);
drop table t1;

#
# aggregate functions reinitialization
#
create table t1(toid int,rd int);
create table t2(userid int,pmnew int,pmtotal int);
insert into t2 values(1,0,0),(2,0,0);
insert into t1 values(1,0),(1,0),(1,0),(1,12),(1,15),(1,123),(1,12312),(1,12312),(1,123),(2,0),(2,0),(2,1),(2,2);
select userid,pmtotal,pmnew, (select count(rd) from t1 where toid=t2.userid) calc_total, (select count(rd) from t1 where rd=0 and toid=t2.userid) calc_new from t2 where userid in (select distinct toid from t1);
drop table t1, t2;

#
# row union
#
create table t1 (s1 char(5));
-- error ER_OPERAND_COLUMNS
select (select 'a','b' from t1 union select 'a','b' from t1) from t1;
insert into t1 values ('tttt');
select * from t1 where ('a','b')=(select 'a','b' from t1 union select 'a','b' from t1);
explain extended (select * from t1);
(select * from t1);
drop table t1;

#
# IN optimisation test results
#
create table t1 (s1 char(5), index s1(s1));
create table t2 (s1 char(5), index s1(s1));
insert into t1 values ('a1'),('a2'),('a3');
insert into t2 values ('a1'),('a2');
select s1, s1 NOT IN (SELECT s1 FROM t2) from t1;
select s1, s1 = ANY (SELECT s1 FROM t2) from t1;
select s1, s1 <> ALL (SELECT s1 FROM t2) from t1;
select s1, s1 NOT IN (SELECT s1 FROM t2 WHERE s1 < 'a2') from t1;
explain extended select s1, s1 NOT IN (SELECT s1 FROM t2) from t1;
explain extended select s1, s1 = ANY (SELECT s1 FROM t2) from t1;
explain extended select s1, s1 <> ALL (SELECT s1 FROM t2) from t1;
explain extended select s1, s1 NOT IN (SELECT s1 FROM t2 WHERE s1 < 'a2') from t1;
drop table t1,t2;

#
# correct ALL optimisation
#
create table t2 (a int, b int);
create table t3 (a int);
insert into t3 values (6),(7),(3);
select * from t3 where a >= all (select b from t2);
explain extended select * from t3 where a >= all (select b from t2);
select * from t3 where a >= some (select b from t2);
explain extended select * from t3 where a >= some (select b from t2);
select * from t3 where a >= all (select b from t2 group by 1);
explain extended select * from t3 where a >= all (select b from t2 group by 1);
select * from t3 where a >= some (select b from t2 group by 1);
explain extended select * from t3 where a >= some (select b from t2 group by 1);
select * from t3 where NULL >= any (select b from t2);
explain extended select * from t3 where NULL >= any (select b from t2);
select * from t3 where NULL >= any (select b from t2 group by 1);
explain extended select * from t3 where NULL >= any (select b from t2 group by 1);
select * from t3 where NULL >= some (select b from t2);
explain extended select * from t3 where NULL >= some (select b from t2);
select * from t3 where NULL >= some (select b from t2 group by 1);
explain extended select * from t3 where NULL >= some (select b from t2 group by 1);
#
# optimized static ALL/ANY with grouping
#
insert into t2 values (2,2), (2,1), (3,3), (3,1);
select * from t3 where a > all (select max(b) from t2 group by a);
explain extended select * from t3 where a > all (select max(b) from t2 group by a);
drop table t2, t3;

#
# correct used_tables()
#

CREATE TABLE `t1` ( `id` mediumint(9) NOT NULL auto_increment, `taskid` bigint(20) NOT NULL default '0', `dbid` int(11) NOT NULL default '0', `create_date` datetime NOT NULL default '0000-00-00 00:00:00', `last_update` datetime NOT NULL default '0000-00-00 00:00:00', PRIMARY KEY  (`id`)) ENGINE=MyISAM CHARSET=latin1 AUTO_INCREMENT=3 ;
INSERT INTO `t1` (`id`, `taskid`, `dbid`, `create_date`,`last_update`) VALUES (1, 1, 15, '2003-09-29 10:31:36', '2003-09-29 10:31:36'), (2, 1, 21, now(), now());
CREATE TABLE `t2` (`db_id` int(11) NOT NULL auto_increment,`name` varchar(200) NOT NULL default '',`primary_uid` smallint(6) NOT NULL default '0',`secondary_uid` smallint(6) NOT NULL default '0',PRIMARY KEY  (`db_id`),UNIQUE KEY `name_2` (`name`),FULLTEXT KEY `name` (`name`)) ENGINE=MyISAM CHARSET=latin1 AUTO_INCREMENT=2147483647;
INSERT INTO `t2` (`db_id`, `name`, `primary_uid`, `secondary_uid`) VALUES (18, 'Not Set 1', 0, 0),(19, 'Valid', 1, 2),(20, 'Valid 2', 1, 2),(21, 'Should Not Return', 1, 2),(26, 'Not Set 2', 0, 0),(-1, 'ALL DB\'S', 0, 0);
CREATE TABLE `t3` (`taskgenid` mediumint(9) NOT NULL auto_increment,`dbid` int(11) NOT NULL default '0',`taskid` int(11) NOT NULL default '0',`mon` tinyint(4) NOT NULL default '1',`tues` tinyint(4) NOT NULL default '1',`wed` tinyint(4) NOT NULL default '1',`thur` tinyint(4) NOT NULL default '1',`fri` tinyint(4) NOT NULL default '1',`sat` tinyint(4) NOT NULL default '0',`sun` tinyint(4) NOT NULL default '0',`how_often` smallint(6) NOT NULL default '1',`userid` smallint(6) NOT NULL default '0',`active` tinyint(4) NOT NULL default '1',PRIMARY KEY  (`taskgenid`)) ENGINE=MyISAM CHARSET=latin1 AUTO_INCREMENT=2 ;
INSERT INTO `t3` (`taskgenid`, `dbid`, `taskid`, `mon`, `tues`,`wed`, `thur`, `fri`, `sat`, `sun`, `how_often`, `userid`, `active`) VALUES (1,-1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1);
CREATE TABLE `t4` (`task_id` smallint(6) NOT NULL default '0',`description` varchar(200) NOT NULL default '') ENGINE=MyISAM CHARSET=latin1;
INSERT INTO `t4` (`task_id`, `description`) VALUES (1, 'Daily Check List'),(2, 'Weekly Status');
select  dbid, name, (date_format(now() , '%Y-%m-%d') - INTERVAL how_often DAY) >= ifnull((SELECT date_format(max(create_date),'%Y-%m-%d') FROM t1 WHERE dbid = b.db_id AND taskid = a.taskgenid), '1950-01-01') from t3 a, t2 b, t4  WHERE dbid = - 1 AND primary_uid = '1' AND t4.task_id = taskid;
SELECT dbid, name FROM t3 a, t2 b, t4 WHERE dbid = - 1 AND primary_uid = '1' AND ((date_format(now() , '%Y-%m-%d') - INTERVAL how_often DAY) >= ifnull((SELECT date_format(max(create_date),'%Y-%m-%d') FROM t1 WHERE dbid = b.db_id AND taskid = a.taskgenid), '1950-01-01')) AND t4.task_id = taskid;
drop table t1,t2,t3,t4;

#
# cardinality check
#
CREATE TABLE t1 (id int(11) default NULL) ENGINE=MyISAM CHARSET=latin1;
INSERT INTO t1 VALUES (1),(5);
CREATE TABLE t2 (id int(11) default NULL) ENGINE=MyISAM CHARSET=latin1;
INSERT INTO t2 VALUES (2),(6);
-- error ER_OPERAND_COLUMNS
select * from t1 where (1,2,6) in (select * from t2);
DROP TABLE t1,t2;

#
# DO and SET with errors
#
create table t1 (s1 int);
insert into t1 values (1);
insert into t1 values (2);
-- error ER_SUBQUERY_NO_1_ROW
set sort_buffer_size = (select s1 from t1);
do (select * from t1);
drop table t1;

#
# optimized ALL/ANY with union
#
create table t1 (s1 char);
insert into t1 values ('e');
select * from t1 where 'f' > any (select s1 from t1);
select * from t1 where 'f' > any (select s1 from t1 union select s1 from t1);
explain extended select * from t1 where 'f' > any (select s1 from t1 union select s1 from t1);
drop table t1;

#
# filesort in subquery (restoring join_tab)
#
CREATE TABLE t1 (number char(11) NOT NULL default '') ENGINE=MyISAM CHARSET=latin1;
INSERT INTO t1 VALUES ('69294728265'),('18621828126'),('89356874041'),('95895001874');
CREATE TABLE t2 (code char(5) NOT NULL default '',UNIQUE KEY code (code)) ENGINE=MyISAM CHARSET=latin1;
INSERT INTO t2 VALUES ('1'),('1226'),('1245'),('1862'),('18623'),('1874'),('1967'),('6');
select c.number as phone,(select p.code from t2 p where c.number like concat(p.code, '%') order by length(p.code) desc limit 1) as code from t1 c;
drop table t1, t2;

#
# unresolved field error
#
create table t1 (s1 int);
create table t2 (s1 int);
-- error ER_BAD_FIELD_ERROR
select * from t1 where (select count(*) from t2 where t1.s2) = 1;
-- error ER_BAD_FIELD_ERROR
select * from t1 where (select count(*) from t2 group by t1.s2) = 1;
-- error ER_BAD_FIELD_ERROR
select count(*) from t2 group by t1.s2;
drop table t1, t2;

#
# fix_fields() in add_ref_to_table_cond()
#
CREATE TABLE t1(COLA FLOAT NOT NULL,COLB FLOAT NOT NULL,COLC VARCHAR(20) DEFAULT NULL,PRIMARY KEY (COLA, COLB));
CREATE TABLE t2(COLA FLOAT NOT NULL,COLB FLOAT NOT NULL,COLC CHAR(1) NOT NULL,PRIMARY KEY (COLA));
INSERT INTO t1 VALUES (1,1,'1A3240'), (1,2,'4W2365');
INSERT INTO t2 VALUES (100, 200, 'C');
SELECT DISTINCT COLC FROM t1 WHERE COLA = (SELECT COLA FROM t2 WHERE COLB = 200 AND COLC ='C' LIMIT 1);
DROP TABLE t1, t2;

CREATE TABLE t1 (a int(1));
INSERT INTO t1 VALUES (1),(1),(1),(1),(1),(2),(3),(4),(5);
SELECT DISTINCT (SELECT a) FROM t1 LIMIT 100;
DROP TABLE t1;


#
# Bug#2198 SELECT INTO OUTFILE (with Sub-Select) Problem
#

create table t1 (a int, b decimal(13, 3));
insert into t1 values (1, 0.123);
let $outfile_abs= $MYSQLTEST_VARDIR/tmp/subselect.out.file.1;
let $outfile_rel= ../../tmp/subselect.out.file.1;
--error 0,1
--remove_file $outfile_abs
eval select a, (select max(b) from t1) into outfile "$outfile_rel" from t1;
delete from t1;
eval load data infile "$outfile_rel" into table t1;
--remove_file $outfile_abs
select * from t1;
drop table t1;


#
# Bug#2479 dependant subquery with limit crash
#

CREATE TABLE `t1` (
  `id` int(11) NOT NULL auto_increment,
  `id_cns` tinyint(3) unsigned NOT NULL default '0',
  `tipo` enum('','UNO','DUE') NOT NULL default '',
  `anno_dep` smallint(4) unsigned zerofill NOT NULL default '0000',
  `particolare` mediumint(8) unsigned NOT NULL default '0',
  `generale` mediumint(8) unsigned NOT NULL default '0',
  `bis` tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `idx_cns_gen_anno` (`anno_dep`,`id_cns`,`generale`,`particolare`),
  UNIQUE KEY `idx_cns_par_anno` (`id_cns`,`anno_dep`,`tipo`,`particolare`,`bis`)
);
INSERT INTO `t1` VALUES (1,16,'UNO',1987,2048,9681,0),(2,50,'UNO',1987,1536,13987,0),(3,16,'UNO',1987,2432,14594,0),(4,16,'UNO',1987,1792,13422,0),(5,16,'UNO',1987,1025,10240,0),(6,16,'UNO',1987,1026,7089,0);
CREATE TABLE `t2` (
  `id` tinyint(3) unsigned NOT NULL auto_increment,
  `max_anno_dep` smallint(6) unsigned NOT NULL default '0',
  PRIMARY KEY  (`id`)
);
INSERT INTO `t2` VALUES (16,1987),(50,1990),(51,1990);

SELECT cns.id, cns.max_anno_dep, cns.max_anno_dep = (SELECT s.anno_dep FROM t1 AS s WHERE s.id_cns = cns.id ORDER BY s.anno_dep DESC LIMIT 1) AS PIPPO FROM t2 AS cns;

DROP TABLE t1, t2;

#
# GLOBAL LIMIT
#
create table t1 (a int);
insert into t1 values (1), (2), (3);
SET SQL_SELECT_LIMIT=1;
select sum(a) from (select * from t1) as a;
select 2 in (select * from t1);
SET SQL_SELECT_LIMIT=default;
drop table t1;


#
# Bug#3118 subselect + order by
#

CREATE TABLE t1 (a int, b int, INDEX (a));
INSERT INTO t1 VALUES (1, 1), (1, 2), (1, 3);
SELECT * FROM t1 WHERE a = (SELECT MAX(a) FROM t1 WHERE a = 1) ORDER BY b;
DROP TABLE t1;

# Item_cond fix field
#
create table t1(val varchar(10));
insert into t1 values ('aaa'), ('bbb'),('eee'),('mmm'),('ppp');
select count(*) from t1 as w1 where w1.val in (select w2.val from t1 as w2 where w2.val like 'm%') and w1.val in (select w3.val from t1 as w3 where w3.val like 'e%');
drop table t1;

#
# ref_or_null replacing with ref
#
create table t1 (id int not null, text varchar(20) not null default '', primary key (id));
insert into t1 (id, text) values (1, 'text1'), (2, 'text2'), (3, 'text3'), (4, 'text4'), (5, 'text5'), (6, 'text6'), (7, 'text7'), (8, 'text8'), (9, 'text9'), (10, 'text10'), (11, 'text11'), (12, 'text12');
select * from t1 where id not in (select id from t1 where id < 8);
select * from t1 as tt where not exists (select id from t1 where id < 8 and (id = tt.id or id is null) having id is not null);
explain extended select * from t1 where id not in (select id from t1 where id < 8);
explain extended select * from t1 as tt where not exists (select id from t1 where id < 8 and (id = tt.id or id is null) having id is not null);
insert into t1 (id, text) values (1000, 'text1000'), (1001, 'text1001');
create table t2 (id int not null, text varchar(20) not null default '', primary key (id));
insert into t2 (id, text) values (1, 'text1'), (2, 'text2'), (3, 'text3'), (4, 'text4'), (5, 'text5'), (6, 'text6'), (7, 'text7'), (8, 'text8'), (9, 'text9'), (10, 'text10'), (11, 'text1'), (12, 'text2'), (13, 'text3'), (14, 'text4'), (15, 'text5'), (16, 'text6'), (17, 'text7'), (18, 'text8'), (19, 'text9'), (20, 'text10'),(21, 'text1'), (22, 'text2'), (23, 'text3'), (24, 'text4'), (25, 'text5'), (26, 'text6'), (27, 'text7'), (28, 'text8'), (29, 'text9'), (30, 'text10'), (31, 'text1'), (32, 'text2'), (33, 'text3'), (34, 'text4'), (35, 'text5'), (36, 'text6'), (37, 'text7'), (38, 'text8'), (39, 'text9'), (40, 'text10'), (41, 'text1'), (42, 'text2'), (43, 'text3'), (44, 'text4'), (45, 'text5'), (46, 'text6'), (47, 'text7'), (48, 'text8'), (49, 'text9'), (50, 'text10');
select * from t1 a left join t2 b on (a.id=b.id or b.id is null) join t1 c on (if(isnull(b.id), 1000, b.id)=c.id);
explain extended select * from t1 a left join t2 b on (a.id=b.id or b.id is null) join t1 c on (if(isnull(b.id), 1000, b.id)=c.id);
drop table t1,t2;

#
# Static tables & rund() in subqueries
#
create table t1 (a int);
insert into t1 values (1);
explain select benchmark(1000, (select a from t1 where a=sha(rand())));
drop table t1;


#
# Bug#3188 Ambiguous Column in Subselect crashes server
#
create table t1(id int);
create table t2(id int);
create table t3(flag int);
-- error ER_PARSE_ERROR
select (select * from t3 where id not null) from t1, t2;
drop table t1,t2,t3;


#
# aggregate functions (Bug#3505 Wrong results on use of ORDER BY with subqueries)
#
CREATE TABLE t1 (id INT);
CREATE TABLE t2 (id INT);
INSERT INTO t1 VALUES (1), (2);
INSERT INTO t2 VALUES (1);
SELECT t1.id, ( SELECT COUNT(t.id) FROM t2 AS t WHERE t.id = t1.id ) AS c FROM t1 LEFT JOIN t2 USING (id);
SELECT id, ( SELECT COUNT(t.id) FROM t2 AS t WHERE t.id = t1.id ) AS c FROM t1 LEFT JOIN t2 USING (id);
SELECT t1.id, ( SELECT COUNT(t.id) FROM t2 AS t WHERE t.id = t1.id ) AS c FROM t1 LEFT JOIN t2 USING (id) ORDER BY t1.id;
SELECT id, ( SELECT COUNT(t.id) FROM t2 AS t WHERE t.id = t1.id ) AS c FROM t1 LEFT JOIN t2 USING (id) ORDER BY id;
DROP TABLE t1,t2;

#
# ALL/ANY test
#
CREATE TABLE t1 ( a int, b int );
INSERT INTO t1 VALUES (1,1),(2,2),(3,3);
SELECT a FROM t1 WHERE a > ANY ( SELECT a FROM t1 WHERE b = 2 );
SELECT a FROM t1 WHERE a < ANY ( SELECT a FROM t1 WHERE b = 2 );
SELECT a FROM t1 WHERE a = ANY ( SELECT a FROM t1 WHERE b = 2 );
SELECT a FROM t1 WHERE a >= ANY ( SELECT a FROM t1 WHERE b = 2 );
SELECT a FROM t1 WHERE a <= ANY ( SELECT a FROM t1 WHERE b = 2 );
SELECT a FROM t1 WHERE a <> ANY ( SELECT a FROM t1 WHERE b = 2 );
SELECT a FROM t1 WHERE a > ALL ( SELECT a FROM t1 WHERE b = 2 );
SELECT a FROM t1 WHERE a < ALL ( SELECT a FROM t1 WHERE b = 2 );
SELECT a FROM t1 WHERE a = ALL ( SELECT a FROM t1 WHERE b = 2 );
SELECT a FROM t1 WHERE a >= ALL ( SELECT a FROM t1 WHERE b = 2 );
SELECT a FROM t1 WHERE a <= ALL ( SELECT a FROM t1 WHERE b = 2 );
SELECT a FROM t1 WHERE a <> ALL ( SELECT a FROM t1 WHERE b = 2 );
# with index
ALTER TABLE t1 ADD INDEX (a);
SELECT a FROM t1 WHERE a > ANY ( SELECT a FROM t1 WHERE b = 2 );
SELECT a FROM t1 WHERE a < ANY ( SELECT a FROM t1 WHERE b = 2 );
SELECT a FROM t1 WHERE a = ANY ( SELECT a FROM t1 WHERE b = 2 );
SELECT a FROM t1 WHERE a >= ANY ( SELECT a FROM t1 WHERE b = 2 );
SELECT a FROM t1 WHERE a <= ANY ( SELECT a FROM t1 WHERE b = 2 );
SELECT a FROM t1 WHERE a <> ANY ( SELECT a FROM t1 WHERE b = 2 );
SELECT a FROM t1 WHERE a > ALL ( SELECT a FROM t1 WHERE b = 2 );
SELECT a FROM t1 WHERE a < ALL ( SELECT a FROM t1 WHERE b = 2 );
SELECT a FROM t1 WHERE a = ALL ( SELECT a FROM t1 WHERE b = 2 );
SELECT a FROM t1 WHERE a >= ALL ( SELECT a FROM t1 WHERE b = 2 );
SELECT a FROM t1 WHERE a <= ALL ( SELECT a FROM t1 WHERE b = 2 );
SELECT a FROM t1 WHERE a <> ALL ( SELECT a FROM t1 WHERE b = 2 );
# having clause test
SELECT a FROM t1 WHERE a > ANY (SELECT a FROM t1 HAVING a = 2);
SELECT a FROM t1 WHERE a < ANY (SELECT a FROM t1 HAVING a = 2);
SELECT a FROM t1 WHERE a = ANY (SELECT a FROM t1 HAVING a = 2);
SELECT a FROM t1 WHERE a >= ANY (SELECT a FROM t1 HAVING a = 2);
SELECT a FROM t1 WHERE a <= ANY (SELECT a FROM t1 HAVING a = 2);
SELECT a FROM t1 WHERE a <> ANY (SELECT a FROM t1 HAVING a = 2);
SELECT a FROM t1 WHERE a > ALL (SELECT a FROM t1 HAVING a = 2);
SELECT a FROM t1 WHERE a < ALL (SELECT a FROM t1 HAVING a = 2);
SELECT a FROM t1 WHERE a = ALL (SELECT a FROM t1 HAVING a = 2);
SELECT a FROM t1 WHERE a >= ALL (SELECT a FROM t1 HAVING a = 2);
SELECT a FROM t1 WHERE a <= ALL (SELECT a FROM t1 HAVING a = 2);
SELECT a FROM t1 WHERE a <> ALL (SELECT a FROM t1 HAVING a = 2);
# union test
SELECT a FROM t1 WHERE a > ANY (SELECT a FROM t1 WHERE b = 2 UNION SELECT a FROM t1 WHERE b = 2);
SELECT a FROM t1 WHERE a < ANY (SELECT a FROM t1 WHERE b = 2 UNION SELECT a FROM t1 WHERE b = 2);
SELECT a FROM t1 WHERE a = ANY (SELECT a FROM t1 WHERE b = 2 UNION SELECT a FROM t1 WHERE b = 2);
SELECT a FROM t1 WHERE a >= ANY (SELECT a FROM t1 WHERE b = 2 UNION SELECT a FROM t1 WHERE b = 2);
SELECT a FROM t1 WHERE a <= ANY (SELECT a FROM t1 WHERE b = 2 UNION SELECT a FROM t1 WHERE b = 2);
SELECT a FROM t1 WHERE a <> ANY (SELECT a FROM t1 WHERE b = 2 UNION SELECT a FROM t1 WHERE b = 2);
SELECT a FROM t1 WHERE a > ALL (SELECT a FROM t1 WHERE b = 2 UNION SELECT a FROM t1 WHERE b = 2);
SELECT a FROM t1 WHERE a < ALL (SELECT a FROM t1 WHERE b = 2 UNION SELECT a FROM t1 WHERE b = 2);
SELECT a FROM t1 WHERE a = ALL (SELECT a FROM t1 WHERE b = 2 UNION SELECT a FROM t1 WHERE b = 2);
SELECT a FROM t1 WHERE a >= ALL (SELECT a FROM t1 WHERE b = 2 UNION SELECT a FROM t1 WHERE b = 2);
SELECT a FROM t1 WHERE a <= ALL (SELECT a FROM t1 WHERE b = 2 UNION SELECT a FROM t1 WHERE b = 2);
SELECT a FROM t1 WHERE a <> ALL (SELECT a FROM t1 WHERE b = 2 UNION SELECT a FROM t1 WHERE b = 2);
# union + having test
SELECT a FROM t1 WHERE a > ANY (SELECT a FROM t1 HAVING a = 2 UNION SELECT a FROM t1 HAVING a = 2);
SELECT a FROM t1 WHERE a < ANY (SELECT a FROM t1 HAVING a = 2 UNION SELECT a FROM t1 HAVING a = 2);
SELECT a FROM t1 WHERE a = ANY (SELECT a FROM t1 HAVING a = 2 UNION SELECT a FROM t1 HAVING a = 2);
SELECT a FROM t1 WHERE a >= ANY (SELECT a FROM t1 HAVING a = 2 UNION SELECT a FROM t1 HAVING a = 2);
SELECT a FROM t1 WHERE a <= ANY (SELECT a FROM t1 HAVING a = 2 UNION SELECT a FROM t1 HAVING a = 2);
SELECT a FROM t1 WHERE a <> ANY (SELECT a FROM t1 HAVING a = 2 UNION SELECT a FROM t1 HAVING a = 2);
SELECT a FROM t1 WHERE a > ALL (SELECT a FROM t1 HAVING a = 2 UNION SELECT a FROM t1 HAVING a = 2);
SELECT a FROM t1 WHERE a < ALL (SELECT a FROM t1 HAVING a = 2 UNION SELECT a FROM t1 HAVING a = 2);
SELECT a FROM t1 WHERE a = ALL (SELECT a FROM t1 HAVING a = 2 UNION SELECT a FROM t1 HAVING a = 2);
SELECT a FROM t1 WHERE a >= ALL (SELECT a FROM t1 HAVING a = 2 UNION SELECT a FROM t1 HAVING a = 2);
SELECT a FROM t1 WHERE a <= ALL (SELECT a FROM t1 HAVING a = 2 UNION SELECT a FROM t1 HAVING a = 2);
SELECT a FROM t1 WHERE a <> ALL (SELECT a FROM t1 HAVING a = 2 UNION SELECT a FROM t1 HAVING a = 2);
# row tests
# < > >= <= and = ALL/ <> ANY do not support row operation
-- error ER_OPERAND_COLUMNS
SELECT a FROM t1 WHERE (1,2) > ANY (SELECT a FROM t1 WHERE b = 2);
-- error ER_OPERAND_COLUMNS
SELECT a FROM t1 WHERE a > ANY (SELECT a,2 FROM t1 WHERE b = 2);
-- error ER_OPERAND_COLUMNS
SELECT a FROM t1 WHERE (1,2) > ANY (SELECT a,2 FROM t1 WHERE b = 2);
-- error ER_OPERAND_COLUMNS
SELECT a FROM t1 WHERE (1,2) > ALL (SELECT a FROM t1 WHERE b = 2);
-- error ER_OPERAND_COLUMNS
SELECT a FROM t1 WHERE a > ALL (SELECT a,2 FROM t1 WHERE b = 2);
-- error ER_OPERAND_COLUMNS
SELECT a FROM t1 WHERE (1,2) > ALL (SELECT a,2 FROM t1 WHERE b = 2);
-- error ER_OPERAND_COLUMNS
SELECT a FROM t1 WHERE (1,2) = ALL (SELECT a,2 FROM t1 WHERE b = 2);
-- error ER_OPERAND_COLUMNS
SELECT a FROM t1 WHERE (1,2) <> ANY (SELECT a,2 FROM t1 WHERE b = 2);
# following should be converted to IN
-- error ER_OPERAND_COLUMNS
SELECT a FROM t1 WHERE (1,2) = ANY (SELECT a FROM t1 WHERE b = 2);
-- error ER_OPERAND_COLUMNS
SELECT a FROM t1 WHERE a = ANY (SELECT a,2 FROM t1 WHERE b = 2);
SELECT a FROM t1 WHERE (1,2) = ANY (SELECT a,2 FROM t1 WHERE b = 2);
-- error ER_OPERAND_COLUMNS
SELECT a FROM t1 WHERE (1,2) <> ALL (SELECT a FROM t1 WHERE b = 2);
-- error ER_OPERAND_COLUMNS
SELECT a FROM t1 WHERE a <> ALL (SELECT a,2 FROM t1 WHERE b = 2);
SELECT a FROM t1 WHERE (1,2) <> ALL (SELECT a,2 FROM t1 WHERE b = 2);
SELECT a FROM t1 WHERE (a,1) = ANY (SELECT a,1 FROM t1 WHERE b = 2);
SELECT a FROM t1 WHERE (a,1) <> ALL (SELECT a,1 FROM t1 WHERE b = 2);
SELECT a FROM t1 WHERE (a,1) = ANY (SELECT a,1 FROM t1 HAVING a = 2);
SELECT a FROM t1 WHERE (a,1) <> ALL (SELECT a,1 FROM t1 HAVING a = 2);
SELECT a FROM t1 WHERE (a,1) = ANY (SELECT a,1 FROM t1 WHERE b = 2 UNION SELECT a,1 FROM t1 WHERE b = 2);
SELECT a FROM t1 WHERE (a,1) <> ALL (SELECT a,1 FROM t1 WHERE b = 2 UNION SELECT a,1 FROM t1 WHERE b = 2);
SELECT a FROM t1 WHERE (a,1) = ANY (SELECT a,1 FROM t1 HAVING a = 2 UNION SELECT a,1 FROM t1 HAVING a = 2);
SELECT a FROM t1 WHERE (a,1) <> ALL (SELECT a,1 FROM t1 HAVING a = 2 UNION SELECT a,1 FROM t1 HAVING a = 2);
# without optimisation
SELECT a FROM t1 WHERE a > ANY (SELECT a FROM t1 WHERE b = 2 group by a);
SELECT a FROM t1 WHERE a < ANY (SELECT a FROM t1 WHERE b = 2 group by a);
SELECT a FROM t1 WHERE a = ANY (SELECT a FROM t1 WHERE b = 2 group by a);
SELECT a FROM t1 WHERE a >= ANY (SELECT a FROM t1 WHERE b = 2 group by a);
SELECT a FROM t1 WHERE a <= ANY (SELECT a FROM t1 WHERE b = 2 group by a);
SELECT a FROM t1 WHERE a <> ANY (SELECT a FROM t1 WHERE b = 2 group by a);
SELECT a FROM t1 WHERE a > ALL (SELECT a FROM t1 WHERE b = 2 group by a);
SELECT a FROM t1 WHERE a < ALL (SELECT a FROM t1 WHERE b = 2 group by a);
SELECT a FROM t1 WHERE a = ALL (SELECT a FROM t1 WHERE b = 2 group by a);
SELECT a FROM t1 WHERE a >= ALL (SELECT a FROM t1 WHERE b = 2 group by a);
SELECT a FROM t1 WHERE a <= ALL (SELECT a FROM t1 WHERE b = 2 group by a);
SELECT a FROM t1 WHERE a <> ALL (SELECT a FROM t1 WHERE b = 2 group by a);
# without optimisation + having
SELECT a FROM t1 WHERE a > ANY (SELECT a FROM t1 group by a HAVING a = 2);
SELECT a FROM t1 WHERE a < ANY (SELECT a FROM t1 group by a HAVING a = 2);
SELECT a FROM t1 WHERE a = ANY (SELECT a FROM t1 group by a HAVING a = 2);
SELECT a FROM t1 WHERE a >= ANY (SELECT a FROM t1 group by a HAVING a = 2);
SELECT a FROM t1 WHERE a <= ANY (SELECT a FROM t1 group by a HAVING a = 2);
SELECT a FROM t1 WHERE a <> ANY (SELECT a FROM t1 group by a HAVING a = 2);
SELECT a FROM t1 WHERE a > ALL (SELECT a FROM t1 group by a HAVING a = 2);
SELECT a FROM t1 WHERE a < ALL (SELECT a FROM t1 group by a HAVING a = 2);
SELECT a FROM t1 WHERE a = ALL (SELECT a FROM t1 group by a HAVING a = 2);
SELECT a FROM t1 WHERE a >= ALL (SELECT a FROM t1 group by a HAVING a = 2);
SELECT a FROM t1 WHERE a <= ALL (SELECT a FROM t1 group by a HAVING a = 2);
SELECT a FROM t1 WHERE a <> ALL (SELECT a FROM t1 group by a HAVING a = 2);
# EXISTS in string contence
SELECT concat(EXISTS(SELECT a FROM t1 WHERE b = 2 and a.a > t1.a), '-') from t1 a;
SELECT concat(EXISTS(SELECT a FROM t1 WHERE b = 2 and a.a < t1.a), '-') from t1 a;
SELECT concat(EXISTS(SELECT a FROM t1 WHERE b = 2 and a.a = t1.a), '-') from t1 a;
DROP TABLE t1;
CREATE TABLE t1 ( a double, b double );
INSERT INTO t1 VALUES (1,1),(2,2),(3,3);
SELECT a FROM t1 WHERE a > ANY (SELECT a FROM t1 WHERE b = 2e0);
SELECT a FROM t1 WHERE a < ANY (SELECT a FROM t1 WHERE b = 2e0);
SELECT a FROM t1 WHERE a = ANY (SELECT a FROM t1 WHERE b = 2e0);
SELECT a FROM t1 WHERE a >= ANY (SELECT a FROM t1 WHERE b = 2e0);
SELECT a FROM t1 WHERE a <= ANY (SELECT a FROM t1 WHERE b = 2e0);
SELECT a FROM t1 WHERE a <> ANY (SELECT a FROM t1 WHERE b = 2e0);
SELECT a FROM t1 WHERE a > ALL (SELECT a FROM t1 WHERE b = 2e0);
SELECT a FROM t1 WHERE a < ALL (SELECT a FROM t1 WHERE b = 2e0);
SELECT a FROM t1 WHERE a = ALL (SELECT a FROM t1 WHERE b = 2e0);
SELECT a FROM t1 WHERE a >= ALL (SELECT a FROM t1 WHERE b = 2e0);
SELECT a FROM t1 WHERE a <= ALL (SELECT a FROM t1 WHERE b = 2e0);
SELECT a FROM t1 WHERE a <> ALL (SELECT a FROM t1 WHERE b = 2e0);
DROP TABLE t1;
CREATE TABLE t1 ( a char(1), b char(1));
INSERT INTO t1 VALUES ('1','1'),('2','2'),('3','3');
SELECT a FROM t1 WHERE a > ANY (SELECT a FROM t1 WHERE b = '2');
SELECT a FROM t1 WHERE a < ANY (SELECT a FROM t1 WHERE b = '2');
SELECT a FROM t1 WHERE a = ANY (SELECT a FROM t1 WHERE b = '2');
SELECT a FROM t1 WHERE a >= ANY (SELECT a FROM t1 WHERE b = '2');
SELECT a FROM t1 WHERE a <= ANY (SELECT a FROM t1 WHERE b = '2');
SELECT a FROM t1 WHERE a <> ANY (SELECT a FROM t1 WHERE b = '2');
SELECT a FROM t1 WHERE a > ALL (SELECT a FROM t1 WHERE b = '2');
SELECT a FROM t1 WHERE a < ALL (SELECT a FROM t1 WHERE b = '2');
SELECT a FROM t1 WHERE a = ALL (SELECT a FROM t1 WHERE b = '2');
SELECT a FROM t1 WHERE a >= ALL (SELECT a FROM t1 WHERE b = '2');
SELECT a FROM t1 WHERE a <= ALL (SELECT a FROM t1 WHERE b = '2');
SELECT a FROM t1 WHERE a <> ALL (SELECT a FROM t1 WHERE b = '2');
DROP TABLE t1;


#
# SELECT(EXISTS * ...)optimisation
#
create table t1 (a int, b int);
insert into t1 values (1,2),(3,4);
select * from t1 up where exists (select * from t1 where t1.a=up.a);
explain extended select * from t1 up where exists (select * from t1 where t1.a=up.a);
drop table t1;


#
# Bug#4102 subselect in HAVING
#

CREATE TABLE t1 (t1_a int);
INSERT INTO t1 VALUES (1);
CREATE TABLE t2 (t2_a int, t2_b int, PRIMARY KEY (t2_a, t2_b));
INSERT INTO t2 VALUES (1, 1), (1, 2);
SELECT * FROM t1, t2 table2 WHERE t1_a = 1 AND table2.t2_a = 1
  HAVING table2.t2_b = (SELECT MAX(t2_b) FROM t2 WHERE t2_a = table2.t2_a);
DROP TABLE t1, t2;


#
# Test problem with NULL and derived tables
# (Bug#4097 JOIN with subquery causes entire column to report NULL)
#

CREATE TABLE t1 (id int(11) default NULL,name varchar(10) default NULL);
INSERT INTO t1 VALUES (1,'Tim'),(2,'Rebecca'),(3,NULL);
CREATE TABLE t2 (id int(11) default NULL, pet varchar(10) default NULL);
INSERT INTO t2 VALUES (1,'Fido'),(2,'Spot'),(3,'Felix');
SELECT a.*, b.* FROM (SELECT * FROM t1) AS a JOIN t2 as b on a.id=b.id;
drop table t1,t2;


#
# outer fields resolving in INSERT/REPLACE and CRETE with SELECT
#
CREATE TABLE t1 ( a int, b int );
CREATE TABLE t2 ( c int, d int );
INSERT INTO t1 VALUES (1,2), (2,3), (3,4);
SELECT a AS abc, b FROM t1 outr WHERE b =
  (SELECT MIN(b) FROM t1 WHERE a=outr.a);
INSERT INTO t2 SELECT a AS abc, b FROM t1 outr WHERE b =
  (SELECT MIN(b) FROM t1 WHERE a=outr.a);
select * from t2;
CREATE TABLE t3 SELECT a AS abc, b FROM t1 outr WHERE b =
  (SELECT MIN(b) FROM t1 WHERE a=outr.a);
select * from t3;
prepare stmt1 from "INSERT INTO t2 SELECT a AS abc, b FROM t1 outr WHERE b = (SELECT MIN(b) FROM t1 WHERE a=outr.a);";
execute stmt1;
deallocate prepare stmt1;
select * from t2;
drop table t3;
prepare stmt1 from "CREATE TABLE t3 SELECT a AS abc, b FROM t1 outr WHERE b = (SELECT MIN(b) FROM t1 WHERE a=outr.a);";
execute stmt1;
select * from t3;
deallocate prepare stmt1;
DROP TABLE t1, t2, t3;

#
# Aggregate function comparation with ALL/ANY/SOME subselect
#
CREATE TABLE `t1` ( `a` int(11) default NULL) ENGINE=MyISAM DEFAULT CHARSET=latin1;
insert into t1 values (1);
CREATE TABLE `t2` ( `b` int(11) default NULL, `a` int(11) default NULL) ENGINE=MyISAM DEFAULT CHARSET=latin1;
insert into t2 values (1,2);
select t000.a, count(*) `C` FROM t1 t000 GROUP BY t000.a HAVING count(*) > ALL (SELECT count(*) FROM t2 t001 WHERE t001.a=1);
drop table t1,t2;


#
# Bug#4769 - fulltext in subselect
#
create table t1 (a int not null auto_increment primary key, b varchar(40), fulltext(b));
insert into t1 (b) values ('ball'),('ball games'), ('games'), ('foo'), ('foobar'), ('Serg'), ('Sergei'),('Georg'), ('Patrik'),('Hakan');
create table t2 (a int);
insert into t2 values (1),(3),(2),(7);
select a,b from t1 where match(b) against ('Ball') > 0;
select a from t2 where a in (select a from t1 where match(b) against ('Ball') > 0);
drop table t1,t2;


#
# Bug#5003 - like in subselect
#
CREATE TABLE t1(`IZAVORGANG_ID` VARCHAR(11) CHARACTER SET latin1 COLLATE latin1_bin,`KUERZEL` VARCHAR(10) CHARACTER SET latin1 COLLATE latin1_bin,`IZAANALYSEART_ID` VARCHAR(11) CHARACTER SET latin1 COLLATE latin1_bin,`IZAPMKZ_ID` VARCHAR(11) CHARACTER SET latin1 COLLATE latin1_bin);
CREATE INDEX AK01IZAVORGANG ON t1(izaAnalyseart_id,Kuerzel);
INSERT INTO t1(`IZAVORGANG_ID`,`KUERZEL`,`IZAANALYSEART_ID`,`IZAPMKZ_ID`)VALUES('D0000000001','601','D0000000001','I0000000001');
INSERT INTO t1(`IZAVORGANG_ID`,`KUERZEL`,`IZAANALYSEART_ID`,`IZAPMKZ_ID`)VALUES('D0000000002','602','D0000000001','I0000000001');
INSERT INTO t1(`IZAVORGANG_ID`,`KUERZEL`,`IZAANALYSEART_ID`,`IZAPMKZ_ID`)VALUES('D0000000003','603','D0000000001','I0000000001');
INSERT INTO t1(`IZAVORGANG_ID`,`KUERZEL`,`IZAANALYSEART_ID`,`IZAPMKZ_ID`)VALUES('D0000000004','101','D0000000001','I0000000001');
SELECT `IZAVORGANG_ID` FROM t1 WHERE `KUERZEL` IN(SELECT MIN(`KUERZEL`)`Feld1` FROM t1 WHERE `KUERZEL` LIKE'601%'And`IZAANALYSEART_ID`='D0000000001');
drop table t1;

#
# Optimized IN with compound index
#
CREATE TABLE `t1` ( `aid` int(11) NOT NULL default '0', `bid` int(11) NOT NULL default '0', PRIMARY KEY  (`aid`,`bid`));
CREATE TABLE `t2` ( `aid` int(11) NOT NULL default '0', `bid` int(11) NOT NULL default '0', PRIMARY KEY  (`aid`,`bid`));
insert into t1 values (1,1),(1,2),(2,1),(2,2);
insert into t2 values (1,2),(2,2);
select * from t1 where t1.aid not in (select aid from t2 where bid=t1.bid);
alter table t2 drop primary key;
alter table t2 add key KEY1 (aid, bid);
select * from t1 where t1.aid not in (select aid from t2 where bid=t1.bid);
alter table t2 drop key KEY1;
alter table t2 add primary key (bid, aid);
select * from t1 where t1.aid not in (select aid from t2 where bid=t1.bid);
drop table t1,t2;

#
# resolving fields of grouped outer SELECT
#
CREATE TABLE t1 (howmanyvalues bigint, avalue int);
INSERT INTO t1 VALUES (1, 1),(2, 1),(2, 2),(3, 1),(3, 2),(3, 3),(4, 1),(4, 2),(4, 3),(4, 4);
SELECT howmanyvalues, count(*) from t1 group by howmanyvalues;
SELECT a.howmanyvalues, (SELECT count(*) from t1 b where b.howmanyvalues = a.howmanyvalues) as mycount from t1 a group by a.howmanyvalues;
CREATE INDEX t1_howmanyvalues_idx ON t1 (howmanyvalues);
SELECT a.howmanyvalues, (SELECT count(*) from t1 b where b.howmanyvalues+1 = a.howmanyvalues+1) as mycount from t1 a group by a.howmanyvalues;
SELECT a.howmanyvalues, (SELECT count(*) from t1 b where b.howmanyvalues = a.howmanyvalues) as mycount from t1 a group by a.howmanyvalues;
SELECT a.howmanyvalues, (SELECT count(*) from t1 b where b.howmanyvalues = a.avalue) as mycount from t1 a group by a.howmanyvalues;
drop table t1;

create table t1 (x int);
select  (select b.x from t1 as b where b.x=a.x) from t1 as a where a.x=2 group by a.x;
drop table t1;

#
# Test of correct maybe_null flag returning by subquwery for temporary table
# creation
#
CREATE TABLE `t1` ( `master` int(10) unsigned NOT NULL default '0', `map` smallint(6) unsigned NOT NULL default '0', `slave` int(10) unsigned NOT NULL default '0', `access` int(10) unsigned NOT NULL default '0', UNIQUE KEY `access_u` (`master`,`map`,`slave`));
INSERT INTO `t1` VALUES (1,0,0,700),(1,1,1,400),(1,5,5,400),(1,12,12,400),(1,12,32,400),(4,12,32,400);
CREATE TABLE `t2` ( `id` int(10) unsigned NOT NULL default '0', `pid` int(10) unsigned NOT NULL default '0', `map` smallint(6) unsigned NOT NULL default '0', `level` tinyint(4) unsigned NOT NULL default '0', `title` varchar(255) default NULL, PRIMARY KEY  (`id`,`pid`,`map`), KEY `level` (`level`), KEY `id` (`id`,`map`)) ;
INSERT INTO `t2` VALUES (6,5,12,7,'a'),(12,0,0,7,'a'),(12,1,0,7,'a'),(12,5,5,7,'a'),(12,5,12,7,'a');
-- error ER_BAD_FIELD_ERROR
SELECT b.sc FROM (SELECT (SELECT a.access FROM t1 a WHERE a.map = op.map AND a.slave = op.pid AND a.master = 1) ac FROM t2 op WHERE op.id = 12 AND op.map = 0) b;
SELECT b.ac FROM (SELECT (SELECT a.access FROM t1 a WHERE a.map = op.map AND a.slave = op.pid AND a.master = 1) ac FROM t2 op WHERE op.id = 12 AND op.map = 0) b;
drop tables t1,t2;


#
# Test for Bug#6462 Same request on same data returns different results
# a.k.a. "Proper cleanup of subqueries is missing for SET and DO statements".
#
create table t1 (a int not null, b int not null, c int, primary key (a,b));
insert into t1 values (1,1,1), (2,2,2), (3,3,3);
set @b:= 0;
# Let us check that subquery will use covering index
explain select sum(a) from t1 where b > @b;
# This should not crash -debug server due to failing assertion
set @a:= (select sum(a) from t1 where b > @b);
# And this should not falsely report index usage
explain select a from t1 where c=2;
# Same for DO statement
do @a:= (select sum(a) from t1 where b > @b);
explain select a from t1 where c=2;
drop table t1;

#
# Subselect in non-select command just after connection
# Disconnect new connection and switch back when test is finished
#
connect (root,localhost,root,,test,$MASTER_MYPORT,$MASTER_MYSOCK);
connection root;
set @got_val= (SELECT 1 FROM (SELECT 'A' as my_col) as T1 ) ;
connection default;
disconnect root;

#
# primary query with temporary table and subquery with grouping
#
create table t1 (a int, b int);
create table t2 (a int, b int);
insert into t1 values (1,1),(1,2),(1,3),(2,4),(2,5);
insert into t2 values (1,3),(2,1);
select distinct a,b, (select max(b) from t2 where t1.b=t2.a) from t1 order by t1.b;
drop table t1, t2;

#
# subqueries with full text search
#
create table t1 (id int);
create table t2 (id int, body text, fulltext (body));
insert into t1 values(1),(2),(3);
insert into t2 values (1,'test'), (2,'mysql'), (3,'test'), (4,'test');
select count(distinct id) from t1 where id in (select id from t2 where match(body) against ('mysql' in boolean mode));
drop table t2,t1;

#
# Equal operation under row and empty subquery
#
create table t1 (s1 int,s2 int);
insert into t1 values (20,15);
select * from t1 where  (('a',null) <=> (select 'a',s2 from t1 where s1 = 0));
drop table t1;

#
# ALL/ANY with NULL
#
create table t1 (s1 int);
insert into t1 values (1),(null);
select * from t1 where s1 < all (select s1 from t1);
select s1, s1 < all (select s1 from t1) from t1;
drop table t1;

#
# reference on changable fields from subquery
#
CREATE TABLE t1 (
  Code char(3) NOT NULL default '',
  Name char(52) NOT NULL default '',
  Continent enum('Asia','Europe','North America','Africa','Oceania','Antarctica','South America') NOT NULL default 'Asia',
  Region char(26) NOT NULL default '',
  SurfaceArea float(10,2) NOT NULL default '0.00',
  IndepYear smallint(6) default NULL,
  Population int(11) NOT NULL default '0',
  LifeExpectancy float(3,1) default NULL,
  GNP float(10,2) default NULL,
  GNPOld float(10,2) default NULL,
  LocalName char(45) NOT NULL default '',
  GovernmentForm char(45) NOT NULL default '',
  HeadOfState char(60) default NULL,
  Capital int(11) default NULL,
  Code2 char(2) NOT NULL default ''
) ENGINE=MyISAM;
INSERT INTO t1 VALUES ('XXX','Xxxxx','Oceania','Xxxxxx',26.00,0,0,0,0,0,'Xxxxx','Xxxxx','Xxxxx',NULL,'XX');
INSERT INTO t1 VALUES ('ASM','American Samoa','Oceania','Polynesia',199.00,0,68000,75.1,334.00,NULL,'Amerika Samoa','US Territory','George W. Bush',54,'AS');
INSERT INTO t1 VALUES ('ATF','French Southern territories','Antarctica','Antarctica',7780.00,0,0,NULL,0.00,NULL,'Terres australes françaises','Nonmetropolitan Territory of France','Jacques Chirac',NULL,'TF');
INSERT INTO t1 VALUES ('UMI','United States Minor Outlying Islands','Oceania','Micronesia/Caribbean',16.00,0,0,NULL,0.00,NULL,'United States Minor Outlying Islands','Dependent Territory of the US','George W. Bush',NULL,'UM');
/*!40000 ALTER TABLE t1 ENABLE KEYS */;
SELECT DISTINCT Continent AS c FROM t1 outr WHERE
  Code <> SOME ( SELECT Code FROM t1 WHERE Continent = outr.Continent AND
  Population < 200);
drop table t1;


#
# Test for Bug#7885 Server crash when 'any' subselect compared to
#                   non-existant field.
#
create table t1 (a1 int);
create table t2 (b1 int);
--error ER_BAD_FIELD_ERROR
select * from t1 where a2 > any(select b1 from t2);
select * from t1 where a1 > any(select b1 from t2);
drop table t1,t2;


#
# Comparison subquery with * and row
#
create table t1 (a integer, b integer);
select (select * from t1) = (select 1,2);
select (select 1,2) = (select * from t1);
# queries whih can be converted to IN
select  row(1,2) = ANY (select * from t1);
select  row(1,2) != ALL (select * from t1);
drop table t1;

#
# Comparison subquery and row with nested rows
#
create table t1 (a integer, b integer);
-- error ER_OPERAND_COLUMNS
select row(1,(2,2)) in (select * from t1 );
-- error ER_OPERAND_COLUMNS
select row(1,(2,2)) = (select * from t1 );
-- error ER_OPERAND_COLUMNS
select (select * from t1) = row(1,(2,2));
drop table t1;

#
# Forward reference detection
#
create  table t1 (a integer);
insert into t1 values (1);
-- error ER_ILLEGAL_REFERENCE
select 1 = ALL (select 1 from t1 where 1 = xx ), 1 as xx ;
-- error ER_ILLEGAL_REFERENCE
select 1 = ALL (select 1 from t1 where 1 = xx ), 1 as xx;
select 1 as xx, 1 = ALL (  select 1 from t1 where 1 = xx );
-- error ER_ILLEGAL_REFERENCE
select 1 = ALL (select 1 from t1 where 1 = xx ), 1 as xx;
-- error ER_ILLEGAL_REFERENCE
select 1 = ALL (select 1 from t1 where 1 = xx ), 1 as xx from DUAL;
drop table t1;


#
# Test for Bug#8218 Join does not pass string from right table
#
CREATE TABLE t1 (
  categoryId int(11) NOT NULL,
  courseId int(11) NOT NULL,
  startDate datetime NOT NULL,
  endDate datetime NOT NULL,
  createDate datetime NOT NULL,
  modifyDate timestamp NOT NULL,
  attributes text NOT NULL
);
INSERT INTO t1 VALUES (1,41,'2004-02-09','2010-01-01','2004-02-09','2004-02-09',''),
(1,86,'2004-08-16','2004-08-16','2004-08-16','2004-08-16',''),
(1,87,'2004-08-16','2004-08-16','2004-08-16','2004-08-16',''),
(2,52,'2004-03-15','2004-10-01','2004-03-15','2004-09-17',''),
(2,53,'2004-03-16','2004-10-01','2004-03-16','2004-09-17',''),
(2,88,'2004-08-16','2004-08-16','2004-08-16','2004-08-16',''),
(2,89,'2004-08-16','2004-08-16','2004-08-16','2004-08-16',''),
(3,51,'2004-02-09','2010-01-01','2004-02-09','2004-02-09',''),
(5,12,'2004-02-18','2010-01-01','2004-02-18','2004-02-18','');

CREATE TABLE t2 (
  userId int(11) NOT NULL,
  courseId int(11) NOT NULL,
  date datetime NOT NULL
);
INSERT INTO t2 VALUES (5141,71,'2003-11-18'),
(5141,72,'2003-11-25'),(5141,41,'2004-08-06'),
(5141,52,'2004-08-06'),(5141,53,'2004-08-06'),
(5141,12,'2004-08-06'),(5141,86,'2004-10-21'),
(5141,87,'2004-10-21'),(5141,88,'2004-10-21'),
(5141,89,'2004-10-22'),(5141,51,'2004-10-26');


CREATE TABLE t3 (
  groupId int(11) NOT NULL,
  parentId int(11) NOT NULL,
  startDate datetime NOT NULL,
  endDate datetime NOT NULL,
  createDate datetime NOT NULL,
  modifyDate timestamp NOT NULL,
  ordering int(11)
);
INSERT INTO t3 VALUES (12,9,'1000-01-01','3999-12-31','2004-01-29','2004-01-29',NULL);

CREATE TABLE t4 (
  id int(11) NOT NULL,
  groupTypeId int(11) NOT NULL,
  groupKey varchar(50) NOT NULL,
  name text,
  ordering int(11),
  description text,
  createDate datetime NOT NULL,
  modifyDate timestamp NOT NULL
);
INSERT INTO t4 VALUES (9,5,'stationer','stationer',0,'Stationer','2004-01-29','2004-01-29'),
(12,5,'group2','group2',0,'group2','2004-01-29','2004-01-29');

CREATE TABLE t5 (
  userId int(11) NOT NULL,
  groupId int(11) NOT NULL,
  createDate datetime NOT NULL,
  modifyDate timestamp NOT NULL
);
INSERT INTO t5 VALUES (5141,12,'2004-08-06','2004-08-06');

select
  count(distinct t2.userid) pass,
  groupstuff.*,
  count(t2.courseid) crse,
  t1.categoryid,
  t2.courseid,
  date_format(date, '%b%y') as colhead
from t2
join t1 on t2.courseid=t1.courseid
join
(
  select
    t5.userid,
    parentid,
    parentgroup,
    childid,
    groupname,
    grouptypeid
  from t5
  join
  (
     select t4.id as parentid,
       t4.name as parentgroup,
       t4.id as childid,
       t4.name as groupname,
       t4.grouptypeid
     from t4
  ) as gin on t5.groupid=gin.childid
) as groupstuff on t2.userid = groupstuff.userid
group by
  groupstuff.groupname, colhead , t2.courseid;

drop table t1, t2, t3, t4, t5;


#
# Transformation in left expression of subquery (Bug#8888)
#
create table t1 (a int);
insert into t1 values (1), (2), (3);
SELECT 1 FROM t1 WHERE (SELECT 1) in (SELECT 1);
drop table t1;
#
# subselect into HAVING clause (code covarage improvement)
#
create table t1 (a int);
create table t2 (a int);
insert into t1 values (1),(2);
insert into t2 values (0),(1),(2),(3);
select a from t2 where a in (select a from t1);
select a from t2 having a in (select a from t1);
prepare stmt1 from "select a from t2 where a in (select a from t1)";
execute stmt1;
execute stmt1;
deallocate prepare stmt1;
prepare stmt1 from "select a from t2 having a in (select a from t1)";
execute stmt1;
execute stmt1;
deallocate prepare stmt1;
drop table t1, t2;

#
# single row subqueries and row operations (code covarage improvement)
#
create table t1 (a int, b int);
insert into t1 values (1,2);
-- error ER_OPERAND_COLUMNS
select 1 = (select * from t1);
-- error ER_OPERAND_COLUMNS
select (select * from t1) = 1;
-- error ER_OPERAND_COLUMNS
select (1,2) = (select a from t1);
-- error ER_OPERAND_COLUMNS
select (select a from t1) = (1,2);
-- error ER_OPERAND_COLUMNS
select (1,2,3) = (select * from t1);
-- error ER_OPERAND_COLUMNS
select (select * from t1) = (1,2,3);
drop table t1;


#
# Item_int_with_ref check (Bug#10020)
#
CREATE TABLE `t1` (
  `itemid` bigint(20) unsigned NOT NULL auto_increment,
  `sessionid` bigint(20) unsigned default NULL,
  `time` int(10) unsigned NOT NULL default '0',
  `type` set('A','D','E','F','G','I','L','N','U') collate latin1_general_ci NOT
NULL default '',
  `data` text collate latin1_general_ci NOT NULL,
  PRIMARY KEY  (`itemid`)
) DEFAULT CHARSET=latin1 COLLATE=latin1_general_ci;
INSERT INTO `t1` VALUES (1, 1, 1, 'D', '');
CREATE TABLE `t2` (
  `sessionid` bigint(20) unsigned NOT NULL auto_increment,
  `pid` int(10) unsigned NOT NULL default '0',
  `date` int(10) unsigned NOT NULL default '0',
  `ip` varchar(15) collate latin1_general_ci NOT NULL default '',
  PRIMARY KEY  (`sessionid`)
) DEFAULT CHARSET=latin1 COLLATE=latin1_general_ci;
INSERT INTO `t2` VALUES (1, 1, 1, '10.10.10.1');
SELECT s.ip, count( e.itemid ) FROM `t1` e JOIN t2 s ON s.sessionid = e.sessionid WHERE e.sessionid = ( SELECT sessionid FROM t2 ORDER BY sessionid DESC LIMIT 1 ) GROUP BY s.ip HAVING count( e.itemid ) >0 LIMIT 0 , 30;
drop tables t1,t2;


# Bug#11821 Select from subselect using aggregate function on an enum segfaults
create table t1 (fld enum('0','1'));
insert into t1 values ('1');
select * from (select max(fld) from t1) as foo;
drop table t1;


#
# Bug#11479 subquery over left join with an empty inner table
#

CREATE TABLE t1 (a int);
CREATE TABLE t2 (a int, b int);
CREATE TABLE t3 (b int NOT NULL);
INSERT INTO t1 VALUES (1), (2), (3), (4);
INSERT INTO t2 VALUES (1,10), (3,30);

SELECT * FROM t2 LEFT JOIN t3 ON t2.b=t3.b
  WHERE t3.b IS NOT NULL OR t2.a > 10;
SELECT * FROM t1
  WHERE t1.a NOT IN (SELECT a FROM t2 LEFT JOIN t3 ON t2.b=t3.b
                       WHERE t3.b IS NOT NULL OR t2.a > 10);

DROP TABLE t1,t2,t3;


#
# Bug#18503 Queries with a quantified subquery returning empty set may
#           return a wrong result.
#
CREATE TABLE t1 (f1 INT);
CREATE TABLE t2 (f2 INT);
INSERT INTO t1 VALUES (1);
SELECT * FROM t1 WHERE f1 > ALL (SELECT f2 FROM t2);
SELECT * FROM t1 WHERE f1 > ALL (SELECT f2 FROM t2 WHERE 1=0);
INSERT INTO t2 VALUES (1);
INSERT INTO t2 VALUES (2);
SELECT * FROM t1 WHERE f1 > ALL (SELECT f2 FROM t2 WHERE f2=0);
DROP TABLE t1, t2;


#
# Bug#16302 Quantified subquery without any tables gives wrong results
#
select 1 from dual where 1 < any (select 2);
select 1 from dual where 1 < all (select 2);
select 1 from dual where 2 > any (select 1);
select 1 from dual where 2 > all (select 1);
select 1 from dual where 1 < any (select 2 from dual);
select 1 from dual where 1 < all (select 2 from dual where 1!=1);


# Bug#20975 Wrong query results for subqueries within NOT
create table t1 (s1 char);
insert into t1 values (1),(2);

select * from t1 where (s1 < any (select s1 from t1));
select * from t1 where not (s1 < any (select s1 from t1));

select * from t1 where (s1 < ALL (select s1+1 from t1));
select * from t1 where not(s1 < ALL (select s1+1 from t1));

select * from t1 where (s1+1 = ANY (select s1 from t1));
select * from t1 where NOT(s1+1 = ANY (select s1 from t1));

select * from t1 where (s1 = ALL (select s1/s1 from t1));
select * from t1 where NOT(s1 = ALL (select s1/s1 from t1));
drop table t1;


#
# Bug#16255 Subquery in where
#
create table t1 (
  retailerID varchar(8) NOT NULL,
  statusID   int(10) unsigned NOT NULL,
  changed    datetime NOT NULL,
  UNIQUE KEY retailerID (retailerID, statusID, changed)
);

INSERT INTO t1 VALUES("0026", "1", "2005-12-06 12:18:56");
INSERT INTO t1 VALUES("0026", "2", "2006-01-06 12:25:53");
INSERT INTO t1 VALUES("0037", "1", "2005-12-06 12:18:56");
INSERT INTO t1 VALUES("0037", "2", "2006-01-06 12:25:53");
INSERT INTO t1 VALUES("0048", "1", "2006-01-06 12:37:50");
INSERT INTO t1 VALUES("0059", "1", "2006-01-06 12:37:50");

select * from t1 r1
  where (r1.retailerID,(r1.changed)) in
         (SELECT r2.retailerId,(max(changed)) from t1 r2
          group by r2.retailerId);
drop table t1;


#
# Bug#21180 Subselect with index for both WHERE and ORDER BY
#           produces empty result
#
create table t1(a int, primary key (a));
insert into t1 values (10);

create table t2 (a int primary key, b varchar(32), c int, unique key b(c, b));
insert into t2(a, c, b) values (1,10,'359'), (2,10,'35988'), (3,10,'35989');

explain SELECT sql_no_cache t1.a, r.a, r.b FROM t1 LEFT JOIN t2 r
  ON r.a = (SELECT t2.a FROM t2 WHERE t2.c = t1.a AND t2.b <= '359899'
             ORDER BY t2.c DESC, t2.b DESC LIMIT 1) WHERE t1.a = 10;
SELECT sql_no_cache t1.a, r.a, r.b FROM t1 LEFT JOIN t2 r
  ON r.a = (SELECT t2.a FROM t2 WHERE t2.c = t1.a AND t2.b <= '359899'
            ORDER BY t2.c DESC, t2.b DESC LIMIT 1) WHERE t1.a = 10;

explain SELECT sql_no_cache t1.a, r.a, r.b FROM t1 LEFT JOIN t2 r
  ON r.a = (SELECT t2.a FROM t2 WHERE t2.c = t1.a AND t2.b <= '359899'
            ORDER BY t2.c, t2.b LIMIT 1) WHERE t1.a = 10;
SELECT sql_no_cache t1.a, r.a, r.b FROM t1 LEFT JOIN t2 r
  ON r.a = (SELECT t2.a FROM t2 WHERE t2.c = t1.a AND t2.b <= '359899'
            ORDER BY t2.c, t2.b LIMIT 1) WHERE t1.a = 10;

drop table t1,t2;


#
# Bug#21853 assert failure for a grouping query with
#           an ALL/ANY quantified subquery in HAVING
#

CREATE TABLE t1 (
  field1 int NOT NULL,
  field2 int NOT NULL,
  field3 int NOT NULL,
  PRIMARY KEY  (field1,field2,field3)
);
CREATE TABLE t2 (
  fieldA int NOT NULL,
  fieldB int NOT NULL,
  PRIMARY KEY  (fieldA,fieldB)
);

INSERT INTO t1 VALUES
  (1,1,1), (1,1,2), (1,2,1), (1,2,2), (1,2,3), (1,3,1);
INSERT INTO t2 VALUES (1,1), (1,2), (1,3);

SELECT field1, field2, COUNT(*)
  FROM t1 GROUP BY field1, field2;

SELECT field1, field2
  FROM  t1
    GROUP BY field1, field2
      HAVING COUNT(*) >= ALL (SELECT fieldB
                                FROM t2 WHERE fieldA = field1);
SELECT field1, field2
  FROM  t1
    GROUP BY field1, field2
      HAVING COUNT(*) < ANY (SELECT fieldB
                               FROM t2 WHERE fieldA = field1);

DROP TABLE t1, t2;


#
# Bug#23478 not top-level IN subquery returning a non-empty result set
#           with possible NULL values by index access from the outer query
#

CREATE TABLE t1(a int, INDEX (a));
INSERT INTO t1 VALUES (1), (3), (5), (7);
INSERT INTO t1 VALUES (NULL);

CREATE TABLE t2(a int);
INSERT INTO t2 VALUES (1),(2),(3);

EXPLAIN SELECT a, a IN (SELECT a FROM t1) FROM t2;
SELECT a, a IN (SELECT a FROM t1) FROM t2;

DROP TABLE t1,t2;


#
# Bug#11302 getObject() returns a String for a sub-query of type datetime
#
CREATE TABLE t1 (a DATETIME);
INSERT INTO t1 VALUES ('1998-09-23'), ('2003-03-25');

CREATE TABLE t2 AS SELECT
  (SELECT a FROM t1 WHERE a < '2000-01-01') AS sub_a
   FROM t1 WHERE a > '2000-01-01';
SHOW CREATE TABLE t2;

CREATE TABLE t3 AS (SELECT a FROM t1 WHERE a < '2000-01-01') UNION (SELECT a FROM t1 WHERE a > '2000-01-01');
SHOW CREATE TABLE t3;

DROP TABLE t1,t2,t3;


#
# Bug#24670 subquery witout tables but with a WHERE clause
#

CREATE TABLE t1 (a int);
INSERT INTO t1 VALUES (1), (2);

SELECT a FROM t1 WHERE (SELECT 1 FROM DUAL WHERE 1=0) > 0;
SELECT a FROM t1 WHERE (SELECT 1 FROM DUAL WHERE 1=0) IS NULL;
EXPLAIN SELECT a FROM t1 WHERE (SELECT 1 FROM DUAL WHERE 1=0) IS NULL;

DROP TABLE t1;


#
# Bug#24653 sorting by expressions containing subselects
#           that return more than one row
#

CREATE TABLE t1 (a int);
INSERT INTO t1 VALUES (2), (4), (1), (3);

CREATE TABLE t2 (b int, c int);
INSERT INTO t2 VALUES
  (2,1), (1,3), (2,1), (4,4), (2,2), (1,4);

SELECT a FROM t1 ORDER BY (SELECT c FROM t2 WHERE b > 2 );
--error ER_SUBQUERY_NO_1_ROW
SELECT a FROM t1 ORDER BY (SELECT c FROM t2 WHERE b > 1);
SELECT a FROM t1 ORDER BY (SELECT c FROM t2 WHERE b > 2), a;
--error ER_SUBQUERY_NO_1_ROW
SELECT a FROM t1 ORDER BY (SELECT c FROM t2 WHERE b > 1), a;

SELECT b, MAX(c) FROM t2 GROUP BY b, (SELECT c FROM t2 WHERE b > 2);
--error ER_SUBQUERY_NO_1_ROW
SELECT b, MAX(c) FROM t2 GROUP BY b, (SELECT c FROM t2 WHERE b > 1);


SELECT a FROM t1 GROUP BY a
  HAVING IFNULL((SELECT b FROM t2 WHERE b > 2),
                (SELECT c FROM t2 WHERE c=a AND b > 2 ORDER BY b)) > 3;
--error ER_SUBQUERY_NO_1_ROW
SELECT a FROM t1 GROUP BY a
  HAVING IFNULL((SELECT b FROM t2 WHERE b > 1),
                (SELECT c FROM t2 WHERE c=a AND b > 2 ORDER BY b)) > 3;

SELECT a FROM t1 GROUP BY a
  HAVING IFNULL((SELECT b FROM t2 WHERE b > 4),
                (SELECT c FROM t2 WHERE c=a AND b > 2 ORDER BY b)) > 3;
--error ER_SUBQUERY_NO_1_ROW
SELECT a FROM t1 GROUP BY a
  HAVING IFNULL((SELECT b FROM t2 WHERE b > 4),
                (SELECT c FROM t2 WHERE c=a AND b > 1 ORDER BY b)) > 3;

SELECT a FROM t1
  ORDER BY IFNULL((SELECT b FROM t2 WHERE b > 2),
                  (SELECT c FROM t2 WHERE c=a AND b > 2 ORDER BY b));
--error ER_SUBQUERY_NO_1_ROW
SELECT a FROM t1
  ORDER BY IFNULL((SELECT b FROM t2 WHERE b > 1),
                  (SELECT c FROM t2 WHERE c=a AND b > 1 ORDER BY b));

SELECT a FROM t1
  ORDER BY IFNULL((SELECT b FROM t2 WHERE b > 4),
                  (SELECT c FROM t2 WHERE c=a AND b > 2 ORDER BY b));
--error ER_SUBQUERY_NO_1_ROW
SELECT a FROM t1
  ORDER BY IFNULL((SELECT b FROM t2 WHERE b > 4),
                  (SELECT c FROM t2 WHERE c=a AND b > 1 ORDER BY b));

DROP TABLE t1,t2;

# End of 4.1 tests

#
#decimal-related tests
#
create table t1 (df decimal(5,1));
insert into t1 values(1.1);
insert into t1 values(2.2);

select * from t1 where df <= all (select avg(df) from t1 group by df);
select * from t1 where df >= all (select avg(df) from t1 group by df);
drop table t1;

create table t1 (df decimal(5,1));
insert into t1 values(1.1);
select 1.1 * exists(select * from t1);
drop table t1;

CREATE TABLE t1 (
  grp int(11) default NULL,
  a decimal(10,2) default NULL);

insert into t1 values (1, 1), (2, 2), (2, 3), (3, 4), (3, 5), (3, 6), (NULL, NULL);
select * from t1;
select min(a) from t1 group by grp;
drop table t1;


#
# Test for Bug#9338 lame substitution of c1 instead of c2
#

CREATE table t1 ( c1 integer );
INSERT INTO t1 VALUES ( 1 );
INSERT INTO t1 VALUES ( 2 );
INSERT INTO t1 VALUES ( 3 );

CREATE TABLE t2 ( c2 integer );
INSERT INTO t2 VALUES ( 1 );
INSERT INTO t2 VALUES ( 4 );
INSERT INTO t2 VALUES ( 5 );

SELECT * FROM t1 LEFT JOIN t2 ON c1 = c2 WHERE c2 IN (1);

SELECT * FROM t1 LEFT JOIN t2 ON c1 = c2
  WHERE c2 IN ( SELECT c2 FROM t2 WHERE c2 IN ( 1 ) );

DROP TABLE t1,t2;


#
# Test for Bug#9516 wrong evaluation of not_null_tables attribute in SQ
#
CREATE TABLE t1 ( c1 integer );
INSERT INTO t1 VALUES ( 1 );
INSERT INTO t1 VALUES ( 2 );
INSERT INTO t1 VALUES ( 3 );
INSERT INTO t1 VALUES ( 6 );

CREATE TABLE t2 ( c2 integer );
INSERT INTO t2 VALUES ( 1 );
INSERT INTO t2 VALUES ( 4 );
INSERT INTO t2 VALUES ( 5 );
INSERT INTO t2 VALUES ( 6 );

CREATE TABLE t3 ( c3 integer );
INSERT INTO t3 VALUES ( 7 );
INSERT INTO t3 VALUES ( 8 );

SELECT c1,c2 FROM t1 LEFT JOIN t2 ON c1 = c2
  WHERE EXISTS (SELECT c3 FROM t3 WHERE c2 IS NULL );

DROP TABLE t1,t2,t3;


#
# Item_int_with_ref check (Bug#10020)
#
CREATE TABLE `t1` (
  `itemid` bigint(20) unsigned NOT NULL auto_increment,
  `sessionid` bigint(20) unsigned default NULL,
  `time` int(10) unsigned NOT NULL default '0',
  `type` set('A','D','E','F','G','I','L','N','U') collate latin1_general_ci NOT
NULL default '',
  `data` text collate latin1_general_ci NOT NULL,
  PRIMARY KEY  (`itemid`)
) DEFAULT CHARSET=latin1 COLLATE=latin1_general_ci;
INSERT INTO `t1` VALUES (1, 1, 1, 'D', '');
CREATE TABLE `t2` (
  `sessionid` bigint(20) unsigned NOT NULL auto_increment,
  `pid` int(10) unsigned NOT NULL default '0',
  `date` int(10) unsigned NOT NULL default '0',
  `ip` varchar(15) collate latin1_general_ci NOT NULL default '',
  PRIMARY KEY  (`sessionid`)
) DEFAULT CHARSET=latin1 COLLATE=latin1_general_ci;
INSERT INTO `t2` VALUES (1, 1, 1, '10.10.10.1');
SELECT s.ip, count( e.itemid ) FROM `t1` e JOIN t2 s ON s.sessionid = e.sessionid WHERE e.sessionid = ( SELECT sessionid FROM t2 ORDER BY sessionid DESC LIMIT 1 ) GROUP BY s.ip HAVING count( e.itemid ) >0 LIMIT 0 , 30;
drop tables t1,t2;


#
# Correct building of equal fields list (do not include outer
# fields) (Bug#6384)
#
CREATE TABLE t1 (EMPNUM   CHAR(3));
CREATE TABLE t2 (EMPNUM   CHAR(3) );
INSERT INTO t1 VALUES ('E1'),('E2');
INSERT INTO t2 VALUES ('E1');
DELETE FROM t1
WHERE t1.EMPNUM NOT IN
      (SELECT t2.EMPNUM
       FROM t2
       WHERE t1.EMPNUM = t2.EMPNUM);
select * from t1;
DROP TABLE t1,t2;


#
# Test for Bug#11487 range access in a subquery
#

CREATE TABLE t1(select_id BIGINT, values_id BIGINT);
INSERT INTO t1 VALUES (1, 1);
CREATE TABLE t2 (select_id BIGINT, values_id BIGINT,
                 PRIMARY KEY(select_id,values_id));
INSERT INTO t2 VALUES (0, 1), (0, 2), (0, 3), (1, 5);

SELECT values_id FROM t1
WHERE values_id IN (SELECT values_id FROM t2
                    WHERE select_id IN (1, 0));
SELECT values_id FROM t1
WHERE values_id IN (SELECT values_id FROM t2
                    WHERE select_id BETWEEN 0 AND 1);
SELECT values_id FROM t1
WHERE values_id IN (SELECT values_id FROM t2
                    WHERE select_id = 0 OR select_id = 1);

DROP TABLE t1, t2;


# Bug#11821 Select from subselect using aggregate function on an enum segfaults
create table t1 (fld enum('0','1'));
insert into t1 values ('1');
select * from (select max(fld) from t1) as foo;
drop table t1;


#
# Test for Bug#11762 subquery with an aggregate function in HAVING
#

CREATE TABLE t1 (a int, b int);
CREATE TABLE t2 (c int, d int);
CREATE TABLE t3 (e int);

INSERT INTO t1 VALUES
  (1,10), (2,10), (1,20), (2,20), (3,20), (2,30), (4,40);
INSERT INTO t2 VALUES
  (2,10), (2,20), (4,10), (5,10), (3,20), (2,40);
INSERT INTO t3 VALUES (10), (30), (10), (20) ;

SELECT a, MAX(b), MIN(b) FROM t1 GROUP BY a;
SELECT * FROM t2;
SELECT * FROM t3;

SELECT a FROM t1 GROUP BY a
  HAVING a IN (SELECT c FROM t2 WHERE MAX(b)>20);
SELECT a FROM t1 GROUP BY a
  HAVING a IN (SELECT c FROM t2 WHERE MAX(b)<d);
SELECT a FROM t1 GROUP BY a
  HAVING a IN (SELECT c FROM t2 WHERE MAX(b)>d);
SELECT a FROM t1 GROUP BY a
  HAVING a IN (SELECT c FROM t2
                 WHERE d >= SOME(SELECT e FROM t3 WHERE MAX(b)=e));
SELECT a FROM t1 GROUP BY a
  HAVING a IN (SELECT c FROM t2
                 WHERE  EXISTS(SELECT e FROM t3 WHERE MAX(b)=e AND e <= d));
SELECT a FROM t1 GROUP BY a
  HAVING a IN (SELECT c FROM t2
                 WHERE d > SOME(SELECT e FROM t3 WHERE MAX(b)=e));
SELECT a FROM t1 GROUP BY a
  HAVING a IN (SELECT c FROM t2
                 WHERE  EXISTS(SELECT e FROM t3 WHERE MAX(b)=e AND e < d));
SELECT a FROM t1 GROUP BY a
  HAVING a IN (SELECT c FROM t2
                 WHERE MIN(b) < d AND
                       EXISTS(SELECT e FROM t3 WHERE MAX(b)=e AND e <= d));

SELECT a, SUM(a) FROM t1 GROUP BY a;

SELECT a FROM t1
   WHERE EXISTS(SELECT c FROM t2 GROUP BY c HAVING SUM(a) = c) GROUP BY a;
SELECT a FROM t1 GROUP BY a
   HAVING EXISTS(SELECT c FROM t2 GROUP BY c HAVING SUM(a) = c);

SELECT a FROM t1
   WHERE a < 3 AND
         EXISTS(SELECT c FROM t2 GROUP BY c HAVING SUM(a) != c) GROUP BY a;
SELECT a FROM t1
   WHERE a < 3 AND
         EXISTS(SELECT c FROM t2 GROUP BY c HAVING SUM(a) != c);

SELECT t1.a FROM t1 GROUP BY t1.a
  HAVING t1.a < ALL(SELECT t2.c FROM t2 GROUP BY t2.c
                       HAVING EXISTS(SELECT t3.e FROM t3 GROUP BY t3.e
                                       HAVING SUM(t1.a+t2.c) < t3.e/4));
SELECT t1.a FROM t1 GROUP BY t1.a
       HAVING t1.a > ALL(SELECT t2.c FROM t2
                           WHERE EXISTS(SELECT t3.e FROM t3 GROUP BY t3.e
                                          HAVING SUM(t1.a+t2.c) < t3.e/4));
-- error ER_INVALID_GROUP_FUNC_USE
SELECT t1.a FROM t1 GROUP BY t1.a
       HAVING t1.a > ALL(SELECT t2.c FROM t2
                           WHERE EXISTS(SELECT t3.e FROM t3
                                          WHERE SUM(t1.a+t2.c) < t3.e/4));
-- error ER_INVALID_GROUP_FUNC_USE
SELECT t1.a from t1 GROUP BY t1.a HAVING AVG(SUM(t1.b)) > 20;

SELECT t1.a FROM t1 GROUP BY t1.a
  HAVING t1.a IN (SELECT t2.c FROM t2 GROUP BY t2.c
                    HAVING AVG(t2.c+SUM(t1.b)) > 20);
SELECT t1.a FROM t1 GROUP BY t1.a
  HAVING t1.a IN (SELECT t2.c FROM t2 GROUP BY t2.c
                    HAVING AVG(SUM(t1.b)) > 20);

SELECT t1.a, SUM(b) AS sum  FROM t1 GROUP BY t1.a
  HAVING t1.a IN (SELECT t2.c FROM t2 GROUP BY t2.c
                    HAVING t2.c+sum > 20);

DROP TABLE t1,t2,t3;


#
# Bug#17366 Unchecked Item_int results in server crash
#
create table t1( f1 int,f2 int);
insert into t1 values (1,1),(2,2);
select tt.t from (select 'crash1' as t, f2 from t1) as tt left join t1 on tt.t = 'crash2' and tt.f2 = t1.f2 where tt.t = 'crash1';
drop table t1;


#
# Bug#18306 server crash on delete using subquery.
#

create table t1 (c int, key(c));
insert into t1 values (1142477582), (1142455969);
create table t2 (a int, b int);
insert into t2 values (2, 1), (1, 0);
delete from t1 where c <= 1140006215 and (select b from t2 where a = 2) = 1;
drop table t1, t2;


#
# Bug#7549 Missing error message for invalid view selection with subquery
#

CREATE TABLE t1 (a INT);

--error ER_BAD_FIELD_ERROR
CREATE VIEW v1 AS SELECT * FROM t1 WHERE no_such_column = ANY (SELECT 1);
--error ER_BAD_FIELD_ERROR
CREATE VIEW v2 AS SELECT * FROM t1 WHERE no_such_column = (SELECT 1);
--error ER_BAD_FIELD_ERROR
SELECT * FROM t1 WHERE no_such_column = ANY (SELECT 1);

DROP TABLE t1;


#
# Bug#19077 A nested materialized derived table is used before being populated.
#
create table t1 (i int, j bigint);
insert into t1 values (1, 2), (2, 2), (3, 2);
select * from (select min(i) from t1 where j=(select * from (select min(j) from t1) t2)) t3;
drop table t1;


#
# Bug#19700 subselect returning BIGINT always returned it as SIGNED
#
CREATE TABLE t1 (i BIGINT UNSIGNED);
INSERT INTO t1 VALUES (10000000000000000000); # > MAX SIGNED BIGINT 9323372036854775807
INSERT INTO t1 VALUES (1);

CREATE TABLE t2 (i BIGINT UNSIGNED);
INSERT INTO t2 VALUES (10000000000000000000); # same as first table
INSERT INTO t2 VALUES (1);

/* simple test */
SELECT t1.i FROM t1 JOIN t2 ON t1.i = t2.i;

/* subquery test */
SELECT t1.i FROM t1 WHERE t1.i = (SELECT MAX(i) FROM t2);

/* subquery test with cast*/
SELECT t1.i FROM t1 WHERE t1.i = CAST((SELECT MAX(i) FROM t2) AS UNSIGNED);

DROP TABLE t1;
DROP TABLE t2;


#
# Bug#20519 subselect with LIMIT M, N
#

CREATE TABLE t1 (
  id bigint(20) unsigned NOT NULL auto_increment,
  name varchar(255) NOT NULL,
  PRIMARY KEY  (id)
);
INSERT INTO t1 VALUES
  (1, 'Balazs'), (2, 'Joe'), (3, 'Frank');

CREATE TABLE t2 (
  id bigint(20) unsigned NOT NULL auto_increment,
  mid bigint(20) unsigned NOT NULL,
  date date NOT NULL,
  PRIMARY KEY  (id)
);
INSERT INTO t2 VALUES
  (1, 1, '2006-03-30'), (2, 2, '2006-04-06'), (3, 3, '2006-04-13'),
  (4, 2, '2006-04-20'), (5, 1, '2006-05-01');

SELECT *,
      (SELECT date FROM t2 WHERE mid = t1.id
         ORDER BY date DESC LIMIT 0, 1) AS date_last,
      (SELECT date FROM t2 WHERE mid = t1.id
         ORDER BY date DESC LIMIT 3, 1) AS date_next_to_last
  FROM t1;
SELECT *,
      (SELECT COUNT(*) FROM t2 WHERE mid = t1.id
         ORDER BY date DESC LIMIT 1, 1) AS date_count
  FROM t1;
SELECT *,
      (SELECT date FROM t2 WHERE mid = t1.id
         ORDER BY date DESC LIMIT 0, 1) AS date_last,
      (SELECT date FROM t2 WHERE mid = t1.id
         ORDER BY date DESC LIMIT 1, 1) AS date_next_to_last
  FROM t1;
DROP TABLE t1,t2;


#
# Bug#20869 subselect with range access by DESC
#

CREATE TABLE t1 (
  i1 int(11) NOT NULL default '0',
  i2 int(11) NOT NULL default '0',
  t datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (i1,i2,t)
);
INSERT INTO t1 VALUES
(24,1,'2005-03-03 16:31:31'),(24,1,'2005-05-27 12:40:07'),
(24,1,'2005-05-27 12:40:08'),(24,1,'2005-05-27 12:40:10'),
(24,1,'2005-05-27 12:40:25'),(24,1,'2005-05-27 12:40:30'),
(24,2,'2005-03-03 13:43:05'),(24,2,'2005-03-03 16:23:31'),
(24,2,'2005-03-03 16:31:30'),(24,2,'2005-05-27 12:37:02'),
(24,2,'2005-05-27 12:40:06');

CREATE TABLE t2 (
  i1 int(11) NOT NULL default '0',
  i2 int(11) NOT NULL default '0',
  t datetime default NULL,
  PRIMARY KEY  (i1)
);
INSERT INTO t2 VALUES (24,1,'2006-06-20 12:29:40');

EXPLAIN
SELECT * FROM t1,t2
  WHERE t1.t = (SELECT t1.t FROM t1
                  WHERE t1.t < t2.t  AND t1.i2=1 AND t2.i1=t1.i1
                    ORDER BY t1.t DESC LIMIT 1);
SELECT * FROM t1,t2
  WHERE t1.t = (SELECT t1.t FROM t1
                  WHERE t1.t < t2.t  AND t1.i2=1 AND t2.i1=t1.i1
                    ORDER BY t1.t DESC LIMIT 1);

DROP TABLE t1, t2;


#
# Bug#14654 Cannot select from the same table twice within a UNION statement
#
CREATE TABLE t1 (i INT);

(SELECT i FROM t1) UNION (SELECT i FROM t1);
SELECT sql_no_cache * FROM t1 WHERE NOT EXISTS
  (
   (SELECT i FROM t1) UNION
   (SELECT i FROM t1)
  );

#TODO:not supported
--error ER_PARSE_ERROR
SELECT * FROM t1
WHERE NOT EXISTS (((SELECT i FROM t1) UNION (SELECT i FROM t1)));

#TODO:not supported
--error ER_PARSE_ERROR
explain select ((select t11.i from t1 t11) union (select t12.i from t1 t12))
  from t1;

explain select * from t1 where not exists
  ((select t11.i from t1 t11) union (select t12.i from t1 t12));

DROP TABLE t1;


#
# Bug#21798 memory leak during query execution with subquery in column
#           list using a function
#
CREATE TABLE t1 (a VARCHAR(250), b INT auto_increment, PRIMARY KEY (b));
insert into t1 (a) values (FLOOR(rand() * 100));
insert into t1 (a) select FLOOR(rand() * 100) from t1;
insert into t1 (a) select FLOOR(rand() * 100) from t1;
insert into t1 (a) select FLOOR(rand() * 100) from t1;
insert into t1 (a) select FLOOR(rand() * 100) from t1;
insert into t1 (a) select FLOOR(rand() * 100) from t1;
insert into t1 (a) select FLOOR(rand() * 100) from t1;
insert into t1 (a) select FLOOR(rand() * 100) from t1;
insert into t1 (a) select FLOOR(rand() * 100) from t1;
insert into t1 (a) select FLOOR(rand() * 100) from t1;
insert into t1 (a) select FLOOR(rand() * 100) from t1;
insert into t1 (a) select FLOOR(rand() * 100) from t1;
insert into t1 (a) select FLOOR(rand() * 100) from t1;
insert into t1 (a) select FLOOR(rand() * 100) from t1;

SELECT a,
       (SELECT REPEAT(' ',250) FROM t1 i1
        WHERE i1.b=t1.a ORDER BY RAND() LIMIT 1) AS a
FROM t1 ORDER BY a LIMIT 5;
DROP TABLE t1;


#
# Bug#21540 Subqueries with no from and aggregate functions return
#           wrong results
CREATE TABLE t1 (a INT, b INT);
CREATE TABLE t2 (a INT);
INSERT INTO t2 values (1);
INSERT INTO t1 VALUES (1,1),(1,2),(2,3),(3,4);
SELECT (SELECT COUNT(DISTINCT t1.b) from t2) FROM t1 GROUP BY t1.a;
SELECT (SELECT COUNT(DISTINCT t1.b) from t2 union select 1 from t2 where 12 < 3)
  FROM t1 GROUP BY t1.a;
SELECT COUNT(DISTINCT t1.b), (SELECT COUNT(DISTINCT t1.b)) FROM t1 GROUP BY t1.a;
SELECT COUNT(DISTINCT t1.b),
       (SELECT COUNT(DISTINCT t1.b) union select 1 from DUAL where 12 < 3)
  FROM t1 GROUP BY t1.a;
SELECT (
    SELECT (
      SELECT COUNT(DISTINCT t1.b)
    )
)
FROM t1 GROUP BY t1.a;
SELECT (
  SELECT (
      SELECT (
        SELECT COUNT(DISTINCT t1.b)
      )
  )
  FROM t1 GROUP BY t1.a LIMIT 1)
FROM t1 t2
GROUP BY t2.a;
DROP TABLE t1,t2;


#
# Bug#21727 Correlated subquery that requires filesort:
#           slow with big sort_buffer_size
#

CREATE TABLE t1 (a int, b int, PRIMARY KEY (b));
CREATE TABLE t2 (x int auto_increment, y int, z int,
                 PRIMARY KEY (x), FOREIGN KEY (y) REFERENCES t1 (b));
create table t3 (a int);
insert into t3 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);

insert into t1 select RAND()*1000, A.a + 10*(B.a+10*(C.a+10*D.a))
from t3 A, t3 B, t3 C, t3 D where D.a<3;
insert into t2(y,z) select t1.b, RAND()*1000 from t1, t3;
enable_query_log;

SET SESSION sort_buffer_size = 32 * 1024;
SELECT SQL_NO_CACHE COUNT(*)
  FROM (SELECT  a, b, (SELECT x FROM t2 WHERE y=b ORDER BY z DESC LIMIT 1) c
          FROM t1) t;

SET SESSION sort_buffer_size = 8 * 1024 * 1024;
SELECT SQL_NO_CACHE COUNT(*)
  FROM (SELECT  a, b, (SELECT x FROM t2 WHERE y=b ORDER BY z DESC LIMIT 1) c
          FROM t1) t;

DROP TABLE t1,t2,t3;


#
# Bug#25219 EXIST subquery with UNION over a mix of
#           correlated and uncorrelated selects
#

CREATE TABLE t1 (id char(4) PRIMARY KEY, c int);
CREATE TABLE t2 (c int);

INSERT INTO t1 VALUES ('aa', 1);
INSERT INTO t2 VALUES (1);

SELECT * FROM t1
  WHERE EXISTS (SELECT c FROM t2 WHERE c=1
                UNION
                SELECT c from t2 WHERE c=t1.c);

INSERT INTO t1 VALUES ('bb', 2), ('cc', 3), ('dd',1);

SELECT * FROM t1
  WHERE EXISTS (SELECT c FROM t2 WHERE c=1
                UNION
                SELECT c from t2 WHERE c=t1.c);

INSERT INTO t2 VALUES (2);
CREATE TABLE t3 (c int);
INSERT INTO t3 VALUES (1);

SELECT * FROM t1
  WHERE EXISTS (SELECT t2.c FROM t2 JOIN t3 ON t2.c=t3.c WHERE t2.c=1
                UNION
                SELECT c from t2 WHERE c=t1.c);

DROP TABLE t1,t2,t3;


#
# Bug#23800 Outer fields in correlated subqueries is used in a temporary
#           table created for sorting.
#
CREATE TABLE t1(f1 int);
CREATE TABLE t2(f2 int, f21 int, f3 timestamp);
INSERT INTO t1 VALUES (1),(1),(2),(2);
INSERT INTO t2 VALUES (1,1,"2004-02-29 11:11:11"), (2,2,"2004-02-29 11:11:11");
SELECT ((SELECT f2 FROM t2 WHERE f21=f1 LIMIT 1) * COUNT(f1)) AS sq FROM t1 GROUP BY f1;
SELECT (SELECT SUM(1) FROM t2 ttt GROUP BY t2.f3 LIMIT 1) AS tt FROM t2;
PREPARE stmt1 FROM 'SELECT ((SELECT f2 FROM t2 WHERE f21=f1 LIMIT 1) * COUNT(f1)) AS sq FROM t1 GROUP BY f1';
EXECUTE stmt1;
EXECUTE stmt1;
DEALLOCATE PREPARE stmt1;
SELECT f2, AVG(f21),
      (SELECT t.f3 FROM t2 AS t WHERE t2.f2=t.f2 AND t.f3=MAX(t2.f3)) AS test
  FROM t2 GROUP BY f2;
DROP TABLE t1,t2;
CREATE TABLE t1 (a int, b INT, c CHAR(10) NOT NULL);
INSERT INTO t1 VALUES
  (1,1,'a'), (1,2,'b'), (1,3,'c'), (1,4,'d'), (1,5,'e'),
  (2,1,'f'), (2,2,'g'), (2,3,'h'), (3,4,'i'), (3,3,'j'),
  (3,2,'k'), (3,1,'l'), (1,9,'m');
SELECT a, MAX(b),
      (SELECT t.c FROM t1 AS t WHERE t1.a=t.a AND t.b=MAX(t1.b)) AS test
  FROM t1 GROUP BY a;
DROP TABLE t1;


#
# Bug#21904 (parser problem when using IN with a double "(())")
#

--disable_warnings
DROP TABLE IF EXISTS t1;
DROP TABLE IF EXISTS t2;
DROP TABLE IF EXISTS t1xt2;
--enable_warnings

CREATE TABLE t1 (
  id_1 int(5) NOT NULL,
  t varchar(4) DEFAULT NULL
);

CREATE TABLE t2 (
  id_2 int(5) NOT NULL,
  t varchar(4) DEFAULT NULL
);

CREATE TABLE t1xt2 (
  id_1 int(5) NOT NULL,
  id_2 int(5) NOT NULL
);

INSERT INTO t1 VALUES (1, 'a'), (2, 'b'), (3, 'c'), (4, 'd');

INSERT INTO t2 VALUES (2, 'bb'), (3, 'cc'), (4, 'dd'), (12, 'aa');

INSERT INTO t1xt2 VALUES (2, 2), (3, 3), (4, 4);

# subselect returns 0 rows

SELECT DISTINCT t1.id_1 FROM t1 WHERE
(12 IN (SELECT t1xt2.id_2 FROM t1xt2 WHERE t1.id_1 = t1xt2.id_1));

SELECT DISTINCT t1.id_1 FROM t1 WHERE
(12 IN ((SELECT t1xt2.id_2 FROM t1xt2 WHERE t1.id_1 = t1xt2.id_1)));

SELECT DISTINCT t1.id_1 FROM t1 WHERE
(12 IN (((SELECT t1xt2.id_2 FROM t1xt2 WHERE t1.id_1 = t1xt2.id_1))));

SELECT DISTINCT t1.id_1 FROM t1 WHERE
(12 NOT IN (SELECT t1xt2.id_2 FROM t1xt2 WHERE t1.id_1 = t1xt2.id_1));

SELECT DISTINCT t1.id_1 FROM t1 WHERE
(12 NOT IN ((SELECT t1xt2.id_2 FROM t1xt2 where t1.id_1 = t1xt2.id_1)));

SELECT DISTINCT t1.id_1 FROM t1 WHERE
(12 NOT IN (((SELECT t1xt2.id_2 FROM t1xt2 where t1.id_1 = t1xt2.id_1))));

insert INTO t1xt2 VALUES (1, 12);

# subselect returns 1 row

SELECT DISTINCT t1.id_1 FROM t1 WHERE
(12 IN (SELECT t1xt2.id_2 FROM t1xt2 WHERE t1.id_1 = t1xt2.id_1));

SELECT DISTINCT t1.id_1 FROM t1 WHERE
(12 IN ((SELECT t1xt2.id_2 FROM t1xt2 WHERE t1.id_1 = t1xt2.id_1)));

SELECT DISTINCT t1.id_1 FROM t1 WHERE
(12 IN (((SELECT t1xt2.id_2 FROM t1xt2 WHERE t1.id_1 = t1xt2.id_1))));

SELECT DISTINCT t1.id_1 FROM t1 WHERE
(12 NOT IN (SELECT t1xt2.id_2 FROM t1xt2 WHERE t1.id_1 = t1xt2.id_1));

SELECT DISTINCT t1.id_1 FROM t1 WHERE
(12 NOT IN ((SELECT t1xt2.id_2 FROM t1xt2 WHERE t1.id_1 = t1xt2.id_1)));

SELECT DISTINCT t1.id_1 FROM t1 WHERE
(12 NOT IN (((SELECT t1xt2.id_2 FROM t1xt2 WHERE t1.id_1 = t1xt2.id_1))));

insert INTO t1xt2 VALUES (2, 12);

# subselect returns more than 1 row

SELECT DISTINCT t1.id_1 FROM t1 WHERE
(12 IN (SELECT t1xt2.id_2 FROM t1xt2 WHERE t1.id_1 = t1xt2.id_1));

SELECT DISTINCT t1.id_1 FROM t1 WHERE
(12 IN ((SELECT t1xt2.id_2 FROM t1xt2 WHERE t1.id_1 = t1xt2.id_1)));

SELECT DISTINCT t1.id_1 FROM t1 WHERE
(12 IN (((SELECT t1xt2.id_2 FROM t1xt2 WHERE t1.id_1 = t1xt2.id_1))));

SELECT DISTINCT t1.id_1 FROM t1 WHERE
(12 NOT IN (SELECT t1xt2.id_2 FROM t1xt2 WHERE t1.id_1 = t1xt2.id_1));

SELECT DISTINCT t1.id_1 FROM t1 WHERE
(12 NOT IN ((SELECT t1xt2.id_2 FROM t1xt2 WHERE t1.id_1 = t1xt2.id_1)));

SELECT DISTINCT t1.id_1 FROM t1 WHERE
(12 NOT IN (((SELECT t1xt2.id_2 FROM t1xt2 WHERE t1.id_1 = t1xt2.id_1))));

DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t1xt2;


#
# Bug#26728 derived table with concatanation of literals in select list
#

CREATE TABLE t1 (a int);
INSERT INTO t1 VALUES (3), (1), (2);

SELECT 'this is ' 'a test.' AS col1, a AS col2 FROM t1;
SELECT * FROM (SELECT 'this is ' 'a test.' AS col1, a AS t2 FROM t1) t;

DROP table t1;


#
# Bug#27257 COUNT(*) aggregated in outer query
#

CREATE TABLE t1 (a int, b int);
CREATE TABLE t2 (m int, n int);
INSERT INTO t1 VALUES (2,2), (2,2), (3,3), (3,3), (3,3), (4,4);
INSERT INTO t2 VALUES (1,11), (2,22), (3,32), (4,44), (4,44);

SELECT COUNT(*), a,
       (SELECT m FROM t2 WHERE m = count(*) LIMIT 1)
  FROM t1 GROUP BY a;

SELECT COUNT(*), a,
       (SELECT MIN(m) FROM t2 WHERE m = count(*))
  FROM t1 GROUP BY a;

SELECT COUNT(*), a
  FROM t1 GROUP BY a
    HAVING (SELECT MIN(m) FROM t2 WHERE m = count(*)) > 1;

DROP TABLE t1,t2;


#
# Bug#27229 GROUP_CONCAT in subselect with COUNT() as an argument
#

CREATE TABLE t1 (a int, b int);
CREATE TABLE t2 (m int, n int);
INSERT INTO t1 VALUES (2,2), (2,2), (3,3), (3,3), (3,3), (4,4);
INSERT INTO t2 VALUES (1,11), (2,22), (3,32), (4,44), (4,44);

SELECT COUNT(*) c, a,
       (SELECT GROUP_CONCAT(COUNT(a)) FROM t2 WHERE m = a)
  FROM t1 GROUP BY a;

SELECT COUNT(*) c, a,
       (SELECT GROUP_CONCAT(COUNT(a)+1) FROM t2 WHERE m = a)
  FROM t1 GROUP BY a;

DROP table t1,t2;


#
# Bug#27321 Wrong subquery result in a grouping select
#
CREATE TABLE t1 (a int, b INT, d INT, c CHAR(10) NOT NULL, PRIMARY KEY (a, b));
INSERT INTO t1 VALUES (1,1,0,'a'), (1,2,0,'b'), (1,3,0,'c'), (1,4,0,'d'),
(1,5,0,'e'), (2,1,0,'f'), (2,2,0,'g'), (2,3,0,'h'), (3,4,0,'i'), (3,3,0,'j'),
(3,2,0,'k'), (3,1,0,'l'), (1,9,0,'m'), (1,0,10,'n'), (2,0,5,'o'), (3,0,7,'p');

SELECT a, MAX(b),
  (SELECT t.c FROM t1 AS t WHERE t1.a=t.a AND t.b=MAX(t1.b + 0)) as test
  FROM t1 GROUP BY a;
SELECT a x, MAX(b),
  (SELECT t.c FROM t1 AS t WHERE x=t.a AND t.b=MAX(t1.b + 0)) as test
  FROM t1 GROUP BY a;
SELECT a, AVG(b),
  (SELECT t.c FROM t1 AS t WHERE t1.a=t.a AND t.b=AVG(t1.b)) AS test
  FROM t1 WHERE t1.d=0 GROUP BY a;

SELECT tt.a,
 (SELECT (SELECT c FROM t1 as t WHERE t1.a=t.a AND t.d=MAX(t1.b + tt.a)
  LIMIT 1) FROM t1 WHERE t1.a=tt.a GROUP BY a LIMIT 1) as test
  FROM t1 as tt;

SELECT tt.a,
 (SELECT (SELECT t.c FROM t1 AS t WHERE t1.a=t.a AND t.d=MAX(t1.b + tt.a)
  LIMIT 1)
  FROM t1 WHERE t1.a=tt.a GROUP BY a LIMIT 1) as test
  FROM t1 as tt GROUP BY tt.a;

SELECT tt.a, MAX(
 (SELECT (SELECT t.c FROM t1 AS t WHERE t1.a=t.a AND t.d=MAX(t1.b + tt.a)
  LIMIT 1)
  FROM t1 WHERE t1.a=tt.a GROUP BY a LIMIT 1)) as test
  FROM t1 as tt GROUP BY tt.a;

DROP TABLE t1;


#
# Bug#27348 SET FUNCTION used in a subquery from WHERE condition
#

CREATE TABLE t1 (a int, b int);
INSERT INTO t1 VALUES (2,22),(1,11),(2,22);

SELECT a FROM t1 WHERE (SELECT COUNT(b) FROM DUAL) > 0 GROUP BY a;
SELECT a FROM t1 WHERE (SELECT COUNT(b) FROM DUAL) > 1 GROUP BY a;

SELECT a FROM t1 t0
  WHERE (SELECT COUNT(t0.b) FROM t1 t WHERE t.b>20) GROUP BY a;

SET @@sql_mode='ansi';
--error ER_INVALID_GROUP_FUNC_USE
SELECT a FROM t1 WHERE (SELECT COUNT(b) FROM DUAL) > 0 GROUP BY a;
--error ER_INVALID_GROUP_FUNC_USE
SELECT a FROM t1 WHERE (SELECT COUNT(b) FROM DUAL) > 1 GROUP BY a;

--error ER_INVALID_GROUP_FUNC_USE
SELECT a FROM t1 t0
  WHERE (SELECT COUNT(t0.b) FROM t1 t WHERE t.b>20) GROUP BY a;

SET @@sql_mode=default;

DROP TABLE t1;


#
# Bug#27363 nested aggregates in outer, subquery / sum(select count(outer))
#
CREATE TABLE t1 (a INT); INSERT INTO t1 values (1),(1),(1),(1);
CREATE TABLE t2 (x INT); INSERT INTO t1 values (1000),(1001),(1002);

--error ER_INVALID_GROUP_FUNC_USE
SELECT SUM( (SELECT COUNT(a) FROM t2) ) FROM t1;
--error ER_INVALID_GROUP_FUNC_USE
SELECT SUM( (SELECT SUM(COUNT(a)) FROM t2) ) FROM t1;
SELECT COUNT(1) FROM DUAL;

--error ER_INVALID_GROUP_FUNC_USE
SELECT SUM( (SELECT AVG( (SELECT t1.a FROM t2) ) FROM DUAL) ) FROM t1;

--error ER_INVALID_GROUP_FUNC_USE
SELECT
  SUM( (SELECT AVG( (SELECT COUNT(*) FROM t1 t HAVING t1.a < 12) ) FROM t2) )
FROM t1;

--error ER_INVALID_GROUP_FUNC_USE
SELECT t1.a as XXA,
   SUM( (SELECT AVG( (SELECT COUNT(*) FROM t1 t HAVING XXA < 12) ) FROM t2) )
FROM t1;

DROP TABLE t1,t2;


#
# Bug#27807 Server crash when executing subquery with EXPLAIN
#
CREATE TABLE t1 (a int, b int, KEY (a));
INSERT INTO t1 VALUES (1,1),(2,1);
EXPLAIN SELECT 1 FROM t1 WHERE a = (SELECT COUNT(*) FROM t1 GROUP BY b);
DROP TABLE t1;


#
# Bug#28377 grouping query with a correlated subquery in WHERE condition
#

CREATE TABLE t1 (id int NOT NULL, st CHAR(2), INDEX idx(id));
INSERT INTO t1 VALUES
  (3,'FL'), (2,'GA'), (4,'FL'), (1,'GA'), (5,'NY'), (7,'FL'), (6,'NY');
CREATE TABLE t2 (id int NOT NULL, INDEX idx(id));
INSERT INTO t2 VALUES (7), (5), (1), (3);

SELECT id, st FROM t1
  WHERE st IN ('GA','FL') AND EXISTS(SELECT 1 FROM t2 WHERE t2.id=t1.id);
SELECT id, st FROM t1
  WHERE st IN ('GA','FL') AND EXISTS(SELECT 1 FROM t2 WHERE t2.id=t1.id)
    GROUP BY id;

SELECT id, st FROM t1
  WHERE st IN ('GA','FL') AND NOT EXISTS(SELECT 1 FROM t2 WHERE t2.id=t1.id);
SELECT id, st FROM t1
  WHERE st IN ('GA','FL') AND NOT EXISTS(SELECT 1 FROM t2 WHERE t2.id=t1.id)
    GROUP BY id;

DROP TABLE t1,t2;


#
# Bug#28728 crash with EXPLAIN EXTENDED for a query with a derived table
#           over a grouping subselect
#

CREATE TABLE t1 (a int);

INSERT INTO t1 VALUES (1), (2);

EXPLAIN EXTENDED
SELECT * FROM (SELECT count(*) FROM t1 GROUP BY a) as res;

DROP TABLE t1;


#
# Bug#28811 crash for query containing subquery with ORDER BY and LIMIT 1
#

CREATE TABLE t1 (
  a varchar(255) default NULL,
  b timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  INDEX idx(a,b)
);
CREATE TABLE t2 (
  a varchar(255) default NULL
);

INSERT INTO t1 VALUES ('abcdefghijk','2007-05-07 06:00:24');
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO `t1` VALUES ('asdf','2007-02-08 01:11:26');
INSERT INTO `t2` VALUES ('abcdefghijk');
INSERT INTO `t2` VALUES ('asdf');

SET session sort_buffer_size=8192;

SELECT (SELECT 1 FROM  t1 WHERE t1.a=t2.a ORDER BY t1.b LIMIT 1) AS d1 FROM t2;

DROP TABLE t1,t2;


#
# Bug#27333 subquery grouped for aggregate of outer query / no aggregate
#           of subquery
#
CREATE TABLE t1 (a INTEGER, b INTEGER);
CREATE TABLE t2 (x INTEGER);
INSERT INTO t1 VALUES (1,11), (2,22), (2,22);
INSERT INTO t2 VALUES (1), (2);

# wasn't failing, but should
--error ER_SUBQUERY_NO_1_ROW
SELECT a, COUNT(b), (SELECT COUNT(b) FROM t2) FROM t1 GROUP BY a;

# fails as it should
--error ER_SUBQUERY_NO_1_ROW
SELECT a, COUNT(b), (SELECT COUNT(b)+0 FROM t2) FROM t1 GROUP BY a;

SELECT (SELECT SUM(t1.a)/AVG(t2.x) FROM t2) FROM t1;
DROP TABLE t1,t2;

# second test case from 27333
CREATE TABLE t1 (a INT, b INT);
INSERT INTO t1 VALUES (1, 2), (1,3), (1,4), (2,1), (2,2);

# returns no rows, when it should
SELECT a1.a, COUNT(*) FROM t1 a1 WHERE a1.a = 1
AND EXISTS( SELECT a2.a FROM t1 a2 WHERE a2.a = a1.a)
GROUP BY a1.a;
DROP TABLE t1;

#test cases from 29297
CREATE TABLE t1 (a INT);
CREATE TABLE t2 (a INT);
INSERT INTO t1 VALUES (1),(2);
INSERT INTO t2 VALUES (1),(2);
SELECT (SELECT SUM(t1.a) FROM t2 WHERE a=0) FROM t1;
--error ER_SUBQUERY_NO_1_ROW
SELECT (SELECT SUM(t1.a) FROM t2 WHERE a!=0) FROM t1;
SELECT (SELECT SUM(t1.a) FROM t2 WHERE a=1) FROM t1;
DROP TABLE t1,t2;

#
# Bug#31884 Assertion + crash in subquery in the SELECT clause.
#

CREATE TABLE t1 (a1 INT, a2 INT);
CREATE TABLE t2 (b1 INT, b2 INT);

INSERT INTO t1 VALUES (100, 200);
INSERT INTO t1 VALUES (101, 201);
INSERT INTO t2 VALUES (101, 201);
INSERT INTO t2 VALUES (103, 203);

SELECT ((a1,a2) IN (SELECT * FROM t2 WHERE b2 > 0)) IS NULL FROM t1;
DROP TABLE t1, t2;

#
# Bug#28076 inconsistent binary/varbinary comparison
#

CREATE TABLE t1 (s1 BINARY(5), s2 VARBINARY(5));
INSERT INTO t1 VALUES (0x41,0x41), (0x42,0x42), (0x43,0x43);

SELECT s1, s2 FROM t1 WHERE s2 IN (SELECT s1 FROM t1);
SELECT s1, s2 FROM t1 WHERE (s2, 10) IN (SELECT s1, 10 FROM t1);

CREATE INDEX I1 ON t1 (s1);
CREATE INDEX I2 ON t1 (s2);

SELECT s1, s2 FROM t1 WHERE s2 IN (SELECT s1 FROM t1);
SELECT s1, s2 FROM t1 WHERE (s2, 10) IN (SELECT s1, 10 FROM t1);

TRUNCATE t1;
INSERT INTO t1 VALUES (0x41,0x41);
SELECT * FROM t1 WHERE s1 = (SELECT s2 FROM t1);

DROP TABLE t1;

CREATE TABLE t1 (a1 VARBINARY(2) NOT NULL DEFAULT '0', PRIMARY KEY (a1));
CREATE TABLE t2 (a2 BINARY(2) default '0', INDEX (a2));
CREATE TABLE t3 (a3 BINARY(2) default '0');
INSERT INTO t1 VALUES (1),(2),(3),(4);
INSERT INTO t2 VALUES (1),(2),(3);
INSERT INTO t3 VALUES (1),(2),(3);
SELECT LEFT(t2.a2, 1) FROM t2,t3 WHERE t3.a3=t2.a2;
SELECT t1.a1, t1.a1 in (SELECT t2.a2 FROM t2,t3 WHERE t3.a3=t2.a2) FROM t1;
DROP TABLE t1,t2,t3;

CREATE TABLE t1 (a1 BINARY(3) PRIMARY KEY, b1 VARBINARY(3));
CREATE TABLE t2 (a2 VARBINARY(3) PRIMARY KEY);
CREATE TABLE t3 (a3 VARBINARY(3) PRIMARY KEY);
INSERT INTO t1 VALUES (1,10), (2,20), (3,30), (4,40);
INSERT INTO t2 VALUES (2), (3), (4), (5);
INSERT INTO t3 VALUES (10), (20), (30);
SELECT LEFT(t1.a1,1) FROM t1,t3 WHERE t1.b1=t3.a3;
SELECT a2 FROM t2 WHERE t2.a2 IN (SELECT t1.a1 FROM t1,t3 WHERE t1.b1=t3.a3);
DROP TABLE t1, t2, t3;


#
# Bug#32400 Complex SELECT query returns correct result only on some occasions
#

CREATE TABLE t1(a INT, b INT);
INSERT INTO t1 VALUES (1,1), (1,2), (2,3), (2,4);

--error ER_BAD_FIELD_ERROR
EXPLAIN
SELECT a AS out_a, MIN(b) FROM t1
WHERE b > (SELECT MIN(b) FROM t1 WHERE a = out_a)
GROUP BY a;

--error ER_BAD_FIELD_ERROR
SELECT a AS out_a, MIN(b) FROM t1
WHERE b > (SELECT MIN(b) FROM t1 WHERE a = out_a)
GROUP BY a;

EXPLAIN
SELECT a AS out_a, MIN(b) FROM t1 t1_outer
WHERE b > (SELECT MIN(b) FROM t1 WHERE a = t1_outer.a)
GROUP BY a;

SELECT a AS out_a, MIN(b) FROM t1 t1_outer
WHERE b > (SELECT MIN(b) FROM t1 WHERE a = t1_outer.a)
GROUP BY a;

DROP TABLE t1;


#
# Bug#32036 EXISTS within a WHERE clause with a UNION crashes MySQL 5.122
#

CREATE TABLE t1 (a INT);
CREATE TABLE t2 (a INT);

INSERT INTO t1 VALUES (1),(2);
INSERT INTO t2 VALUES (1),(2);

SELECT 2 FROM t1 WHERE EXISTS ((SELECT 1 FROM t2 WHERE t1.a=t2.a));
EXPLAIN EXTENDED
SELECT 2 FROM t1 WHERE EXISTS ((SELECT 1 FROM t2 WHERE t1.a=t2.a));


EXPLAIN EXTENDED
SELECT 2 FROM t1 WHERE EXISTS ((SELECT 1 FROM t2 WHERE t1.a=t2.a) UNION
                               (SELECT 1 FROM t2 WHERE t1.a = t2.a));

DROP TABLE t1,t2;


#
# Bug#33675 Usage of an uninitialized memory by filesort in a subquery
#           caused server crash.
#
create table t0(a int);
insert into t0 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);
create table t1(f11 int, f12 int);
create table t2(f21 int unsigned not null, f22 int, f23 varchar(10));
insert into t1 values(1,1),(2,2), (3, 3);
--disable_warnings
insert into t2 
select -1 , (@a:=(A.a + 10 * (B.a + 10 * (C.a+10*D.a))))/5000 + 1, @a 
from t0 A, t0 B, t0 C, t0 D;
--enable_warnings
set session sort_buffer_size= 33*1024;
select count(*) from t1 where f12 =
(select f22 from t2 where f22 = f12 order by f21 desc, f22, f23 limit 1);

drop table t0,t1,t2;


#
# Bug#33794 "MySQL crashes executing specific query on specific dump"
#
CREATE TABLE t4 (
  f7 varchar(32) collate utf8_bin NOT NULL default '',
  f10 varchar(32) collate utf8_bin default NULL,
  PRIMARY KEY  (f7)
);
INSERT INTO t4 VALUES(1,1), (2,null);

CREATE TABLE t2 (
  f4 varchar(32) collate utf8_bin NOT NULL default '',
  f2 varchar(50) collate utf8_bin default NULL,
  f3 varchar(10) collate utf8_bin default NULL,
  PRIMARY KEY  (f4),
  UNIQUE KEY uk1 (f2)
);
INSERT INTO t2 VALUES(1,1,null), (2,2,null);

CREATE TABLE t1 (
  f8 varchar(32) collate utf8_bin NOT NULL default '',
  f1 varchar(10) collate utf8_bin default NULL,
  f9 varchar(32) collate utf8_bin default NULL,
  PRIMARY KEY  (f8)
);
INSERT INTO t1 VALUES (1,'P',1), (2,'P',1), (3,'R',2);

CREATE TABLE t3 (
  f6 varchar(32) collate utf8_bin NOT NULL default '',
  f5 varchar(50) collate utf8_bin default NULL,
  PRIMARY KEY (f6)
);
INSERT INTO t3 VALUES (1,null), (2,null);

SELECT
  IF(t1.f1 = 'R', a1.f2, t2.f2) AS a4,
  IF(t1.f1 = 'R', a1.f3, t2.f3) AS f3,
  SUM(
    IF(
      (SELECT VPC.f2
       FROM t2 VPC, t4 a2, t2 a3
       WHERE
         VPC.f4 = a2.f10 AND a3.f2 = a4
       LIMIT 1) IS NULL,
       0,
       t3.f5
    )
  ) AS a6
FROM
  t2, t3, t1 JOIN t2 a1 ON t1.f9 = a1.f4
GROUP BY a4;

DROP TABLE t1, t2, t3, t4;


#
# Bug#36139 "float, zerofill, crash with subquery"
#
create table t1 (a float(5,4) zerofill);
create table t2 (a float(5,4),b float(2,0));

select t1.a from t1 where
  t1.a= (select b from t2 limit 1) and not
  t1.a= (select a from t2 limit 1) ;

drop table t1, t2;


--echo #
--echo # Bug#45061: Incorrectly market field caused wrong result.
--echo #
CREATE TABLE `C` (
  `int_nokey` int(11) NOT NULL,
  `int_key` int(11) NOT NULL,
  KEY `int_key` (`int_key`)
);

INSERT INTO `C` VALUES (9,9), (0,0), (8,6), (3,6), (7,6), (0,4),
(1,7), (9,4), (0,8), (9,4), (0,7), (5,5), (0,0), (8,5), (8,7),
(5,2), (1,8), (7,0), (0,9), (9,5);

--disable_warnings
SELECT * FROM C WHERE `int_key` IN (SELECT `int_nokey`);
EXPLAIN EXTENDED SELECT * FROM C WHERE `int_key` IN (SELECT `int_nokey`);
--enable_warnings

DROP TABLE C;
--echo # End of test for bug#45061.


--echo #
--echo # Bug #46749: Segfault in add_key_fields() with outer subquery level 
--echo #   field references
--echo #

CREATE TABLE t1 (
  a int,
  b int,
  UNIQUE (a), KEY (b)
);
INSERT INTO t1 VALUES (1,1), (2,1);

CREATE TABLE st1 like t1;
INSERT INTO st1 VALUES (1,1), (2,1);

CREATE TABLE st2 like t1;
INSERT INTO st2 VALUES (1,1), (2,1);

# should have "impossible where"
EXPLAIN
SELECT MAX(b), (SELECT COUNT(*) FROM st1,st2 WHERE st2.b <= t1.b)
FROM t1 
WHERE a = 230;

# should not crash
SELECT MAX(b), (SELECT COUNT(*) FROM st1,st2 WHERE st2.b <= t1.b)
FROM t1 
WHERE a = 230;

DROP TABLE t1, st1, st2;

--echo #
--echo # Bug #48709: Assertion failed in sql_select.cc:11782: 
--echo #   int join_read_key(JOIN_TAB*)
--echo #

CREATE TABLE t1 (pk int PRIMARY KEY, int_key int);
INSERT INTO t1 VALUES (10,1), (14,1);

CREATE TABLE t2 (pk int PRIMARY KEY, int_key int);
INSERT INTO t2 VALUES (3,3), (5,NULL), (7,3);

--echo # should have eq_ref for t1, unless subquery materialization is used
--replace_column 1 x 2 x 5 x 6 x 7 x 8 x 9 x 10 x
EXPLAIN
SELECT * FROM t2 outr
WHERE outr.int_key NOT IN (SELECT t1.pk FROM t1, t2)  
ORDER BY outr.pk;

--echo # should not crash on debug binaries
SELECT * FROM t2 outr
WHERE outr.int_key NOT IN (SELECT t1.pk FROM t1, t2)  
ORDER BY outr.pk;

DROP TABLE t1,t2;

#
# 1. Subquery with GROUP/HAVING
#
create table t1 (oref int, grp int, ie int) ;
insert into t1 (oref, grp, ie) values
 (1, 1, 1),
 (1, 1, 1),
 (1, 2, NULL),

 (2, 1, 3),

 (3, 1, 4),
 (3, 2, NULL);

# Ok, for
#   select max(ie) from t1 where oref=PARAM group by grp
# we'll have:
# PARAM      subquery result  
#   1   ->   {(1), (NULL)}    matching + NULL
#   2   ->   {(3)}            non-matching
#   3   ->   {(3), (NULL)}    non-matching + NULL
#   4   ->   {}               empty set

create table t2 (oref int, a int);
insert into t2 values 
  (1, 1),
  (2, 2),
  (3, 3),
  (4, NULL),
  (2, NULL);

# true, false, null, false, null
select a, oref, a in (select max(ie) 
  from t1 where oref=t2.oref group by grp) Z from t2;

# This must have a trigcond
explain extended
select a, oref, a in (select max(ie) 
  from t1 where oref=t2.oref group by grp) Z from t2;

# This must not have a trigcond:
explain extended
select a, oref from t2 
where a in (select max(ie) from t1 where oref=t2.oref group by grp);
select a, oref, a in (
  select max(ie) from t1 where oref=t2.oref group by grp union
  select max(ie) from t1 where oref=t2.oref group by grp
  ) Z from t2;

# Non-correlated subquery, 2 NULL evaluations
create table t3 (a int);
insert into t3 values (NULL), (NULL);
flush status;
select a in (select max(ie) from t1 where oref=4 group by grp) from t3;
show status like 'Handler_read_rnd_next';
select ' ^ This must show 11' Z;

# This must show trigcond:
explain extended select a in (select max(ie) from t1 where oref=4 group by grp) from t3;

drop table t1, t2, t3;

# 
#  2. Subquery handled with 'index_subquery':
# 
create table t1 (a int, oref int, key(a));
insert into t1 values 
  (1, 1),
  (1, NULL),
  (2, 3),
  (2, NULL),
  (3, NULL);

create table t2 (a int, oref int);
insert into t2 values (1, 1), (2,2), (NULL, 3), (NULL, 4);

select oref, a, a in (select a from t1 where oref=t2.oref) Z from t2;

# The next explain shows "using index" but that is just incorrect display
# (there is a bug filed about this).
explain extended 
select oref, a, a in (select a from t1 where oref=t2.oref) Z from t2;

flush status;
select oref, a from t2 where a in (select a from t1 where oref=t2.oref);
# This will only show access to t2:
show status like '%Handler_read_rnd_next';

# Check that repeated NULL-scans are not cached (subq. is not correlated):
delete from t2;
insert into t2 values (NULL, 0),(NULL, 0), (NULL, 0), (NULL, 0);

flush status;
select oref, a, a in (select a from t1 where oref=t2.oref) Z from t2;
show status like '%Handler_read%';
select 'No key lookups, seq reads: 29= 5 reads from t2 + 4 * 6 reads from t1.' Z;

drop table t1, t2;

#
# 3. Subquery handled with 'unique_index_subquery':
#
create table t1 (a int, b int, primary key (a));
insert into t1 values (1,1), (3,1),(100,1);

create table t2 (a int, b int);
insert into t2 values (1,1),(2,1),(NULL,1),(NULL,0);

select a,b, a in (select a from t1 where t1.b = t2.b) Z from t2 ;

drop table t1, t2;

#
# 4. Subquery that is a join, with ref access
#
create table t1 (a int, b int, key(a));
insert into t1 values 
  (0,0),(1,1),(2,2),(3,3),(4,4),(5,5),(6,6),(7,7),(8,8),(9,9);

create table t2 like t1;
insert into t2 select * from t1;
update t2 set b=1;

create table t3 (a int, oref int);
insert into t3 values (1, 1), (NULL,1), (NULL,0);
select a, oref, 
       t3.a in (select t1.a from t1, t2 where t1.b=t2.a and t2.b=t3.oref) Z 
from t3;

# This must have trigcond in WHERE and HAVING:
explain extended
select a, oref, 
       t3.a in (select t1.a from t1, t2 where t1.b=t2.a and t2.b=t3.oref) Z 
from t3;

drop table t1, t2, t3;


#
# BUG#24085: Wrong query result for "NULL IN (SELECT ... UNION SELECT ...)"
#

# case 1: NULL IN (SELECT not_null_val FROM ...) w/o HAVING/GROUP-BY/etc
create table t1 (a int NOT NULL, b int NOT NULL, key(a));
insert into t1 values 
  (0,0),(1,1),(2,2),(3,3),(4,4),(5,5),(6,6),(7,7),(8,8),(9,9);

create table t2 like t1;
insert into t2 select * from t1;
update t2 set b=1;

create table t3 (a int, oref int);
insert into t3 values (1, 1), (NULL,1), (NULL,0);
select a, oref, 
       t3.a in (select t1.a from t1, t2 where t1.b=t2.a and t2.b=t3.oref) Z 
from t3;

--echo This must show a trig_cond:
explain extended
select a, oref, 
       t3.a in (select t1.a from t1, t2 where t1.b=t2.a and t2.b=t3.oref) Z 
from t3;
drop table t1,t2,t3;


# case 2: NULL IN (SELECT not_null_val FROM) where SELECT has GROUP BY
create table t1 (oref int, grp int);
insert into t1 (oref, grp) values
 (1, 1),
 (1, 1);

# Ok, for  
#   select count(*) from t1 group by grp having grp=PARAM
# we'll have:
#  PARAM    subuqery result
#    1  ->    {(2)}   
#    2  ->    {} - empty set
create table t2 (oref int, a int);
insert into t2 values 
  (1, NULL),
  (2, NULL);

select a, oref, 
       a in (select count(*) from t1 group by grp having grp=t2.oref) Z from t2;

--echo This must show a trig_cond:
explain extended
select a, oref, 
       a in (select count(*) from t1 group by grp having grp=t2.oref) Z from t2;

drop table t1, t2;

create table t1 (a int, b int, primary key (a));
insert into t1 values (1,1), (3,1),(100,1);
create table t2 (a int, b int);
insert into t2 values (1,1),(2,1),(NULL,1),(NULL,0);

select a,b, a in (select a from t1 where t1.b = t2.b union select a from
t1 where t1.b = t2.b) Z from t2 ;
select a,b, a in (select a from t1 where t1.b = t2.b) Z from t2 ;
drop table t1, t2;


#
# BUG#24127: Incorrect results of row-based subqueries with NULLs on the left side.
#
create table t3 (a int);
insert into t3 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);
create table t2 (a int, b int, oref int);
insert into t2 values (NULL,1, 100), (NULL,2, 100);

create table t1 (a int, b int, c int, key(a,b));
insert into t1 select 2*A, 2*A, 100 from t3;

# First test index subquery engine
explain extended select a,b, oref, (a,b) in (select a,b from t1 where c=t2.oref) Z from t2;
select a,b, oref, (a,b) in (select a,b from t1 where c=t2.oref) Z from t2;

# Then check that we do turn off 'ref' scans in the subquery
create table t4 (x int);
insert into t4 select A.a + 10*B.a from t1 A, t1 B;
explain extended 
  select a,b, oref, 
         (a,b) in (select a,b from t1,t4 where c=t2.oref) Z 
  from t2;
select a,b, oref, 
       (a,b) in (select a,b from t1,t4 where c=t2.oref) Z 
from t2;

drop table t1,t2,t3,t4;

# More tests for tricky multi-column cases, where some of pushed-down
# equalities are used for index lookups and some arent.
create table t1 (oref char(4), grp int, ie1 int, ie2 int);
insert into t1 (oref, grp, ie1, ie2) values
 ('aa', 10, 2, 1),
 ('aa', 10, 1, 1),
 ('aa', 20, 2, 1),
 ('bb', 10, 3, 1),
 ('cc', 10, 4, 2),
 ('cc', 20, 3, 2),
 
 ('ee', 10, 2, 1),
 ('ee', 10, 1, 2),
 
 ('ff', 20, 2, 2),
 ('ff', 20, 1, 2);
create table t2 (oref char(4), a int, b int);
insert into t2 values 
  ('ee', NULL, 1),
  ('bb', 2, 1),
  ('ff', 2, 2),
  ('cc', 3, NULL),
  ('bb', NULL, NULL),
  ('aa', 1, 1),
  ('dd', 1, NULL);
alter table t1 add index idx(ie1,ie2);

# cc 3 NULL NULL
select oref, a, b, (a,b) in (select ie1,ie2 from t1 where oref=t2.oref) Z from t2 where a=3 and b is null ;
insert into t2 values ('new1', 10,10);
insert into t1 values ('new1', 1234, 10, NULL); 
# new1, 10, 10, NULL,
select oref, a, b, (a,b) in (select ie1,ie2 from t1 where oref=t2.oref) Z from t2 where a=10 and b=10; 
explain extended
select oref, a, b, (a,b) in (select ie1,ie2 from t1 where oref=t2.oref) Z from t2 where a=10 and b=10; 
drop table t1, t2;

# Now test different column types:
create table t1 (oref char(4), grp int, ie int);
insert into t1 (oref, grp, ie) values
 ('aa', 10, 2),
 ('aa', 10, 1),
 ('aa', 20, NULL),

 ('bb', 10, 3),

 ('cc', 10, 4),
 ('cc', 20, NULL),
 
 ('ee', 10, NULL),
 ('ee', 10, NULL),
 
 ('ff', 20, 2),
 ('ff', 20, 1);

create table t2 (oref char(4), a int);
insert into t2 values 
  ('ee', NULL),
  ('bb', 2),
  ('ff', 2),
  ('cc', 3),
  ('aa', 1),
  ('dd', NULL),
  ('bb', NULL);

select oref, a, a in (select ie from t1 where oref=t2.oref) Z from t2;

--sorted_result
select oref, a from t2 where a in (select ie from t1 where oref=t2.oref); 

select oref, a from t2 where a not in (select ie from t1 where oref=t2.oref);


select oref, a, a in (select min(ie) from t1 where oref=t2.oref group by grp) Z from t2;

select oref, a from t2 where 
  a in (select min(ie) from t1 where oref=t2.oref group by grp);
  
select oref, a from t2 where 
  a not in (select min(ie) from t1 where oref=t2.oref group by grp);

#
update t1 set ie=3 where oref='ff' and ie=1;

select oref, a, a in (select min(ie) from t1 where oref=t2.oref group by
grp) Z from t2;


select oref, a from t2 where a in (select min(ie) from t1 where
oref=t2.oref group by grp);

select oref, a from t2 where a not in (select min(ie) from t1 where
oref=t2.oref group by grp);

select oref, a, a in (select min(ie) from t1 where oref=t2.oref group by
grp having min(ie) > 1) Z from t2;

select oref, a from t2 where a in (select min(ie) from t1 where
oref=t2.oref group by grp having min(ie) > 1);
  
select oref, a from t2 where a not in (select min(ie) from t1 where
oref=t2.oref group by grp having min(ie) > 1);

#
alter table t1 add index idx(ie);

explain select oref, a, a in (select ie from t1 where oref=t2.oref) Z from t2;

select oref, a, a in (select ie from t1 where oref=t2.oref) Z from t2;

--sorted_result
select oref, a from t2 where a in (select ie from t1 where oref=t2.oref); 

select oref, a from t2 where a not in (select ie from t1 where oref=t2.oref);


alter table t1 drop index idx;
alter table t1 add index idx(oref,ie);

explain select oref, a, a in (select ie from t1 where oref=t2.oref) Z from t2;

select oref, a, a in (select ie from t1 where oref=t2.oref) Z from t2;

select oref, a from t2 where a in (select ie from t1 where oref=t2.oref); 

select oref, a from t2 where a not in (select ie from t1 where oref=t2.oref);

explain 
select oref, a, 
       a in (select min(ie) from t1 where oref=t2.oref 
             group by grp having min(ie) > 1) Z 
from t2;

select oref, a, 
       a in (select min(ie) from t1 where oref=t2.oref 
             group by grp having min(ie) > 1) Z 
from t2;

select oref, a from t2 where a in (select min(ie) from t1 where oref=t2.oref 
                                   group by grp having min(ie) > 1);
  
select oref, a from t2 where a not in (select min(ie) from t1 where oref=t2.oref 
                                       group by grp having min(ie) > 1);

drop table t1,t2;

create table t1 (oref char(4), grp int, ie1 int, ie2 int);
insert into t1 (oref, grp, ie1, ie2) values
 ('aa', 10, 2, 1),
 ('aa', 10, 1, 1),
 ('aa', 20, 2, 1),

 ('bb', 10, 3, 1),

 ('cc', 10, 4, 2),
 ('cc', 20, 3, 2),
 
 ('ee', 10, 2, 1),
 ('ee', 10, 1, 2),
 
 ('ff', 20, 2, 2),
 ('ff', 20, 1, 2);

create table t2 (oref char(4), a int, b int);
insert into t2 values 
  ('ee', NULL, 1),
  ('bb', 2, 1),
  ('ff', 2, 2),
  ('cc', 3, NULL),
  ('bb', NULL, NULL),
  ('aa', 1, 1),
  ('dd', 1, NULL);

select oref, a, b, (a,b) in (select ie1,ie2 from t1 where oref=t2.oref) Z from t2;

--sorted_result
select oref, a, b from t2 where (a,b) in (select ie1,ie2 from t1 where oref=t2.oref); 

select oref, a, b from t2 where (a,b) not in (select ie1,ie2 from t1 where oref=t2.oref);

select oref, a, b, 
             (a,b) in (select min(ie1),max(ie2) from t1 
                       where oref=t2.oref group by grp) Z 
from t2;

select oref, a, b from t2 where 
  (a,b) in (select min(ie1), max(ie2) from t1 where oref=t2.oref group by grp);
  
select oref, a, b from t2 where
  (a,b) not in (select min(ie1), max(ie2) from t1 where oref=t2.oref group by grp);

alter table t1 add index idx(ie1,ie2);

explain select oref, a, b, (a,b) in (select ie1,ie2 from t1 where oref=t2.oref) Z from t2;

select oref, a, b, (a,b) in (select ie1,ie2 from t1 where oref=t2.oref) Z from t2;

--sorted_result
select oref, a, b from t2 where (a,b) in (select ie1,ie2 from t1 where oref=t2.oref); 

select oref, a, b from t2 where (a,b) not in (select ie1,ie2 from t1 where oref=t2.oref);

explain extended 
select oref, a, b, (a,b) in (select ie1,ie2 from t1 where oref=t2.oref) Z from t2;

drop table t1,t2;

create table t1 (oref char(4), grp int, ie int primary key);
insert into t1 (oref, grp, ie) values
 ('aa', 10, 2),
 ('aa', 10, 1),

 ('bb', 10, 3),

 ('cc', 10, 4),
 ('cc', 20, 5),
 ('cc', 10, 6);
 
create table t2 (oref char(4), a int);
insert into t2 values 
  ('ee', NULL),
  ('bb', 2),
  ('cc', 5),
  ('cc', 2),
  ('cc', NULL),
  ('aa', 1),
  ('bb', NULL);

explain select oref, a, a in (select ie from t1 where oref=t2.oref) Z from t2;

select oref, a, a in (select ie from t1 where oref=t2.oref) Z from t2;

--sorted_result
select oref, a from t2 where a in (select ie from t1 where oref=t2.oref);

select oref, a from t2 where a not in (select ie from t1 where oref=t2.oref);

explain 
select oref, a, a in (select min(ie) from t1 where oref=t2.oref group by grp) Z from t2;

select oref, a, a in (select min(ie) from t1 where oref=t2.oref group by grp) Z from t2;

drop table t1,t2;

#
# BUG#24420: row-based IN suqueries with aggregation when the left operand
#            of the subquery predicate may contain NULL values
#

create table t1 (a int, b int);
insert into t1 values (0,0), (2,2), (3,3);
create table t2 (a int, b int);
insert into t2 values (1,1), (3,3);

select a, b, (a,b) in (select a, min(b) from t2 group by a) Z from t1;

insert into t2 values (NULL,4);
select a, b, (a,b) in (select a, min(b) from t2 group by a) Z from t1;

drop table t1,t2;

#
# Bug #24484: Aggregate function used in column list subquery gives erroneous 
# error
#
CREATE TABLE t1 (a int, b INT, c CHAR(10) NOT NULL, PRIMARY KEY (a, b));
INSERT INTO t1 VALUES (1,1,'a'), (1,2,'b'), (1,3,'c'), (1,4,'d'), (1,5,'e'),
  (2,1,'f'), (2,2,'g'), (2,3,'h'), (3,4,'i'),(3,3,'j'), (3,2,'k'), (3,1,'l'),
  (1,9,'m');
CREATE TABLE t2 (a int, b INT, c CHAR(10) NOT NULL, PRIMARY KEY (a, b));
INSERT INTO t2 SELECT * FROM t1;

# Gives error, but should work since it is (a, b) is the PK so only one 
# given match possible
SELECT a, MAX(b), (SELECT t.c FROM t1 AS t WHERE t1.a=t.a AND t.b=MAX(t1.b))
  as test FROM t1 GROUP BY a;
SELECT * FROM t1 GROUP by t1.a
  HAVING (MAX(t1.b) > (SELECT MAX(t2.b) FROM t2 WHERE t2.c < t1.c
                                                HAVING MAX(t2.b+t1.a) < 10));

SELECT a,b,c FROM t1 WHERE b in (9,3,4) ORDER BY b,c;

SELECT a, MAX(b),
 (SELECT COUNT(DISTINCT t.c) FROM t1 AS t WHERE t1.a=t.a AND t.b=MAX(t1.b) 
  LIMIT 1) 
  as cnt, 
 (SELECT t.b FROM t1 AS t WHERE t1.a=t.a AND t.b=MAX(t1.b) LIMIT 1) 
  as t_b,
 (SELECT t.c FROM t1 AS t WHERE t1.a=t.a AND t.b=MAX(t1.b) LIMIT 1) 
  as t_b,
 (SELECT t.c FROM t1 AS t WHERE t1.a=t.a AND t.b=MAX(t1.b) ORDER BY t.c LIMIT 1)
  as t_b
 FROM t1 GROUP BY a;

SELECT a, MAX(b),
 (SELECT t.c FROM t1 AS t WHERE t1.a=t.a AND t.b=MAX(t1.b) LIMIT 1) as test 
 FROM t1 GROUP BY a;


DROP TABLE t1, t2;


#
# Bug #27870: crash of an equijoin query with WHERE condition containing 
#             a subquery predicate of the form <join attr> NOT IN (SELECT ...)
#

CREATE TABLE t1 (a int);
CREATE TABLE t2 (b int, PRIMARY KEY(b));
INSERT INTO t1 VALUES (1), (NULL), (4);
INSERT INTO t2 VALUES (3), (1),(2), (5), (4), (7), (6);

EXPLAIN EXTENDED 
SELECT a FROM t1, t2 WHERE a=b AND (b NOT IN (SELECT a FROM t1));
SELECT a FROM t1, t2 WHERE a=b AND (b NOT IN (SELECT a FROM t1));
SELECT a FROM t1, t2 WHERE a=b AND (b NOT IN (SELECT a FROM t1 WHERE a > 4));

DROP TABLE t1,t2;

#
# Bug #28375: crash for NOT IN subquery predicate when left operand becomes NULL  
#

CREATE TABLE t1 (id int);
CREATE TABLE t2 (id int PRIMARY KEY);
CREATE TABLE t3 (id int PRIMARY KEY, name varchar(10));
INSERT INTO t1 VALUES (2), (NULL), (3), (1);
INSERT INTO t2 VALUES (234), (345), (457);
INSERT INTO t3 VALUES (222,'bbb'), (333,'ccc'), (111,'aaa');

EXPLAIN
SELECT * FROM t1
  WHERE t1.id NOT IN (SELECT t2.id FROM t2,t3 
                        WHERE t3.name='xxx' AND t2.id=t3.id);
SELECT * FROM t1
  WHERE t1.id NOT IN (SELECT t2.id FROM t2,t3 
                        WHERE t3.name='xxx' AND t2.id=t3.id);

SELECT (t1.id IN (SELECT t2.id FROM t2,t3 
                    WHERE t3.name='xxx' AND t2.id=t3.id)) AS x
  FROM t1;

DROP TABLE t1,t2,t3;   

#
# Bug #22855: Optimizer doesn't rewrite NOT IN subselects to a correlated 
# subquery 
#
CREATE TABLE t1 (a INT NOT NULL);
INSERT INTO t1 VALUES (1),(-1), (65),(66);

CREATE TABLE t2 (a INT UNSIGNED NOT NULL PRIMARY KEY);
INSERT INTO t2 VALUES (65),(66);

SELECT a FROM t1 WHERE a NOT IN (65,66);
SELECT a FROM t1 WHERE a NOT IN (SELECT a FROM t2);
EXPLAIN SELECT a FROM t1 WHERE a NOT IN (SELECT a FROM t2);

DROP TABLE t1, t2;

#
# Bug #34763: item_subselect.cc:1235:Item_in_subselect::row_value_transformer:
#             Assertion failed, unexpected error message:
#             ERROR 1247 (42S22): Reference '<list ref>' not supported (forward
#             reference in item list)
#
CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES(1);

CREATE TABLE t2 (placeholder CHAR(11));
INSERT INTO t2 VALUES("placeholder");

SELECT ROW(1, 2) IN (SELECT t1.a, 2)         FROM t1 GROUP BY t1.a;
SELECT ROW(1, 2) IN (SELECT t1.a, 2 FROM t2) FROM t1 GROUP BY t1.a;

DROP TABLE t1, t2;

#
# Bug #36005: crash in subselect with single row
#             (subselect_single_select_engine::exec)
#

CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1),(2),(3);
CREATE TABLE t2 SELECT * FROM t1;

SELECT 1 FROM t1 WHERE t1.a NOT IN (SELECT 1 FROM t1, t2 WHERE 0);

DROP TABLE t1, t2;

#
# Bug 2198
#

create table t1 (a int, b decimal(13, 3)); 
insert into t1 values (1, 0.123);
select a, (select max(b) from t1) into outfile "subselect.out.file.1" from t1;
delete from t1;
load data infile "subselect.out.file.1" into table t1;
select * from t1;
drop table t1;
let $datadir=`select @@datadir`;
--remove_file $datadir/test/subselect.out.file.1

#
# Bug #37894: Assertion in init_read_record_seq in handler.h line 1444
#

CREATE TABLE t1 (
  pk INT PRIMARY KEY,
  int_key INT,
  varchar_key VARCHAR(5) UNIQUE,
  varchar_nokey VARCHAR(5)
);
INSERT INTO t1 VALUES (9, 7,NULL,NULL), (10,8,'p' ,'p');

SELECT varchar_nokey
FROM t1
WHERE NULL NOT IN (
 SELECT INNR.pk FROM t1 AS INNR2
   LEFT JOIN t1 AS INNR ON ( INNR2.int_key = INNR.int_key )
   WHERE INNR.varchar_key > 'n{'
);

DROP TABLE t1;

#
# Bug #39069: <row constructor> IN <table-subquery> seriously messed up
#

CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1), (2), (11);

--echo # 2nd and 3rd columns should be same
SELECT a, ROW(11, 12) = (SELECT a, 22), ROW(11, 12) IN (SELECT a, 22) FROM t1 GROUP BY t1.a;
SELECT a, ROW(11, 12) = (SELECT a, 12), ROW(11, 12) IN (SELECT a, 12) FROM t1 GROUP BY t1.a;
SELECT a, ROW(11, 12) = (SELECT a, 22), ROW(11, 12) IN (SELECT a, 22) FROM t1;
SELECT a, ROW(11, 12) = (SELECT a, 12), ROW(11, 12) IN (SELECT a, 12) FROM t1;

# The x alias is used below to workaround bug #40674.
# Regression tests for sum function on outer column in subselect from dual:
SELECT a AS x, ROW(11, 12) = (SELECT MAX(x), 22), ROW(11, 12) IN (SELECT MAX(x), 22) FROM t1;
SELECT a AS x, ROW(11, 12) = (SELECT MAX(x), 12), ROW(11, 12) IN (SELECT MAX(x), 12) FROM t1;

DROP TABLE t1;

--echo # both columns should be same
SELECT ROW(1,2) = (SELECT NULL, NULL), ROW(1,2) IN (SELECT NULL, NULL);
SELECT ROW(1,2) = (SELECT   1,  NULL), ROW(1,2) IN (SELECT    1, NULL);
SELECT ROW(1,2) = (SELECT NULL,    2), ROW(1,2) IN (SELECT NULL,    2);
SELECT ROW(1,2) = (SELECT NULL,    1), ROW(1,2) IN (SELECT NULL,    1);
SELECT ROW(1,2) = (SELECT    1,    1), ROW(1,2) IN (SELECT    1,    1);
SELECT ROW(1,2) = (SELECT    1,    2), ROW(1,2) IN (SELECT    1,    2);

#
# Bug #37362      Crash in do_field_eq
#
CREATE TABLE t1 (a INT, b INT, c INT);
INSERT INTO t1 VALUES (1,1,1), (1,1,1);

--error 1054
EXPLAIN EXTENDED 
  SELECT c FROM 
    ( SELECT 
      (SELECT COUNT(a) FROM 
        (SELECT COUNT(b) FROM t1) AS x GROUP BY c
      ) FROM t1 GROUP BY b
    ) AS y;
SHOW WARNINGS;

DROP TABLE t1;

--echo #
--echo # Bug #46791: Assertion failed:(table->key_read==0),function unknown
--echo #    function,file sql_base.cc
--echo #

CREATE TABLE t1 (a INT, b INT, KEY(a));
INSERT INTO t1 VALUES (1,1),(2,2);
CREATE TABLE t2 LIKE t1;
INSERT INTO t2 VALUES (1,1),(2,2);
CREATE TABLE t3 LIKE t1;

--echo # should have 1 impossible where and 2 dependent subqueries
EXPLAIN
SELECT 1 FROM t1
WHERE NOT EXISTS (SELECT 1 FROM t2 WHERE 1 = (SELECT MIN(t2.b) FROM t3))
ORDER BY count(*);

--echo # should not crash the next statement
SELECT 1 FROM t1
WHERE NOT EXISTS (SELECT 1 FROM t2 WHERE 1 = (SELECT MIN(t2.b) FROM t3))
ORDER BY count(*);

--echo # should not crash: the crash is caused by the previous statement
SELECT 1;

DROP TABLE t1,t2,t3;

--echo #
--echo # Bug #47106: Crash / segfault on adding EXPLAIN to a non-crashing 
--echo # query
--echo #

CREATE TABLE t1 (
  a INT,
  b INT,
  PRIMARY KEY (a),
  KEY b (b)
);
INSERT INTO t1 VALUES (1, 1), (2, 1);

CREATE TABLE t2 LIKE t1;
INSERT INTO t2 SELECT * FROM t1;

CREATE TABLE t3 LIKE t1;
INSERT INTO t3 SELECT * FROM t1;

--echo # Should not crash.
--echo # Should have 1 impossible where and 2 dependent subqs.
EXPLAIN
SELECT
  (SELECT 1 FROM t1,t2 WHERE t2.b > t3.b)
FROM t3 WHERE 1 = 0 GROUP BY 1;

--echo # should return 0 rows
SELECT
  (SELECT 1 FROM t1,t2 WHERE t2.b > t3.b)
FROM t3 WHERE 1 = 0 GROUP BY 1;

DROP TABLE t1,t2,t3;
--echo #
--echo # Bug#12329653 
--echo # EXPLAIN, UNION, PREPARED STATEMENT, CRASH, SQL_FULL_GROUP_BY
--echo #

CREATE TABLE t1(a1 int);
INSERT INTO t1 VALUES (1),(2);

SELECT @@session.sql_mode INTO @old_sql_mode;
SET SESSION sql_mode='ONLY_FULL_GROUP_BY';

## First a simpler query, illustrating the transformation
## '1 < some (...)' => '1 < max(...)'
SELECT 1 FROM t1 WHERE 1 < SOME (SELECT a1 FROM t1);

## The query which made the server crash.
PREPARE stmt FROM 
'SELECT 1 UNION ALL 
SELECT 1 FROM t1
ORDER BY
(SELECT 1 FROM t1 AS t1_0  
  WHERE 1 < SOME (SELECT a1 FROM t1)
)' ;

--error ER_SUBQUERY_NO_1_ROW
EXECUTE stmt ;
--error ER_SUBQUERY_NO_1_ROW
EXECUTE stmt ;

SET SESSION sql_mode=@old_sql_mode;

DEALLOCATE PREPARE stmt;
DROP TABLE t1;

--echo #
--echo # Bug#12763207 - ASSERT IN SUBSELECT::SINGLE_VALUE_TRANSFORMER
--echo #

CREATE TABLE t1(a1 int);
INSERT INTO t1 VALUES (1),(2);

CREATE TABLE t2(a1 int);
INSERT INTO t2 VALUES (3);

SELECT @@session.sql_mode INTO @old_sql_mode;
SET SESSION sql_mode='ONLY_FULL_GROUP_BY';

## All these are subject to the transformation
## '1 < some (...)' => '1 < max(...)'
SELECT 1 FROM t1 WHERE 1 < SOME (SELECT 2 FROM t2);
SELECT 1 FROM t1 WHERE 1 < SOME (SELECT 2.0 FROM t2);
SELECT 1 FROM t1 WHERE 1 < SOME (SELECT 'a' FROM t2);
SELECT 1 FROM t1 WHERE 1 < SOME (SELECT a1 FROM t2);

SET SESSION sql_mode=@old_sql_mode;

DROP TABLE t1, t2;

--echo #
--echo # Bug#12763207 - ASSERT IN SUBSELECT::SINGLE_VALUE_TRANSFORMER
--echo #

create table t2(i int);
insert into t2 values(0);

SELECT @@session.sql_mode INTO @old_sql_mode;
SET SESSION sql_mode='ONLY_FULL_GROUP_BY';

CREATE VIEW v1 AS  
SELECT 'f' FROM t2 UNION SELECT 'x' FROM t2
;

CREATE TABLE t1 (
  pk int NOT NULL,
  col_varchar_key varchar(1) DEFAULT NULL,
  PRIMARY KEY (pk),
  KEY col_varchar_key (col_varchar_key)
);

SELECT t1.pk
FROM t1
WHERE t1.col_varchar_key < ALL ( SELECT * FROM v1 )
;

SET SESSION sql_mode=@old_sql_mode;

drop table t2, t1;
drop view v1;

--echo End of 5.0 tests.

#
#  Test [NOT] IN truth table (both as top-level and general predicate).
#

create table t_out (subcase char(3),
                    a1 char(2), b1 char(2), c1 char(2));
create table t_in  (a2 char(2), b2 char(2), c2 char(2));

insert into t_out values ('A.1','2a', NULL, '2a');
# -------------------------- A.2 - impossible
insert into t_out values ('A.3', '2a', NULL, '2a');
insert into t_out values ('A.4', '2a', NULL, 'xx');
insert into t_out values ('B.1', '2a', '2a', '2a');
insert into t_out values ('B.2', '2a', '2a', '2a');
insert into t_out values ('B.3', '3a', 'xx', '3a');
insert into t_out values ('B.4', 'xx', '3a', '3a');

insert into t_in values ('1a', '1a', '1a');
insert into t_in values ('2a', '2a', '2a');
insert into t_in values (NULL, '2a', '2a');
insert into t_in values ('3a', NULL, '3a');
-- echo 
-- echo Test general IN semantics (not top-level)
-- echo 
-- echo case A.1
select subcase,
       (a1, b1, c1)     IN (select * from t_in where a2 = 'no_match') pred_in,
       (a1, b1, c1) NOT IN (select * from t_in where a2 = 'no_match') pred_not_in
from t_out where subcase = 'A.1';

-- echo case A.2 - impossible

-- echo case A.3
select subcase,
       (a1, b1, c1)     IN (select * from t_in) pred_in,
       (a1, b1, c1) NOT IN (select * from t_in) pred_not_in
from t_out where subcase = 'A.3';

-- echo case A.4
select subcase,
       (a1, b1, c1)     IN (select * from t_in) pred_in,
       (a1, b1, c1) NOT IN (select * from t_in) pred_not_in
from t_out where subcase = 'A.4';

-- echo case B.1
select subcase,
       (a1, b1, c1)     IN (select * from t_in where a2 = 'no_match') pred_in,
       (a1, b1, c1) NOT IN (select * from t_in where a2 = 'no_match') pred_not_in
from t_out where subcase = 'B.1';

-- echo case B.2
select subcase,
       (a1, b1, c1)     IN (select * from t_in) pred_in,
       (a1, b1, c1) NOT IN (select * from t_in) pred_not_in
from t_out where subcase = 'B.2';

-- echo case B.3
select subcase,
       (a1, b1, c1)     IN (select * from t_in) pred_in,
       (a1, b1, c1) NOT IN (select * from t_in) pred_not_in
from t_out where subcase = 'B.3';

-- echo case B.4
select subcase,
       (a1, b1, c1)     IN (select * from t_in) pred_in,
       (a1, b1, c1) NOT IN (select * from t_in) pred_not_in
from t_out where subcase = 'B.4';

-- echo 
-- echo Test IN as top-level predicate, and
-- echo as non-top level for cases A.3, B.3 (the only cases with NULL result).
-- echo 
-- echo case A.1
select case when count(*) > 0 then 'T' else 'F' end as pred_in from t_out
where subcase = 'A.1' and
      (a1, b1, c1) IN (select * from t_in where a1 = 'no_match');

select case when count(*) > 0 then 'T' else 'F' end as pred_not_in from t_out
where subcase = 'A.1' and
      (a1, b1, c1) NOT IN (select * from t_in where a1 = 'no_match');

select case when count(*) > 0 then 'T' else 'F' end as not_pred_in from t_out
where subcase = 'A.1' and
      NOT((a1, b1, c1) IN (select * from t_in where a1 = 'no_match'));

-- echo case A.3
select case when count(*) > 0 then 'T' else 'F' end as pred_in from t_out
where subcase = 'A.3' and
      (a1, b1, c1) IN (select * from t_in);

select case when count(*) > 0 then 'T' else 'F' end as pred_not_in from t_out
where subcase = 'A.3' and
      (a1, b1, c1) NOT IN (select * from t_in);

select case when count(*) > 0 then 'T' else 'F' end as not_pred_in from t_out
where subcase = 'A.3' and
      NOT((a1, b1, c1) IN (select * from t_in));
-- echo test non-top level result indirectly
select case when count(*) > 0 then 'N' else 'wrong result' end as pred_in from t_out
where subcase = 'A.3' and
      ((a1, b1, c1) IN (select * from t_in)) is NULL and
      ((a1, b1, c1) NOT IN (select * from t_in)) is NULL;

-- echo case A.4
select case when count(*) > 0 then 'T' else 'F' end as pred_in from t_out
where subcase = 'A.4' and
      (a1, b1, c1) IN (select * from t_in);

select case when count(*) > 0 then 'T' else 'F' end as pred_not_in from t_out
where subcase = 'A.4' and
      (a1, b1, c1) NOT IN (select * from t_in);

select case when count(*) > 0 then 'T' else 'F' end as not_pred_in from t_out
where subcase = 'A.4' and
      NOT((a1, b1, c1) IN (select * from t_in));

-- echo case B.1
select case when count(*) > 0 then 'T' else 'F' end as pred_in from t_out
where subcase = 'B.1' and
      (a1, b1, c1) IN (select * from t_in where a1 = 'no_match');

select case when count(*) > 0 then 'T' else 'F' end as pred_not_in from t_out
where subcase = 'B.1' and
      (a1, b1, c1) NOT IN (select * from t_in where a1 = 'no_match');

select case when count(*) > 0 then 'T' else 'F' end as not_pred_in from t_out
where subcase = 'B.1' and
      NOT((a1, b1, c1) IN (select * from t_in where a1 = 'no_match'));

-- echo case B.2
select case when count(*) > 0 then 'T' else 'F' end as pred_in from t_out
where subcase = 'B.2' and
      (a1, b1, c1) IN (select * from t_in);

select case when count(*) > 0 then 'T' else 'F' end as pred_not_in from t_out
where subcase = 'B.2' and
      (a1, b1, c1) NOT IN (select * from t_in);

select case when count(*) > 0 then 'T' else 'F' end as not_pred_in from t_out
where subcase = 'B.2' and
      NOT((a1, b1, c1) IN (select * from t_in));

-- echo case B.3
select case when count(*) > 0 then 'T' else 'F' end as pred_in from t_out
where subcase = 'B.3' and
      (a1, b1, c1) IN (select * from t_in);

select case when count(*) > 0 then 'T' else 'F' end as pred_not_in from t_out
where subcase = 'B.3' and
      (a1, b1, c1) NOT IN (select * from t_in);

select case when count(*) > 0 then 'T' else 'F' end as not_pred_in from t_out
where subcase = 'B.3' and
      NOT((a1, b1, c1) IN (select * from t_in));
-- echo test non-top level result indirectly
select case when count(*) > 0 then 'N' else 'wrong result' end as pred_in from t_out
where subcase = 'B.3' and
      ((a1, b1, c1) IN (select * from t_in)) is NULL and
      ((a1, b1, c1) NOT IN (select * from t_in)) is NULL;

-- echo case B.4
select case when count(*) > 0 then 'T' else 'F' end as pred_in from t_out
where subcase = 'B.4' and
      (a1, b1, c1) IN (select * from t_in);

select case when count(*) > 0 then 'T' else 'F' end as pred_not_in from t_out
where subcase = 'B.4' and
      (a1, b1, c1) NOT IN (select * from t_in);

select case when count(*) > 0 then 'T' else 'F' end as not_pred_in from t_out
where subcase = 'B.4' and
      NOT((a1, b1, c1) IN (select * from t_in));

drop table t_out;
drop table t_in;


#
# Bug#27348 SET FUNCTION used in  a subquery from WHERE condition
#

CREATE TABLE t1 (a INT, b INT);
INSERT INTO t1 VALUES (2,22),(1,11),(2,22);

SELECT a FROM t1 WHERE (SELECT COUNT(b) FROM DUAL) > 0 GROUP BY a;
SELECT a FROM t1 WHERE (SELECT COUNT(b) FROM DUAL) > 1 GROUP BY a;

SELECT a FROM t1 t0
  WHERE (SELECT COUNT(t0.b) FROM t1 t WHERE t.b>20) GROUP BY a;

SET @@sql_mode='ansi';
--error ER_INVALID_GROUP_FUNC_USE
SELECT a FROM t1 WHERE (SELECT COUNT(b) FROM DUAL) > 0 GROUP BY a;
--error ER_INVALID_GROUP_FUNC_USE
SELECT a FROM t1 WHERE (SELECT COUNT(b) FROM DUAL) > 1 GROUP BY a;

--error ER_INVALID_GROUP_FUNC_USE
SELECT a FROM t1 t0
  WHERE (SELECT COUNT(t0.b) FROM t1 t WHERE t.b>20) GROUP BY a;

SET @@sql_mode=default;
DROP TABLE t1;

#
# Bug#20835 (literal string with =any values)
#
CREATE TABLE t1 (s1 CHAR(1));
INSERT INTO t1 VALUES ('a');
SELECT * FROM t1 WHERE _utf8'a' = ANY (SELECT s1 FROM t1);
DROP TABLE t1;

#
# Bug#37004 NOT IN subquery with MAX over an empty set
#

CREATE TABLE t1(c INT, KEY(c));
CREATE TABLE t2(a INT, b INT);
INSERT INTO t2 VALUES (1, 10), (2, NULL);
INSERT INTO t1 VALUES (1), (3);

SELECT * FROM t2 WHERE b NOT IN (SELECT max(t.c) FROM t1, t1 t WHERE t.c>10);

DROP TABLE t1,t2;

#
# Bug#38191 Server crash with subquery containing DISTINCT and ORDER BY
#

CREATE TABLE t1(pk INT PRIMARY KEY, a INT, INDEX idx(a));
INSERT INTO t1 VALUES (1, 10), (3, 30), (2, 20);
CREATE TABLE t2(pk INT PRIMARY KEY, a INT, b INT, INDEX idxa(a));
INSERT INTO t2 VALUES (2, 20, 700), (1, 10, 200), (4, 10, 100);
SELECT * FROM t1
   WHERE EXISTS (SELECT DISTINCT a FROM t2 WHERE t1.a < t2.a ORDER BY b);
DROP TABLE t1,t2;

#
# Bug#37548 result value erronously reported being NULL in certain subqueries
#

CREATE TABLE t1 (a INT, b INT, PRIMARY KEY (a), KEY b (b));

INSERT INTO t1 VALUES (1,NULL), (9,NULL);

CREATE TABLE t2 (
  a INT,
  b INT,
  c INT,
  d INT,
  PRIMARY KEY (a),
  UNIQUE KEY b (b,c,d),
  KEY b_2 (b),
  KEY c (c),
  KEY d (d)
);

INSERT INTO t2 VALUES
  (43, 2, 11 ,30),
  (44, 2, 12 ,30),
  (45, 1, 1  ,10000),
  (46, 1, 2  ,10000),
  (556,1, 32 ,10000);

CREATE TABLE t3 (
  a INT,
  b INT,
  c INT,
  PRIMARY KEY (a),
  UNIQUE KEY b (b,c),
  KEY c (c),
  KEY b_2 (b)
);

INSERT INTO t3 VALUES (1,1,1), (2,32,1);

explain
SELECT t1.a, (SELECT 1 FROM t2 WHERE t2.b=t3.c AND t2.c=t1.a ORDER BY t2.d LIMIT 1) AS incorrect FROM t1, t3 WHERE t3.b=t1.a;
SELECT t1.a, (SELECT 1 FROM t2 WHERE t2.b=t3.c AND t2.c=t1.a ORDER BY t2.d LIMIT 1) AS incorrect FROM t1, t3 WHERE t3.b=t1.a;

DROP TABLE t1,t2,t3;

#
# Bug#37460 Assertion failed:
# !table->file || table->file->inited == handler::NONE
#
CREATE TABLE t1 (id int);
CREATE TABLE t2 (id int, c int);

INSERT INTO t1 (id) VALUES (1);
INSERT INTO t2 (id) VALUES (1);
INSERT INTO t1 (id) VALUES (1);
INSERT INTO t2 (id) VALUES (1);

CREATE VIEW v1 AS
SELECT t2.c AS c FROM t1, t2
WHERE t1.id=t2.id AND 1 IN (SELECT id FROM t1) WITH CHECK OPTION;
UPDATE v1 SET c=1;

CREATE VIEW v2 (a,b) AS
SELECT t2.id, t2.c AS c FROM t1, t2
WHERE t1.id=t2.id AND 1 IN (SELECT id FROM t1) WITH CHECK OPTION;

--error ER_VIEW_CHECK_FAILED
INSERT INTO v2(a,b) VALUES (2,2);
INSERT INTO v2(a,b) VALUES (1,2);
SELECT * FROM v1;

CREATE VIEW v3 AS
SELECT t2.c AS c FROM t2
WHERE 1 IN (SELECT id FROM t1) WITH CHECK OPTION;

DELETE FROM v3;

DROP VIEW v1,v2,v3;
DROP TABLE t1,t2;

--echo #
--echo # Bug#37822 Correlated subquery with IN and IS UNKNOWN provides wrong result
--echo #
create table t1(id integer primary key, g integer, v integer, s char(1));
create table t2(id integer primary key, g integer, v integer, s char(1));
insert into t1 values
  (10, 10, 10,   'l'),
  (20, 20, 20,   'l'),
  (40, 40, 40,   'l'),
  (41, 40, null, 'l'),
  (50, 50, 50,   'l'),
  (51, 50, null, 'l'),
  (60, 60, 60,   'l'),
  (61, 60, null, 'l'),
  (70, 70, 70,   'l'),
  (90, 90, null, 'l');
insert into t2 values
  (10, 10, 10,   'r'),
  (30, 30, 30,   'r'),
  (50, 50, 50,   'r'),
  (60, 60, 60,   'r'),
  (61, 60, null, 'r'),
  (70, 70, 70,   'r'),
  (71, 70, null, 'r'),
  (80, 80, 80,   'r'),
  (81, 80, null, 'r'),
  (100,100,null, 'r');

select *
from t1
where v in(select v
           from t2
           where t1.g=t2.g) is unknown;
drop table t1, t2;

-- echo #
-- echo # Bug#33204: INTO is allowed in subselect, causing inconsistent results
-- echo #
CREATE TABLE t1( a INT );
INSERT INTO t1 VALUES (1),(2);

CREATE TABLE t2( a INT, b INT );

--error ER_PARSE_ERROR
SELECT * 
FROM (SELECT a INTO @var FROM t1 WHERE a = 2) t1a;
--error ER_PARSE_ERROR
SELECT * 
FROM (SELECT a INTO OUTFILE 'file' FROM t1 WHERE a = 2) t1a;
--error ER_PARSE_ERROR
SELECT * 
FROM (SELECT a INTO DUMPFILE 'file' FROM t1 WHERE a = 2) t1a;

--error ER_PARSE_ERROR
SELECT * FROM ( 
  SELECT 1 a 
  UNION 
  SELECT a INTO @var FROM t1 WHERE a = 2 
) t1a;

--error ER_PARSE_ERROR
SELECT * FROM ( 
  SELECT 1 a 
  UNION 
  SELECT a INTO OUTFILE 'file' FROM t1 WHERE a = 2 
) t1a;

--error ER_PARSE_ERROR
SELECT * FROM ( 
  SELECT 1 a 
  UNION 
  SELECT a INTO DUMPFILE 'file' FROM t1 WHERE a = 2 
) t1a;

SELECT * FROM (SELECT a FROM t1 WHERE a = 2) t1a;

SELECT * FROM ( 
  SELECT a FROM t1 WHERE a = 2 
  UNION 
  SELECT a FROM t1 WHERE a = 2 
) t1a;

SELECT * FROM ( 
  SELECT 1 a 
  UNION 
  SELECT a FROM t1 WHERE a = 2 
  UNION 
  SELECT a FROM t1 WHERE a = 2 
) t1a;

# This was not allowed previously. Possibly, it should be allowed on the future.
# For now, the intent is to keep the fix as non-intrusive as possible.
--error ER_PARSE_ERROR
SELECT * FROM ((SELECT 1 a) UNION SELECT 1 a);
SELECT * FROM (SELECT 1 a UNION (SELECT 1 a)) alias;
SELECT * FROM (SELECT 1 UNION SELECT 1) t1a;
--error ER_PARSE_ERROR
SELECT * FROM ((SELECT 1 a INTO @a)) t1a;
--error ER_PARSE_ERROR
SELECT * FROM ((SELECT 1 a INTO OUTFILE 'file' )) t1a;
--error ER_PARSE_ERROR
SELECT * FROM ((SELECT 1 a INTO DUMPFILE 'file' )) t1a;

--error ER_PARSE_ERROR
SELECT * FROM (SELECT 1 a UNION (SELECT 1 a INTO @a)) t1a;
--error ER_PARSE_ERROR
SELECT * FROM (SELECT 1 a UNION (SELECT 1 a INTO DUMPFILE 'file' )) t1a;
--error ER_PARSE_ERROR
SELECT * FROM (SELECT 1 a UNION (SELECT 1 a INTO OUTFILE 'file' )) t1a;

--error ER_PARSE_ERROR
SELECT * FROM (SELECT 1 a UNION ((SELECT 1 a INTO @a))) t1a;
--error ER_PARSE_ERROR
SELECT * FROM (SELECT 1 a UNION ((SELECT 1 a INTO DUMPFILE 'file' ))) t1a;
--error ER_PARSE_ERROR
SELECT * FROM (SELECT 1 a UNION ((SELECT 1 a INTO OUTFILE 'file' ))) t1a;

SELECT * FROM (SELECT 1 a ORDER BY a) t1a;
SELECT * FROM (SELECT 1 a UNION SELECT 1 a ORDER BY a) t1a;
SELECT * FROM (SELECT 1 a UNION SELECT 1 a LIMIT 1) t1a;
SELECT * FROM (SELECT 1 a UNION SELECT 1 a ORDER BY a LIMIT 1) t1a;

# Test of rule
# table_factor:  '(' get_select_lex query_expression_body ')' opt_table_alias
# UNION should not be allowed inside the parentheses, nor should
# aliases after.
# 
SELECT * FROM t1 JOIN  (SELECT 1 UNION SELECT 1) alias ON 1;
--error ER_PARSE_ERROR
SELECT * FROM t1 JOIN ((SELECT 1 UNION SELECT 1)) ON 1;
--error ER_PARSE_ERROR
SELECT * FROM t1 JOIN  (t1 t1a UNION SELECT 1)  ON 1;
--error ER_PARSE_ERROR
SELECT * FROM t1 JOIN ((t1 t1a UNION SELECT 1)) ON 1;
--error ER_PARSE_ERROR
SELECT * FROM t1 JOIN  (t1 t1a)  t1a ON 1;
--error ER_PARSE_ERROR
SELECT * FROM t1 JOIN ((t1 t1a)) t1a ON 1;

SELECT * FROM t1 JOIN  (t1 t1a)  ON 1;
SELECT * FROM t1 JOIN ((t1 t1a)) ON 1;

SELECT * FROM (t1 t1a);
SELECT * FROM ((t1 t1a));

SELECT * FROM t1 JOIN  (SELECT 1 t1a) alias ON 1;
SELECT * FROM t1 JOIN ((SELECT 1 t1a)) alias ON 1;

SELECT * FROM t1 JOIN  (SELECT 1 a)  a ON 1;
SELECT * FROM t1 JOIN ((SELECT 1 a)) a ON 1;

# For the join, TABLE_LIST::select_lex == NULL
# Check that we handle this.
--error ER_PARSE_ERROR
SELECT * FROM (t1 JOIN (SELECT 1) t1a1 ON 1) t1a2;

SELECT * FROM t1 WHERE a = ALL ( SELECT 1 );
SELECT * FROM t1 WHERE a = ALL ( SELECT 1 UNION SELECT 1 );
SELECT * FROM t1 WHERE a = ANY ( SELECT 3 UNION SELECT 1 );

--error ER_PARSE_ERROR
SELECT * FROM t1 WHERE a = ANY ( SELECT 1 UNION SELECT 1 INTO @a);
--error ER_PARSE_ERROR
SELECT * FROM t1 WHERE a = ANY ( SELECT 1 UNION SELECT 1 INTO OUTFILE 'file' );
--error ER_PARSE_ERROR
SELECT * FROM t1 WHERE a = ANY ( SELECT 1 UNION SELECT 1 INTO DUMPFILE 'file' );

SELECT * FROM t1 WHERE a = ( SELECT 1 );
SELECT * FROM t1 WHERE a = ( SELECT 1 UNION SELECT 1 );
--error ER_PARSE_ERROR
SELECT * FROM t1 WHERE a = ( SELECT 1 INTO @a);
--error ER_PARSE_ERROR
SELECT * FROM t1 WHERE a = ( SELECT 1 INTO OUTFILE 'file' );
--error ER_PARSE_ERROR
SELECT * FROM t1 WHERE a = ( SELECT 1 INTO DUMPFILE 'file' );

--error ER_PARSE_ERROR
SELECT * FROM t1 WHERE a = ( SELECT 1 UNION SELECT 1 INTO @a);
--error ER_PARSE_ERROR
SELECT * FROM t1 WHERE a = ( SELECT 1 UNION SELECT 1 INTO OUTFILE 'file' );
--error ER_PARSE_ERROR
SELECT * FROM t1 WHERE a = ( SELECT 1 UNION SELECT 1 INTO DUMPFILE 'file' );

--error ER_PARSE_ERROR
SELECT ( SELECT 1 INTO @v );
--error ER_PARSE_ERROR
SELECT ( SELECT 1 INTO OUTFILE 'file' );
--error ER_PARSE_ERROR
SELECT ( SELECT 1 INTO DUMPFILE 'file' );

--error ER_PARSE_ERROR
SELECT ( SELECT 1 UNION SELECT 1 INTO @v );
--error ER_PARSE_ERROR
SELECT ( SELECT 1 UNION SELECT 1 INTO OUTFILE 'file' );
--error ER_PARSE_ERROR
SELECT ( SELECT 1 UNION SELECT 1 INTO DUMPFILE 'file' );

# Make sure context is popped when we leave the nested select
SELECT ( SELECT a FROM t1 WHERE a = 1 ), a FROM t1;
SELECT ( SELECT a FROM t1 WHERE a = 1 UNION SELECT 1 ), a FROM t1;

# Make sure we have feature F561 (see .yy file)
SELECT * FROM t2 WHERE (a, b) IN (SELECT a, b FROM t2);

# Make sure the parser does not allow nested UNIONs anywhere

--error ER_PARSE_ERROR
SELECT 1 UNION ( SELECT 1 UNION SELECT 1 );
--error ER_PARSE_ERROR
( SELECT 1 UNION SELECT 1 ) UNION SELECT 1;

--error ER_PARSE_ERROR
SELECT ( SELECT 1 UNION ( SELECT 1 UNION SELECT 1 ) );
--error ER_PARSE_ERROR
SELECT ( ( SELECT 1 UNION SELECT 1 ) UNION SELECT 1;
SELECT ( SELECT 1 UNION SELECT 1 UNION SELECT 1 );
SELECT ((SELECT 1 UNION SELECT 1 UNION SELECT 1));

--error ER_PARSE_ERROR
SELECT * FROM ( SELECT 1 UNION ( SELECT 1 UNION SELECT 1 ) );
--error ER_PARSE_ERROR
SELECT * FROM ( ( SELECT 1 UNION SELECT 1 ) UNION SELECT 1 );
SELECT * FROM ( SELECT 1 UNION SELECT 1 UNION SELECT 1 ) a;

--error ER_PARSE_ERROR
SELECT * FROM t1 WHERE a =     ( SELECT 1 UNION ( SELECT 1 UNION SELECT 1 ) );
--error ER_PARSE_ERROR
SELECT * FROM t1 WHERE a = ALL ( SELECT 1 UNION ( SELECT 1 UNION SELECT 1 ) );
--error ER_PARSE_ERROR
SELECT * FROM t1 WHERE a = ANY ( SELECT 1 UNION ( SELECT 1 UNION SELECT 1 ) );
--error ER_PARSE_ERROR
SELECT * FROM t1 WHERE a IN    ( SELECT 1 UNION ( SELECT 1 UNION SELECT 1 ) );

--error ER_PARSE_ERROR
SELECT * FROM t1 WHERE a =     ( ( SELECT 1 UNION SELECT 1 )  UNION SELECT 1 );
--error ER_PARSE_ERROR
SELECT * FROM t1 WHERE a = ALL ( ( SELECT 1 UNION SELECT 1 )  UNION SELECT 1 );
--error ER_PARSE_ERROR
SELECT * FROM t1 WHERE a = ANY ( ( SELECT 1 UNION SELECT 1 )  UNION SELECT 1 );
--error ER_PARSE_ERROR
SELECT * FROM t1 WHERE a IN    ( ( SELECT 1 UNION SELECT 1 )  UNION SELECT 1 );

SELECT * FROM t1 WHERE a =     ( SELECT 1 UNION SELECT 1 UNION SELECT 1 );
SELECT * FROM t1 WHERE a = ALL ( SELECT 1 UNION SELECT 1 UNION SELECT 1 );
SELECT * FROM t1 WHERE a = ANY ( SELECT 1 UNION SELECT 1 UNION SELECT 1 );
SELECT * FROM t1 WHERE a IN    ( SELECT 1 UNION SELECT 1 UNION SELECT 1 );

--error ER_PARSE_ERROR
SELECT * FROM t1 WHERE EXISTS ( SELECT 1 UNION SELECT 1 INTO @v );
SELECT EXISTS(SELECT 1+1);
--error ER_PARSE_ERROR
SELECT EXISTS(SELECT 1+1 INTO @test);
--error ER_PARSE_ERROR
SELECT * FROM t1 WHERE a IN ( SELECT 1 UNION SELECT 1 INTO @v );

--error ER_PARSE_ERROR
SELECT * FROM t1 WHERE EXISTS ( SELECT 1 INTO @v );
--error ER_PARSE_ERROR
SELECT * FROM t1 WHERE a IN ( SELECT 1 INTO @v );

DROP TABLE t1, t2;
#
# Bug #31157: Crash when select+order by the avg of some field within the
# group by
#
CREATE TABLE t1 (a ENUM('rainbow'));
INSERT INTO t1 VALUES (),(),(),(),();
SELECT 1 FROM t1 GROUP BY (SELECT 1 FROM t1 ORDER BY AVG(LAST_INSERT_ID()));
DROP TABLE t1;
CREATE TABLE t1 (a LONGBLOB);
INSERT INTO t1 SET a = 'aaaa';
INSERT INTO t1 SET a = 'aaaa';
SELECT 1 FROM t1 GROUP BY
  (SELECT LAST_INSERT_ID() FROM t1 ORDER BY MIN(a) ASC LIMIT 1);
DROP TABLE t1;

--echo #
--echo # Bug #49512 : subquery with aggregate function crash 
--echo #   subselect_single_select_engine::exec()

CREATE TABLE t1(a INT);
INSERT INTO t1 VALUES();

--echo # should not crash
SELECT 1 FROM t1 WHERE a <> SOME
(
 SELECT MAX((SELECT a FROM t1 LIMIT 1)) AS d
 FROM t1,t1 a
);
DROP TABLE t1;

--echo #
--echo # Bug #45989 take 2 : memory leak after explain encounters an 
--echo # error in the query
--echo #

CREATE TABLE t1(a LONGTEXT);
INSERT INTO t1 VALUES (repeat('a',@@global.max_allowed_packet));
INSERT INTO t1 VALUES (repeat('b',@@global.max_allowed_packet));

--error ER_BAD_FIELD_ERROR
EXPLAIN EXTENDED SELECT DISTINCT 1 FROM t1,
(SELECT a AS away FROM t1 GROUP BY a WITH ROLLUP) AS d1  
WHERE t1.a = d1.a;
--error ER_WRONG_USAGE
EXPLAIN EXTENDED SELECT DISTINCT 1 FROM t1,
(SELECT DISTINCTROW a AS away FROM t1 GROUP BY a WITH ROLLUP) AS d1  
WHERE t1.a = d1.a;

DROP TABLE t1;

#
# BUG#36135 "void Diagnostics_area::set_eof_status(THD*): Assertion `!is_set()' failed."
#
create table t0 (a int);
insert into t0 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);

create table t1 (
  a int(11) default null,
  b int(11) default null,
  key (a)
);
# produce numbers 0..999
insert into t1 select A.a+10*(B.a+10*C.a),A.a+10*(B.a+10*C.a) from t0 A, t0 B, t0 C;

create table t2 (a int(11) default null);
insert into t2 values (0),(1);

create table t3 (a int(11) default null);
insert into t3 values (0),(1);

create table t4 (a int(11) default null);
insert into t4 values (0),(1);

create table t5 (a int(11) default null);
insert into t5 values (0),(1),(0),(1);

# this must not fail assertion
--error 1242
select * from t2, t3 
where
    t2.a < 10 and
    t3.a+1 = 2 and
    t3.a in (select t1.b from t1
                 where t1.a+1=t1.a+1 and
                       t1.a < (select t4.a+10                                  
                                  from t4, t5 limit 2));

drop table t0, t1, t2, t3, t4, t5;

--echo # 
--echo # BUG#48177 - SELECTs with NOT IN subqueries containing NULL 
--echo #             values return too many records
--echo # 

CREATE TABLE t1 (
  i1 int DEFAULT NULL,
  i2 int DEFAULT NULL
) ;

INSERT INTO t1 VALUES (1,    NULL);
INSERT INTO t1 VALUES (2,    3);
INSERT INTO t1 VALUES (4,    NULL);
INSERT INTO t1 VALUES (4,    0);
INSERT INTO t1 VALUES (NULL, NULL);

CREATE TABLE t2 (
  i1 int DEFAULT NULL,
  i2 int DEFAULT NULL
) ;

INSERT INTO t2 VALUES (4, NULL);
INSERT INTO t2 VALUES (5, 0);

--echo 
--echo Data in t1
SELECT i1, i2 FROM t1;

--echo 
--echo Data in subquery (should be filtered out)
SELECT i1, i2 FROM t2 ORDER BY i1;

FLUSH STATUS;

--echo 
SELECT i1, i2
FROM t1
WHERE (i1, i2) 
      NOT IN (SELECT i1, i2 FROM t2);

--echo 
--echo # Check that the subquery only has to be evaluated once 
--echo # for all-NULL values even though there are two (NULL,NULL) records
--echo # Baseline:
SHOW STATUS LIKE '%Handler_read_rnd_next';

--echo 
INSERT INTO t1 VALUES (NULL, NULL); 
FLUSH STATUS;

--echo 
SELECT i1, i2
FROM t1
WHERE (i1, i2) 
      NOT IN (SELECT i1, i2 FROM t2);

--echo 
--echo # Handler_read_rnd_next should be one more than baseline 
--echo # (read record from t1, but do not read from t2)
SHOW STATUS LIKE '%Handler_read_rnd_next';


DROP TABLE t1,t2;

--echo #
--echo # Bug#54568: create view cause Assertion failed: 0, 
--echo # file .\item_subselect.cc, line 836
--echo #
EXPLAIN SELECT 1 LIKE ( 1 IN ( SELECT 1 ) );
DESCRIBE SELECT 1 LIKE ( 1 IN ( SELECT 1 ) );
--echo # None of the below should crash
CREATE VIEW v1 AS SELECT 1 LIKE ( 1 IN ( SELECT 1 ) );
CREATE VIEW v2 AS SELECT 1 LIKE '%' ESCAPE ( 1 IN ( SELECT 1 ) );
DROP VIEW v1, v2;

#
# Bug #47904 Incorrect results w/ table subquery, derived SQs, and LEFT JOIN on index
#

--echo Set up test tables.
CREATE TABLE t1 (
	t1_id INT UNSIGNED,

	PRIMARY KEY(t1_id)
) Engine=MyISAM;

INSERT INTO t1 (t1_id) VALUES (1), (2), (3), (4), (5);

CREATE TABLE t2 SELECT * FROM t1;
	
CREATE TABLE t3 (
	t3_id INT UNSIGNED AUTO_INCREMENT,
	t1_id INT UNSIGNED,
	amount DECIMAL(16,2),

	PRIMARY KEY(t3_id),
	KEY(t1_id)
) Engine=MyISAM;

INSERT INTO t3 (t1_id, t3_id, amount) 
        VALUES (1, 1, 100.00), (2, 2, 200.00), (4, 4, 400.00);

--echo This is the 'inner query' running by itself.
--echo Produces correct results.
SELECT
	t1.t1_id,
	IFNULL((SELECT SUM(amount) FROM t3 WHERE t3.t1_id=t1.t1_id), 0) AS total_amount
FROM
	t1
	LEFT JOIN t2 ON t2.t1_id=t1.t1_id
GROUP BY
	t1.t1_id
;

--echo SELECT * FROM (the same inner query)
--echo Produces correct results.
SELECT * FROM (
SELECT
	t1.t1_id,
	IFNULL((SELECT SUM(amount) FROM t3 WHERE t3.t1_id=t1.t1_id), 0) AS total_amount
FROM
	t1
	LEFT JOIN t2 ON t2.t1_id=t1.t1_id
GROUP BY
	t1.t1_id
) AS t;

--echo Now make t2.t1_id part of a key.
ALTER TABLE t2 ADD PRIMARY KEY(t1_id);

--echo Same inner query by itself.
--echo Still correct results.
SELECT
	t1.t1_id,
	IFNULL((SELECT SUM(amount) FROM t3 WHERE t3.t1_id=t1.t1_id), 0) AS total_amount
FROM
	t1
	LEFT JOIN t2 ON t2.t1_id=t1.t1_id
GROUP BY
	t1.t1_id;

--echo SELECT * FROM (the same inner query), now with indexes on the LEFT JOIN
SELECT * FROM (
SELECT
	t1.t1_id,
	IFNULL((SELECT SUM(amount) FROM t3 WHERE t3.t1_id=t1.t1_id), 0) AS total_amount
FROM
	t1
	LEFT JOIN t2 ON t2.t1_id=t1.t1_id
GROUP BY
	t1.t1_id
) AS t;


DROP TABLE t3;
DROP TABLE t2;
DROP TABLE t1;

#
# BUG#36135 "void Diagnostics_area::set_eof_status(THD*): Assertion `!is_set()' failed."
#
create table t0 (a int);
insert into t0 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);

create table t1 (
  a int(11) default null,
  b int(11) default null,
  key (a)
);
# produce numbers 0..999
insert into t1 select A.a+10*(B.a+10*C.a),A.a+10*(B.a+10*C.a) from t0 A, t0 B, t0 C;

create table t2 (a int(11) default null);
insert into t2 values (0),(1);

create table t3 (a int(11) default null);
insert into t3 values (0),(1);

create table t4 (a int(11) default null);
insert into t4 values (0),(1);

create table t5 (a int(11) default null);
insert into t5 values (0),(1),(0),(1);

# this must not fail assertion
--error 1242
select * from t2, t3 
where
    t2.a < 10 and
    t3.a+1 = 2 and
    t3.a in (select t1.b from t1
                 where t1.a+1=t1.a+1 and
                       t1.a < (select t4.a+10                                  
                                  from t4, t5 limit 2));

drop table t0, t1, t2, t3, t4, t5;

--echo # 
--echo # BUG#48177 - SELECTs with NOT IN subqueries containing NULL 
--echo #             values return too many records
--echo # 

CREATE TABLE t1 (
  i1 int DEFAULT NULL,
  i2 int DEFAULT NULL
) ;

INSERT INTO t1 VALUES (1,    NULL);
INSERT INTO t1 VALUES (2,    3);
INSERT INTO t1 VALUES (4,    NULL);
INSERT INTO t1 VALUES (4,    0);
INSERT INTO t1 VALUES (NULL, NULL);

CREATE TABLE t2 (
  i1 int DEFAULT NULL,
  i2 int DEFAULT NULL
) ;

INSERT INTO t2 VALUES (4, NULL);
INSERT INTO t2 VALUES (5, 0);

--echo 
--echo Data in t1
SELECT i1, i2 FROM t1;

--echo 
--echo Data in subquery (should be filtered out)
SELECT i1, i2 FROM t2 ORDER BY i1;

FLUSH STATUS;

--echo 
SELECT i1, i2
FROM t1
WHERE (i1, i2) 
      NOT IN (SELECT i1, i2 FROM t2);

--echo 
--echo # Check that the subquery only has to be evaluated once 
--echo # for all-NULL values even though there are two (NULL,NULL) records
--echo # Baseline:
SHOW STATUS LIKE '%Handler_read_rnd_next';

--echo 
INSERT INTO t1 VALUES (NULL, NULL); 
FLUSH STATUS;

--echo 
SELECT i1, i2
FROM t1
WHERE (i1, i2) 
      NOT IN (SELECT i1, i2 FROM t2);

--echo 
--echo # Handler_read_rnd_next should be one more than baseline 
--echo # (read record from t1, but do not read from t2)
SHOW STATUS LIKE '%Handler_read_rnd_next';
DROP TABLE t1,t2;



--echo #
--echo # Bug #52711: Segfault when doing EXPLAIN SELECT with 
--echo #  union...order by (select... where...)
--echo #

CREATE TABLE t1 (a VARCHAR(10), FULLTEXT KEY a (a));
INSERT INTO t1 VALUES (1),(2);
CREATE TABLE t2 (b INT);
INSERT INTO t2 VALUES (1),(2);

--echo # Should not crash
--disable_result_log
EXPLAIN
SELECT * FROM t2 UNION SELECT * FROM t2
  ORDER BY (SELECT * FROM t1 WHERE MATCH(a) AGAINST ('+abc' IN BOOLEAN MODE));

--echo # Should not crash
SELECT * FROM t2 UNION SELECT * FROM t2
  ORDER BY (SELECT * FROM t1 WHERE MATCH(a) AGAINST ('+abc' IN BOOLEAN MODE));
DROP TABLE t1,t2;
--enable_result_log

--echo #
--echo # Bug #58818: Incorrect result for IN/ANY subquery
--echo # with HAVING condition 
--echo #

CREATE TABLE t1(i INT);
INSERT INTO t1 VALUES (1), (2), (3);
CREATE TABLE t1s(i INT);
INSERT INTO t1s VALUES (10), (20), (30);
CREATE TABLE t2s(i INT);
INSERT INTO t2s VALUES (100), (200), (300);

SELECT * FROM t1
WHERE t1.i NOT IN
(
  SELECT STRAIGHT_JOIN t2s.i 
  FROM
  t1s LEFT OUTER JOIN t2s ON t2s.i = t1s.i
  HAVING t2s.i = 999
);

SELECT * FROM t1
WHERE t1.I IN
(
  SELECT STRAIGHT_JOIN t2s.i 
  FROM
  t1s LEFT OUTER JOIN t2s ON t2s.i = t1s.i
  HAVING t2s.i = 999
) IS UNKNOWN;

SELECT * FROM t1
WHERE NOT t1.I = ANY
(
  SELECT STRAIGHT_JOIN t2s.i 
  FROM
  t1s LEFT OUTER JOIN t2s ON t2s.i = t1s.i
  HAVING t2s.i = 999
);

SELECT * FROM t1
 WHERE t1.i = ANY (
  SELECT STRAIGHT_JOIN t2s.i 
  FROM
  t1s LEFT OUTER JOIN t2s ON t2s.i = t1s.i
  HAVING t2s.i = 999
 ) IS UNKNOWN;

DROP TABLE t1,t1s,t2s;

--echo #
--echo # Bug #56690  Wrong results with subquery with 
--echo # GROUP BY inside < ANY clause
--echo #

CREATE TABLE t1 (
 pk INT NOT NULL PRIMARY KEY,
 number INT,
 KEY key_number (number)
);
INSERT INTO t1 VALUES (8,8);

CREATE TABLE t2 (
 pk INT NOT NULL PRIMARY KEY,
 number INT,
 KEY key_number (number)
);

INSERT INTO t2 VALUES (1,2);
INSERT INTO t2 VALUES (2,8);
INSERT INTO t2 VALUES (3,NULL);
INSERT INTO t2 VALUES (4,166);

SELECT * FROM t1 WHERE t1.number < ANY(SELECT number FROM t2 GROUP BY number);
SELECT * FROM t1 WHERE t1.number < ANY(SELECT number FROM t2);

DROP TABLE t1,t2;

--echo End of 5.1 tests
 
--echo #
--echo # BUG#50257: Missing info in REF column of the EXPLAIN 
--echo #            lines for subselects
--echo #

CREATE TABLE t1 (a INT, b INT, INDEX (a));
INSERT INTO t1 VALUES (3, 10), (2, 20), (7, 10), (5, 20);

--echo
EXPLAIN SELECT * FROM (SELECT * FROM t1 WHERE a=7) t;
--echo
EXPLAIN SELECT * FROM t1 WHERE EXISTS (SELECT * FROM t1 WHERE a=7);

--echo
DROP TABLE t1;


--echo #
--echo # BUG#52317: Assertion failing in Field_varstring::store() 
--echo # 	    at field.cc:6833
--echo #

CREATE TABLE t1 (i INTEGER);
INSERT INTO t1 VALUES (1);
CREATE TABLE t2 (i INTEGER, KEY k(i));
INSERT INTO t2 VALUES (1), (2);

EXPLAIN  
SELECT i FROM t1 WHERE (1) NOT IN (SELECT i FROM t2);

DROP TABLE t2;
DROP TABLE t1;


--echo #
--echo # Bug #11765713 58705:
--echo # OPTIMIZER LET ENGINE DEPEND ON UNINITIALIZED VALUES
--echo # CREATED BY OPT_SUM_QUERY
--echo #

CREATE TABLE t1(a INT NOT NULL, KEY (a));
INSERT INTO t1 VALUES (0), (1);

--error ER_SUBQUERY_NO_1_ROW
SELECT 1 as foo FROM t1 WHERE a < SOME
  (SELECT a FROM t1 WHERE a <=>
    (SELECT a FROM t1)
  );

SELECT 1 as foo FROM t1 WHERE a < SOME
  (SELECT a FROM t1 WHERE a <=>
    (SELECT a FROM t1 where a is null)
  ); 

DROP TABLE t1;

--echo #
--echo # Bug #57704: Cleanup code dies with void TABLE::set_keyread(bool): 
--echo #             Assertion `file' failed.
--echo #

CREATE TABLE t1 (a INT);

--error ER_OPERAND_COLUMNS
SELECT 1 FROM 
  (SELECT ROW(
    (SELECT 1 FROM t1 RIGHT JOIN 
      (SELECT 1 FROM t1, t1 t2) AS d ON 1),
    1) FROM t1) AS e;

DROP TABLE t1;

--echo #
--echo # Bug#11764086: Null left operand to NOT IN in WHERE clause
--echo # behaves differently than real NULL
--echo #

CREATE TABLE parent (id int);
INSERT INTO parent VALUES (1), (2);

CREATE TABLE child (parent_id int, other int);
INSERT INTO child VALUES (1,NULL);

--echo # Offending query (c.parent_id is NULL for null-complemented rows only)

SELECT    p.id, c.parent_id
FROM      parent p
LEFT JOIN child  c
ON        p.id = c.parent_id
WHERE     c.parent_id NOT IN (
              SELECT parent_id 
              FROM   child
              WHERE  parent_id = 3
          );

--echo # Some syntactic variations with IS FALSE and IS NOT TRUE

SELECT    p.id, c.parent_id
FROM      parent p
LEFT JOIN child  c
ON        p.id = c.parent_id
WHERE     c.parent_id IN (
              SELECT parent_id 
              FROM   child
              WHERE  parent_id = 3
          ) IS NOT TRUE;

SELECT    p.id, c.parent_id
FROM      parent p
LEFT JOIN child  c
ON        p.id = c.parent_id
WHERE     c.parent_id IN (
              SELECT parent_id 
              FROM   child
              WHERE  parent_id = 3
          ) IS FALSE;

DROP TABLE parent, child;

--echo # End of test for bug#11764086.

--echo End of 5.5 tests.

--echo #
--echo # BUG#48920: COUNT DISTINCT returns 1 for NULL values when in a subquery 
--echo #            in the select list
--echo #

--echo 
CREATE TABLE t1 (
  i int(11) DEFAULT NULL,
  v varchar(1) DEFAULT NULL
);

--echo 
INSERT INTO t1 VALUES (8,'v');
INSERT INTO t1 VALUES (9,'r');
INSERT INTO t1 VALUES (NULL,'y');

--echo 
CREATE TABLE t2 (
  i int(11) DEFAULT NULL,
  v varchar(1) DEFAULT NULL,
  KEY i_key (i)
);

--echo 
INSERT INTO t2 VALUES (NULL,'r');
INSERT INTO t2 VALUES (0,'c');
INSERT INTO t2 VALUES (0,'o');
INSERT INTO t2 VALUES (2,'v');
INSERT INTO t2 VALUES (7,'c');

--echo 
SELECT i, v, (SELECT COUNT(DISTINCT i)
              FROM t1
              WHERE v  = t2.v) as subsel
FROM t2;

--echo 
EXPLAIN EXTENDED
SELECT i, v, (SELECT COUNT(DISTINCT i)
              FROM t1
              WHERE v  = t2.v) as subsel
FROM t2;

DROP TABLE t1,t2;


--echo #
--echo # BUG#50257: Missing info in REF column of the EXPLAIN 
--echo #            lines for subselects
--echo #

CREATE TABLE t1 (a INT, b INT, INDEX (a));
INSERT INTO t1 VALUES (3, 10), (2, 20), (7, 10), (5, 20);

--echo
EXPLAIN SELECT * FROM (SELECT * FROM t1 WHERE a=7) t;
--echo
EXPLAIN SELECT * FROM t1 WHERE EXISTS (SELECT * FROM t1 WHERE a=7);

--echo
DROP TABLE t1;

--echo #
--echo # BUG#58561: Server Crash with correlated subquery and MyISAM tables 
--echo #

CREATE TABLE cc (
  pk INT,
  col_int_key INT,
  col_varchar_key VARCHAR(1),
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
) ENGINE=MyISAM;
INSERT INTO cc VALUES (10,7,'v');
INSERT INTO cc VALUES (11,1,'r');

CREATE TABLE bb (
  pk INT,
  col_date_key DATE,
  PRIMARY KEY (pk),
  KEY col_date_key (col_date_key)
) ENGINE=MyISAM;
INSERT INTO bb VALUES (10,'2002-02-21');

CREATE TABLE c (
  pk INT,
  col_int_key INT,
  col_varchar_key VARCHAR(1),
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
) ENGINE=MyISAM;
INSERT INTO c VALUES (1,NULL,'w');
INSERT INTO c VALUES (19,NULL,'f');

CREATE TABLE b (
  pk INT,
  col_int_key INT,
  col_varchar_key VARCHAR(1),
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
) ENGINE=MyISAM;
INSERT INTO b VALUES (1,7,'f');

SELECT col_int_key
FROM b granparent1
WHERE (col_int_key, col_int_key) IN (
    SELECT parent1.pk, parent1.pk
    FROM bb parent1 JOIN cc parent2
                    ON parent2.col_varchar_key = parent2.col_varchar_key
    WHERE granparent1.col_varchar_key IN (
        SELECT col_varchar_key
        FROM c)
      AND parent1.pk = granparent1.col_int_key
    ORDER BY parent1.col_date_key 
);

DROP TABLE bb, b, cc, c;

--echo End of 5.6 tests

--echo #
--echo # BUG#46743 "Azalea processing correlated, aggregate SELECT
--echo # subqueries incorrectly"
--echo #
# To see the bug, one would have to undo both the fix for BUG#46743
# and the one for BUG#47123.
CREATE TABLE t1 (c int);
INSERT INTO t1 VALUES (NULL);
CREATE TABLE t2 (d int , KEY (d)); # index is needed for bug
INSERT INTO t2 VALUES (NULL),(NULL); # two rows needed for bug
# we see that subquery returns 0 rows
--echo 0 rows in subquery
SELECT 1 AS RESULT FROM t2,t1 WHERE d = c;
# so here it ends up as NULL
--echo base query
SELECT (SELECT 1 FROM t2 WHERE d = c) AS RESULT FROM t1 ;
EXPLAIN EXTENDED SELECT (SELECT 1 FROM t2 WHERE d = c) AS RESULT FROM t1 ;
--echo first equivalent variant
SELECT (SELECT 1 FROM t2 WHERE d = IFNULL(c,NULL)) AS RESULT FROM t1 GROUP BY c ;
EXPLAIN EXTENDED SELECT (SELECT 1 FROM t2 WHERE d = IFNULL(c,NULL)) AS RESULT FROM t1 GROUP BY c;
--echo second equivalent variant
# used to fail with 1242: Subquery returns more than 1 row
SELECT (SELECT 1 FROM t2 WHERE d = c) AS RESULT FROM t1 GROUP BY c ;
EXPLAIN EXTENDED SELECT (SELECT 1 FROM t2 WHERE d = c) AS RESULT FROM t1 GROUP BY c ;

DROP TABLE t1,t2;

--echo
--echo BUG#37842: Assertion in DsMrr_impl::dsmrr_init, at handler.cc:4307
--echo
CREATE TABLE t1 (
  `pk` int(11) NOT NULL AUTO_INCREMENT,
  `int_key` int(11) DEFAULT NULL,
  PRIMARY KEY (`pk`),
  KEY `int_key` (`int_key`)
) ENGINE=MyISAM;

INSERT INTO t1 VALUES (1,9),(2,3),(3,8),(4,6),(5,9),(6,5),(7,5),(8,9),(9,1),(10,10);

SELECT `pk` FROM t1 AS OUTR WHERE `int_key` = ALL (
 SELECT `int_key` FROM t1 AS INNR WHERE INNR . `pk` >= 9
);
DROP TABLE t1;

--echo #
--echo # Bug#53236 Segfault in DTCollation::set(DTCollation&)
--echo #

CREATE TABLE t1 (
  pk INTEGER AUTO_INCREMENT,
  col_varchar VARCHAR(1),
  PRIMARY KEY (pk)
)
;

INSERT INTO t1 (col_varchar) 
VALUES
('w'),
('m')
;

SELECT  table1.pk
FROM ( t1 AS table1 JOIN t1 AS table2 ON (table1.col_varchar =
                                          table2.col_varchar) ) 
WHERE ( 1, 2 ) IN ( SELECT SUBQUERY1_t1.pk AS SUBQUERY1_field1,
                           SUBQUERY1_t1.pk AS SUBQUERY1_field2
                    FROM ( t1 AS SUBQUERY1_t1 JOIN t1 AS SUBQUERY1_t2
                           ON (SUBQUERY1_t2.col_varchar =
                               SUBQUERY1_t1.col_varchar) ) ) 
;

drop table t1;
--echo #
--echo # Bug#58207: invalid memory reads when using default column value and 
--echo # tmptable needed
--echo #
CREATE TABLE t(a VARCHAR(245) DEFAULT
'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa');
INSERT INTO t VALUES (''),(''),(''),(''),(''),(''),(''),(''),(''),(''),('');
SELECT * FROM (SELECT default(a) FROM t GROUP BY a) d;
DROP TABLE t;

--echo #
--echo # Bug 11765699 - 58690: !TABLE || (!TABLE->READ_SET || 
--echo #                BITMAP_IS_SET(TABLE->READ_SET, FIELD_INDEX
--echo #

CREATE TABLE t1(a INT);
INSERT INTO t1 VALUES (0), (1); 

CREATE TABLE t2(
  b TEXT, 
  c INT, 
  PRIMARY KEY (b(1))
);
INSERT INTO t2 VALUES ('a', 2), ('b', 3);

SELECT 1 FROM t1 WHERE a = 
 (SELECT 1 FROM t2 WHERE b = 
  (SELECT 1 FROM t1 t11 WHERE c = 1 OR t1.a = 1 AND 1 = 2)
   ORDER BY b
 );

SELECT 1 FROM t1 WHERE a = 
 (SELECT 1 FROM t2 WHERE b = 
  (SELECT 1 FROM t1 t11 WHERE c = 1 OR t1.a = 1 AND 1 = 2)
   GROUP BY b
 );

DROP TABLE t1, t2;

--echo #
--echo # BUG#12616253 - WRONG RESULT WITH EXISTS(SUBQUERY) (MISSING ROWS)
--echo #

CREATE TABLE t1 (f1 varchar(1));
INSERT INTO t1 VALUES ('v'),('s');

CREATE TABLE t2 (f1_key varchar(1), KEY (f1_key));
INSERT INTO t2 VALUES ('j'),('v'),('c'),('m'),('d'),
('d'),('y'),('t'),('d'),('s');

let $query=SELECT table1.f1, table2.f1_key
FROM t1 AS table1, t2 AS table2
WHERE EXISTS 
( 
SELECT DISTINCT f1_key 
FROM t2 
WHERE f1_key != table2.f1_key AND f1_key >= table1.f1 );

eval $query;
eval explain $query;

DROP TABLE t1,t2;

--echo #
--echo # BUG#12616477 - 0 VS NULL DIFFERENCES WITH OUTER JOIN, SUBQUERY
--echo #

CREATE TABLE t1 (
col_int_key int,
col_varchar_key varchar(1),
col_varchar_nokey varchar(1),
KEY (col_int_key)
);
INSERT INTO t1 VALUES (224,'p','p'),(9,'e','e');

CREATE TABLE t3 (
col_int_key int,
col_varchar_key varchar(1),
KEY col_int_key (col_int_key),
KEY col_varchar_key (col_varchar_key,col_int_key))
;
INSERT INTO t3 VALUES (4,'p'),(8,'e'),(10,'a');

# At jcl>=1, if join buffering is on t1 bug doesn't happen, so we
# force join order so that join buffering is rather on t3.
# Reverse join order if you want to see bug at jcl=0.

let $query=SELECT t1f.*,t3f.*,(
SELECT MIN(t3s.col_int_key)
FROM t3 AS t3s JOIN
t1 AS t1s ON t1s.col_int_key = 9 and
t1s.col_varchar_key = 'e'
WHERE 'e' <> t1f.col_varchar_nokey )
FROM
t1 AS t1f STRAIGHT_JOIN t3 AS t3f;

eval $query;
eval explain $query;

DROP TABLE t1,t3;

--echo #
--echo # Bug#12795555: Missing rows with ALL/ANY subquery
--echo #
# Test the case when Item_maxmin_subselect is used
CREATE TABLE t1 (f1 INT);
INSERT INTO t1 VAlUES (NULL),(1),(NULL),(2);
SELECT f1 FROM t1 WHERE f1 < ALL (SELECT 1 FROM DUAL WHERE 0);
DROP TABLE t1;
# Test the case when Item_sum_[max|min] is used
CREATE TABLE t1 (k VARCHAR(1), KEY k(k));
INSERT INTO t1 VALUES ('r'), (NULL), (NULL);

CREATE TABLE t2 (c VARCHAR(1));
INSERT INTO t2 VALUES ('g'), (NULL);

CREATE TABLE t3 (c VARCHAR(1));

SELECT COUNT(*)
FROM t1 JOIN t2
WHERE t1.k < ALL(
  SELECT c
  FROM t3
);

DROP TABLE t1, t2, t3; 

--echo #
--echo # Bug#12838171: 51VS56: TRANSFORMED IN()+SUBQ QUERY 
--echo #               PRODUCES EMPTY RESULT ON 5.6, 1 ROW ON 5.1
--echo #
CREATE TABLE ot (
  col_int_nokey int(11), 
  col_varchar_nokey varchar(1)
) ;

INSERT INTO ot VALUES (1,'x');

CREATE TABLE it (
  col_int_key int(11), 
  col_varchar_key varchar(1), 
  KEY idx_cvk_cik (col_varchar_key,col_int_key)
) ;

INSERT INTO it VALUES (NULL,'x'), (NULL,'f');

--echo
SELECT col_int_nokey 
FROM ot 
WHERE col_varchar_nokey IN
   (SELECT col_varchar_key
    FROM it 
    WHERE col_int_key IS NULL);

--echo
EXPLAIN EXTENDED
SELECT col_int_nokey 
FROM ot 
WHERE col_varchar_nokey IN
   (SELECT col_varchar_key
    FROM it 
    WHERE col_int_key IS NULL);

--echo
SELECT col_int_nokey 
FROM ot 
WHERE col_varchar_nokey IN
   (SELECT col_varchar_key
    FROM it 
    WHERE coalesce(col_int_nokey, 1) );

--echo
EXPLAIN EXTENDED
SELECT col_int_nokey 
FROM ot 
WHERE col_varchar_nokey IN
   (SELECT col_varchar_key
    FROM it 
    WHERE coalesce(col_int_nokey, 1) );

DROP TABLE it;

CREATE TABLE it (
   col_int_key int(11),
   col_varchar_key varchar(1),
   col_varchar_key2 varchar(1),
   KEY idx_cvk_cvk2_cik (col_varchar_key, col_varchar_key2, col_int_key),
   KEY idx_cvk_cik (col_varchar_key, col_int_key)
);

INSERT INTO it VALUES (NULL,'x','x'), (NULL,'f','f');

SELECT col_int_nokey
FROM ot
WHERE (col_varchar_nokey, 'x') IN
    (SELECT col_varchar_key, col_varchar_key2
     FROM it
     WHERE col_int_key IS NULL);

--echo
EXPLAIN EXTENDED
SELECT col_int_nokey
FROM ot
WHERE (col_varchar_nokey, 'x') IN
    (SELECT col_varchar_key, col_varchar_key2
     FROM it
     WHERE col_int_key IS NULL);

--echo
DROP TABLE it, ot;

--echo #
--echo # Bug #11829691: Pure virtual method called in Item_bool_func2::fix...()
--echo #

CREATE TABLE t1(a INTEGER);
CREATE TABLE t2(b INTEGER);

PREPARE stmt FROM "
SELECT SUM(b) FROM t2 GROUP BY b HAVING b IN (SELECT b FROM t1)";

EXECUTE stmt;
EXECUTE stmt;

DEALLOCATE PREPARE stmt;

DROP TABLE t1, t2;

--echo #
--echo # Bug #13595212 EXTRA ROWS RETURNED ON RIGHT JOIN WITH VIEW AND
--echo # IN-SUBQUERY IN WHERE
--echo #

CREATE TABLE t1 (
  pk int(11) NOT NULL AUTO_INCREMENT,
  col_int_key int(11) NOT NULL,
  col_varchar_key varchar(1) NOT NULL,
  col_varchar_nokey varchar(1) NOT NULL,
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
);

INSERT INTO t1 VALUES (1,0,'j','j'),(2,8,'v','v'),
(3,1,'c','c'),(4,8,'m','m'),(5,9,'d','d');

CREATE VIEW v1 AS SELECT * FROM t1;

# RIGHT JOIN

let $query=
SELECT alias2.col_varchar_nokey
FROM v1 AS alias1
  RIGHT JOIN t1 AS alias2 ON 1
WHERE alias2.col_varchar_key IN (
  SELECT sq2_alias1.col_varchar_nokey
  FROM v1 AS sq2_alias1
    LEFT JOIN t1 AS sq2_alias2
    ON (sq2_alias2.col_int_key = sq2_alias1.pk)
  WHERE sq2_alias1.pk != alias1.col_int_key
    AND sq2_alias1.col_varchar_key > alias1.col_varchar_key
)
;

eval EXPLAIN EXTENDED $query;
--sorted_result
eval $query;

# Same, but in view:

eval CREATE VIEW v2 AS $query;

EXPLAIN EXTENDED SELECT * FROM v2;
--sorted_result
SELECT * FROM v2;

# Same, with LEFT JOIN

let $query=SELECT alias2.col_varchar_nokey
FROM t1 AS alias2
  LEFT JOIN v1 AS alias1 ON 1
WHERE alias2.col_varchar_key IN (
  SELECT sq2_alias1.col_varchar_nokey
  FROM v1 AS sq2_alias1
    LEFT JOIN t1 AS sq2_alias2
    ON (sq2_alias2.col_int_key = sq2_alias1.pk)
  WHERE sq2_alias1.pk != alias1.col_int_key
    AND sq2_alias1.col_varchar_key > alias1.col_varchar_key
);

eval EXPLAIN EXTENDED $query;
--sorted_result
eval $query;

DROP TABLE t1;
DROP VIEW v1,v2;

--echo #
--echo # Bug#13651009 WRONG RESULT FROM DERIVED TABLE IF THE SUBQUERY
--echo # HAS AN EMPTY RESULT
--echo #

CREATE TABLE t1 (
  pk int NOT NULL,
  col_int_nokey int NOT NULL,
  col_int_key int NOT NULL,
  col_time_key time NOT NULL,
  col_varchar_key varchar(1) NOT NULL,
  col_varchar_nokey varchar(1) NOT NULL,
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key),
  KEY col_time_key (col_time_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
) ENGINE=MyISAM;

CREATE TABLE t2 (
  pk int NOT NULL AUTO_INCREMENT,
  col_int_nokey int NOT NULL,
  col_int_key int NOT NULL,
  col_time_key time NOT NULL,
  col_varchar_key varchar(1) NOT NULL,
  col_varchar_nokey varchar(1) NOT NULL,
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key),
  KEY col_time_key (col_time_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
) ENGINE=MyISAM;

INSERT INTO t2 VALUES (1,4,4,'00:00:00','b','b');

SET @var2:=4, @var3:=8;

--echo
--echo Testcase without inner subquery

let $subq=
SELECT @var3:=12, sq4_alias1.*
FROM t1 AS sq4_alias1
WHERE (sq4_alias1.col_varchar_key + NULL) IS NULL OR
      sq4_alias1.col_varchar_key = @var3;

eval EXPLAIN $subq;
eval $subq;
SELECT @var3;

# Now as derived table:
eval EXPLAIN SELECT * FROM ( $subq ) AS alias3;
eval SELECT * FROM ( $subq ) AS alias3;
SELECT @var3;

--echo
--echo Testcase with inner subquery; crashed WL#6095
SET @var3=8;
let $subq=
SELECT sq4_alias1.*
FROM t1 AS sq4_alias1
WHERE (sq4_alias1.col_varchar_key , sq4_alias1.col_varchar_nokey)
      NOT IN
      (SELECT c_sq1_alias1.col_varchar_key AS c_sq1_field1,
              c_sq1_alias1.col_varchar_nokey AS c_sq1_field2
       FROM t2 AS c_sq1_alias1
       WHERE (c_sq1_alias1.col_int_nokey != @var2
              OR c_sq1_alias1.pk != @var3));

eval EXPLAIN $subq;
eval $subq;
# Now as derived table:
eval EXPLAIN SELECT * FROM ( $subq ) AS alias3;
eval SELECT * FROM ( $subq ) AS alias3;

DROP TABLE t1,t2;

--echo #
--echo # Test that indexsubquery_engine only does one lookup if
--echo # the technique is unique_subquery: does not try to read the
--echo # next row if the first row failed the subquery's WHERE
--echo # condition (here: b=3).
--echo #

create table t1(a int);
insert into t1 values(1),(2);
create table t2(a int primary key, b int);
insert into t2 values(1,10),(2,10);
let $query=select * from t1 where a in (select a from t2 where b=3);
eval explain $query;
flush status;
eval $query;
show status like "handler_read%";
drop table t1,t2;

--echo #
--echo # Bug#13735980 Difference in number of rows when using subqueries
--echo #

CREATE TABLE m (c1 VARCHAR(1), c2 INTEGER, c3 INTEGER);
INSERT INTO m VALUES ('',6,8), ('',75,NULL);

CREATE TABLE o (c1 VARCHAR(1));
INSERT INTO o VALUES ('S'), ('S'), ('S');

CREATE VIEW v1 AS
SELECT m.c1 AS c1,m.c2 AS c2,m.c3 AS c3 FROM m;

CREATE VIEW v2 AS
SELECT o.c1 AS c1 FROM o;

SELECT t1.c3
FROM v1 AS t1 JOIN v2 AS t2 ON t1.c1 < t2.c1
WHERE t1.c3 BETWEEN 2 AND 6 OR
      (t1.c3 IN(4) AND t1.c3 <> 2) OR t1.c2 >= 8;

CREATE TABLE integers (i1 INTEGER);
INSERT IGNORE INTO integers VALUES (2),(4),(6),(8);

SELECT t1.c3
FROM v1 AS t1 JOIN v2 AS t2 ON t1.c1 < t2.c1
WHERE t1.c3 BETWEEN (SELECT i1 FROM integers WHERE i1 = 2) AND
                    (SELECT i1 FROM integers WHERE i1 = 6)  OR
      t1.c3 IN((SELECT i1 FROM integers WHERE i1 = 4)) AND
      t1.c3 <>(SELECT i1 FROM integers WHERE i1 = 2) OR
      t1.c2 >=(SELECT i1 FROM integers WHERE i1 = 8);

# Outcome is unaffected when replacing views with tables:

SELECT t1.c3
FROM m AS t1 JOIN o AS t2 ON t1.c1 < t2.c1
WHERE t1.c3 BETWEEN (SELECT i1 FROM integers WHERE i1 = 2) AND
                    (SELECT i1 FROM integers WHERE i1 = 6)  OR
      t1.c3 IN((SELECT i1 FROM integers WHERE i1 = 4)) AND
      t1.c3 <>(SELECT i1 FROM integers WHERE i1 = 2) OR
      t1.c2 >=(SELECT i1 FROM integers WHERE i1 = 8);

DROP VIEW v1, v2;
DROP TABLE m, o, integers;

--echo #
--echo # Bug#13721076 CRASH WITH TIME TYPE/TIMESTAMP() AND WARNINGS IN SUBQUERY
--echo #

CREATE TABLE t1(a TIME NOT NULL);
INSERT INTO t1 VALUES ('00:00:32');

SELECT 1 FROM t1 WHERE a >
(SELECT timestamp(a) AS a FROM t1);

DROP TABLE t1;

--echo #
--echo # Bug #13736664 RIGHT JOIN+WHERE QUERY GIVES DIFF RESULTS WHEN USING
--echo # SELECT * SUBQ FOR TABLES
--echo #

CREATE TABLE t1(a INT, b CHAR(1));
INSERT INTO t1 VALUES (NULL, 'x');

CREATE TABLE t2(c INT, d CHAR(1));
INSERT INTO t2 VALUES (NULL, 'y'), (9, 'z');

SELECT d
FROM t2 AS a2 RIGHT JOIN t1 AS a1 ON c = a
WHERE d LIKE '_'
ORDER BY d;

SELECT d
FROM (SELECT * FROM t2) AS a2 RIGHT JOIN (SELECT * FROM t1) AS a1 ON c = a
WHERE d LIKE '_'
ORDER BY d;

DROP TABLE t1, t2;

--echo #
--echo # Bug#13468414 Query shows different results when literals are selected
--echo #              from separate table
--echo #

CREATE TABLE t1 (
  col_varchar_key varchar(1) NOT NULL,
  col_varchar_nokey varchar(1) NOT NULL,
  KEY col_varchar_key (col_varchar_key)
) ENGINE=InnoDB;

INSERT INTO t1 VALUES
 ('v','v'), ('s','s'), ('l','l'), ('y','y'), ('c','c'),
 ('i','i'), ('h','h'), ('q','q'), ('a','a'), ('v','v'),
 ('u','u'), ('s','s'), ('y','y'), ('z','z'), ('h','h'),
 ('p','p'), ('e','e'), ('i','i'), ('y','y'), ('w','w');

CREATE TABLE t2 (
  col_int_nokey INT NOT NULL,
  col_varchar_nokey varchar(1) NOT NULL
) ENGINE=InnoDB;

INSERT INTO t2 VALUES
 (4,'j'), (6,'v'), (3,'c'), (5,'m'), (3,'d'), (246,'d'), (2,'y'), (9,'t'),
 (3,'d'), (8,'s'), (1,'r'), (8,'m'), (8,'b'), (5,'x'), (7,'g'), (5,'p'),
 (1,'q'), (6,'w'), (2,'d'), (9,'e');

CREATE TABLE t0 (
 i1 INTEGER NOT NULL PRIMARY KEY
);
INSERT INTO t0 VALUES (7);

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t0;
ANALYZE TABLE t1;
ANALYZE TABLE t2;
-- enable_result_log
-- enable_query_log

SELECT col_varchar_nokey
FROM t1
WHERE (7) NOT IN
 (SELECT it2.col_int_nokey
  FROM t2 AS it2 LEFT JOIN t1 AS it1
       ON it2.col_varchar_nokey = it1.col_varchar_key
 );

let $query=
SELECT col_varchar_nokey
FROM t1
WHERE (SELECT i1 FROM t0 WHERE i1 = 7) NOT IN
 (SELECT it2.col_int_nokey
  FROM t2 AS it2 LEFT JOIN t1 AS it1
       ON it2.col_varchar_nokey = it1.col_varchar_key
 );

eval $query;
eval explain extended $query;

DROP TABLE t0, t1, t2;

--echo #
--echo # Bug#13735704 Memory engine + NOT IN + subquery produces different
--echo # result set
--echo #

CREATE TABLE t1(
 col_varchar_key varchar(1),
 KEY col_varchar_key(col_varchar_key)
) engine=MEMORY;
INSERT INTO t1 VALUES
 ('v'), ('s'), ('y'), ('z'), ('h'), ('p');

CREATE TABLE t2(
 col_int_nokey int,
 col_int_key int,
 KEY col_int_key(col_int_key)
) engine=MEMORY;
INSERT INTO t2 VALUES
 (4,4), (2,7), (2,5), (2,3), (197,188), (4,4), (6,2), (1,4),
 (156,231), (2,1), (1,2), (80,194), (3,2), (8,3), (91,98), (6,3),
 (7,1), (3,0), (161,189), (7,8), (7,3), (213,120), (248,25), (1,1),
 (6,3), (6,1), (3,3), (140,153), (0,5), (7,9), (6,1), (1,2),
 (1,7), (9,2), (0,1), (6,5);

CREATE TABLE t0(i1 INTEGER NOT NULL PRIMARY KEY);
INSERT INTO t0 VALUES(7),(2);

SELECT col_varchar_key
FROM t1
WHERE (7,2) NOT IN
   (SELECT col_int_key,col_int_nokey FROM t2);

let $query=
SELECT col_varchar_key
FROM t1
WHERE ((SELECT i1 FROM t0 WHERE i1 = 7),
       (SELECT i1 FROM t0 WHERE i1 = 2)) NOT IN
    (SELECT col_int_key,col_int_nokey FROM t2);

eval explain extended $query;
eval $query;

DROP TABLE t0, t1, t2;


--echo #
--echo # Bug #13639204 64111: CRASH ON SELECT SUBQUERY WITH NON UNIQUE
--echo # INDEX
--echo #
CREATE TABLE t1 (
id int
) ENGINE=InnoDB;
INSERT INTO t1 (id) VALUES (11);

CREATE TABLE t2 (
t1_id int,
position int,
KEY t1_id (t1_id),
KEY t1_id_position (t1_id,position)
) ENGINE=InnoDB;

let $query=SELECT
(SELECT position FROM t2
WHERE t2.t1_id = t1.id
ORDER BY t2.t1_id , t2.position
LIMIT 10,1
) AS maxkey
FROM t1
LIMIT 1;

eval EXPLAIN $query;
eval $query;

DROP TABLE t1,t2;


--echo #
--echo # Bug#13731417 WRONG RESULT WITH NOT IN (SUBQUERY) AND
--echo # COMPOSITE INDEX ON SUBQUERY'S TABLE
--echo #

CREATE TABLE t1 (a int, b int);
CREATE TABLE t2 (a int, b int, KEY a_b (a,b));
CREATE TABLE t4 (a int);
INSERT INTO t1 VALUES(0,1);
INSERT INTO t2 VALUES(NULL,1),(NULL,1);
INSERT INTO t4 VALUES(1);

SELECT ((0,1) NOT IN (SELECT NULL,1)) IS NULL;

let $query=SELECT * FROM t1
WHERE ((a,b) NOT IN (SELECT t2.a,t2.b FROM
        t4 STRAIGHT_JOIN t2 WHERE t4.a=t2.b)) IS NULL;

eval EXPLAIN EXTENDED $query;
eval $query;

ALTER TABLE t2 DROP INDEX a_b;

EVAL EXPLAIN EXTENDED $query;
eval $query;

DROP TABLE t1,t2,t4;

--echo #
--echo # Bug#13725821 ASSERT NULL_REF_KEY == __NULL FAILED IN
--echo # CREATE_REF_FOR_KEY() IN SQL_SELECT.CC
--echo #

CREATE TABLE t1 (
  pk INT NOT NULL,
  col_int_nokey INT,
  col_int_key INT,
  col_varchar_key VARCHAR(1),
  col_varchar_nokey VARCHAR(1),
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
) ENGINE=MyISAM;

INSERT INTO t1 VALUES (17,NULL,9,NULL,NULL),
(18,2,2,'o','o'),(19,NULL,9,'w','w'),(20,6,2,'m','m'),(21,7,4,'q','q'),
(22,2,0,NULL,NULL),(23,5,4,'d','d'),(24,7,8,'g','g'),(25,6,NULL,'x','x'),
(26,6,NULL,'f','f'),(27,2,0,'p','p'),(28,9,NULL,'j','j'),(29,6,8,'c','c');

ALTER TABLE t1 DISABLE KEYS;
ALTER TABLE t1 ENABLE KEYS;

let $query=
SELECT table1.col_varchar_key AS field1,
       table1.col_int_nokey AS field2,
       table2.col_varchar_key AS field3,
       table1.col_int_nokey AS field4,
       table2.col_int_nokey AS field5,
       table1.col_varchar_nokey AS field6
FROM ( t1 AS table1
  INNER JOIN t1 AS table2
  ON (( table2.col_int_nokey = table1.col_int_key )
    AND ( table2.col_int_key = table1.col_int_key )
  ) )
WHERE ( table1.col_varchar_key = table2.col_varchar_key OR table1.pk = 154 )
  OR table1.pk != 201
  AND ( table1.col_varchar_key LIKE '%a%' OR table1.col_varchar_key LIKE
'%b%')
;

eval CREATE TABLE t2 $query ;

eval SELECT *
     FROM t2
     WHERE (field1, field2, field3, field4, field5, field6) NOT IN ( $query
);

DROP TABLE t2,t1;

--echo #
--echo # Bug #13735712 SELECT W/ SUBQUERY PRODUCES MORE ROWS WHEN USING
--echo # VARIABLES
--echo #

CREATE TABLE t1(a INT);
INSERT INTO t1 VALUES(9);
CREATE TABLE t2(b INT);
INSERT INTO t2 VALUES(8);
CREATE TABLE t3(c INT);
INSERT INTO t3 VALUES(3);

SELECT *
FROM t2 RIGHT JOIN t3 ON(c = b)
WHERE b < ALL(SELECT a FROM t1 WHERE a <= 7);
SET @var = 7;
SELECT *
FROM t2 RIGHT JOIN t3 ON(c = b)
WHERE b < ALL(SELECT a FROM t1 WHERE a <= @var);

DROP TABLE t1, t2, t3;

--echo #
--echo # Bug #13330886 TOO MANY ROWS WITH ALL|ANY
--echo #

CREATE TABLE t1 (a INT, b INT);
INSERT INTO t1 VALUES (NULL,8), (8,7);
CREATE TABLE t2 (c INT);
INSERT INTO t2 VALUES (10);

SELECT 1 FROM t2 WHERE c > ALL (SELECT a FROM t1 WHERE b >= 3);
SET @var = 3;
SELECT 1 FROM t2 WHERE c > ALL (SELECT a FROM t1 WHERE b >= @var);

DROP TABLE t1, t2;

CREATE TABLE t1 (a INT NOT NULL, b INT NOT NULL);
INSERT INTO t1 VALUES (0,8), (8,7);
CREATE TABLE t2 (c INT NOT NULL);
INSERT INTO t2 VALUES (10);

EXPLAIN EXTENDED
SELECT 1 FROM t2 WHERE c > ANY (SELECT a FROM t1 WHERE b >= 3) IS TRUE;
SELECT 1 FROM t2 WHERE c > ANY (SELECT a FROM t1 WHERE b >= 3) IS TRUE;

DROP TABLE t1, t2;

--echo # Bug #18014565 - WRONG RESULT COMPUTATION USING ALL() AND GROUP BY
--echo #

CREATE TABLE t1 (
  f1 INTEGER,
  f2 INTEGER
);

INSERT INTO t1 VALUES (1,3),(1,6),(2,3),(2,6);

SELECT f1, SUM(f2) AS sum FROM t1 GROUP BY f1 HAVING sum > ALL (SELECT 1);

DROP TABLE t1;

--echo # End of test for Bug#18014565

set @@optimizer_switch=@old_opt_switch;
# New tests go here.
