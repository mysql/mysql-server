--disable_warnings
DROP TABLE IF EXISTS t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11;
DROP DATABASE IF EXISTS world;
--enable_warnings

set names utf8;

CREATE DATABASE world;

use world;

--source include/world_schema1.inc

--disable_query_log
--disable_result_log
--disable_warnings
--source include/world.inc
--enable_warnings
--enable_result_log
--enable_query_log

SELECT COUNT(*) FROM Country;
SELECT COUNT(*) FROM City;
SELECT COUNT(*) FROM CountryLanguage;

show variables like 'join_buffer_size';

EXPLAIN
SELECT City.Name, Country.Name FROM City,Country
  WHERE City.Country=Country.Code AND 
        Country.Name LIKE 'L%' AND City.Population > 100000;

--sorted_result
SELECT City.Name, Country.Name FROM City,Country
  WHERE City.Country=Country.Code AND 
        Country.Name LIKE 'L%' AND City.Population > 100000;

EXPLAIN
SELECT City.Name, Country.Name, CountryLanguage.Language
  FROM City,Country,CountryLanguage
  WHERE City.Country=Country.Code AND
        CountryLanguage.Country=Country.Code AND
        City.Name LIKE 'L%' AND Country.Population > 3000000 AND
        CountryLanguage.Percentage > 50;

--sorted_result
SELECT City.Name, Country.Name, CountryLanguage.Language
  FROM City,Country,CountryLanguage
  WHERE City.Country=Country.Code AND
        CountryLanguage.Country=Country.Code AND
        City.Name LIKE 'L%' AND Country.Population > 3000000 AND
        CountryLanguage.Percentage > 50;

set join_buffer_size=256;
show variables like 'join_buffer_size';

EXPLAIN
SELECT City.Name, Country.Name FROM City,Country
  WHERE City.Country=Country.Code AND 
        Country.Name LIKE 'L%' AND City.Population > 100000;

--sorted_result
SELECT City.Name, Country.Name FROM City,Country
  WHERE City.Country=Country.Code AND 
        Country.Name LIKE 'L%' AND City.Population > 100000;

EXPLAIN
SELECT City.Name, Country.Name, CountryLanguage.Language
  FROM City,Country,CountryLanguage
  WHERE City.Country=Country.Code AND
        CountryLanguage.Country=Country.Code AND
        City.Name LIKE 'L%' AND Country.Population > 3000000 AND
        CountryLanguage.Percentage > 50;

--sorted_result
SELECT City.Name, Country.Name, CountryLanguage.Language
  FROM City,Country,CountryLanguage
  WHERE City.Country=Country.Code AND
        CountryLanguage.Country=Country.Code AND
        City.Name LIKE 'L%' AND Country.Population > 3000000 AND
        CountryLanguage.Percentage > 50;

set join_buffer_size=default;
show variables like 'join_buffer_size';

DROP DATABASE world; 


CREATE DATABASE world;

use world;

--source include/world_schema.inc

--disable_query_log
--disable_result_log
--disable_warnings
--source include/world.inc
--enable_warnings
--enable_result_log
--enable_query_log

show variables like 'join_buffer_size';

EXPLAIN
SELECT City.Name, Country.Name FROM City,Country
  WHERE City.Country=Country.Code AND 
        Country.Name LIKE 'L%' AND City.Population > 100000;

--sorted_result
SELECT City.Name, Country.Name FROM City,Country
  WHERE City.Country=Country.Code AND 
        Country.Name LIKE 'L%' AND City.Population > 100000;

--replace_result 185 # 188 #
EXPLAIN
SELECT City.Name, Country.Name, CountryLanguage.Language
  FROM City,Country,CountryLanguage
  WHERE City.Country=Country.Code AND
        CountryLanguage.Country=Country.Code AND
        City.Name LIKE 'L%' AND Country.Population > 3000000 AND
        CountryLanguage.Percentage > 50;

--sorted_result
SELECT City.Name, Country.Name, CountryLanguage.Language
  FROM City,Country,CountryLanguage
  WHERE City.Country=Country.Code AND
        CountryLanguage.Country=Country.Code AND
        City.Name LIKE 'L%' AND Country.Population > 3000000 AND
        CountryLanguage.Percentage > 50;

EXPLAIN
SELECT Name FROM City
  WHERE City.Country IN (SELECT Code FROM Country WHERE Country.Name LIKE 'L%') AND
        City.Population > 100000;

--sorted_result
SELECT Name FROM City
  WHERE City.Country IN (SELECT Code FROM Country WHERE Country.Name LIKE 'L%') AND
        City.Population > 100000;

EXPLAIN
SELECT Country.Name, IF(ISNULL(CountryLanguage.Country), NULL, CountryLanguage.Percentage)
  FROM Country LEFT JOIN CountryLanguage ON
       (CountryLanguage.Country=Country.Code AND Language='English')
  WHERE 
       Country.Population > 10000000;

--sorted_result
SELECT Country.Name, IF(ISNULL(CountryLanguage.Country), NULL, CountryLanguage.Percentage)
  FROM Country LEFT JOIN CountryLanguage ON
       (CountryLanguage.Country=Country.Code AND Language='English')
  WHERE 
       Country.Population > 10000000;

set join_buffer_size=256;
show variables like 'join_buffer_size';

EXPLAIN
SELECT City.Name, Country.Name FROM City,Country
  WHERE City.Country=Country.Code AND 
        Country.Name LIKE 'L%' AND City.Population > 100000;

--sorted_result
SELECT City.Name, Country.Name FROM City,Country
  WHERE City.Country=Country.Code AND 
        Country.Name LIKE 'L%' AND City.Population > 100000;

--replace_result 185 # 188 #
EXPLAIN
SELECT City.Name, Country.Name, CountryLanguage.Language
  FROM City,Country,CountryLanguage
  WHERE City.Country=Country.Code AND
        CountryLanguage.Country=Country.Code AND
        City.Name LIKE 'L%' AND Country.Population > 3000000 AND
        CountryLanguage.Percentage > 50;

--sorted_result
SELECT City.Name, Country.Name, CountryLanguage.Language
  FROM City,Country,CountryLanguage
  WHERE City.Country=Country.Code AND
        CountryLanguage.Country=Country.Code AND
        City.Name LIKE 'L%' AND Country.Population > 3000000 AND
        CountryLanguage.Percentage > 50;

EXPLAIN
SELECT Name FROM City
  WHERE City.Country IN (SELECT Code FROM Country WHERE Country.Name LIKE 'L%') AND
        City.Population > 100000;

--sorted_result
SELECT Name FROM City
  WHERE City.Country IN (SELECT Code FROM Country WHERE Country.Name LIKE 'L%') AND
        City.Population > 100000;

set join_buffer_size=default;
show variables like 'join_buffer_size';

--sorted_result
SELECT City.Name, Country.Name FROM City,Country
  WHERE City.Country=Country.Code AND City.Population > 3000000;

set join_buffer_size=256;

--replace_column 9 #
EXPLAIN
SELECT City.Name, Country.Name FROM City,Country
  WHERE City.Country=Country.Code AND City.Population > 3000000;

--sorted_result
SELECT City.Name, Country.Name FROM City,Country
  WHERE City.Country=Country.Code AND City.Population > 3000000;

set join_buffer_size=default;

ALTER TABLE Country MODIFY Name varchar(52) NOT NULL default '';

--sorted_result
SELECT City.Name, Country.Name FROM City,Country
  WHERE City.Country=Country.Code AND 
        Country.Name LIKE 'L%' AND City.Population > 100000;

ALTER TABLE Country MODIFY Name varchar(300) NOT NULL default '';

--sorted_result
SELECT City.Name, Country.Name FROM City,Country
  WHERE City.Country=Country.Code AND 
        Country.Name LIKE 'L%' AND City.Population > 100000;

ALTER TABLE Country ADD COLUMN PopulationBar text;
UPDATE Country 
  SET PopulationBar=REPEAT('x', CAST(Population/100000 AS unsigned int));

--sorted_result
SELECT City.Name, Country.Name, Country.PopulationBar FROM City,Country
  WHERE City.Country=Country.Code AND 
        Country.Name LIKE 'L%' AND City.Population > 100000;

set join_buffer_size=256;

--sorted_result
SELECT City.Name, Country.Name, Country.PopulationBar FROM City,Country
  WHERE City.Country=Country.Code AND 
        Country.Name LIKE 'L%' AND City.Population > 100000;

set join_buffer_size=default;

DROP DATABASE world;

use test;

#
# Bug #35685: assertion abort when initializing a BKA cache
#

CREATE TABLE t1(
  affiliatetometaid int  NOT NULL default '0',
  uniquekey int NOT NULL default '0',
  metaid int  NOT NULL default '0',
  affiliateid int  NOT NULL default '0',
  xml text,
  isactive char(1) NOT NULL default 'Y',
  PRIMARY KEY  (affiliatetometaid)
);
CREATE UNIQUE INDEX t1_uniquekey ON t1(uniquekey);
CREATE INDEX t1_affiliateid ON t1(affiliateid);
CREATE INDEX t1_metaid on t1 (metaid);
INSERT INTO t1 VALUES
  (1616, 1571693233, 1391, 2, NULL, 'Y'), (1943, 1993216749, 1726, 2, NULL, 'Y');

CREATE TABLE t2(
  metaid int  NOT NULL default '0',
  name varchar(80) NOT NULL default '',
  dateadded timestamp NOT NULL ,
  xml text,
  status int default NULL,
  origin int default NULL,
  gid int NOT NULL default '1',
  formattypeid int  default NULL,
  PRIMARY KEY  (metaid)
);
CREATE INDEX t2_status ON t2(status);
CREATE INDEX t2_gid ON t2(gid);
CREATE INDEX t2_formattypeid ON t2(formattypeid);
INSERT INTO t2 VALUES
 (1391, "I Just Died", "2003-10-02 10:07:37", "", 1, NULL, 3, NULL),
 (1726, "Me, Myself & I", "2003-12-05 11:24:36", " ", 1, NULL, 3, NULL);

CREATE TABLE t3(
  mediaid int  NOT NULL ,
  metaid int  NOT NULL default '0',
  formatid int  NOT NULL default '0',
  status int default NULL,
  path varchar(100) NOT NULL default '',
  datemodified timestamp NOT NULL ,
  resourcetype int  NOT NULL default '1',
  parameters text,
  signature int  default NULL,
  quality int  NOT NULL default '255',
  PRIMARY KEY  (mediaid)
);
CREATE INDEX t3_metaid ON t3(metaid);
CREATE INDEX t3_formatid ON t3(formatid);
CREATE INDEX t3_status ON t3(status);
CREATE INDEX t3_metaidformatid ON t3(metaid,formatid);
CREATE INDEX t3_signature ON t3(signature);
CREATE INDEX t3_quality ON t3(quality);
INSERT INTO t3 VALUES
  (6, 4, 8, 0, "010101_anastacia_spmidi.mid", "2004-03-16 13:40:00", 1, NULL, NULL, 255),
  (3343, 3, 8, 1, "010102_4VN4bsPwnxRQUJW5Zp1RhG2IL9vvl_8.mid", "2004-03-16 13:40:00", 1, NULL, NULL, 255);

CREATE TABLE t4(
  formatid int  NOT NULL ,
  name varchar(60) NOT NULL default '',
  formatclassid int  NOT NULL default '0',
  mime varchar(60) default NULL,
  extension varchar(10) default NULL,
  priority int NOT NULL default '0',
  canaddtocapability char(1) NOT NULL default 'Y',
  PRIMARY KEY  (formatid)
);
CREATE INDEX t4_formatclassid ON t4(formatclassid);
CREATE INDEX t4_formats_idx ON t4(canaddtocapability);
INSERT INTO t4 VALUES
  (19, "XHTML", 11, "text/html", "xhtml", 10, 'Y'),
  (54, "AMR (wide band)", 13, "audio/amr-wb", "awb", 0, 'Y');

CREATE TABLE t5(
  formatclassid int  NOT NULL ,
  name varchar(60) NOT NULL default '',
  priority int NOT NULL default '0',
  formattypeid int  NOT NULL default '0',
  PRIMARY KEY  (formatclassid)
);
CREATE INDEX t5_formattypeid on t5(formattypeid);
INSERT INTO t5 VALUES
  (11, "Info", 0, 4), (13, "Digital Audio", 0, 2);

CREATE TABLE t6(
  formattypeid int  NOT NULL ,
  name varchar(60) NOT NULL default '',
  priority int default NULL,
  PRIMARY KEY  (formattypeid)
);
INSERT INTO t6 VALUES
 (2, "Ringtones", 0);

CREATE TABLE t7(
  metaid int  NOT NULL default '0',
  artistid int  NOT NULL default '0',
  PRIMARY KEY  (metaid,artistid)
);
INSERT INTO t7 VALUES
  (4, 5), (3, 4);

CREATE TABLE t8(
  artistid int  NOT NULL ,
  name varchar(80) NOT NULL default '',
  PRIMARY KEY  (artistid)
);
INSERT INTO t8 VALUES
  (5, "Anastacia"), (4, "John Mayer");

CREATE TABLE t9(
  subgenreid int  NOT NULL default '0',
  metaid int  NOT NULL default '0',
  PRIMARY KEY  (subgenreid,metaid)
) ;
CREATE INDEX t9_subgenreid ON t9(subgenreid);
CREATE INDEX t9_metaid ON t9(metaid);
INSERT INTO t9 VALUES 
  (138, 4), (31, 3);

CREATE TABLE t10(
  subgenreid int  NOT NULL ,
  genreid int  NOT NULL default '0',
  name varchar(80) NOT NULL default '',
  PRIMARY KEY  (subgenreid)
) ;
CREATE INDEX t10_genreid ON t10(genreid);
INSERT INTO t10 VALUES 
  (138, 19, ''), (31, 3, '');

CREATE TABLE t11(
  genreid int  NOT NULL default '0',
  name char(80) NOT NULL default '',
  priority int NOT NULL default '0',
  masterclip char(1) default NULL,
  PRIMARY KEY  (genreid)
) ;
CREATE INDEX t11_masterclip ON t11( masterclip);
INSERT INTO t11 VALUES
  (19, "Pop & Dance", 95, 'Y'), (3, "Rock & Alternative", 100, 'Y');

EXPLAIN
SELECT t1.uniquekey, t1.xml AS affiliateXml,
       t8.name AS artistName, t8.artistid, 
       t11.name AS genreName, t11.genreid, t11.priority AS genrePriority,
       t10.subgenreid, t10.name AS subgenreName,
       t2.name AS metaName, t2.metaid, t2.xml AS metaXml,
       t4.priority + t5.priority + t6.priority AS overallPriority,
       t3.path AS path, t3.mediaid, 
       t4.formatid, t4.name AS formatName, 
       t5.formatclassid, t5.name AS formatclassName, 
       t6.formattypeid, t6.name AS formattypeName 
FROM t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11
WHERE t7.metaid = t2.metaid AND t7.artistid = t8.artistid AND
      t9.metaid = t2.metaid AND t9.subgenreid = t10.subgenreid AND 
      t10.genreid = t11.genreid AND  t3.metaid = t2.metaid AND
      t3.formatid = t4.formatid AND t4.formatclassid = t5.formatclassid AND
      t4.canaddtocapability =  'Y' AND t5.formattypeid = t6.formattypeid AND
      t6.formattypeid IN (2) AND (t3.formatid IN (31, 8, 76)) AND
      t1.metaid = t2.metaid AND t1.affiliateid = '2';

SELECT t1.uniquekey, t1.xml AS affiliateXml,
       t8.name AS artistName, t8.artistid, 
       t11.name AS genreName, t11.genreid, t11.priority AS genrePriority,
       t10.subgenreid, t10.name AS subgenreName,
       t2.name AS metaName, t2.metaid, t2.xml AS metaXml,
       t4.priority + t5.priority + t6.priority AS overallPriority,
       t3.path AS path, t3.mediaid, 
       t4.formatid, t4.name AS formatName, 
       t5.formatclassid, t5.name AS formatclassName, 
       t6.formattypeid, t6.name AS formattypeName 
FROM t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11
WHERE t7.metaid = t2.metaid AND t7.artistid = t8.artistid AND
      t9.metaid = t2.metaid AND t9.subgenreid = t10.subgenreid AND 
      t10.genreid = t11.genreid AND  t3.metaid = t2.metaid AND
      t3.formatid = t4.formatid AND t4.formatclassid = t5.formatclassid AND
      t4.canaddtocapability =  'Y' AND t5.formattypeid = t6.formattypeid AND
      t6.formattypeid IN (2) AND (t3.formatid IN (31, 8, 76)) AND
      t1.metaid = t2.metaid AND t1.affiliateid = '2';

DROP TABLE t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11;

#
# Bug #37131: 3-way join query with BKA used with a small buffer and
#             only for the third table
#

CREATE TABLE t1 (a1 int, filler1 char(64) default ' ' );
CREATE TABLE t2 (
  a2 int, b2 int, filler2 char(64) default ' ', 
  PRIMARY KEY idx(a2,b2,filler2)
) ;
CREATE TABLE t3 (b3 int, c3 int, INDEX idx(b3));

INSERT INTO t1(a1) VALUES 
 (4), (7), (1), (9), (8), (5), (3), (6), (2);
INSERT INTO t2(a2,b2) VALUES
 (1,30), (3,40), (2,61), (6,73), (8,92), (9,27), (4,18), (5,84), (7,56),
 (4,14), (6,76), (8,98), (7,55), (1,39), (2,68), (3,45), (9,21), (5,81),
 (5,88), (2,65), (6,74), (9,23), (1,37), (3,44), (4,17), (8,99), (7,51),
 (9,28), (7,52), (1,33), (4,13), (5,87), (3,43), (8,91), (2,62), (6,79),
 (3,49), (8,93), (7,34), (5,82), (6,78), (2,63), (1,32), (9,22), (4,11);
INSERT INTO t3 VALUES
 (30,302), (92,923), (18,187), (45,459), (30,309), 
 (39,393), (68,685), (45,458), (21,210), (81,817),
 (40,405), (61,618), (73,738), (92,929), (27,275),
 (18,188), (84,846), (56,564), (14,144), (76,763), 
 (98,982), (55,551), (17,174), (99,998), (51,513),
 (28,282), (52,527), (33,336), (13,138), (87,878), 
 (43,431), (91,916), (62,624), (79,797), (49,494),
 (93,933), (34,347), (82,829), (78,780), (63,634), 
 (32,329), (22,228), (11,114), (74,749), (23,236);

EXPLAIN
SELECT a1<>a2, a1, a2, b2, b3, c3,
       SUBSTR(filler1,1,1) AS s1, SUBSTR(filler2,1,1) AS s2
FROM t1,t2,t3 WHERE a1=a2 AND b2=b3 AND MOD(c3,10)>7; 

--sorted_result
SELECT a1<>a2, a1, a2, b2, b3, c3,
       SUBSTR(filler1,1,1) AS s1, SUBSTR(filler2,1,1) AS s2
FROM t1,t2,t3 WHERE a1=a2 AND b2=b3 AND MOD(c3,10)>7;
 
set join_buffer_size=512;

EXPLAIN
SELECT a1<>a2, a1, a2, b2, b3, c3,
       SUBSTR(filler1,1,1) AS s1, SUBSTR(filler2,1,1) AS s2
FROM t1,t2,t3 WHERE a1=a2 AND b2=b3 AND MOD(c3,10)>7; 

--sorted_result
SELECT a1<>a2, a1, a2, b2, b3, c3,
       SUBSTR(filler1,1,1) AS s1, SUBSTR(filler2,1,1) AS s2
FROM t1,t2,t3 WHERE a1=a2 AND b2=b3 AND MOD(c3,10)>7;
 
DROP TABLE t1,t2,t3;

#
# Bug #37690: crash with a tiny buffer when using BKA_JOIN_CACHE_UNIQUE
#

CREATE TABLE t1 (a int, b int, INDEX idx(b));
CREATE TABLE t2 (a int, b int, INDEX idx(a)); 
INSERT INTO t1 VALUES (5,30), (3,20), (7,40), (2,10), (8,30), (1,10), (4,20);     
INSERT INTO t2 VALUES (7,10), (1,20), (2,20), (8,20), (8,10), (1,20);
INSERT INTO t2 VALUES (1,10), (4,20), (3,20), (7,20), (7,10), (1,20); 
    
set join_buffer_size=32; 
 
EXPLAIN SELECT * FROM t1,t2 WHERE t1.a=t2.a AND t1.b >= 30; 
--sorted_result  
SELECT * FROM t1,t2 WHERE t1.a=t2.a AND t1.b >= 30;  

DROP TABLE t1,t2;
 
--echo 
--echo BUG#40136: Group by is ignored when join buffer is used for an outer join
--echo 
create table t1(a int PRIMARY KEY, b int);
insert into t1 values
  (5, 10), (2, 70), (7, 80), (6, 20), (1, 50), (9, 40), (8, 30), (3, 60);
create table t2 (p int, a int, INDEX i_a(a));
insert into t2 values
  (103, 7), (109, 3), (102, 3), (108, 1), (106, 3),
  (107, 7), (105, 1), (101, 3), (100, 7), (110, 1);
explain
select t1.a, count(t2.p) as count
  from t1 left join t2 on t1.a=t2.a and t2.p % 2 = 1 group by t1.a;
select t1.a, count(t2.p) as count
  from t1 left join t2 on t1.a=t2.a and t2.p % 2 = 1 group by t1.a;
drop table t1, t2;

--echo #
--echo # Bug #40134: outer join with not exists optimization and join buffer
--echo #

set join_buffer_size=default;

CREATE TABLE t1 (a int NOT NULL);
INSERT INTO t1 VALUES (2), (4), (3), (5), (1);
CREATE TABLE t2 (a int NOT NULL, b int NOT NULL, INDEX i_a(a));
INSERT INTO t2 VALUES (4,10), (2,10), (2,30), (2,20), (4,20);

EXPLAIN
SELECT * FROM t1 LEFT JOIN t2 ON t1.a=t2.a WHERE t2.b IS NULL;
SELECT * FROM t1 LEFT JOIN t2 ON t1.a=t2.a WHERE t2.b IS NULL;

DROP TABLE t1, t2;

--echo #
--echo # BUG#40268: Nested outer join with not null-rejecting where condition
--echo #            over an inner table which is not the last in the nest
--echo #

CREATE TABLE t2 (a int, b int, c int);
CREATE TABLE t3 (a int, b int, c int);
CREATE TABLE t4 (a int, b int, c int);

INSERT INTO t2 VALUES (3,3,0), (4,2,0), (5,3,0);
INSERT INTO t3 VALUES (1,2,0), (2,2,0);
INSERT INTO t4 VALUES (3,2,0), (4,2,0);

--sorted_result
SELECT t2.a,t2.b,t3.a,t3.b,t4.a,t4.b
  FROM t2 LEFT JOIN (t3, t4) ON t2.b=t4.b
     WHERE t3.a+2<t2.a OR t3.c IS NULL;

DROP TABLE t2, t3, t4;

--echo #
--echo # Bug #40192: outer join with where clause when using BNL 
--echo #

create table t1 (a int, b int);
insert into t1 values (2, 20), (3, 30), (1, 10);
create table t2 (a int, c int);
insert into t2 values (1, 101), (3, 102), (1, 100);

--sorted_result
select * from t1 left join t2 on t1.a=t2.a;
explain select * from t1 left join t2 on t1.a=t2.a where t2.c=102 or t2.c is null;
--sorted_result
select * from t1 left join t2 on t1.a=t2.a where t2.c=102 or t2.c is null;

drop table t1, t2;

--echo #
--echo # Bug #40317: outer join with with constant on expression equal to FALSE
--echo #

create table t1 (a int);
insert into t1 values (30), (40), (20);
create table t2 (b int);
insert into t2 values (200), (100);

select * from t1 left join t2 on (1=0);
explain select * from t1 left join t2 on (1=0) where a=40;
select * from t1 left join t2 on (1=0) where a=40;

drop table t1, t2;

--echo #
--echo # Bug #41204: small buffer with big rec_per_key for ref access
--echo #

CREATE TABLE t1 (a int);

INSERT INTO t1 VALUES (0);
INSERT INTO t1(a) SELECT a FROM t1;
INSERT INTO t1(a) SELECT a FROM t1;
INSERT INTO t1(a) SELECT a FROM t1;
INSERT INTO t1(a) SELECT a FROM t1;
INSERT INTO t1(a) SELECT a FROM t1;
INSERT INTO t1(a) SELECT a FROM t1;
INSERT INTO t1(a) SELECT a FROM t1;
INSERT INTO t1(a) SELECT a FROM t1;
INSERT INTO t1(a) SELECT a FROM t1;
INSERT INTO t1(a) SELECT a FROM t1;
INSERT INTO t1(a) SELECT a FROM t1;
INSERT INTO t1 VALUES (20000), (10000);

CREATE TABLE t2 (pk int AUTO_INCREMENT PRIMARY KEY, b int, c int, INDEX idx(b));
INSERT INTO t2(b,c) VALUES (10000, 3), (20000, 7), (20000, 1), (10000, 9), (20000, 5);
INSERT INTO t2(b,c) SELECT b,c FROM t2;
INSERT INTO t2(b,c) SELECT b,c FROM t2;
INSERT INTO t2(b,c) SELECT b,c FROM t2;
INSERT INTO t2(b,c) SELECT b,c FROM t2;
INSERT INTO t2(b,c) SELECT b,c FROM t2;
INSERT INTO t2(b,c) SELECT b,c FROM t2;
INSERT INTO t2(b,c) SELECT b,c FROM t2;
INSERT INTO t2(b,c) SELECT b,c FROM t2;

--disable_result_log
ANALYZE TABLE t1,t2;
--enable_result_log

set join_buffer_size=1024;

EXPLAIN SELECT AVG(c) FROM t1,t2 WHERE t1.a=t2.b;
SELECT AVG(c) FROM t1,t2 WHERE t1.a=t2.b;

set join_buffer_size=default;

DROP TABLE t1, t2;

--echo #
--echo # Bug #41894: big join buffer of level 7 used to join records
--echo #              with null values in place of varchar strings
--echo #

CREATE TABLE t1 (a int NOT NULL AUTO_INCREMENT PRIMARY KEY,
                 b varchar(127) DEFAULT NULL);

INSERT INTO t1(a) VALUES (1);
INSERT INTO t1(b) SELECT b FROM t1;
INSERT INTO t1(b) SELECT b FROM t1;
INSERT INTO t1(b) SELECT b FROM t1;
INSERT INTO t1(b) SELECT b FROM t1;
INSERT INTO t1(b) SELECT b FROM t1;
INSERT INTO t1(b) SELECT b FROM t1;
INSERT INTO t1(b) SELECT b FROM t1;
INSERT INTO t1(b) SELECT b FROM t1;
INSERT INTO t1(b) SELECT b FROM t1;
INSERT INTO t1(b) SELECT b FROM t1;
INSERT INTO t1(b) SELECT b FROM t1;
INSERT INTO t1(b) SELECT b FROM t1;
INSERT INTO t1(b) SELECT b FROM t1;
INSERT INTO t1(b) SELECT b FROM t1;

CREATE TABLE t2 (a int NOT NULL PRIMARY KEY, b varchar(127) DEFAULT NULL);
INSERT INTO t2 SELECT * FROM t1;

CREATE TABLE t3 (a int NOT NULL PRIMARY KEY, b varchar(127) DEFAULT NULL);
INSERT INTO t3 SELECT * FROM t1;

set join_buffer_size=1024*1024;

EXPLAIN
SELECT COUNT(*) FROM t1,t2,t3
  WHERE t1.a=t2.a AND t2.a=t3.a AND
        t1.b IS NULL AND t2.b IS NULL AND t3.b IS NULL;

SELECT COUNT(*) FROM t1,t2,t3
  WHERE t1.a=t2.a AND t2.a=t3.a AND
        t1.b IS NULL AND t2.b IS NULL AND t3.b IS NULL;

set join_buffer_size=default;

DROP TABLE t1,t2,t3;

--echo #
--echo # Bug #42020: join buffer is used  for outer join with fields of 
--echo #             several outer tables in join buffer
--echo #

CREATE TABLE t1 (
  a bigint NOT NULL,
  PRIMARY KEY (a) 
);
INSERT INTO t1 VALUES
  (2), (1);

CREATE TABLE t2 (
  a bigint NOT NULL,
  b bigint NOT NULL,
  PRIMARY KEY (a,b)
);
INSERT INTO t2 VALUES
  (2,30), (2,40), (2,50), (2,60), (2,70), (2,80),
  (1,10), (1, 20), (1,30), (1,40), (1,50);

CREATE TABLE t3 (
  pk bigint NOT NULL AUTO_INCREMENT,
  a bigint NOT NULL,
  b bigint NOT NULL,
  val bigint DEFAULT '0',
  PRIMARY KEY (pk),
  KEY idx (a,b)
);
INSERT INTO t3(a,b) VALUES
  (2,30), (2,40), (2,50), (2,60), (2,70), (2,80),
  (4,30), (4,40), (4,50), (4,60), (4,70), (4,80),
  (5,30), (5,40), (5,50), (5,60), (5,70), (5,80),
  (7,30), (7,40), (7,50), (7,60), (7,70), (7,80);

--sorted_result
SELECT t1.a, t2.a, t3.a, t2.b, t3.b, t3.val 
  FROM (t1,t2) LEFT JOIN t3 ON (t1.a=t3.a AND t2.b=t3.b) 
    WHERE t1.a=t2.a; 

set join_buffer_size=256;

EXPLAIN
SELECT t1.a, t2.a, t3.a, t2.b, t3.b, t3.val 
  FROM (t1,t2) LEFT JOIN t3 ON (t1.a=t3.a AND t2.b=t3.b) 
    WHERE t1.a=t2.a; 
--sorted_result
SELECT t1.a, t2.a, t3.a, t2.b, t3.b, t3.val 
  FROM (t1,t2) LEFT JOIN t3 ON (t1.a=t3.a AND t2.b=t3.b) 
    WHERE t1.a=t2.a;

DROP INDEX idx ON t3;

EXPLAIN
SELECT t1.a, t2.a, t3.a, t2.b, t3.b, t3.val 
  FROM (t1,t2) LEFT JOIN t3 ON (t1.a=t3.a AND t2.b=t3.b) 
    WHERE t1.a=t2.a; 

--sorted_result
SELECT t1.a, t2.a, t3.a, t2.b, t3.b, t3.val 
  FROM (t1,t2) LEFT JOIN t3 ON (t1.a=t3.a AND t2.b=t3.b) 
    WHERE t1.a=t2.a; 

set join_buffer_size=default;
DROP TABLE t1,t2,t3;

#
# WL#4424 Full index condition pushdown with batched key access join
#
create table t1(f1 int, f2 int);
insert into t1 values (1,1),(2,2),(3,3);
create table t2(f1 int not null, f2 int not null, f3 char(200), key(f1,f2));
insert into t2 values (1,1, 'qwerty'),(1,2, 'qwerty'),(1,3, 'qwerty');
insert into t2 values (2,1, 'qwerty'),(2,2, 'qwerty'),(2,3, 'qwerty'),
                      (2,4, 'qwerty'),(2,5, 'qwerty');
insert into t2 values (3,1, 'qwerty'),(3,4, 'qwerty');
insert into t2 values (4,1, 'qwerty'),(4,2, 'qwerty'),(4,3, 'qwerty'),
                      (4,4, 'qwerty');
insert into t2 values (1,1, 'qwerty'),(1,2, 'qwerty'),(1,3, 'qwerty');
insert into t2 values (2,1, 'qwerty'),(2,2, 'qwerty'),(2,3, 'qwerty'),
                      (2,4, 'qwerty'),(2,5, 'qwerty');
insert into t2 values (3,1, 'qwerty'),(3,4, 'qwerty');
insert into t2 values (4,1, 'qwerty'),(4,2, 'qwerty'),(4,3, 'qwerty'),
                      (4,4, 'qwerty');

--sorted_result
select t2.f1, t2.f2, t2.f3 from t1,t2
where t1.f1=t2.f1 and t2.f2 between t1.f1 and t1.f2 and t2.f2 + 1 >= t1.f1 + 1;

explain select t2.f1, t2.f2, t2.f3 from t1,t2
where t1.f1=t2.f1 and t2.f2 between t1.f1 and t2.f2;

drop table t1,t2;

--echo #
--echo # Bug #42955: join with GROUP BY/ORDER BY and when BKA is enabled 
--echo #             

create table t1 (d int, id1 int, index idx1 (d, id1));
insert into t1 values
  (3, 20), (2, 40), (3, 10), (1, 10), (3, 20), (1, 40), (2, 30), (3, 30);

create table t2 (id1 int, id2 int, index idx2 (id1));
insert into t2 values 
  (20, 100), (30, 400), (20, 400), (30, 200), (10, 300), (10, 200), (40, 100),
  (40, 200), (30, 300), (10, 400), (20, 200), (20, 300);   

explain
select t1.id1, sum(t2.id2) from t1 join t2 on t1.id1=t2.id1 
  where t1.d=3 group by t1.id1;

select t1.id1, sum(t2.id2) from t1 join t2 on t1.id1=t2.id1 
  where t1.d=3 group by t1.id1;

explain
select t1.id1  from t1 join t2 on t1.id1=t2.id1 
 where t1.d=3 and t2.id2 > 200 order by t1.id1;

select t1.id1  from t1 join t2 on t1.id1=t2.id1 
 where t1.d=3 and t2.id2 > 200 order by t1.id1;

drop table t1,t2;

--echo #
--echo # Bug #44019: star-like multi-join query executed optimizer_join_cache_level=6 
--echo #             

create table t1 (a int, b int, c int, d int);
create table t2 (b int, e varchar(16), index idx(b));
create table t3 (d int, f varchar(16), index idx(d));
create table t4 (c int, g varchar(16), index idx(c));

insert into t1 values
  (5, 50, 500, 5000), (3, 30, 300, 3000), (9, 90, 900, 9000),
  (2, 20, 200, 2000), (4, 40, 400, 4000), (8, 80, 800, 800),
  (7, 70, 700, 7000);
insert into t2 values
  (30, 'bbb'), (10, 'b'), (70, 'bbbbbbb'), (60, 'bbbbbb'),
  (31, 'bbb'), (11, 'b'), (71, 'bbbbbbb'), (61, 'bbbbbb'),
  (32, 'bbb'), (12, 'b'), (72, 'bbbbbbb'), (62, 'bbbbbb');
insert into t3 values
  (4000, 'dddd'), (3000, 'ddd'), (1000, 'd'), (8000, 'dddddddd'),
  (4001, 'dddd'), (3001, 'ddd'), (1001, 'd'), (8001, 'dddddddd'),
  (4002, 'dddd'), (3002, 'ddd'), (1002, 'd'), (8002, 'dddddddd');
insert into t4 values
  (200, 'cc'), (600, 'cccccc'), (300, 'ccc'), (500, 'ccccc'),
  (201, 'cc'), (601, 'cccccc'), (301, 'ccc'), (501, 'ccccc'),
  (202, 'cc'), (602, 'cccccc'), (302, 'ccc'), (502, 'ccccc');

--disable_result_log
--disable_warnings
analyze table t2,t3,t4;
--enable_warnings
--enable_result_log

explain 
select t1.a, t1.b, t1.c, t1.d, t2.e, t3.f, t4.g from t1,t2,t3,t4
  where t2.b=t1.b and t3.d=t1.d and t4.c=t1.c;

select t1.a, t1.b, t1.c, t1.d, t2.e, t3.f, t4.g from t1,t2,t3,t4
  where t2.b=t1.b and t3.d=t1.d and t4.c=t1.c;

drop table t1,t2,t3,t4;

--echo #
--echo # Bug #44250: Corruption of linked join buffers when using BKA 
--echo #             

CREATE TABLE t1 (
  id1 bigint(20) DEFAULT NULL,
  id2 bigint(20) DEFAULT NULL,
  id3 bigint(20) DEFAULT NULL,
  num1 bigint(20) DEFAULT NULL,
  num2 int(11) DEFAULT NULL,
  num3 bigint(20) DEFAULT NULL
);

CREATE TABLE t2 (
  id3 bigint(20) NOT NULL DEFAULT '0',
  id4 bigint(20) DEFAULT NULL,
  enum1 enum('Enabled','Disabled','Paused') DEFAULT NULL,
  PRIMARY KEY (id3)
);

CREATE TABLE t3 (
  id4 bigint(20) NOT NULL DEFAULT '0',
  text1 text,
  PRIMARY KEY (id4)
);

CREATE TABLE t4 (
  id2 bigint(20) NOT NULL DEFAULT '0',
  dummy int(11) DEFAULT '0',
  PRIMARY KEY (id2)
);

CREATE TABLE t5 (
  id1 bigint(20) NOT NULL DEFAULT '0',
  id2 bigint(20) NOT NULL DEFAULT '0',
  enum2 enum('Active','Deleted','Paused') DEFAULT NULL,
  PRIMARY KEY (id1,id2)
);

--disable_query_log
--disable_result_log
--disable_warnings

INSERT INTO t1 VALUES 
(228172702,72485641,2667134182,10,1,14),(228172702,94266195,2667134182,134,0,134),
(228172702,94266195,2667134182,15,0,15),(228172702,94266195,2667134182,2,0,3),
(228172702,818095880,2667134182,1,1,1),(228172702,1004959639,2667134182,3,0,3),
(228172702,1297484422,2667134182,1,2,1),(228172702,1730911800,2667134182,11,0,28),
(228172702,1730911800,2667134182,4,0,4),(228172702,2182755982,2667134182,5,0,15),
(228172702,2182755982,2667134182,1,0,1),(228172702,2968841184,2667134182,1,0,1),
(228172702,4765525626,2667134182,2,0,3),(228172702,4765525626,2667134182,29,0,38),
(228172702,4765525626,2667134182,7,0,7),(228172702,4765525626,2667134182,7,0,8),
(228172702,5330573302,2667134182,1,0,1),(228512602,191149872,935692942,3,0,17),
(228512602,259118753,935692942,13,7,13),(228512602,259118753,935692942,83,33,83),
(228512602,585705465,935692942,1,0,1),(228512602,585716775,935692942,1,0,1),
(228512602,585716775,935692942,6,6,6),(228512602,585716775,935692942,1,1,1),
(228512602,1105371172,935692942,2,0,3),(228512602,1105371172,935692942,7,2,7),
(228512602,1314223462,935692942,1,0,1),(228512602,1314223642,935692942,1,1,1),
(228512602,1411060522,935692942,1,0,1),(228512602,1467398182,935692942,1,0,1),
(228512602,1467398182,935692942,3,0,4),(228512602,1467398242,935692942,10,0,41),
(228512602,1467398242,935692942,28,0,40),(228512602,1467398242,935692942,0,0,0),
(228512602,1467398242,935692942,29,2,33),(228512602,1734178942,935692942,1,0,1),
(228512602,1734179122,935692942,1,0,4),(228512602,1734179122,935692942,3,0,6),
(228512602,1953612870,935692942,1,0,1),(228512602,2271510562,935692942,1,1,1),
(228512602,2271525022,935692942,0,0,0),(228512602,3058831402,935692942,1,1,1),
(228512602,3723638842,935692942,1,1,1),(228512602,3723638842,935692942,4,3,4),
(228512602,3723836602,935692942,1,1,1),(228512602,3723836842,935692942,1,1,1),
(228512602,3723836962,935692942,1,1,1),(228512602,3723988102,935692942,11,4,11),
(228512602,3723989182,935692942,8,3,8),(228512602,5920283002,935692942,1,0,1),
(228512602,5920314232,935692942,1,0,1),(228512602,191149872,1241589892,0,0,0),
(228512602,191149872,1241589892,2,0,4),(228512602,191149872,1241589892,0,0,0),
(228512602,259118753,1241589892,8,4,8),(228512602,259118753,1241589892,70,33,70),
(228512602,259118753,1241589892,1,1,1),(228512602,585716775,1241589892,8,7,8),
(228512602,1105371172,1241589892,1,0,1),(228512602,1105371172,1241589892,9,0,9),
(228512602,1314223462,1241589892,1,0,1),(228512602,1411060522,1241589892,1,1,1),
(228512602,1467398182,1241589892,1,0,1),(228512602,1467398182,1241589892,4,1,4),
(228512602,1467398182,1241589892,1,0,1),(228512602,1467398242,1241589892,10,0,28),
(228512602,1467398242,1241589892,37,1,78),(228512602,1467398242,1241589892,28,9,30),
(228512602,1467398242,1241589892,5,0,6),(228512602,1734179122,1241589892,3,1,18),
(228512602,1734179122,1241589892,1,1,1),(228512602,1734179122,1241589892,2,0,3),
(228512602,1953611430,1241589892,1,1,1),(228512602,1953611430,1241589892,1,1,1),
(228512602,1953612870,1241589892,1,0,1),(228512602,2026844250,1241589892,1,0,1),
(228512602,2271510562,1241589892,1,1,1),(228512602,2271525022,1241589892,1,0,1),
(228512602,2941612417,1241589892,1,0,1),(228512602,3723988102,1241589892,1,0,1);
INSERT INTO t1 VALUES
(228512602,3723988102,1241589892,11,4,11),(228512602,3723989002,1241589892,1,0,1),
(228512602,3752960902,1241589892,2,2,4),(228808822,17304242,935693782,6,0,17),
(228808822,17304242,935693782,28,1,50),(228808822,17304242,935693782,29,3,61),
(228808822,17304242,935693782,6,0,13),(228808822,30931012,935693782,21,0,60),
(228808822,30931012,935693782,5,0,13),(228808822,37254452,935693782,3,0,3),
(228808822,42726891,935693782,1,0,4),(228808822,42726891,935693782,3,0,6),
(228808822,76261151,935693782,8,0,18),(228808822,88240139,935693782,1,0,1),
(228808822,88240139,935693782,3,0,3),(228808822,94730895,935693782,2,0,4),
(228808822,179737402,935693782,10,0,13),(228808822,179737402,935693782,7,0,8),
(228808822,179737402,935693782,3,0,4),(228808822,271288782,935693782,1,0,6),
(228808822,304690943,935693782,5,2,10),(228808822,304691183,935693782,4,0,16),
(228808822,568994960,935693782,1,0,1),(228808822,631705925,935693782,1,0,1),
(228808822,631745165,935693782,1,0,1),(228808822,631749605,935693782,1,0,4),
(228808822,1057787002,935693782,1,0,1),(228808822,1057787002,935693782,2,1,4),
(228808822,1057787002,935693782,12,1,20),(228808822,1057788022,935693782,2,0,40),
(228808822,1057788022,935693782,2,1,3),(228808822,1057788022,935693782,9,2,16),
(228808822,1335646822,935693782,3,1,6),(228808822,1335646882,935693782,1,0,3),
(228808822,1335646882,935693782,1,0,3),(228808822,1335646942,935693782,7,2,15),
(228808822,5510586183,935693782,1,1,1),(228808822,17304242,2482416112,11,0,28),
(228808822,17304242,2482416112,34,0,62),(228808822,17304242,2482416112,43,2,89),
(228808822,17304242,2482416112,9,0,19),(228808822,30931012,2482416112,32,2,84),
(228808822,30931012,2482416112,6,0,14),(228808822,30931012,2482416112,2,0,9),
(228808822,37254452,2482416112,1,1,1),(228808822,42726891,2482416112,2,0,10),
(228808822,76261151,2482416112,11,0,26),(228808822,88240139,2482416112,3,0,3),
(228808822,88240139,2482416112,1,0,1),(228808822,88240139,2482416112,3,0,4),
(228808822,94730895,2482416112,1,0,3),(228808822,125469602,2482416112,0,0,0),
(228808822,179737402,2482416112,4,0,10),(228808822,179737402,2482416112,8,1,9),
(228808822,179737402,2482416112,7,1,9),(228808822,179737402,2482416112,1,0,1),
(228808822,271288782,2482416112,2,0,14),(228808822,304690943,2482416112,3,0,6),
(228808822,304691183,2482416112,1,0,4),(228808822,555689643,2482416112,2,1,8),
(228808822,555689643,2482416112,1,0,4),(228808822,631705925,2482416112,1,0,1),
(228808822,631712555,2482416112,1,0,1),(228808822,631745165,2482416112,1,0,1),
(228808822,710348755,2482416112,1,0,1),(228808822,753718113,2482416112,1,0,1),
(228808822,1057787002,2482416112,1,0,4),(228808822,1057787002,2482416112,1,0,1),
(228808822,1057787002,2482416112,4,1,7),(228808822,1057788022,2482416112,7,0,12),
(228808822,1057788022,2482416112,3,0,37),(228808822,1057788022,2482416112,0,0,0),
(228808822,1057788022,2482416112,12,0,15),(228808822,1335646822,2482416112,14,1,28),
(228808822,1335646882,2482416112,1,1,3),(228808822,1335646942,2482416112,5,1,9),
(228808822,1335646942,2482416112,1,0,1),(230941762,16069490,2691187582,0,0,0),
(230941762,16705991,2691187582,16,0,30),(230941762,16705991,2691187582,12,3,12);
INSERT INTO t1 VALUES
(230941762,16705991,2691187582,1,0,1),(230941762,27714032,2691187582,6,0,16),
(230941762,27714032,2691187582,1,0,1),(230941762,27714032,2691187582,9,0,14),
(230941762,28676710,2691187582,3,1,4),(230941762,370319272,2691187582,7,0,7),
(230941762,1409814802,2691187582,1,0,3),(230941762,1409814982,2691187582,1,0,1),
(230941762,1409814982,2691187582,1,1,1),(230941762,2069703256,2691187582,1,0,3),
(230941762,16705991,2691187672,8,1,20),(230941762,16705991,2691187672,11,6,11),
(230941762,16705991,2691187672,1,0,1),(230941762,27714032,2691187672,5,0,20),
(230941762,27714032,2691187672,1,0,10),(230941762,27714032,2691187672,12,2,17),
(230941762,28676710,2691187672,1,0,1),(230941762,142889951,2691187672,2,0,10),
(230941762,172526592,2691187672,1,1,1),(230941762,293109282,2691187672,1,0,1),
(230941762,370319272,2691187672,10,0,10),(230941762,1409814802,2691187672,1,0,3),
(230941762,1409814922,2691187672,1,0,1),(230941762,1409814982,2691187672,1,0,1),
(230941762,16069490,2694472582,1,1,1),(230941762,16069490,2694472582,1,1,1),
(230941762,16705991,2694472582,15,0,45),(230941762,16705991,2694472582,13,2,15),
(230941762,27714032,2694472582,9,0,34),(230941762,27714032,2694472582,2,0,4),
(230941762,27714032,2694472582,10,2,14),(230941762,28676710,2694472582,4,0,12),
(230941762,28676710,2694472582,1,0,1),(230941762,172526592,2694472582,1,0,4),
(230941762,293109282,2694472582,1,0,1),(230941762,370319272,2694472582,6,0,6),
(230941762,1409814802,2694472582,1,0,3),(230941762,1409814862,2694472582,1,0,4),
(230941762,1409814982,2694472582,1,0,1),(230941762,2680867980,2694472582,1,0,3),
(230942122,25451690,935695702,1,0,9),(230942122,31549341,935695702,2,0,18),
(230942122,31549341,935695702,2,0,4),(230942122,38900150,935695702,4,0,29),
(230942122,38900150,935695702,4,1,13),(230942122,906919252,935695702,39,0,271),
(230942122,906919252,935695702,20,0,83),(230942122,906919252,935695702,2,1,9),
(230942122,1409816782,935695702,3,0,18),(230942122,1409816842,935695702,1,0,7),
(230942122,1409816842,935695702,1,0,3),(230942122,1409816902,935695702,1,0,6),
(230942122,2145075862,935695702,4,1,4),(230942122,25451690,935695822,2,0,16),
(230942122,38900150,935695822,3,0,26),(230942122,38900150,935695822,1,0,3),
(230942122,906919252,935695822,24,0,176),(230942122,906919252,935695822,20,0,74),
(230942122,906919252,935695822,1,0,3),(230942122,1409816782,935695822,2,0,21),
(230942122,1409816782,935695822,2,0,21),(230942122,1409816842,935695822,1,0,3),
(230942122,1409816902,935695822,1,0,7),(231112162,1413675742,935696902,1,0,1),
(231112162,1413675742,935696962,0,0,0),(231112162,1413675742,935696962,4,2,4),
(231112162,1413675922,935696962,1,0,1),(231112162,1413675922,935696962,1,0,1),
(231112162,1413675742,1248588922,1,0,1),(231112162,1413675922,1248588922,3,0,3),
(233937022,12641121,935697562,2,0,13),(233937022,12653871,935697562,1,0,1),
(233937022,12693551,935697562,1,0,1),(233937022,12910461,935697562,2,0,6),
(233937022,12910461,935697562,26,0,65),(233937022,12910461,935697562,44,8,45),
(233937022,12910481,935697562,12,0,19),(233937022,12910481,935697562,7,2,9),
(233937022,12910481,935697562,1,0,1),(233937022,12910511,935697562,8,0,8);
INSERT INTO t1 VALUES
(233937022,12910511,935697562,20,6,22),(233937022,30879781,935697562,34,0,34),
(233937022,30879781,935697562,3,0,4),(233937022,30879781,935697562,1,0,1),
(233937022,45631730,935697562,8,0,39),(233937022,54079090,935697562,12,0,12),
(233937022,54079090,935697562,7,0,11),(233937022,54079090,935697562,14,0,16),
(233937022,94431735,935697562,6,0,31),(233937022,96876131,935697562,3,0,4),
(233937022,105436492,935697562,4,0,4),(233937022,128981555,935697562,3,0,3),
(233937022,145211004,935697562,1,0,1),(233937022,146382622,935697562,1,0,1),
(233937022,175678702,935697562,1,0,4),(233937022,298998998,935697562,1,0,1),
(233937022,335995773,935697562,3,0,3),(233937022,335995773,935697562,2,0,3),
(233937022,347447636,935697562,0,0,0),(233937022,459295955,935697562,3,0,3),
(233937022,459376625,935697562,1,0,1),(233937022,495877773,935697562,1,0,1),
(233937022,497008702,935697562,1,0,3),(233937022,561944105,935697562,1,0,1),
(233937022,561944105,935697562,1,0,1),(233937022,586535965,935697562,3,0,3),
(233937022,631549775,935697562,1,0,7),(233937022,647138479,935697562,1,0,1),
(233937022,655870453,935697562,4,0,7),(233937022,694832725,935697562,1,0,1),
(233937022,864475057,935697562,1,0,1),(233937022,1010757503,935697562,1,0,4),
(233937022,1010847736,935697562,2,0,9),(233937022,1287437116,935697562,2,0,4),
(233937022,1337693056,935697562,1,0,1),(233937022,1569279742,935697562,1,1,1),
(233937022,1569280102,935697562,2,0,7),(233937022,1569280882,935697562,2,1,3),
(233937022,1569281062,935697562,1,0,1),(233937022,1569281962,935697562,1,0,3),
(233937022,2823580588,935697562,2,0,8),(233937022,2823580588,935697562,3,1,10),
(233937022,2842066134,935697562,1,0,1),(233937022,2904542181,935697562,1,0,1),
(233937022,3058483627,935697562,1,0,1),(233937022,4507287318,935697562,1,0,1),
(233937022,5283489892,935697562,1,0,1),(233937022,11890554322,935697562,16,0,16),
(233937022,11890756102,935697562,3,1,3),(233937022,12641121,953996482,1,0,7),
(233937022,12641851,953996482,1,0,1),(233937022,12641851,953996482,1,0,1),
(233937022,12910461,953996482,4,0,14),(233937022,12910461,953996482,20,2,23),
(233937022,12910461,953996482,43,5,43),(233937022,12910461,953996482,1,0,1),
(233937022,12910481,953996482,17,2,30),(233937022,12910511,953996482,7,1,8),
(233937022,12910511,953996482,23,5,23),(233937022,14913951,953996482,2,0,3),
(233937022,21835210,953996482,1,1,1),(233937022,26481052,953996482,1,1,1),
(233937022,26481052,953996482,1,0,1),(233937022,30879781,953996482,2,0,3),
(233937022,30879781,953996482,22,0,22),(233937022,35617681,953996482,1,0,1),
(233937022,45631730,953996482,3,0,11),(233937022,54079090,953996482,13,0,13),
(233937022,54079090,953996482,11,0,16),(233937022,54079090,953996482,29,0,34),
(233937022,94431735,953996482,3,0,9),(233937022,96876131,953996482,3,0,4),
(233937022,105436492,953996482,1,0,1),(233937022,105437952,953996482,3,1,3),
(233937022,123639716,953996482,1,0,6),(233937022,145211004,953996482,2,0,3),
(233937022,145211004,953996482,2,1,3),(233937022,146382622,953996482,1,0,1),
(233937022,146382622,953996482,1,0,1),(233937022,155454324,953996482,1,0,1);
INSERT INTO t1 VALUES
(233937022,298998998,953996482,1,1,1),(233937022,335995773,953996482,1,0,1),
(233937022,335995773,953996482,7,2,9),(233937022,459295955,953996482,2,0,4),
(233937022,561944105,953996482,1,0,1),(233937022,655870453,953996482,5,0,9),
(233937022,694832725,953996482,1,0,1),(233937022,694832725,953996482,1,0,1),
(233937022,864475057,953996482,4,1,4),(233937022,897886118,953996482,1,0,1),
(233937022,897886118,953996482,1,0,3),(233937022,1005147016,953996482,1,0,1),
(233937022,1010757503,953996482,1,0,1),(233937022,1082217873,953996482,1,0,1),
(233937022,1286925326,953996482,1,0,1),(233937022,1337693056,953996482,4,0,4),
(233937022,1407236408,953996482,2,0,3),(233937022,1569280102,953996482,1,0,6),
(233937022,1569280222,953996482,1,0,1),(233937022,1569281062,953996482,1,0,1),
(233937022,1569284362,953996482,1,0,3),(233937022,2823580588,953996482,1,0,3),
(233937022,2904542181,953996482,3,0,7),(233937022,4371581485,953996482,1,0,1),
(233937022,5283491332,953996482,1,0,1),(233937022,7300486013,953996482,1,1,1),
(233937022,11890554322,953996482,16,0,16),(233937022,11890754392,953996482,1,0,1),
(233937022,11890754392,953996482,0,0,0);

INSERT INTO t2 VALUES
(2667134182,2567095402,'Enabled'),(935692942,826927822,'Enabled'),
(1241589892,1130891152,'Enabled'),(935693782,826928662,'Enabled'),
(2482416112,2381969632,'Enabled'),(2691187582,2591198842,'Enabled'),
(2691187672,2591198932,'Enabled'),(2694472582,2594492212,'Paused'),
(935695702,826930582,'Enabled'),(935695822,826930702,'Enabled'),
(935696902,826931782,'Enabled'),(935696962,826931842,'Enabled'),
(1248588922,1137805582,'Enabled'),(935697562,826932442,'Paused'),
(953996482,845181202,'Enabled'),(2702549092,2602579882,'Enabled'),
(2702549182,2602579972,'Enabled'),(2702550712,2602581502,'Enabled'),
(1125312412,1015179502,'Enabled'),(2708245462,2608290202,'Enabled'),
(2708247262,2608292002,'Enabled'),(935699242,826934122,'Enabled'),
(1125312502,1015179592,'Enabled'),(1125312592,1015179682,'Enabled'),
(2711450452,2611502302,'Enabled'),(2711452252,2611504102,'Enabled'),
(935699902,826934782,'Enabled'),(935700262,826935142,'Enabled'),
(1215381442,1104677032,'Enabled'),(2503848082,2403457762,'Enabled'),
(935701762,826936642,'Enabled'),(935701822,826936702,'Enabled'),
(1468810282,1355227402,'Enabled'),(935702842,826937722,'Enabled'),
(1125312682,1015179772,'Enabled'),(2713816102,2613869392,'Enabled'),
(2688452032,2588455012,'Enabled'),(2688452212,2588455192,'Enabled'),
(2701527412,2601556942,'Enabled'),(1623918712,1510242412,'Enabled'),
(2701521922,2601551452,'Enabled'),(2701527772,2601557302,'Enabled');

INSERT INTO `t3` VALUES 
(2567095402,'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA'),
(826927822,'BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB'),(1130891152,'BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB'),
(826928662,'CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC'),
(2381969632,'CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC'),
(2591198842,'DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD'),
(2591198932,'EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE'),
(2594492212,'FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF'),
(826930582,'GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG'),
(826930702,'GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG'),
(826931782,'BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB'),
(826931842,'BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB'),
(1137805582,'BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB');

INSERT INTO t4 VALUES
(12618121,0),(12641121,0),(12641851,0),(12653871,0),(12665801,0),(12666811,0),
(12693551,0),(12910461,0),(12910481,0),(12910511,0),(14787251,0),(14913941,0),
(14913951,0),(16069490,0),(16705901,0),(16705991,0),(17291062,0),(17304242,0),
(20737411,0),(21524370,0),(21835210,0),(25300361,0),(25451690,0),(25728842,0),
(26481052,0),(27714032,0),(28676710,0),(30879781,0),(30931012,0),(31549341,0),
(35617681,0),(37254452,0),(38619430,0),(38895490,0),(38900150,0),(39798990,0),
(42726891,0),(42867050,0),(43439030,0),(45631730,0),(47171711,0),(49539832,0),
(54079090,0),(60442241,0),(65320501,0),(72485641,0),(76261151,0),(87949714,0),
(88240139,0),(94266195,0),(94431735,0),(94730895,0),(96876131,0);

INSERT INTO t5 VALUES
(228172702,72485641,'Active'),(228172702,94266195,'Active'),
(228172702,818095880,'Active'),(228172702,1004959639,'Active'),
(228172702,1297484242,'Active'),(228172702,1297484422,'Active'),
(228172702,1730911800,'Active'),(228172702,1808277389,'Active'),
(228172702,2182755982,'Active'),(228172702,2968841184,'Active'),
(228172702,3015116542,'Active'),(228172702,3752383170,'Active'),
(228172702,4765525626,'Active'),(228172702,5330573302,'Active'),
(228512602,191149872,'Active'),(228512602,259118753,'Active'),
(228512602,585705465,'Active'),(228512602,585716775,'Active'),
(228512602,1105371172,'Active'),(228512602,1314223462,'Active'),
(228512602,1314223642,'Active'),(228512602,1411060522,'Active'),
(228512602,1467398182,'Active'),(228512602,1467398242,'Active'),
(228512602,1734178942,'Active'),(228512602,1734179122,'Active'),
(228512602,1953612870,'Active'),(228512602,2271510562,'Active'),
(228512602,2271525022,'Active'),(228512602,2941612417,'Active'),
(228512602,3058831402,'Active'),(228512602,3723638842,'Active'),
(228512602,3723836602,'Active'),(228512602,3723836842,'Active'),
(228512602,3723836962,'Active'),(228512602,3723988102,'Active'),
(228512602,3723989182,'Active'),(228512602,5920283002,'Active'),
(228512602,5920314232,'Active'),(228512602,585717615,'Active'),
(228512602,1953611430,'Active'),(228512602,2026844250,'Active'),
(228512602,3058831462,'Active'),(228512602,3723836902,'Active'),
(228512602,3723989002,'Active'),(228512602,3752960902,'Active'),
(228808822,17304242,'Active'),(228808822,30931012,'Active'),
(228808822,37254452,'Active'),(228808822,42726891,'Active'),
(228808822,76261151,'Active'),(228808822,88240139,'Active'),
(228808822,94730895,'Active'),(228808822,125469622,'Active'),
(228808822,179737402,'Active'),(228808822,271288782,'Active'),
(228808822,304690943,'Active'),(228808822,304691183,'Active'),
(228808822,496123368,'Active'),(228808822,555689643,'Active'),
(228808822,568994960,'Active'),(228808822,631705925,'Active'),
(228808822,631745165,'Active'),(228808822,631749605,'Active'),
(228808822,1057787002,'Active'),(228808822,1057788022,'Active'),
(228808822,1335646822,'Active'),(228808822,1335646882,'Active'),
(228808822,1335646942,'Active'),(228808822,1612792238,'Active'),
(228808822,5510586183,'Active'),(228808822,47171711,'Active'),
(228808822,125469602,'Active'),(228808822,631712555,'Active'),
(228808822,710348755,'Active'),(228808822,753718113,'Active'),
(230941762,16069490,'Active'),(230941762,16705991,'Active'),
(230941762,27714032,'Active'),(230941762,28676710,'Active');
INSERT INTO t5 VALUES
(230941762,370319272,'Active'),(230941762,1409814802,'Active'),
(230941762,1409814982,'Active'),(230941762,2069703256,'Active'),
(230941762,142889951,'Active'),(230941762,172526592,'Active'),
(230941762,293109282,'Active'),(230941762,1409814922,'Active'),
(230941762,1409814862,'Active'),(230941762,2680867980,'Active'),
(230942122,25451690,'Active'),(230942122,31549341,'Active'),
(230942122,38900150,'Active'),(230942122,464554745,'Active'),
(230942122,906919252,'Active'),(230942122,1409816782,'Active'),
(230942122,1409816842,'Active'),(230942122,1409816902,'Active'),
(230942122,2145075862,'Active'),(231112162,1413675742,'Active'),
(231112162,1413675922,'Active'),(231112162,1413675562,'Active'),
(231112162,1413675802,'Active'),(233937022,12641121,'Active'),
(233937022,12653871,'Active'),(233937022,12693551,'Active'),
(233937022,12910461,'Active'),(233937022,12910481,'Active'),
(233937022,12910511,'Active'),(233937022,14913941,'Active'),
(233937022,30879781,'Active'),(233937022,45631730,'Active'),
(233937022,54079090,'Active'),(233937022,65320501,'Active'),
(233937022,94431735,'Active'),(233937022,96876131,'Active'),
(233937022,105436492,'Active'),(233937022,105437952,'Active'),
(233937022,128981555,'Active'),(233937022,145211004,'Active'),
(233937022,146382622,'Active'),(233937022,148832422,'Active'),
(233937022,175678702,'Active'),(233937022,260507673,'Active'),
(233937022,298998998,'Active'),(233937022,335995773,'Active'),
(233937022,347447636,'Active'),(233937022,459295955,'Active'),
(233937022,459376625,'Active'),(233937022,495877773,'Active'),
(233937022,497008702,'Active'),(233937022,561944105,'Active'),
(233937022,586535965,'Active'),(233937022,631549775,'Active'),
(233937022,647138479,'Active'),(233937022,655870453,'Active'),
(233937022,694832725,'Active'),(233937022,835712045,'Active'),
(233937022,864475057,'Active'),(233937022,864484777,'Active'),
(233937022,1010757503,'Active'),(233937022,1010847736,'Active'),
(233937022,1091554836,'Active'),(233937022,1287437116,'Active'),
(233937022,1337693056,'Active'),(233937022,1569279742,'Active'),
(233937022,1569280102,'Active'),(233937022,1569280222,'Active'),
(233937022,1569280582,'Active'),(233937022,1569280882,'Active'),
(233937022,1569281062,'Active'),(233937022,1569281962,'Active'),
(233937022,1569284362,'Active'),(233937022,1743317015,'Active'),
(233937022,2698799002,'Active'),(233937022,2698800742,'Active'),
(233937022,2823580588,'Active'),(233937022,2842066134,'Active'),
(233937022,2904542181,'Active'),(233937022,3058483627,'Active');
INSERT INTO t5 VALUES
(233937022,4507287318,'Active'),(233937022,5283489892,'Active'),
(233937022,11890554322,'Active'),(233937022,11890756102,'Active'),
(233937022,12641851,'Active'),(233937022,14913951,'Active'),
(233937022,21835210,'Active'),(233937022,26481052,'Active'),
(233937022,35617681,'Active'),(233937022,123639716,'Active'),
(233937022,155454324,'Active'),(233937022,299001668,'Active'),
(233937022,897886118,'Active'),(233937022,1005147016,'Active'),
(233937022,1082217873,'Active'),(233937022,1286925326,'Active'),
(233937022,1407236408,'Active'),(233937022,4371581485,'Active'),
(233937022,5283491332,'Active'),(233937022,7300486013,'Active'),
(233937022,11890754392,'Active');

--enable_warnings
--enable_result_log
--enable_query_log

set join_buffer_size=2048;

EXPLAIN
SELECT STRAIGHT_JOIN t1.id1, t1.num3, t3.text1, t3.id4, t2.id3, t4.dummy
  FROM t1 JOIN  t2 JOIN  t3 JOIN  t4 JOIN  t5 
    WHERE t1.id1=t5.id1 AND t1.id2=t5.id2 and  t4.id2=t1.id2 AND
          t5.enum2='Active' AND t3.id4=t2.id4 AND t2.id3=t1.id3 AND t3.text1<'D';

--sorted_result
SELECT STRAIGHT_JOIN t1.id1, t1.num3, t3.text1, t3.id4, t2.id3, t4.dummy
  FROM t1 JOIN  t2 JOIN  t3 JOIN  t4 JOIN  t5 
    WHERE t1.id1=t5.id1 AND t1.id2=t5.id2 and  t4.id2=t1.id2 AND
          t5.enum2='Active' AND t3.id4=t2.id4 AND t2.id3=t1.id3 AND t3.text1<'D';

set join_buffer_size=default;

DROP TABLE t1,t2,t3,t4,t5;

--echo #
--echo # Bug #46328: Use of aggregate function without GROUP BY clause 
--echo #             returns many rows (vs. one )
--echo #             

CREATE TABLE t1 (
  int_key int(11) NOT NULL,
  KEY int_key (int_key)
);

INSERT INTO t1 VALUES
(0),(2),(2),(2),(3),(4),(5),(5),(6),(6),(8),(8),(9),(9);

CREATE TABLE t2 (
  int_key int(11) NOT NULL,
  KEY int_key (int_key)
);

INSERT INTO t2 VALUES (2),(3);

--echo

--echo # The query shall return 1 record with a max value 9 and one of the 
--echo # int_key values inserted above (undefined which one). A changed 
--echo # execution plan may change the value in the second column
SELECT  MAX(t1.int_key), t1.int_key
FROM t1 STRAIGHT_JOIN t2  
ORDER BY t1.int_key;

--echo

explain 
SELECT  MAX(t1.int_key), t1.int_key
FROM t1 STRAIGHT_JOIN t2  
ORDER BY t1.int_key;

--echo

DROP TABLE t1,t2;

--echo #
--echo # Bug #45019: join buffer contains two blob columns one of which is
--echo #             used in the key employed to access the joined table
--echo #

CREATE TABLE t1 (c1 int, c2 int, key (c2));
INSERT INTO t1 VALUES (1,1);
INSERT INTO t1 VALUES (2,2);

CREATE TABLE t2 (c1 text, c2 text);
INSERT INTO t2 VALUES('tt', 'uu');
INSERT INTO t2 VALUES('zzzz', 'xxxxxxxxx');

--disable_result_log
ANALYZE TABLE t1,t2;
--enable_result_log

SELECT t1.*, t2.*, LENGTH(t2.c1), LENGTH(t2.c2) FROM t1,t2
  WHERE t1.c2=LENGTH(t2.c2) and t1.c1=LENGTH(t2.c1);

DROP TABLE t1,t2;

--echo #
--echo # Regression test for
--echo # Bug#46733 - NULL value not returned for aggregate on empty result 
--echo #             set w/ semijoin on
CREATE TABLE t1 (
  i int(11) NOT NULL,
  v varchar(1) DEFAULT NULL,
  PRIMARY KEY (i)
);

INSERT INTO t1 VALUES (10,'a'),(11,'b'),(12,'c'),(13,'d');

CREATE TABLE t2 (
  i int(11) NOT NULL,
  v varchar(1) DEFAULT NULL,
  PRIMARY KEY (i)
);

INSERT INTO t2 VALUES (1,'x'),(2,'y');

--echo

SELECT MAX(t1.i) 
FROM t1 JOIN t2 ON t2.v
ORDER BY t2.v;

--echo

EXPLAIN
SELECT MAX(t1.i) 
FROM t1 JOIN t2 ON t2.v
ORDER BY t2.v;

--echo

DROP TABLE t1,t2;

--echo #
--echo # Bug#51092: Linked join buffer gives wrong result 
--echo #            for 3-way cross join
--echo #

CREATE TABLE t1 (a INT, b INT);
INSERT INTO t1 VALUES (1,1),(2,2);

CREATE TABLE t2 (a INT, b INT);
INSERT INTO t2 VALUES (1,1),(2,2);

CREATE TABLE t3 (a INT, b INT);
INSERT INTO t3 VALUES (1,1),(2,2);

EXPLAIN SELECT t1.* FROM t1,t2,t3;
SELECT t1.* FROM t1,t2,t3;

DROP TABLE t1,t2,t3;

--echo #
--echo # BUG#52394 Segfault in JOIN_CACHE::get_offset () at sql_select.h:445
--echo #

CREATE TABLE C(a int);
INSERT INTO C VALUES(1),(2),(3),(4),(5);

CREATE TABLE D (a int(11), b varchar(1));
INSERT INTO D VALUES (6,'r'),(27,'o');

CREATE TABLE E (a int(11) primary key, b varchar(1));
INSERT INTO E VALUES
(14,'d'),(15,'z'),(16,'e'),(17,'h'),(18,'b'),(19,'s'),(20,'e'),(21,'j'),(22,'e'),(23,'f'),(24,'v'),(25,'x'),(26,'m'),(27,'c');

SELECT 1 FROM C,D,E WHERE D.a = E.a AND D.b = E.b;
DROP TABLE C,D,E;

--echo #
--echo # BUG#52540 Crash in JOIN_CACHE::set_match_flag_if_none () at sql_join_cache.cc:1883
--echo #

CREATE TABLE t1 (a int);
INSERT INTO t1 VALUES (2);
CREATE TABLE t2 (a varchar(10));
INSERT INTO t2 VALUES ('f'),('x');
CREATE TABLE t3 (pk int(11) PRIMARY KEY);
INSERT INTO t3 VALUES (2);
CREATE TABLE t4 (a varchar(10));

EXPLAIN SELECT 1
FROM t2 LEFT JOIN
       ((t1 JOIN t3 ON t1.a = t3.pk)
        LEFT JOIN t4 ON 1 )
     ON 1 ;

SELECT 1
FROM t2 LEFT JOIN
       ((t1 JOIN t3 ON t1.a = t3.pk)
        LEFT JOIN t4 ON 1 )
     ON 1 ;

DROP TABLE t1,t2,t3,t4;

--echo #
--echo # Bug#51084: Batched key access crashes for SELECT with
--echo #            derived table and LEFT JOIN 
--echo #

CREATE TABLE t1 (
  carrier int,
  id int PRIMARY KEY
);
INSERT INTO t1 VALUES (1,11),(1,12),(2,13);

CREATE TABLE t2 (
  scan_date int,
  package_id int
);
INSERT INTO t2 VALUES (2008,21),(2008,22);

CREATE TABLE t3 (
  carrier int PRIMARY KEY,
  id int
);
INSERT INTO t3 VALUES (1,31);

CREATE TABLE t4 (
  carrier_id int,
  INDEX carrier_id(carrier_id)
);
INSERT INTO t4 VALUES (31),(32);

--echo
SELECT COUNT(*)
  FROM (t2 JOIN t1) LEFT JOIN (t3 JOIN t4 ON t3.id = t4.carrier_id)
       ON t3.carrier = t1.carrier;

--echo
EXPLAIN
SELECT COUNT(*)
  FROM (t2 JOIN t1) LEFT JOIN (t3 JOIN t4 ON t3.id = t4.carrier_id)
       ON t3.carrier = t1.carrier;
--echo
DROP TABLE t1,t2,t3,t4;

--echo #
--echo # Bug#45267: Incomplete check caused wrong result.
--echo #
CREATE TABLE t1 (
  `pk` int(11) NOT NULL AUTO_INCREMENT PRIMARY KEY
);
CREATE TABLE t3 (
  `pk` int(11) NOT NULL AUTO_INCREMENT PRIMARY KEY
);
INSERT INTO t3 VALUES
(1),(2),(3),(4),(5),(6),(7),(8),(9),(10),(11),(12),(13),(14),(15),
(16),(17),(18),(19),(20);
CREATE TABLE t2 (
  `pk` int(11) NOT NULL AUTO_INCREMENT,
  `int_nokey` int(11) NOT NULL,
  `time_key` time NOT NULL,
  PRIMARY KEY (`pk`),
  KEY `time_key` (`time_key`)
);
INSERT INTO t2 VALUES (10,9,'22:36:46'),(11,0,'08:46:46');

SELECT DISTINCT t1.`pk`
FROM t1 RIGHT JOIN t2 STRAIGHT_JOIN t3 ON t2.`int_nokey`  ON t2.`time_key`
GROUP BY 1;

DROP TABLE IF EXISTS t1, t2, t3;

--echo #
--echo # BUG#52636 6.0 allowing JOINs on NULL values w/ optimizer_join_cache_level = 5-8
--echo #

CREATE TABLE t1 (b int);
INSERT INTO t1 VALUES (NULL),(3);

CREATE TABLE t2 (a int, b int, KEY (b));
INSERT INTO t2 VALUES (100,NULL),(150,200);

let $query= SELECT t2.a FROM t1 LEFT JOIN t2 FORCE INDEX (b) ON t2.b  = t1.b;
--eval EXPLAIN $query
--sorted_result
--eval $query

# test crash when no key is worth collecting by BKA for t2's ref
delete from t1;
INSERT INTO t1 VALUES (NULL),(NULL);

--eval EXPLAIN $query
--sorted_result
--eval $query

DROP TABLE t1,t2;

# test varchar keys
CREATE TABLE t1 (b varchar(100));
INSERT INTO t1 VALUES (NULL),("some varchar");

CREATE TABLE t2 (a int, b varchar(100), KEY (b));
INSERT INTO t2 VALUES (100,NULL),(150,"varchar"),(200,NULL),(250,"long long varchar");

explain SELECT t2.a FROM t1 LEFT JOIN t2 ON t2.b  = t1.b;
--sorted_result
SELECT t2.a FROM t1 LEFT JOIN t2 ON t2.b  = t1.b;

DROP TABLE t1,t2;

--echo #
--echo # BUG#54359 "Extra rows with join_cache_level=7,8 and two joins
--echo # --and multi-column index"
--echo #

CREATE TABLE t1 (
  `pk` int(11) NOT NULL,
  `col_int_key` int(11) DEFAULT NULL,
  `col_varchar_key` varchar(1) DEFAULT NULL,
  `col_varchar_nokey` varchar(1) DEFAULT NULL,
  KEY `col_varchar_key` (`col_varchar_key`,`col_int_key`))
;

INSERT INTO t1 VALUES (4,9,'k','k');
INSERT INTO t1 VALUES (12,5,'k','k');

let $query_i= SELECT table2 .`col_int_key` FROM t1 table2,
t1 table3 force index (`col_varchar_key`)
where table3 .`pk` and table3 .`col_int_key`  >= table2 .`pk`  
 and table3 .`col_varchar_key`  = table2 .`col_varchar_nokey`;

eval explain $query_i;
eval $query_i;

drop table t1;

--echo #
--echo # BUG#54481 "GROUP BY loses effect with JOIN + ORDER BY + LIMIT
--echo # and join_cache_level=5-8"
--echo #

CREATE TABLE t1 (
  `col_int_key` int,
  `col_datetime` datetime,
  KEY `col_int_key` (`col_int_key`)
);

INSERT INTO t1 VALUES (2,'2003-02-11 21:19:41');
INSERT INTO t1 VALUES (3,'2009-10-18 02:27:49');
INSERT INTO t1 VALUES (0,'2000-09-26 07:45:57');

CREATE TABLE t2 (
  `col_int` int,
  `col_int_key` int,
  KEY `col_int_key` (`col_int_key`)
);

INSERT INTO t2 VALUES (14,1);
INSERT INTO t2 VALUES (98,1);

# The WHERE clause is true for all rows of t2
# but is needed to trigger the desired plan.
# Query uses BKA.
let $query=SELECT t1.col_int_key, t1.col_datetime 
FROM t1,t2
WHERE t2.col_int_key = 1 AND t2.col_int >= 3
GROUP BY t1.col_int_key
ORDER BY t1.col_int_key, t1.col_datetime
LIMIT 2;

eval explain $query;
eval $query;

# by disabling one index and forcing another, we hit 
# block-nested-loop join and see the same bug
let $query=SELECT t1.col_int_key, t1.col_datetime 
FROM t1 force index (col_int_key), t2 ignore index (col_int_key)
WHERE t2.col_int_key = 1 AND t2.col_int >= 3
GROUP BY t1.col_int_key
ORDER BY t1.col_int_key, t1.col_datetime
LIMIT 2;

eval explain $query;
eval $query;

drop table t1,t2;

--echo
--echo # Bug#11766522 "59651: ASSERTION `TABLE_REF->HAS_RECORD' FAILED
--echo # WITH JOIN_CACHE_LEVEL=3"
--echo

CREATE TABLE t1 (
  b varchar(20)
)  ;
INSERT INTO t1 VALUES ('1'),('1');

CREATE TABLE t4 (
  col253 text
)  ;
INSERT INTO t4 VALUES (''),('pf');

CREATE TABLE t6 (
  col282 timestamp
)  ;
INSERT INTO t6 VALUES ('2010-11-07 01:04:45'),('2010-12-13 01:36:32');

CREATE TABLE t7 (
  col319 timestamp NOT NULL,
  UNIQUE KEY idx263 (col319)
)  ;
# zero rows would do, if there was no const-table optimization
insert into t7 values("2000-01-01"),("2000-01-02");

CREATE TABLE t3 (
  col582 char(230) CHARACTER SET utf8 DEFAULT NULL
)  ;
# one single row would do, if there was no const-table optimization
INSERT INTO t3 VALUES ('cymej'),('spb');

CREATE TABLE t5 (
  col712 time
)  ;
# zero rows would do, if there was no const-table optimization
insert into t5 values(0),(0);

CREATE TABLE t8 (
  col804 char(169),
  col805 varchar(51)
)  ;
INSERT INTO t8 VALUES ('tmqcb','pwk');

CREATE TABLE t2 (
  col841 varchar(10)
)  ;
# one single row would do, if there was no const-table optimization
INSERT INTO t2 VALUES (''),('');

# Small buffer, to trigger "full buffer" in both caches of t8 and t6.
# Setting to 1 will actually set to the smallest allowed value,
# with a "rounding" warning message.
set join_buffer_size=1;
select @@join_buffer_size;

--disable_warnings
select count(*) from
(t1 join t2 join t3)
left join t4 on 1
left join t5 on 1 like t4.col253
left join t6 on t5.col712 is null
left join t7 on t1.b <=>t7.col319
left join t8 on t3.col582 <=  1;
--enable_warnings

drop table t1,t2,t3,t4,t5,t6,t7,t8;

--echo #
--echo # Bug#12616131 - JCL: NULL VS DATE + TWICE AS MANY ROWS 
--echo #                RETURNED WHEN JCL>=7
--echo #

CREATE TABLE t1 (t1a int, t1b int);
INSERT INTO t1 VALUES (99, NULL),(99, 3),(99,0);

CREATE TABLE t2 (t2a int, t2b int, KEY idx (t2b));
INSERT INTO t2 VALUES (100,0),(150,200),(999, 0),(999, NULL);

let $query1= SELECT * FROM t1 LEFT JOIN t2 force index (idx) ON t2.t2b = t1.t1b;
let $query2= SELECT * FROM t1 LEFT JOIN t2 force index (idx) ON t2.t2b <=> t1.t1b;

--echo
--echo # t2b is NULL-able
--echo
--eval EXPLAIN $query1
--eval $query1
--echo
--eval EXPLAIN $query2
--eval $query2
--echo

DROP TABLE t2;

CREATE TABLE t2 (t2a int, t2b int NOT NULL, KEY idx (t2b));
INSERT INTO t2 VALUES (100,0),(150,200),(999, 0);

--echo
--echo # t2b is NOT NULL
--echo
--eval EXPLAIN $query1
--eval $query1
--echo
--eval EXPLAIN $query2
--eval $query2
--echo

DROP TABLE t1,t2;

--echo #
--echo # BUG#12619399 - JCL: NO ROWS VS 3X NULL QUERY OUTPUT WHEN JCL>=5
--echo #

CREATE TABLE t1 (
  c1 INTEGER NOT NULL,
  c2_key INTEGER NOT NULL,
  KEY col_int_key (c2_key)
) ENGINE=InnoDB;

INSERT INTO t1 VALUES (24,204);

CREATE TABLE t2 (  
  pk INTEGER NOT NULL,
  PRIMARY KEY (pk)
) ENGINE=InnoDB;

INSERT INTO t2 VALUES (10);

CREATE TABLE t3 (
  c1 INTEGER,
  KEY k1 (c1)
) ENGINE=InnoDB;

INSERT INTO t3 VALUES (NULL), (NULL);

# Bug was specific of IN->EXISTS:
set @old_opt_switch=@@optimizer_switch;
--disable_query_log
if (`select locate('materialization', @@optimizer_switch) > 0`) 
{
  set optimizer_switch='materialization=off';
}
--enable_query_log

--echo

let query_in=
SELECT t3.c1 FROM t3
WHERE t3.c1 IN (SELECT t1.c2_key FROM t2 JOIN t1 ON t2.pk < t1.c1)
XOR TRUE;

#BKA is OK for this query
let query_in_toplevel=
SELECT t3.c1 FROM t3
WHERE t3.c1 IN (SELECT t1.c2_key FROM t2 JOIN t1 ON t2.pk < t1.c1);

let query_notin=
SELECT t3.c1 FROM t3
WHERE t3.c1 NOT IN (SELECT t1.c2_key FROM t2 JOIN t1 ON t2.pk < t1.c1);

let query_any=
SELECT t3.c1 FROM t3
WHERE t3.c1 = ANY (SELECT t1.c2_key FROM t2 JOIN t1 ON t2.pk < t1.c1)
XOR TRUE;

let query_some=
SELECT t3.c1 FROM t3
WHERE t3.c1 = SOME (SELECT t1.c2_key FROM t2 JOIN t1 ON t2.pk < t1.c1)
XOR TRUE;

eval explain $query_some;
eval explain $query_any;
eval explain $query_in;
eval explain $query_notin;
eval explain $query_in_toplevel;
eval $query_some;
eval $query_any;
eval $query_in;
eval $query_notin;
eval $query_in_toplevel;


--echo
set @@optimizer_switch=@old_opt_switch;
DROP TABLE t1, t2, t3;

set @@join_buffer_size=default;

--echo
--echo # BUG#12586926 "EXTRA ROW WITH JOIN + GROUP BY + ORDER BY WITH
--echo # JCL>=5 AND MRR ENABLED"
--echo

CREATE TABLE t1 (  col_int_key int(11) NOT NULL,
   col_varchar_key varchar(1) NOT NULL,    
   KEY col_int_key (col_int_key),
   KEY col_varchar_key (col_varchar_key,col_int_key)
) ENGINE=innodb;

INSERT INTO t1 VALUES (0,'j'),(4,'b'),(4,'d');

CREATE TABLE t2 (
   col_datetime_key datetime NOT NULL,
   col_varchar_key varchar(1) NOT NULL,
   KEY col_varchar_key (col_varchar_key)
) ENGINE=innodb;

INSERT INTO t2 VALUES ('2003-08-21 00:00:00','b');

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
ANALYZE TABLE t2;
-- enable_query_log
-- enable_result_log

# need to force the index, or it picks BNL for t2 (lower cost),
# whereas we want to test BKA
let $query=
SELECT MIN(t2.col_datetime_key) AS field1,
       t1.col_int_key AS field2  
FROM t1
  LEFT JOIN t2 force index (col_varchar_key)
  ON t1.col_varchar_key = t2.col_varchar_key  
GROUP BY field2
ORDER BY field1;

eval explain $query;
eval $query;

DROP TABLE t1,t2;

--echo
--echo # BUG#12619510 "JCL: MORE ROWS AND DIFFERENT OUTPUT WITH JCL>=5"
--echo

CREATE TABLE t1 (
col_int_key int(11) NOT NULL,
col_datetime_key datetime NOT NULL,
col_varchar_nokey varchar(1) NOT NULL,
KEY col_int_key (col_int_key),
KEY col_datetime_key (col_datetime_key)
);
INSERT INTO t1 VALUES (7,'2004-06-06 04:22:12','v');
INSERT INTO t1 VALUES (0,'2005-11-13 01:12:31','s');
INSERT INTO t1 VALUES (9,'2002-05-04 01:50:00','l');
INSERT INTO t1 VALUES (3,'2004-10-27 10:28:45','y');
INSERT INTO t1 VALUES (4,'2006-07-22 05:24:23','c');
INSERT INTO t1 VALUES (2,'2002-05-16 21:34:03','i');
INSERT INTO t1 VALUES (5,'2008-04-17 10:45:30','h');
INSERT INTO t1 VALUES (3,'2009-04-21 02:58:02','q');
INSERT INTO t1 VALUES (1,'2008-01-11 11:01:51','a');
INSERT INTO t1 VALUES (3,'1900-01-01 00:00:00','v');
INSERT INTO t1 VALUES (6,'2007-05-17 18:24:57','u');
INSERT INTO t1 VALUES (7,'2007-08-07 00:00:00','s');
INSERT INTO t1 VALUES (5,'2001-08-28 00:00:00','y');
INSERT INTO t1 VALUES (1,'2004-04-16 00:27:28','z');
INSERT INTO t1 VALUES (204,'2005-05-03 07:06:22','h');
INSERT INTO t1 VALUES (224,'2009-03-11 17:09:50','p');
INSERT INTO t1 VALUES (9,'2007-12-08 01:54:28','e');
INSERT INTO t1 VALUES (5,'2009-07-28 18:19:54','i');
INSERT INTO t1 VALUES (0,'2008-06-08 00:00:00','y');
INSERT INTO t1 VALUES (3,'2005-02-09 09:20:26','w');

CREATE TABLE t2 (
pk int(11) NOT NULL,
col_varchar_key varchar(1) NOT NULL,
PRIMARY KEY (pk)
);
INSERT INTO t2 VALUES
(1,'j'),(2,'v'),(3,'c'),(4,'m'),(5,'d'),(6,'d'),(7,'y'),
(8,'t'),(9,'d'),(10,'s'),(11,'r'),(12,'m'),(13,'b'),(14,'x'),
(15,'g'),(16,'p'),(17,'q'),(18,'w'),(19,'d'),(20,'e');

let $query=SELECT t2.col_varchar_key AS field1 , COUNT(DISTINCT t1.col_varchar_nokey), t2.pk AS field4
FROM t1
RIGHT JOIN t2 ON t2.pk = t1.col_int_key 
GROUP BY field1 , field4 
ORDER BY t1.col_datetime_key ;

eval explain $query;
# even though there is ORDER BY, it does not cover all columns, so
# there is still randomness, so we have to sort client-side:
--sorted_result
eval $query;

DROP TABLE t1,t2;

--echo
--echo # BUG#12619868 "JCL: MORE ROWS OF OUTPUT WHEN JCL>=5"
--echo

CREATE TABLE t1 (col_varchar_key varchar(1));
CREATE TABLE t2 (
 pk int(11) NOT NULL,
 col_int_nokey int(11) NOT NULL,
 col_int_key int(11) NOT NULL,
 PRIMARY KEY (pk),
 KEY col_int_key (col_int_key)
);
INSERT INTO t2 VALUES (5,3,9);
INSERT INTO t2 VALUES (6,246,24);
INSERT INTO t2 VALUES (7,2,6);
INSERT INTO t2 VALUES (8,9,1);
INSERT INTO t2 VALUES (9,3,6);
INSERT INTO t2 VALUES (10,8,2);
INSERT INTO t2 VALUES (11,1,4);
INSERT INTO t2 VALUES (12,8,8);
INSERT INTO t2 VALUES (13,8,4);
INSERT INTO t2 VALUES (14,5,4);
INSERT INTO t2 VALUES (15,7,7);
INSERT INTO t2 VALUES (16,5,4);
INSERT INTO t2 VALUES (17,1,1);
INSERT INTO t2 VALUES (18,6,9);
INSERT INTO t2 VALUES (19,2,4);
INSERT INTO t2 VALUES (20,9,8);

let $query=SELECT t1.col_varchar_key AS field1, alias2.col_int_key AS field4 
FROM t2 AS alias2 STRAIGHT_JOIN t2 AS alias3 ON alias3.pk =
alias2.col_int_nokey 
left join t1 
ON alias3.col_int_nokey 
GROUP BY field1, field4
LIMIT 15;

eval explain $query;
--sorted_result
eval $query;

DROP TABLE t1,t2;

--echo
--echo # BUG#12722133 - JCL: JOIN QUERY GIVES DIFFERENT RESULTS AT
--echo # JCL=6 ONLY [NULL VERSUS NULL+#INTS]
--echo

CREATE TABLE t1 (pk INTEGER PRIMARY KEY, k INTEGER, i INTEGER, KEY k(k));
CREATE TABLE t2 LIKE t1;
CREATE TABLE t3 LIKE t1;
CREATE TABLE t4 LIKE t1;

INSERT INTO t1 VALUES (6,NULL,6),(0,1,11);
INSERT INTO t2 VALUES (1,NULL,NULL),(4,7,NULL);
INSERT INTO t3 VALUES (2,3,0),(3,4,4);
INSERT INTO t4 VALUES (1,9,-1),(4,7,NULL);

let $query=SELECT t2.pk as t2_pk, t4.pk as t4_pk, t4.k as t4_k, t4.i
  as t4_i FROM t1
  LEFT JOIN t2 ON t1.k = t2.pk
  LEFT JOIN t3 ON t3.i
  LEFT JOIN t4 ON t4.pk = t2.pk;

eval EXPLAIN $query;
eval $query;

DROP TABLE t1, t2, t3, t4; 

--echo
--echo # BUG#12827509 - BNL/BKA: SELECT LEFT/RIGHT JOIN QUERY GIVES 
--echo #                DIFFERENT OUTPUT ON BNL=OFF+BKA=ON
--echo # (Duplicate of BUG#12722133)
--echo
CREATE TABLE t1 (
  col_int INTEGER
);
INSERT INTO t1 VALUES (3), (7), (2), (8), (6);

CREATE TABLE t2 (
  pk INTEGER, 
  col_int INTEGER,
  PRIMARY KEY (pk)
);
INSERT INTO t2 VALUES (1,5), (2,8), (6,3), (8,7), (9,9);

CREATE TABLE t3 (
  pk INTEGER, 
  col_int INTEGER, 
  PRIMARY KEY (pk)
);
INSERT INTO t3 VALUES (3,2), (4,3), (8,2);

CREATE TABLE t4 (
  pk INTEGER,
  col_int INTEGER,
  PRIMARY KEY (pk)
);
INSERT INTO t4 VALUES (2,3), (6,1), (8,2);

let query=
SELECT t4.col_int
  FROM t1 
    LEFT JOIN t2 ON t1.col_int = t2.col_int
    LEFT JOIN t3 ON t2.pk = t3.pk 
    LEFT JOIN t4 ON t4.pk = t2.pk
  WHERE t1.col_int OR t3.col_int;

eval EXPLAIN $query;
eval $query;

DROP TABLE t1, t2, t3, t4;

--echo #
--echo # Bug#12997905: VALGRIND: SYSCALL PARAM PWRITE64(BUF) 
--echo #               POINTS TO UNINITIALISED BYTE(S)
--echo #

CREATE TABLE t1 (
  col1 varchar(10),
  col2 varchar(1024)
) ENGINE=innodb;

INSERT INTO t1 VALUES ('a','a');

CREATE TABLE t2 (i varchar(10)) ENGINE=innodb;
INSERT INTO t2 VALUES ('a');

SELECT t1.col1
FROM t1 JOIN t2 ON t1.col1 = t2.i 
GROUP BY t1.col2;

DROP TABLE t1,t2;

--echo # End of Bug#12997905

--echo #
--echo # Bug 13596330 - EXTRA ROW ON SELECT WITH NESTED IN CLAUSE + IS
--echo # NULL WHEN SEMIJOIN + BNL IS ON
--echo #

CREATE TABLE t1 (  
  col_int_nokey int
);
INSERT INTO t1 VALUES(-1),(-1);

CREATE TABLE t2 (
  col_int_nokey int,
  col_datetime_nokey datetime NOT NULL,
  col_varchar_key varchar(1),
  KEY col_varchar_key (col_varchar_key)
);

INSERT INTO t2 VALUES (9, '2002-08-25 20:35:06', 'e'),
                      (9, '2002-08-25 20:35:06', 'e');

set @optimizer_switch_saved=@@session.optimizer_switch;
set @@session.optimizer_switch='semijoin=off';

let $query=SELECT PARENT1.col_varchar_key
FROM t2 AS PARENT1 LEFT JOIN t1 USING (col_int_nokey)
WHERE PARENT1.col_varchar_key IN
      ( SELECT col_varchar_key FROM t2 AS CHILD1
        WHERE PARENT1.col_datetime_nokey IS NULL
              AND t1.col_int_nokey IS NULL )
;
eval EXPLAIN $query;
eval $query;

set @@session.optimizer_switch=@optimizer_switch_saved;

DROP TABLE t1,t2;
