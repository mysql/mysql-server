#
# Problem with range optimizer
#

--disable_warnings
drop table if exists t1, t2, t3;
--enable_warnings
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
CREATE TABLE t1 (
  event_date date DEFAULT '0000-00-00' NOT NULL,
  type int(11) DEFAULT '0' NOT NULL,
  event_id int(11) DEFAULT '0' NOT NULL,
  PRIMARY KEY (event_date,type,event_id)
);

INSERT INTO t1 VALUES ('1999-07-10',100100,24), ('1999-07-11',100100,25),
('1999-07-13',100600,0), ('1999-07-13',100600,4), ('1999-07-13',100600,26),
('1999-07-14',100600,10), ('1999-07-15',100600,16), ('1999-07-15',100800,45),
('1999-07-15',101000,47), ('1999-07-16',100800,46), ('1999-07-20',100600,5),
('1999-07-20',100600,27), ('1999-07-21',100600,11), ('1999-07-22',100600,17),
('1999-07-23',100100,39), ('1999-07-24',100100,39), ('1999-07-24',100500,40),
('1999-07-25',100100,39), ('1999-07-27',100600,1), ('1999-07-27',100600,6),
('1999-07-27',100600,28), ('1999-07-28',100600,12), ('1999-07-29',100500,41),
('1999-07-29',100600,18), ('1999-07-30',100500,41), ('1999-07-31',100500,41),
('1999-08-01',100700,34), ('1999-08-03',100600,7), ('1999-08-03',100600,29),
('1999-08-04',100600,13), ('1999-08-05',100500,42), ('1999-08-05',100600,19),
('1999-08-06',100500,42), ('1999-08-07',100500,42), ('1999-08-08',100500,42),
('1999-08-10',100600,2), ('1999-08-10',100600,9), ('1999-08-10',100600,30),
('1999-08-11',100600,14), ('1999-08-12',100600,20), ('1999-08-17',100500,8),
('1999-08-17',100600,31), ('1999-08-18',100600,15), ('1999-08-19',100600,22),
('1999-08-24',100600,3), ('1999-08-24',100600,32), ('1999-08-27',100500,43),
('1999-08-31',100600,33), ('1999-09-17',100100,37), ('1999-09-18',100100,37),
('1999-09-19',100100,37), ('2000-12-18',100700,38);

explain select event_date,type,event_id from t1 WHERE event_date >= "1999-07-01" AND event_date < "1999-07-15" AND (type=100600 OR type=100100) ORDER BY event_date;
explain format=tree select event_date,type,event_id from t1 WHERE event_date >= "1999-07-01" AND event_date < "1999-07-15" AND (type=100600 OR type=100100) ORDER BY event_date;
select event_date,type,event_id from t1 WHERE event_date >= "1999-07-01" AND event_date < "1999-07-15" AND (type=100600 OR type=100100) ORDER BY event_date;
explain select event_date,type,event_id from t1 WHERE type = 100601 and event_date >= "1999-07-01" AND event_date < "1999-07-15" AND (type=100600 OR type=100100) ORDER BY event_date;
select event_date,type,event_id from t1 WHERE event_date >= "1999-07-01" AND event_date <= "1999-07-15" AND (type=100600 OR type=100100) or event_date >= "1999-07-01" AND event_date <= "1999-07-15" AND type=100099;
drop table t1;

CREATE TABLE t1 (
  PAPER_ID smallint(6) DEFAULT '0' NOT NULL,
  YEAR smallint(6) DEFAULT '0' NOT NULL,
  ISSUE smallint(6) DEFAULT '0' NOT NULL,
  CLOSED tinyint(4) DEFAULT '0' NOT NULL,
  ISS_DATE date DEFAULT '0000-00-00' NOT NULL,
  PRIMARY KEY (PAPER_ID,YEAR,ISSUE)
);
INSERT INTO t1 VALUES (3,1999,34,0,'1999-07-12'), (1,1999,111,0,'1999-03-23'),
                      (1,1999,222,0,'1999-03-23'), (3,1999,33,0,'1999-07-12'),
                      (3,1999,32,0,'1999-07-12'), (3,1999,31,0,'1999-07-12'),
                      (3,1999,30,0,'1999-07-12'), (3,1999,29,0,'1999-07-12'),
                      (3,1999,28,0,'1999-07-12'), (1,1999,40,1,'1999-05-01'),
                      (1,1999,41,1,'1999-05-01'), (1,1999,42,1,'1999-05-01'),
                      (1,1999,46,1,'1999-05-01'), (1,1999,47,1,'1999-05-01'),
                      (1,1999,48,1,'1999-05-01'), (1,1999,49,1,'1999-05-01'),
                      (1,1999,50,0,'1999-05-01'), (1,1999,51,0,'1999-05-01'),
                      (1,1999,200,0,'1999-06-28'), (1,1999,52,0,'1999-06-28'),
                      (1,1999,53,0,'1999-06-28'), (1,1999,54,0,'1999-06-28'),
                      (1,1999,55,0,'1999-06-28'), (1,1999,56,0,'1999-07-01'),
                      (1,1999,57,0,'1999-07-01'), (1,1999,58,0,'1999-07-01'),
                      (1,1999,59,0,'1999-07-01'), (1,1999,60,0,'1999-07-01'),
                      (3,1999,35,0,'1999-07-12');
select YEAR,ISSUE from t1 where PAPER_ID=3 and (YEAR>1999 or (YEAR=1999 and ISSUE>28))  order by YEAR,ISSUE;
check table t1;
repair table t1;
drop table t1;

CREATE TABLE t1 (
  id int(11) NOT NULL auto_increment,
  parent_id int(11) DEFAULT '0' NOT NULL,
  level tinyint(4) DEFAULT '0' NOT NULL,
  PRIMARY KEY (id),
  KEY parent_id (parent_id),
  KEY level (level)
);
INSERT INTO t1 VALUES (1,0,0), (3,1,1), (4,1,1), (8,2,2), (9,2,2), (17,3,2),
(22,4,2), (24,4,2), (28,5,2), (29,5,2), (30,5,2), (31,6,2), (32,6,2), (33,6,2),
(203,7,2), (202,7,2), (20,3,2), (157,0,0), (193,5,2), (40,7,2), (2,1,1),
(15,2,2), (6,1,1), (34,6,2), (35,6,2), (16,3,2), (7,1,1), (36,7,2), (18,3,2),
(26,5,2), (27,5,2), (183,4,2), (38,7,2), (25,5,2), (37,7,2), (21,4,2),
(19,3,2), (5,1,1), (179,5,2);
SELECT * FROM t1 WHERE level = 1 AND parent_id = 1;
# The following select returned 0 rows in 3.23.8
SELECT * FROM t1 WHERE level = 1 AND parent_id = 1 order by id;
drop table t1;

#
# Testing of bug in range optimizer with many key parts and > and <
#

create table t1(
		Satellite		varchar(25)	not null,
		SensorMode		varchar(25)	not null,
		FullImageCornersUpperLeftLongitude	double	not null,
		FullImageCornersUpperRightLongitude	double	not null,
		FullImageCornersUpperRightLatitude	double	not null,
		FullImageCornersLowerRightLatitude	double	not null,
	        index two (Satellite, SensorMode, FullImageCornersUpperLeftLongitude, FullImageCornersUpperRightLongitude, FullImageCornersUpperRightLatitude, FullImageCornersLowerRightLatitude));

insert into t1 values("OV-3","PAN1",91,-92,40,50);
insert into t1 values("OV-4","PAN1",91,-92,40,50);

select * from t1 where t1.Satellite = "OV-3" and t1.SensorMode = "PAN1" and t1.FullImageCornersUpperLeftLongitude > -90.000000 and t1.FullImageCornersUpperRightLongitude < -82.000000;
drop table t1;

create table t1 ( aString char(100) not null default "", key aString (aString(10)) );
insert t1 (aString) values ( "believe in myself" ), ( "believe" ), ("baaa" ), ( "believe in love");
select * from t1 where aString < "believe in myself" order by aString;
select * from t1 where aString > "believe in love" order by aString;
alter table t1 drop key aString;
select * from t1 where aString < "believe in myself" order by aString;
select * from t1 where aString > "believe in love" order by aString;
drop table t1;

#
# Problem with binary strings
#

CREATE TABLE t1 (
  t1ID int(10) unsigned NOT NULL auto_increment,
  art binary(1) NOT NULL default '',
  KNR char(5) NOT NULL default '',
  RECHNR char(6) NOT NULL default '',
  POSNR char(2) NOT NULL default '',
  ARTNR char(10) NOT NULL default '',
  TEX char(70) NOT NULL default '',
  PRIMARY KEY  (t1ID),
  KEY IdxArt (art),
  KEY IdxKnr (KNR),
  KEY IdxArtnr (ARTNR)
) ENGINE=MyISAM;

INSERT INTO t1 (art) VALUES ('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),
('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),
('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),
('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),
('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),
('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),
('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),
('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),
('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),
('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),
('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),
('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),
('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),
('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),
('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),
('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),
('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),
('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),
('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),
('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),
('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),
('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),
('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),
('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),
('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),
('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),
('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),('j'),('J'),
('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),
('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),
('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),
('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),
('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),
('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),
('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),
('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),
('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),
('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),
('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j'),('j');
select count(*) from t1 where upper(art) = 'J';
select count(*) from t1 where art = 'J' or art = 'j';
select count(*) from t1 where art = 'j' or art = 'J';
select count(*) from t1 where art = 'j';
select count(*) from t1 where art = 'J';
drop table t1;
#
# BETWEEN problems
#
create table t1 (x int, y int, index(x), index(y));
insert into t1 (x) values (1),(2),(3),(4),(5),(6),(7),(8),(9);
update t1 set y=x;
analyze table t1;
# between with only one end fixed
explain select * from t1, t1 t2 where t1.y = 8 and t2.x between 7 and t1.y+0;
explain select * from t1, t1 t2 where t1.y = 8 and t2.x >= 7 and t2.x <= t1.y+0;
# between with both expressions on both ends
explain select * from t1, t1 t2 where t1.y = 2 and t2.x between t1.y-1 and t1.y+1;
explain select * from t1, t1 t2 where t1.y = 2 and t2.x >= t1.y-1 and t2.x <= t1.y+1;
# equation propagation
explain select * from t1, t1 t2 where t1.y = 2 and t2.x between 0 and t1.y;
explain select * from t1, t1 t2 where t1.y = 2 and t2.x >= 0 and t2.x <= t1.y;
# testing IN
explain select count(*) from t1 where x in (1);
explain select count(*) from t1 where x in (1,2);
drop table t1;

#
# bug #1172: "Force index" option caused server crash
#
CREATE TABLE t1 (key1 int(11) NOT NULL default '0', KEY i1 (key1));
INSERT INTO t1 VALUES (0),(0),(0),(0),(0),(1),(1);
CREATE TABLE t2 (keya int(11) NOT NULL default '0', KEY j1 (keya));
INSERT INTO t2 VALUES (0),(0),(1),(1),(2),(2);
explain select * from t1, t2 where (t1.key1 <t2.keya + 1) and t2.keya=3;
explain select * from t1 force index(i1), t2 force index(j1) where 
 (t1.key1 <t2.keya + 1) and t2.keya=3;
DROP TABLE t1,t2;

#
# bug #1724: use RANGE on more selective column instead of REF on less
# selective

CREATE TABLE t1 (
  a int(11) default NULL,
  b int(11) default NULL,
  KEY a (a),
  KEY b (b)
) ENGINE=MyISAM;


INSERT INTO t1 VALUES
(1,1),(2,1),(3,1),(4,1),(5,1),(6,1),(7,1),(8,1),(9,1),(10,2),(10,2),
(13,2),(14,2),(15,2),(16,2),(17,3),(17,3),(16,3),(17,3),(19,3),(20,3),
(21,4),(22,5),(23,5),(24,5),(25,5),(26,5),(30,5),(31,5),(32,5),(33,5),
(33,5),(33,5),(33,5),(33,5),(34,5),(35,5);

# we expect that optimizer will choose index on A
EXPLAIN SELECT * FROM t1 WHERE a IN(1,2) AND b=5;
SELECT * FROM t1 WHERE a IN(1,2) AND b=5;
DROP TABLE t1;

#
# Test problem with range optimzer and sub ranges
#

CREATE TABLE t1 (a int, b int, c int, INDEX (c,a,b));
INSERT INTO t1 VALUES (1,0,0),(1,0,0),(1,0,0);
INSERT INTO t1 VALUES (0,1,0),(0,1,0),(0,1,0);
# -- First reports 3; second reports 6
SELECT COUNT(*) FROM t1 WHERE (c=0 and a=1) or (c=0 and b=1);
SELECT COUNT(*) FROM t1 WHERE (c=0 and b=1) or (c=0 and a=1);
DROP TABLE t1;

#
# Test problem with range optimization over overlapping ranges (#2448)
#

CREATE TABLE t1 ( a int not null, b int not null, INDEX ab(a,b) );
INSERT INTO t1 VALUES (47,1), (70,1), (15,1), (15, 4);
SELECT * FROM t1
WHERE
(
    ( b =1 AND a BETWEEN 14 AND 21 ) OR
    ( b =2 AND a BETWEEN 16 AND 18 ) OR
    ( b =3 AND a BETWEEN 15 AND 19 ) OR
    (a BETWEEN 19 AND 47)
);
DROP TABLE t1;

#
# Test of problem with IN on many different keyparts. (Bug #4157)
#

CREATE TABLE t1 (
id int( 11 ) unsigned NOT NULL AUTO_INCREMENT ,
line int( 5 ) unsigned NOT NULL default '0',
columnid int( 3 ) unsigned NOT NULL default '0',
owner int( 3 ) unsigned NOT NULL default '0',
ordinal int( 3 ) unsigned NOT NULL default '0',
showid smallint( 6 ) unsigned NOT NULL default '1',
tableid int( 1 ) unsigned NOT NULL default '1',
content int( 5 ) unsigned NOT NULL default '188',
PRIMARY KEY ( owner, id ) ,
KEY menu( owner, showid, columnid ) ,
KEY `COLUMN` ( owner, columnid, line ) ,
KEY `LINES` ( owner, tableid, content, id ) ,
KEY recount( owner, line ) 
) ENGINE = MYISAM;

INSERT into t1 (owner,id,columnid,line) values (11,15,15,1),(11,13,13,5);

SELECT id, columnid, tableid, content, showid, line, ordinal FROM t1 WHERE owner=11 AND ((columnid IN ( 15, 13, 14 ) AND line IN ( 1, 2, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 31 )) OR (columnid IN ( 13, 14 ) AND line IN ( 15 ))) LIMIT 0 , 30;
drop table t1;

#
# test for a bug with in() and unique key
#

create  table t1 (id int(10) primary key);
insert into t1 values (1),(2),(3),(4),(5),(6),(7),(8),(9);

select id from t1 where id in (2,5,9) ;
select id from t1 where id=2 or id=5 or id=9 ;
drop table t1;
create table t1 ( id1 int not null, id2 int not null, idnull int null, c char(20), primary key (id1,id2));
insert into t1 values (0,1,NULL,"aaa"), (1,1,NULL,"aaa"), (2,1,NULL,"aaa"),
                      (3,1,NULL,"aaa"), (4,1,NULL,"aaa"), (5,1,NULL,"aaa"),
                      (6,1,NULL,"aaa"), (7,1,NULL,"aaa"), (8,1,NULL,"aaa"),
                      (9,1,NULL,"aaa"), (10,1,NULL,"aaa"), (11,1,NULL,"aaa"),
                      (12,1,NULL,"aaa"), (13,1,NULL,"aaa"), (14,1,NULL,"aaa"),
                      (15,1,NULL,"aaa"), (16,1,NULL,"aaa"), (17,1,NULL,"aaa"),
                      (18,1,NULL,"aaa"), (19,1,NULL,"aaa"), (20,1,NULL,"aaa");
select a.id1, b.idnull from t1 as a, t1 as b where a.id2=1 and a.id1=1 and b.id1=a.idnull order by b.id2 desc limit 1;
drop table t1;


#
# Problem with optimizing !=
#

create table t1 (
  id int not null auto_increment,
  name char(1) not null,
  uid int not null,
  primary key (id),
  index uid_index (uid));
  
create table t2 (
  id int not null auto_increment,
  name char(1) not null,
  uid int not null,
  primary key (id),
  index uid_index (uid));
  
insert into t1(id, uid, name) values(1, 0, ' ');
insert into t1(uid, name) values(0, ' ');

insert into t2(uid, name) select uid, name from t1;
insert into t1(uid, name) select uid, name from t2;
insert into t2(uid, name) select uid, name from t1;
insert into t1(uid, name) select uid, name from t2;
insert into t2(uid, name) select uid, name from t1;
insert into t1(uid, name) select uid, name from t2;
insert into t2(uid, name) select uid, name from t1;
insert into t1(uid, name) select uid, name from t2;
insert into t2(uid, name) select uid, name from t1;
insert into t1(uid, name) select uid, name from t2;
insert into t2(uid, name) select uid, name from t1;
insert into t2(uid, name) select uid, name from t1;
insert into t2(uid, name) select uid, name from t1;
insert into t2(uid, name) select uid, name from t1;
insert into t1(uid, name) select uid, name from t2;

delete from t2;
insert into t2(uid, name) values 
  (1, CHAR(64+1)),
  (2, CHAR(64+2)),
  (3, CHAR(64+3)),
  (4, CHAR(64+4)),
  (5, CHAR(64+5)),
  (6, CHAR(64+6)),
  (7, CHAR(64+7)),
  (8, CHAR(64+8)),
  (9, CHAR(64+9)),
  (10, CHAR(64+10)),
  (11, CHAR(64+11)),
  (12, CHAR(64+12)),
  (13, CHAR(64+13)),
  (14, CHAR(64+14)),
  (15, CHAR(64+15)),
  (16, CHAR(64+16)),
  (17, CHAR(64+17)),
  (18, CHAR(64+18)),
  (19, CHAR(64+19)),
  (20, CHAR(64+20)),
  (21, CHAR(64+21)),
  (22, CHAR(64+22)),
  (23, CHAR(64+23)),
  (24, CHAR(64+24)),
  (25, CHAR(64+25)),
  (26, CHAR(64+26));

insert into t1(uid, name) select uid, name from t2 order by uid;

delete from t2;
insert into t2(id, uid, name) select id, uid, name from t1;

select count(*) from t1;  
select count(*) from t2;

analyze table t1,t2;

explain select * from t1, t2  where t1.uid=t2.uid AND t1.uid > 0;
explain select * from t1, t2  where t1.uid=t2.uid AND t2.uid > 0;
explain select * from t1, t2  where t1.uid=t2.uid AND t1.uid != 0;
explain select * from t1, t2  where t1.uid=t2.uid AND t2.uid != 0;

select * from t1, t2  where t1.uid=t2.uid AND t1.uid > 0;
select * from t1, t2  where t1.uid=t2.uid AND t1.uid != 0;

drop table t1,t2;

# Fix for bug#4488 
#
create table t1 (x bigint unsigned not null);
insert into t1(x) values (0xfffffffffffffff0);
insert into t1(x) values (0xfffffffffffffff1);
select * from t1;
select count(*) from t1 where x>0;
select count(*) from t1 where x=0;
select count(*) from t1 where x<0;
select count(*) from t1 where x < -16;
select count(*) from t1 where x = -16;
select count(*) from t1 where x > -16;
select count(*) from t1 where x = 18446744073709551601;


create table t2 (x bigint not null);
insert into t2(x) values (-16);
insert into t2(x) values (-15);
select * from t2;
select count(*) from t2 where x>0;
select count(*) from t2 where x=0;
select count(*) from t2 where x<0;
select count(*) from t2 where x < -16;
select count(*) from t2 where x = -16;
select count(*) from t2 where x > -16;
select count(*) from t2 where x = 18446744073709551601;
drop table t1,t2;

--disable_warnings
create table t1 (x bigint unsigned not null primary key) engine=innodb;
--enable_warnings
insert into t1(x) values (0xfffffffffffffff0);
insert into t1(x) values (0xfffffffffffffff1);
select * from t1;
select count(*) from t1 where x>0;
select count(*) from t1 where x=0;
select count(*) from t1 where x<0;
select count(*) from t1 where x < -16;
select count(*) from t1 where x = -16;
select count(*) from t1 where x > -16;
select count(*) from t1 where x = 18446744073709551601;

drop table t1;

#
# Bug #11185 incorrect comparison of unsigned int to signed constant
#
create table t1 (a bigint unsigned);
create index t1i on t1(a);
insert into t1 select 18446744073709551615;
insert into t1 select 18446744073709551614;

explain select * from t1 where a <> -1;
select * from t1 where a <> -1;
explain select * from t1 where a > -1 or a < -1;
select * from t1 where a > -1 or a < -1;
explain select * from t1 where a > -1;
select * from t1 where a > -1;
explain select * from t1 where a < -1;
select * from t1 where a < -1;

drop table t1;

#
# Bug #6045: Binary Comparison regression in MySQL 4.1
# Binary searches didn't use a case insensitive index.
#
set names latin1;
create table t1 (a char(10), b text, key (a)) character set latin1;
INSERT INTO t1 (a) VALUES
('111'),('222'),('222'),('222'),('222'),('444'),('aaa'),('AAA'),('bbb');
# all these three can be optimized
explain select * from t1 where a='aaa';
explain select * from t1 where a=binary 'aaa';
explain select * from t1 where a='aaa' collate latin1_bin;
# this one cannot:
explain select * from t1 where a='aaa' collate latin1_german1_ci;
drop table t1;

# Test for BUG#9348 "result for WHERE A AND (B OR C) differs from WHERE a AND (C OR B)"
--disable_warnings
CREATE TABLE t1 (
  `CLIENT` char(3) character set latin1 collate latin1_bin NOT NULL default '000',
  `ARG1` char(3) character set latin1 collate latin1_bin NOT NULL default '',
  `ARG2` char(3) character set latin1 collate latin1_bin NOT NULL default '',
  `FUNCTION` varchar(10) character set latin1 collate latin1_bin NOT NULL default '',
  `FUNCTINT` int(11) NOT NULL default '0',
  KEY `VERI_CLNT~2` (`ARG1`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
--enable_warnings

INSERT INTO t1 VALUES ('000',' 0',' 0','Text 001',0), ('000',' 0',' 1','Text 002',0),
  ('000',' 1',' 2','Text 003',0), ('000',' 2',' 3','Text 004',0),
  ('001',' 3',' 0','Text 017',0);

SELECT count(*) FROM t1 WHERE CLIENT='000' AND (ARG1 != ' 1' OR ARG1 != ' 2');

SELECT count(*) FROM t1 WHERE CLIENT='000' AND (ARG1 != ' 2' OR ARG1 != ' 1');
drop table t1;

# BUG#16168: Wrong range optimizer results, "Use_count: Wrong count ..."
#            warnings in server stderr.
create table t1 (a int);
insert into t1 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);

CREATE TABLE t2 (
  pk1 int(11) NOT NULL,
  pk2 int(11) NOT NULL,
  pk3 int(11) NOT NULL,
  pk4 int(11) NOT NULL,
  filler char(82),
  PRIMARY KEY (pk1,pk2,pk3,pk4)
) DEFAULT CHARSET=latin1;

insert into t2 select 1, A.a+10*B.a, 432, 44, 'fillerZ' from t1 A, t1 B;
INSERT INTO t2 VALUES (2621, 2635, 0, 0,'filler'), (2621, 2635, 1, 0,'filler'),
  (2621, 2635, 10, 0,'filler'), (2621, 2635, 11, 0,'filler'),
  (2621, 2635, 14, 0,'filler'), (2621, 2635, 1000015, 0,'filler');

SELECT * FROM t2
WHERE ((((pk4 =0) AND (pk1 =2621) AND (pk2 =2635)))
OR ((pk4 =1) AND (((pk1 IN ( 7, 2, 1 ))) OR (pk1 =522)) AND ((pk2 IN ( 0, 2635))))
) AND (pk3 >=1000000);
drop table t1, t2;

#
# Bug #20732: Partial index and long sjis search with '>' fails sometimes
#

create table t1(a char(2), key(a(1))) charset utf8mb4;
insert into t1 values ('x'), ('xx');
explain select a from t1 where a > 'x';
select a from t1 where a > 'x';
drop table t1;

#
# Bug #24776: assertion abort for 'range checked for each record' 
#

CREATE TABLE t1 (
  OXID varchar(32) COLLATE latin1_german2_ci NOT NULL DEFAULT '',
  OXPARENTID varchar(32) COLLATE latin1_german2_ci NOT NULL DEFAULT 'oxrootid',
  OXLEFT int NOT NULL DEFAULT '0',
  OXRIGHT int NOT NULL DEFAULT '0',
  OXROOTID varchar(32) COLLATE latin1_german2_ci NOT NULL DEFAULT '',
  PRIMARY KEY  (OXID),
  KEY OXNID (OXID),
  KEY OXLEFT (OXLEFT),
  KEY OXRIGHT (OXRIGHT),
  KEY OXROOTID (OXROOTID)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 COLLATE=latin1_german2_ci;

INSERT INTO t1 VALUES
('d8c4177d09f8b11f5.52725521','oxrootid',1,40,'d8c4177d09f8b11f5.52725521'),
('d8c4177d151affab2.81582770','d8c4177d09f8b11f5.52725521',2,3,
 'd8c4177d09f8b11f5.52725521'),
('d8c4177d206a333d2.74422679','d8c4177d09f8b11f5.52725521',4,5,
 'd8c4177d09f8b11f5.52725521'),
('d8c4177d225791924.30714720','d8c4177d09f8b11f5.52725521',6,7,
 'd8c4177d09f8b11f5.52725521'),
('d8c4177d2380fc201.39666693','d8c4177d09f8b11f5.52725521',8,9,
 'd8c4177d09f8b11f5.52725521'),
('d8c4177d24ccef970.14957924','d8c4177d09f8b11f5.52725521',10,11,
 'd8c4177d09f8b11f5.52725521');

EXPLAIN
SELECT s.oxid FROM t1 v, t1 s 
  WHERE s.oxrootid = 'd8c4177d09f8b11f5.52725521' AND
        v.oxrootid ='d8c4177d09f8b11f5.52725521' AND
        s.oxleft > v.oxleft AND s.oxleft < v.oxright;

SELECT s.oxid FROM t1 v, t1 s 
  WHERE s.oxrootid = 'd8c4177d09f8b11f5.52725521' AND
        v.oxrootid ='d8c4177d09f8b11f5.52725521' AND
        s.oxleft > v.oxleft AND s.oxleft < v.oxright;

DROP TABLE t1;

# BUG#26624 high mem usage (crash) in range optimizer (depends on order of fields in where)
create table t1 (
  c1  char(10), c2  char(10), c3  char(10), c4  char(10),
  c5  char(10), c6  char(10), c7  char(10), c8  char(10),
  c9  char(10), c10 char(10), c11 char(10), c12 char(10),
  c13 char(10), c14 char(10), c15 char(10), c16 char(10),
  index(c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12,c13,c14,c15,c16)
);
insert into t1 (c1) values ('1'),('1'),('1'),('1');

# This must run without crash and fast:
select * from t1 where
     c1 in ("abcdefgh", "123456789", "qwertyuio", "asddfgh", 
            "abcdefg1", "123456781", "qwertyui1", "asddfg1", 
            "abcdefg2", "123456782", "qwertyui2", "asddfg2", 
            "abcdefg3", "123456783", "qwertyui3", "asddfg3", 
            "abcdefg4", "123456784", "qwertyui4", "asddfg4",
            "abcdefg5", "123456785", "qwertyui5", "asddfg5",
            "abcdefg6", "123456786", "qwertyui6", "asddfg6",
            "abcdefg7", "123456787", "qwertyui7", "asddfg7",
            "abcdefg8", "123456788", "qwertyui8", "asddfg8",
            "abcdefg9", "123456789", "qwertyui9", "asddfg9",
            "abcdefgA", "12345678A", "qwertyuiA", "asddfgA",
            "abcdefgB", "12345678B", "qwertyuiB", "asddfgB",
            "abcdefgC", "12345678C", "qwertyuiC", "asddfgC")
 and c2 in ("abcdefgh", "123456789", "qwertyuio", "asddfgh", 
            "abcdefg1", "123456781", "qwertyui1", "asddfg1", 
            "abcdefg2", "123456782", "qwertyui2", "asddfg2", 
            "abcdefg3", "123456783", "qwertyui3", "asddfg3", 
            "abcdefg4", "123456784", "qwertyui4", "asddfg4", 
            "abcdefg5", "123456785", "qwertyui5", "asddfg5",
            "abcdefg6", "123456786", "qwertyui6", "asddfg6",
            "abcdefg7", "123456787", "qwertyui7", "asddfg7",
            "abcdefg8", "123456788", "qwertyui8", "asddfg8",
            "abcdefg9", "123456789", "qwertyui9", "asddfg9",
            "abcdefgA", "12345678A", "qwertyuiA", "asddfgA",
            "abcdefgB", "12345678B", "qwertyuiB", "asddfgB",
            "abcdefgC", "12345678C", "qwertyuiC", "asddfgC")
 and c3 in ("abcdefgh", "123456789", "qwertyuio", "asddfgh", 
            "abcdefg1", "123456781", "qwertyui1", "asddfg1", 
            "abcdefg2", "123456782", "qwertyui2", "asddfg2", 
            "abcdefg3", "123456783", "qwertyui3", "asddfg3", 
            "abcdefg4", "123456784", "qwertyui4", "asddfg4", 
            "abcdefg5", "123456785", "qwertyui5", "asddfg5",
            "abcdefg6", "123456786", "qwertyui6", "asddfg6",
            "abcdefg7", "123456787", "qwertyui7", "asddfg7",
            "abcdefg8", "123456788", "qwertyui8", "asddfg8",
            "abcdefg9", "123456789", "qwertyui9", "asddfg9",
            "abcdefgA", "12345678A", "qwertyuiA", "asddfgA",
            "abcdefgB", "12345678B", "qwertyuiB", "asddfgB",
            "abcdefgC", "12345678C", "qwertyuiC", "asddfgC")
 and c4 in ("abcdefgh", "123456789", "qwertyuio", "asddfgh", 
            "abcdefg1", "123456781", "qwertyui1", "asddfg1", 
            "abcdefg2", "123456782", "qwertyui2", "asddfg2", 
            "abcdefg3", "123456783", "qwertyui3", "asddfg3", 
            "abcdefg4", "123456784", "qwertyui4", "asddfg4", 
            "abcdefg5", "123456785", "qwertyui5", "asddfg5", 
            "abcdefg6", "123456786", "qwertyui6", "asddfg6",
            "abcdefg7", "123456787", "qwertyui7", "asddfg7",
            "abcdefg8", "123456788", "qwertyui8", "asddfg8",
            "abcdefg9", "123456789", "qwertyui9", "asddfg9",
            "abcdefgA", "12345678A", "qwertyuiA", "asddfgA",
            "abcdefgB", "12345678B", "qwertyuiB", "asddfgB",
            "abcdefgC", "12345678C", "qwertyuiC", "asddfgC")
 and c5 in ("abcdefgh", "123456789", "qwertyuio", "asddfgh", 
            "abcdefg1", "123456781", "qwertyui1", "asddfg1", 
            "abcdefg2", "123456782", "qwertyui2", "asddfg2", 
            "abcdefg3", "123456783", "qwertyui3", "asddfg3", 
            "abcdefg4", "123456784", "qwertyui4", "asddfg4",
            "abcdefg5", "123456785", "qwertyui5", "asddfg5",
            "abcdefg6", "123456786", "qwertyui6", "asddfg6",
            "abcdefg7", "123456787", "qwertyui7", "asddfg7",
            "abcdefg8", "123456788", "qwertyui8", "asddfg8",
            "abcdefg9", "123456789", "qwertyui9", "asddfg9",
            "abcdefgA", "12345678A", "qwertyuiA", "asddfgA",
            "abcdefgB", "12345678B", "qwertyuiB", "asddfgB",
            "abcdefgC", "12345678C", "qwertyuiC", "asddfgC")
 and c6 in ("abcdefgh", "123456789", "qwertyuio", "asddfgh", 
            "abcdefg1", "123456781", "qwertyui1", "asddfg1", 
            "abcdefg2", "123456782", "qwertyui2", "asddfg2", 
            "abcdefg3", "123456783", "qwertyui3", "asddfg3", 
            "abcdefg4", "123456784", "qwertyui4", "asddfg4",
            "abcdefg5", "123456785", "qwertyui5", "asddfg5",
            "abcdefg6", "123456786", "qwertyui6", "asddfg6",
            "abcdefg7", "123456787", "qwertyui7", "asddfg7",
            "abcdefg8", "123456788", "qwertyui8", "asddfg8",
            "abcdefg9", "123456789", "qwertyui9", "asddfg9",
            "abcdefgA", "12345678A", "qwertyuiA", "asddfgA",
            "abcdefgB", "12345678B", "qwertyuiB", "asddfgB",
            "abcdefgC", "12345678C", "qwertyuiC", "asddfgC")
 and c7 in ("abcdefgh", "123456789", "qwertyuio", "asddfgh", 
            "abcdefg1", "123456781", "qwertyui1", "asddfg1", 
            "abcdefg2", "123456782", "qwertyui2", "asddfg2", 
            "abcdefg3", "123456783", "qwertyui3", "asddfg3", 
            "abcdefg4", "123456784", "qwertyui4", "asddfg4", 
            "abcdefg5", "123456785", "qwertyui5", "asddfg5",
            "abcdefg6", "123456786", "qwertyui6", "asddfg6",
            "abcdefg7", "123456787", "qwertyui7", "asddfg7",
            "abcdefg8", "123456788", "qwertyui8", "asddfg8",
            "abcdefg9", "123456789", "qwertyui9", "asddfg9",
            "abcdefgA", "12345678A", "qwertyuiA", "asddfgA",
            "abcdefgB", "12345678B", "qwertyuiB", "asddfgB",
            "abcdefgC", "12345678C", "qwertyuiC", "asddfgC")
 and c8 in ("abcdefgh", "123456789", "qwertyuio", "asddfgh", 
            "abcdefg1", "123456781", "qwertyui1", "asddfg1", 
            "abcdefg2", "123456782", "qwertyui2", "asddfg2", 
            "abcdefg3", "123456783", "qwertyui3", "asddfg3", 
            "abcdefg4", "123456784", "qwertyui4", "asddfg4", 
            "abcdefg5", "123456785", "qwertyui5", "asddfg5",
            "abcdefg6", "123456786", "qwertyui6", "asddfg6",
            "abcdefg7", "123456787", "qwertyui7", "asddfg7",
            "abcdefg8", "123456788", "qwertyui8", "asddfg8",
            "abcdefg9", "123456789", "qwertyui9", "asddfg9",
            "abcdefgA", "12345678A", "qwertyuiA", "asddfgA",
            "abcdefgB", "12345678B", "qwertyuiB", "asddfgB",
            "abcdefgC", "12345678C", "qwertyuiC", "asddfgC")
 and c9 in ("abcdefgh", "123456789", "qwertyuio", "asddfgh", 
            "abcdefg1", "123456781", "qwertyui1", "asddfg1", 
            "abcdefg2", "123456782", "qwertyui2", "asddfg2", 
            "abcdefg3", "123456783", "qwertyui3", "asddfg3", 
            "abcdefg4", "123456784", "qwertyui4", "asddfg4", 
            "abcdefg5", "123456785", "qwertyui5", "asddfg5",
            "abcdefg6", "123456786", "qwertyui6", "asddfg6",
            "abcdefg7", "123456787", "qwertyui7", "asddfg7",
            "abcdefg8", "123456788", "qwertyui8", "asddfg8",
            "abcdefg9", "123456789", "qwertyui9", "asddfg9",
            "abcdefgA", "12345678A", "qwertyuiA", "asddfgA",
            "abcdefgB", "12345678B", "qwertyuiB", "asddfgB",
            "abcdefgC", "12345678C", "qwertyuiC", "asddfgC")
 and c10 in ("abcdefgh", "123456789", "qwertyuio", "asddfgh", 
            "abcdefg1", "123456781", "qwertyui1", "asddfg1", 
            "abcdefg2", "123456782", "qwertyui2", "asddfg2", 
            "abcdefg3", "123456783", "qwertyui3", "asddfg3", 
            "abcdefg4", "123456784", "qwertyui4", "asddfg4", 
            "abcdefg5", "123456785", "qwertyui5", "asddfg5",
            "abcdefg6", "123456786", "qwertyui6", "asddfg6",
            "abcdefg7", "123456787", "qwertyui7", "asddfg7",
            "abcdefg8", "123456788", "qwertyui8", "asddfg8",
            "abcdefg9", "123456789", "qwertyui9", "asddfg9",
            "abcdefgA", "12345678A", "qwertyuiA", "asddfgA",
            "abcdefgB", "12345678B", "qwertyuiB", "asddfgB",
            "abcdefgC", "12345678C", "qwertyuiC", "asddfgC");
drop table t1;
--echo End of 4.1 tests

#
# Test for optimization request #10561: to use keys for
# NOT IN (c1,...,cn) and NOT BETWEEN c1 AND c2
#

CREATE TABLE t1 (
  id int(11) NOT NULL auto_increment,
  status varchar(20),
  PRIMARY KEY  (id),
  KEY (status)
) CHARSET utf8mb4;

INSERT INTO t1 VALUES
(1,'B'), (2,'B'), (3,'B'), (4,'B'), (5,'B'), (6,'B'),
(7,'B'), (8,'B'), (9,'B'), (10,'B'), (11,'B'), (12,'B'),
(13,'B'), (14,'B'), (15,'B'), (16,'B'), (17,'B'), (18,'B'),
(19,'B'), (20,'B'), (21,'B'), (22,'B'), (23,'B'), (24,'B'), 
(25,'A'), (26,'A'), (27,'A'), (28,'A'), (29,'A'), (30,'A'),
(31,'A'), (32,'A'), (33,'A'), (34,'A'), (35,'A'), (36,'A'),
(37,'A'), (38,'A'), (39,'A'), (40,'A'), (41,'A'), (42,'A'),
(43,'A'), (44,'A'), (45,'A'), (46,'A'), (47,'A'), (48,'A'),
(49,'A'), (50,'A'), (51,'A'), (52,'A'), (53,'C'), (54,'C'),
(55,'C'), (56,'C'), (57,'C'), (58,'C'), (59,'C'), (60,'C');

EXPLAIN SELECT * FROM t1 WHERE status <> 'A' AND status <> 'B';
EXPLAIN SELECT * FROM t1 WHERE status NOT IN ('A','B');

SELECT * FROM t1 WHERE status <> 'A' AND status <> 'B';
SELECT * FROM t1 WHERE status NOT IN ('A','B');

EXPLAIN SELECT status FROM t1 WHERE status <> 'A' AND status <> 'B';
EXPLAIN SELECT status FROM t1 WHERE status NOT IN ('A','B');

EXPLAIN SELECT * FROM t1 WHERE status NOT BETWEEN 'A' AND 'B';
EXPLAIN SELECT * FROM t1 WHERE status < 'A' OR status > 'B';

SELECT * FROM t1 WHERE status NOT BETWEEN 'A' AND 'B';
SELECT * FROM t1 WHERE status < 'A' OR status > 'B';

DROP TABLE t1;

#
# Test for bug #10031: range to be used over a view
#

CREATE TABLE  t1 (a int, b int, primary key(a,b));

INSERT INTO  t1 VALUES
  (1,1),(1,2),(1,3),(2,1),(2,2),(2,3),(3,1),(3,2),(3,3),(4,1),(4,2),(4,3);

CREATE VIEW v1 as SELECT a,b FROM t1 WHERE b=3;

EXPLAIN SELECT a,b FROM t1 WHERE a < 2 and b=3;
EXPLAIN SELECT a,b FROM v1 WHERE a < 2 and b=3;

EXPLAIN SELECT a,b FROM t1 WHERE a < 2;
EXPLAIN SELECT a,b FROM v1 WHERE a < 2;

SELECT a,b FROM t1 WHERE a < 2 and b=3;
SELECT a,b FROM v1 WHERE a < 2 and b=3; 

DROP VIEW v1;
DROP TABLE t1;

#
# Bug #11853: DELETE statement with a NOT (LIKE/<=>) where condition
#             for an indexed attribute              
#             

CREATE TABLE t1 (name varchar(15) NOT NULL, KEY idx(name));
INSERT INTO t1 VALUES ('Betty'), ('Anna');

SELECT * FROM t1;
DELETE FROM t1 WHERE name NOT LIKE 'A%a';
SELECT * FROM t1;

DROP TABLE t1;

CREATE TABLE t1 (a int, KEY idx(a));
INSERT INTO t1 VALUES (NULL), (1), (2), (3);

SELECT * FROM t1;
DELETE FROM t1 WHERE NOT(a <=> 2);
SELECT * FROM t1;

DROP TABLE t1;

#
# BUG#13317: range optimization doesn't work for IN over VIEW.
#
create table t1 (a int, b int, primary key(a,b));
create view v1 as select a, b from t1;

INSERT INTO `t1` VALUES
(0,0),(1,0),(2,0),(3,0),(4,0),(5,1),(6,1),(7,1),(8,1),(9,1),(10,2),(11,2),(12,2)
,(13,2),(14,2),(15,3),(16,3),(17,3),(18,3),(19,3);

--replace_column 10 #
explain select * from t1 where a in (3,4)  and b in (1,2,3);
--replace_column 10 #
explain select * from v1 where a in (3,4)  and b in (1,2,3);
--replace_column 10 #
explain select * from t1 where a between 3 and 4 and b between 1 and 2;
--replace_column 10 #
explain select * from v1 where a between 3 and 4 and b between 1 and 2;
 
drop view v1;
drop table t1;

# BUG#13455: 
create table t3 (a int);
insert into t3 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);

create table t1 (a varchar(10), filler char(200), key(a)) charset=binary;
insert into t1 values ('a','');
insert into t1 values ('a ','');
insert into t1 values ('a  ', '');
insert into t1 select concat('a', 1000 + A.a + 10 * (B.a + 10 * C.a)), ''
  from t3 A, t3 B, t3 C;

create table t2 (a varchar(10), filler char(200), key(a)) charset utf8mb4;
insert into t2 select * from t1;

--replace_column 10 #
explain select * from t1 where a between 'a' and 'a '; 
--replace_column 10 #
explain select * from t1 where a = 'a' or a='a ';

--replace_column 10 #
explain select * from t2 where a between 'a' and 'A';
--replace_column 10 #
explain select * from t2 where a = 'a' or a='A';

update t1 set a='b' where a<>'a';
--replace_column 10 #
explain select * from t1 where a not between 'b' and 'b'; 
select a, hex(filler) from t1 where a not between 'b' and 'b'; 

drop table t1,t2,t3;

#
# BUG#21282
#
create table t1 (a int);
insert into t1 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);
create table t2 (a int, key(a));
insert into t2 select 2*(A.a + 10*(B.a + 10*C.a)) from t1 A, t1 B, t1 C;

set @a="select * from t2 force index (a) where a NOT IN(0";
select count(*) from (select @a:=concat(@a, ',', a) from t2 ) Z;
set @a=concat(@a, ')');

insert into t2 values (11),(13),(15);

set @b= concat("explain ", @a);

prepare stmt1 from @b;
execute stmt1;

prepare stmt1 from @a;
execute stmt1;

drop table t1, t2;

#
# Bug #18165: range access for BETWEEN with a constant for the first argument 
#

CREATE TABLE t1 (
  id int NOT NULL DEFAULT '0',
  b int NOT NULL DEFAULT '0',
  c int NOT NULL DEFAULT '0', 
  INDEX idx1(b,c), INDEX idx2(c));

INSERT INTO t1(id) VALUES (1), (2), (3), (4), (5), (6), (7), (8);

INSERT INTO t1(b,c) VALUES (3,4), (3,4);

ANALYZE TABLE t1;

SELECT * FROM t1 WHERE b<=3 AND 3<=c;
SELECT * FROM t1 WHERE 3 BETWEEN b AND c;

EXPLAIN  SELECT * FROM t1 WHERE b<=3 AND 3<=c;
EXPLAIN  SELECT * FROM t1 WHERE 3 BETWEEN b AND c;

SELECT * FROM t1 WHERE 0 < b OR 0 > c;
SELECT * FROM t1 WHERE 0 NOT BETWEEN b AND c;

EXPLAIN SELECT * FROM t1 WHERE 0 < b OR 0 > c;
EXPLAIN SELECT * FROM t1 WHERE 0 NOT BETWEEN b AND c;
EXPLAIN FORMAT=tree SELECT * FROM t1 WHERE 0 NOT BETWEEN b AND c;  # Tests printing of sort_union.

DROP TABLE t1;

#
# Bug #16249: different results for a range with and without index
#             when a range condition use an invalid datetime constant
# This case was made obsolete after the changes applied by
# Bug#28940878 WRONG RESULT WHEN COMPARING DATE[TIME] WITH STRING
# as we don't fallback anymore to string comparison if the date is invalid.

CREATE TABLE t1 (                                      
  item char(20) NOT NULL default '',                          
  started datetime NOT NULL default '0000-00-00 00:00:00', 
  price decimal(16,3) NOT NULL default '0.000',                 
  PRIMARY KEY (item,started)                     
) ENGINE=MyISAM, CHARSET utf8mb4;

INSERT INTO t1 VALUES
('A1','2005-11-01 08:00:00',1000),
('A1','2005-11-15 00:00:00',2000),
('A1','2005-12-12 08:00:00',3000),
('A2','2005-12-01 08:00:00',1000);

--error ER_WRONG_VALUE
EXPLAIN SELECT * FROM t1 WHERE item='A1' AND started<='2005-12-01 24:00:00';
--error ER_WRONG_VALUE
SELECT * FROM t1 WHERE item='A1' AND started<='2005-12-01 24:00:00';
SELECT * FROM t1 WHERE item='A1' AND started<='2005-12-02 00:00:00';

DROP INDEX `PRIMARY` ON t1;

--error ER_WRONG_VALUE
EXPLAIN SELECT * FROM t1 WHERE item='A1' AND started<='2005-12-01 24:00:00';
--error ER_WRONG_VALUE
SELECT * FROM t1 WHERE item='A1' AND started<='2005-12-01 24:00:00';
SELECT * FROM t1 WHERE item='A1' AND started<='2005-12-02 00:00:00';

DROP TABLE t1;

--echo
--echo BUG#32198 "Comparison of DATE with DATETIME still not using indexes correctly"
--echo
CREATE TABLE t1 (
  id int(11) NOT NULL auto_increment,
  dateval date default NULL,
  PRIMARY KEY  (id),
  KEY dateval (dateval)
) AUTO_INCREMENT=173;

INSERT INTO t1 VALUES
(1,'2007-01-01'),(2,'2007-01-02'),(3,'2007-01-03'),(4,'2007-01-04'),
(5,'2007-01-05'),(6,'2007-01-06'),(7,'2007-01-07'),(8,'2007-01-08'),
(9,'2007-01-09'),(10,'2007-01-10'),(11,'2007-01-11');

--echo This must use range access:
explain select * from t1 where dateval >= '2007-01-01 00:00:00' and dateval <= '2007-01-02 23:59:59';

drop table t1;

#
# Bug #33833: different or-ed predicates were erroneously merged into one that
# resulted in ref access instead of range access and  a wrong result set
#

CREATE TABLE t1 (
  a varchar(32), index (a)
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

INSERT INTO t1 VALUES
  ('B'), ('A'), ('A'), ('C'), ('B'), ('A'), ('A');

SELECT a FROM t1 WHERE a='b' OR a='B';
EXPLAIN SELECT a FROM t1 WHERE a='b' OR a='B';

DROP TABLE t1;

#
# Bug #34731: highest possible value for INT erroneously filtered by WHERE
#

# test UNSIGNED. only occurs when indexed.
CREATE TABLE t1 (f1 TINYINT(11) UNSIGNED NOT NULL, PRIMARY KEY (f1));

INSERT INTO t1 VALUES (127),(254),(0),(1),(255);

# test upper bound
# count 5
SELECT COUNT(*) FROM t1 WHERE f1 < 256;
SELECT COUNT(*) FROM t1 WHERE f1 < 256.0;
# count 4
SELECT COUNT(*) FROM t1 WHERE f1 < 255;

# show we don't fiddle with lower bound on UNSIGNED
# count 0
SELECT COUNT(*) FROM t1 WHERE f1 < -1;
# count 5
SELECT COUNT(*) FROM t1 WHERE f1 > -1;

DROP TABLE t1;


# test signed. only occurs when index.
CREATE TABLE t1 ( f1 TINYINT(11) NOT NULL, PRIMARY KEY (f1));

INSERT INTO t1 VALUES (127),(126),(0),(-128),(-127);

# test upper bound
# count 5
SELECT COUNT(*) FROM t1 WHERE f1 < 128;
SELECT COUNT(*) FROM t1 WHERE f1 < 128.0;
# count 4
SELECT COUNT(*) FROM t1 WHERE f1 < 127;

# test lower bound
# count 5
SELECT COUNT(*) FROM t1 WHERE f1 > -129;
SELECT COUNT(*) FROM t1 WHERE f1 > -129.0;
# count 4
SELECT COUNT(*) FROM t1 WHERE f1 > -128;

DROP TABLE t1;

# End of 5.0 tests

# BUG#22393 fix: Adjust 'ref' estimate if we have 'range' estimate for
#                a smaller scan interval
create table t1 (a int);
insert into t1 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);

create table t2 (a int, b int, filler char(100));
insert into t2 select A.a + 10 * (B.a + 10 * C.a), 10, 'filler' from t1 A,
t1 B, t1 C where A.a < 5;

insert into t2 select 1000, b, 'filler' from t2;
alter table t2 add index (a,b);
# t2 values 
#  ( 1  , 10, 'filler')
#  ( 2  , 10, 'filler')
#  ( 3  , 10, 'filler')
#  (... , 10, 'filler')
#   ...
#  (1000, 10, 'filler') - 500 times

# 500 rows, 1 row

--echo Access method can be range/ALL with #rows >= 500.
--echo Or it can be ref with #rows = 2, only when there is memory limit.
explain select * from t2 where a=1000 and b<11;

drop table t1, t2;

#
# Bug#42846: wrong result returned for range scan when using covering index
#
CREATE TABLE t1( a INT, b INT, KEY( a, b ) );

CREATE TABLE t2( a INT, b INT, KEY( a, b ) );

CREATE TABLE t3( a INT, b INT, KEY( a, b ) );

INSERT INTO t1( a, b ) 
VALUES (0, 1), (1, 2), (1, 4), (2, 3), (5, 0), (9, 7);

INSERT INTO t2( a, b ) 
VALUES ( 1, 1), ( 2, 1), ( 3, 1), ( 4, 1), ( 5, 1),
       ( 6, 1), ( 7, 1), ( 8, 1), ( 9, 1), (10, 1), 
       (11, 1), (12, 1), (13, 1), (14, 1), (15, 1),
       (16, 1), (17, 1), (18, 1), (19, 1), (20, 1);

INSERT INTO t2 SELECT a, 2 FROM t2 WHERE b = 1;
INSERT INTO t2 SELECT a, 3 FROM t2 WHERE b = 1;

# To make range scan compelling to the optimizer
INSERT INTO t2 SELECT -1, -1 FROM t2;
INSERT INTO t2 SELECT -1, -1 FROM t2;
INSERT INTO t2 SELECT -1, -1 FROM t2;

INSERT INTO t3
VALUES (1, 0), (2, 0), (3, 0), (4, 0), (5, 0),
       (6, 0), (7, 0), (8, 0), (9, 0), (10, 0);

# To make range scan compelling to the optimizer
INSERT INTO t3 SELECT * FROM t3 WHERE a = 10;
INSERT INTO t3 SELECT * FROM t3 WHERE a = 10;


#
# Problem#1 Test queries. Will give missing results unless Problem#1 is fixed.
# With one exception, they are independent of Problem#2.
#
SELECT * FROM t1 WHERE
3 <= a AND a < 5 OR 
5 < a AND b = 3 OR
3 <= a;

EXPLAIN
SELECT * FROM t1 WHERE
3 <= a AND a < 5 OR 
5 < a AND b = 3 OR
3 <= a;

# Query below: Tests both Problem#1 and Problem#2 (EXPLAIN differs as well)
SELECT * FROM t1 WHERE
3 <= a AND a < 5 OR 
5 <= a AND b = 3 OR
3 <= a;

EXPLAIN
SELECT * FROM t1 WHERE
3 <= a AND a < 5 OR 
5 <= a AND b = 3 OR
3 <= a;

SELECT * FROM t1 WHERE
3 <= a AND a <= 5 OR 
5 <= a AND b = 3 OR
3 <= a;

EXPLAIN
SELECT * FROM t1 WHERE
3 <= a AND a <= 5 OR 
5 <= a AND b = 3 OR
3 <= a;

SELECT * FROM t1 WHERE
3 <= a AND a <= 5 OR 
3 <= a;

EXPLAIN
SELECT * FROM t1 WHERE
3 <= a AND a <= 5 OR 
3 <= a;

#
# Problem#2 Test queries. 
# These queries will give missing results if Problem#1 is fixed.
# But Problem#1 also hides this bug.
#
SELECT * FROM t2 WHERE
5 <= a AND a < 10 AND b = 1 OR
15 <= a AND a < 20 AND b = 3
OR
1 <= a AND b = 1;

EXPLAIN
SELECT * FROM t2 WHERE
5 <= a AND a < 10 AND b = 1 OR
15 <= a AND a < 20 AND b = 3
OR
1 <= a AND b = 1;

SELECT * FROM t2 WHERE
5 <= a AND a < 10 AND b = 2 OR
15 <= a AND a < 20 AND b = 3
OR
1 <= a AND b = 1;

EXPLAIN
SELECT * FROM t2 WHERE
5 <= a AND a < 10 AND b = 2 OR
15 <= a AND a < 20 AND b = 3
OR
1 <= a AND b = 1;

SELECT * FROM t3 WHERE
5 <= a AND a < 10 AND b = 3 OR 
a < 5 OR
a < 10;

EXPLAIN
SELECT * FROM t3 WHERE
5 <= a AND a < 10 AND b = 3 OR 
a < 5 OR
a < 10;

DROP TABLE t1, t2, t3;

--echo #
--echo # Bug #47123: Endless 100% CPU loop with STRAIGHT_JOIN
--echo #

CREATE TABLE t1(a INT, KEY(a));
INSERT INTO t1 VALUES (1), (NULL);
SELECT * FROM t1 WHERE a <> NULL and (a <> NULL or a <= NULL);
DROP TABLE t1;

--echo #
--echo # Bug#47925: regression of range optimizer and date comparison in 5.1.39!
--echo #
CREATE TABLE t1 ( a DATE,     KEY ( a ) );
CREATE TABLE t2 ( a DATETIME, KEY ( a ) );

--echo # Make optimizer choose range scan
INSERT INTO t1 VALUES ('2009-09-22'), ('2009-09-22'), ('2009-09-22');
INSERT INTO t1 VALUES ('2009-09-23'), ('2009-09-23'), ('2009-09-23');

INSERT INTO t2 VALUES ('2009-09-22 12:00:00'), ('2009-09-22 12:00:00'),
                      ('2009-09-22 12:00:00');
INSERT INTO t2 VALUES ('2009-09-23 12:00:00'), ('2009-09-23 12:00:00'),
                      ('2009-09-23 12:00:00');

--echo # DATE vs DATE
--replace_column 1 X 2 X 3 X 8 X 9 X 10 X 12 X
EXPLAIN
SELECT * FROM t1 WHERE a >= '2009/09/23';
SELECT * FROM t1 WHERE a >= '2009/09/23';
SELECT * FROM t1 WHERE a >= '20090923';
SELECT * FROM t1 WHERE a >=  20090923;
SELECT * FROM t1 WHERE a >= '2009-9-23';
SELECT * FROM t1 WHERE a >= '2009.09.23';
SELECT * FROM t1 WHERE a >= '2009:09:23';

--echo # DATE vs DATETIME
--replace_column 1 X 2 X 3 X 8 X 9 X 10 X 12 X
EXPLAIN
SELECT * FROM t2 WHERE a >= '2009/09/23';
SELECT * FROM t2 WHERE a >= '2009/09/23';
SELECT * FROM t2 WHERE a >= '2009/09/23';
SELECT * FROM t2 WHERE a >= '20090923';
SELECT * FROM t2 WHERE a >=  20090923;
SELECT * FROM t2 WHERE a >= '2009-9-23';
SELECT * FROM t2 WHERE a >= '2009.09.23';
SELECT * FROM t2 WHERE a >= '2009:09:23';

--echo # DATETIME vs DATETIME
--replace_column 1 X 2 X 3 X 8 X 9 X 10 X 12 X
EXPLAIN
SELECT * FROM t2 WHERE a >= '2009/09/23 12:00:00';
SELECT * FROM t2 WHERE a >= '2009/09/23 12:00:00';
SELECT * FROM t2 WHERE a >= '20090923120000';
SELECT * FROM t2 WHERE a >=  20090923120000;
SELECT * FROM t2 WHERE a >= '2009-9-23 12:00:00';
SELECT * FROM t2 WHERE a >= '2009.09.23 12:00:00';
SELECT * FROM t2 WHERE a >= '2009:09:23 12:00:00';

--echo # DATETIME vs DATE
--replace_column 1 X 2 X 3 X 8 X 9 X 10 X 12 X
EXPLAIN
SELECT * FROM t1 WHERE a >= '2009/09/23 00:00:00';
SELECT * FROM t1 WHERE a >= '2009/09/23 00:00:00';
SELECT * FROM t1 WHERE a >= '2009/09/23 00:00:00';
SELECT * FROM t1 WHERE a >= '20090923000000';
SELECT * FROM t1 WHERE a >=  20090923000000;
SELECT * FROM t1 WHERE a >= '2009-9-23 00:00:00';
SELECT * FROM t1 WHERE a >= '2009.09.23 00:00:00';
SELECT * FROM t1 WHERE a >= '2009:09:23 00:00:00';

--echo # Test of the new get_date_from_str implementation
--echo # Behavior differs slightly between the trunk and mysql-pe.
--echo # The former may give errors for the truncated values, while the latter
--echo # gives warnings. The purpose of this test is not to interfere, and only
--echo # preserve existing behavior.
--echo # This case was made obsolete after the changes applied by
--echo # Bug#28940878 WRONG RESULT WHEN COMPARING DATE[TIME] WITH STRING
--echo # as we don't fallback anymore to string comparison if the date is invalid.
--error ER_WRONG_VALUE
SELECT str_to_date('2007-10-00', '%Y-%m-%d') >= '' AND 
       str_to_date('2007-10-00', '%Y-%m-%d') <= '2007/10/20';

--error ER_WRONG_VALUE
SELECT str_to_date('2007-20-00', '%Y-%m-%d') >= '2007/10/20' AND 
       str_to_date('2007-20-00', '%Y-%m-%d') <= '';

SELECT str_to_date('2007-10-00', '%Y-%m-%d') BETWEEN '' AND '2007/10/20';
SELECT str_to_date('2007-20-00', '%Y-%m-%d') BETWEEN '2007/10/20' AND '';

SELECT str_to_date('', '%Y-%m-%d');

DROP TABLE t1, t2;

--echo #
--echo # Bug#48459: valgrind errors with query using 'Range checked for each 
--echo # record'
--echo #
CREATE TABLE t1 (
  a INT,
  b CHAR(2),
  c INT,
  d INT,
  KEY ( c ),
  KEY ( d, a, b ( 2 ) ),
  KEY ( b ( 1 ) )
);

INSERT INTO t1 VALUES ( NULL, 'a', 1, 2 ), ( NULL, 'a', 1, 2 ),
                      ( 1,    'a', 1, 2 ), ( 1,    'a', 1, 2 );

CREATE TABLE t2 (
  a INT,
  c INT,
  e INT,
  KEY ( e )
);

INSERT INTO t2 VALUES ( 1, 1, NULL ), ( 1, 1, NULL );

--echo # Should not give Valgrind warnings
SELECT 1
FROM t1, t2
WHERE t1.d <> '1' AND t1.b > '1'
AND t1.a = t2.a AND t1.c = t2.c;

DROP TABLE t1, t2;

--echo #
--echo # Bug #48665: sql-bench's insert test fails due to wrong result
--echo #

CREATE TABLE t1 (a INT, b INT, PRIMARY KEY (a));

INSERT INTO t1 VALUES (0,0), (1,1);

--replace_column 1 @ 2 @ 3 @ 6 @ 7 @ 8 @ 9 @ 10 @ 12 @
EXPLAIN 
SELECT * FROM t1 FORCE INDEX (PRIMARY) 
 WHERE (a>=1 AND a<=2) OR (a>=4 AND a<=5) OR (a>=0 AND a <=10);

--echo # Should return 2 rows
SELECT * FROM t1 FORCE INDEX (PRIMARY) 
 WHERE (a>=1 AND a<=2) OR (a>=4 AND a<=5) OR (a>=0 AND a <=10);

DROP TABLE t1;

--echo #
--echo # Bug #54802: 'NOT BETWEEN' evaluation is incorrect
--echo #

CREATE TABLE t1 (c_key INT, c_notkey INT, KEY(c_key));
INSERT INTO t1 VALUES (1, 1), (2, 2), (3, 3);

EXPLAIN SELECT * FROM t1 WHERE 2 NOT BETWEEN c_notkey AND c_key;
SELECT * FROM t1 WHERE 2 NOT BETWEEN c_notkey AND c_key;

DROP TABLE t1;

--echo #
--echo # Bug #57030: 'BETWEEN' evaluation is incorrect
--echo #

# Test some BETWEEN predicates which does *not* follow the
# 'normal' pattern of <field> BETWEEN <low const> AND <high const>

CREATE TABLE t1(pk INT PRIMARY KEY, i4 INT);
CREATE UNIQUE INDEX i4_uq ON t1(i4);

INSERT INTO t1 VALUES (1,10), (2,20), (3,30);

EXPLAIN
SELECT * FROM t1 WHERE i4 BETWEEN 10 AND 10;
SELECT * FROM t1 WHERE i4 BETWEEN 10 AND 10;

EXPLAIN
SELECT * FROM t1 WHERE 10 BETWEEN i4 AND i4;
SELECT * FROM t1 WHERE 10 BETWEEN i4 AND i4;

EXPLAIN
SELECT * FROM t1 WHERE 10 BETWEEN 10 AND i4;
SELECT * FROM t1 WHERE 10 BETWEEN 10 AND i4;

EXPLAIN
SELECT * FROM t1 WHERE 10 BETWEEN i4 AND 10;
SELECT * FROM t1 WHERE 10 BETWEEN i4 AND 10;

EXPLAIN
SELECT * FROM t1 WHERE 10 BETWEEN 10 AND 10;
SELECT * FROM t1 WHERE 10 BETWEEN 10 AND 10;

EXPLAIN
SELECT * FROM t1 WHERE 10 BETWEEN 11 AND 11;
SELECT * FROM t1 WHERE 10 BETWEEN 11 AND 11;

EXPLAIN
SELECT * FROM t1 WHERE 10 BETWEEN 100 AND 0;
SELECT * FROM t1 WHERE 10 BETWEEN 100 AND 0;

EXPLAIN
SELECT * FROM t1 WHERE i4 BETWEEN 100 AND 0;
SELECT * FROM t1 WHERE i4 BETWEEN 100 AND 0;

EXPLAIN
SELECT * FROM t1 WHERE i4 BETWEEN 10 AND 99999999999999999;
SELECT * FROM t1 WHERE i4 BETWEEN 10 AND 99999999999999999;

EXPLAIN
SELECT * FROM t1 WHERE i4 BETWEEN 999999999999999 AND 30;
SELECT * FROM t1 WHERE i4 BETWEEN 999999999999999 AND 30;

EXPLAIN
SELECT * FROM t1 WHERE i4 BETWEEN 10 AND '20';
SELECT * FROM t1 WHERE i4 BETWEEN 10 AND '20';

# Should detect the EQ_REF 't2.pk=t1.i4'.
# Also tests printing of index condition pushdown on EQRefIterator.
EXPLAIN
SELECT * FROM t1, t1 as t2 WHERE t2.pk BETWEEN t1.i4 AND t1.i4;
EXPLAIN FORMAT=tree
SELECT * FROM t1, t1 as t2 WHERE t2.pk BETWEEN t1.i4 AND t1.i4;
SELECT * FROM t1, t1 as t2 WHERE t2.pk BETWEEN t1.i4 AND t1.i4;

EXPLAIN
SELECT * FROM t1, t1 as t2 WHERE t1.i4 BETWEEN t2.pk AND t2.pk;
SELECT * FROM t1, t1 as t2 WHERE t1.i4 BETWEEN t2.pk AND t2.pk;

DROP TABLE t1;

--echo #
--echo # BUG#13519696 - 62940: SELECT RESULTS VARY WITH VERSION AND
--echo # WITH/WITHOUT INDEX RANGE SCAN
--echo #

create table t1 (id int unsigned not null auto_increment primary key);
insert into t1 values (null);  
insert into t1 select null from t1;  
insert into t1 select null from t1;  
insert into t1 select null from t1;  
insert into t1 select null from t1;  
insert into t1 select null from t1;  
insert into t1 select null from t1;  
insert into t1 select null from t1;  
insert into t1 select null from t1; 
create table t2 (
 id int unsigned not null auto_increment,
 val decimal(5,3) not null,
 primary key (id,val),
 unique key (val,id),
 unique key (id));  
--disable_warnings
insert into t2 select null,id*0.0009 from t1;
--enable_warnings

select count(val) from t2 ignore index (val) where val > 0.1155;
select count(val) from t2 force index (val)  where val > 0.1155;

drop table t2, t1;

--echo #
--echo # BUG#13453382 - REGRESSION SINCE 5.1.39, RANGE OPTIMIZER WRONG
--echo # RESULTS WITH DECIMAL CONVERSION
--echo #

create table t1 (a int,b int,c int,primary key (a,c));
insert into t1 values (1,1,2),(1,1,3),(1,1,4);
# show that the integer 3 is bigger than the decimal 2.9,
# which should also apply to comparing "c" with 2.9
# when c is 3.
select convert(3, signed integer) > 2.9;
select * from t1 force  index (primary) where a=1 and c>= 2.9;
select * from t1 ignore index (primary) where a=1 and c>= 2.9;
select * from t1 force  index (primary) where a=1 and c> 2.9;
select * from t1 ignore index (primary) where a=1 and c> 2.9;
drop table t1;

--echo #
--echo # BUG#13463488 - 63437: CHAR & BETWEEN WITH INDEX RETURNS WRONG
--echo # RESULT AFTER MYSQL 5.1.
--echo #

# This test tests interactions between CHAR columns and PAD collations,
# so it doesn't make sense to test it with our default NO PAD collation.

CREATE TABLE t1(
 F1 CHAR(5) NOT NULL,
 F2 CHAR(5) NOT NULL,
 F3 CHAR(5) NOT NULL,
 PRIMARY KEY(F1),
 INDEX IDX_F2(F2)
) COLLATE latin1_swedish_ci;

INSERT INTO t1 VALUES
('A','A','A'),('AA','AA','AA'),('AAA','AAA','AAA'),
('AAAA','AAAA','AAAA'),('AAAAA','AAAAA','AAAAA');

SELECT * FROM t1 WHERE F1 = 'A    ';
SELECT * FROM t1 IGNORE INDEX(PRIMARY) WHERE F1 = 'A    ';
SELECT * FROM t1 WHERE F1 >= 'A    ';
SELECT * FROM t1 WHERE F1 > 'A    ';
SELECT * FROM t1 WHERE F1 BETWEEN 'A    ' AND 'AAAAA';
SELECT * FROM t1 WHERE F2 BETWEEN 'A    ' AND 'AAAAA';
SELECT * FROM t1 WHERE F3 BETWEEN 'A    ' AND 'AAAAA';
SELECT * FROM t1 IGNORE INDEX(PRIMARY) WHERE F1 BETWEEN 'A    ' AND
'AAAAA';

DROP TABLE t1;

--echo End of 5.1 tests

#
# BUG#32262 fix: crash with decimal column...
#

CREATE TABLE t1 (c1 DECIMAL(10,0),INDEX(c1));
INSERT INTO t1 VALUES (1),(2),(3);
SELECT c1 FROM t1 WHERE c1 >= 'A' GROUP BY 1;
DROP TABLE t1;

#
# BUG#32229: Range optimizer crashes for 'range checked for each record' query
#
create table t1 (a int,b int,key (b),key (a),key (b,a));
insert into t1(a,b) values (1,2),(3,4),(5,6),(7,8);
create table t2 (c int);
insert into t2(c) values (1),(5),(6),(7),(8);
select 1 from (select c from t1,t2 where b >= 1 and a <=> c group by 1 limit 1) as d1;
drop table t1, t2;

--echo #
--echo # Bug #26106: Wrong plan may be chosen when there are several possible
--echo # range and ref accesses
--echo #
--echo # Note: The fix for this bug has been reverted. The code will no longer
--echo # select the optimal plan for the two following test queries. This is
--echo # not due to a bug but due to minor differences in range estimates
--echo # produced by the storage engine.

CREATE TABLE t1( 
  a INT,
  b INT,
  KEY k ( a ),
  KEY l ( a, b )
); 

INSERT INTO t1(a) VALUES (1);
INSERT INTO t1 
VALUES (2,3),(2,3),(2,3),(2,3),(2,3),(2,3),(2,3),(2,3),(2,3),(2,3);
INSERT INTO t1 SELECT 3, 4 FROM t1 WHERE a = 2 AND b = 3;
INSERT INTO t1 SELECT 4, 1 FROM t1 WHERE a = 2 AND b = 3;
ANALYZE TABLE t1;
INSERT INTO t1 VALUES (1, 2);
INSERT INTO t1 SELECT a, b FROM t1 WHERE a=1 AND b=2; # 2
INSERT INTO t1 SELECT a, b FROM t1 WHERE a=1 AND b=2; # 4
INSERT INTO t1 SELECT a, b FROM t1 WHERE a=1 AND b=2; # 8
INSERT INTO t1 SELECT a, b FROM t1 WHERE a=1 AND b=2; # 16
INSERT INTO t1 SELECT a, b FROM t1 WHERE a=1 AND b=2; # 32
INSERT INTO t1 SELECT a, b FROM t1 WHERE a=1 AND b=2; # 64
INSERT INTO t1 SELECT a, b FROM t1 WHERE a=1 AND b=2; # 128
--echo # This must use range over index l, not k.
--echo # Update: Due to patch being reverted and minor differences in 
--echo #         range estimates k is selected.
EXPLAIN SELECT * FROM t1 WHERE a = 1 AND b >= 2;

CREATE TABLE t2( 
  a INT, 
  b INT, 
  c INT,
  KEY k ( a ), 
  KEY l ( a, b ),
  KEY m ( b ), 
  KEY n ( a, c )
); 

INSERT INTO t2(a) VALUES (1);
INSERT INTO t2 
VALUES (2,3,3),(2,3,3),(2,3,3),(2,3,3),(2,3,3),
       (2,3,3),(2,3,3),(2,3,3),(2,3,3),(2,3,3);
INSERT INTO t2 SELECT 3, 4, 4 FROM t2 WHERE a = 2 AND b = 3;
INSERT INTO t2 SELECT 4, 1, 1 FROM t2 WHERE a = 2 AND b = 3;
ANALYZE TABLE t2;
INSERT INTO t2 VALUES (1, 2, 2);
INSERT INTO t2 SELECT a, b, c FROM t2 WHERE a=1 AND b=2; # 2
INSERT INTO t2 SELECT a, b, c FROM t2 WHERE a=1 AND b=2; # 4
INSERT INTO t2 SELECT a, b, c FROM t2 WHERE a=1 AND b=2; # 8
INSERT INTO t2 SELECT a, b, c FROM t2 WHERE a=1 AND b=2; # 16
INSERT INTO t2 SELECT a, b, c FROM t2 WHERE a=1 AND b=2; # 32
INSERT INTO t2 SELECT a, b, c FROM t2 WHERE a=1 AND b=2; # 64
INSERT INTO t2 VALUES (1, 1, 2);

--echo # This must use range over index l, not n.
--echo # Update: Due to patch being reverted and minor differences in 
--echo #         range estimates k is selected.
EXPLAIN SELECT * FROM t2 WHERE a = 1 AND b >= 2 AND c >= 2;

DROP TABLE t1, t2;

--echo #
--echo # BUG#11765831: 'RANGE ACCESS' MAY INCORRECTLY FILTER 
--echo #               AWAY QUALIFYING ROWS
--echo #

CREATE TABLE t10(
  K INT NOT NULL AUTO_INCREMENT,
  I INT, J INT,
  PRIMARY KEY(K),
  KEY(I,J)
);
INSERT INTO t10(I,J) VALUES (6,1),(6,2),(6,3),(6,4),(6,5),
                            (6,6),(6,7),(6,8),(6,9),(6,0);

CREATE TABLE t100 LIKE t10;
INSERT INTO t100(I,J) SELECT X.I, X.K+(10*Y.K) FROM t10 AS X,t10 AS Y;

# Insert offending value:
INSERT INTO t100(I,J) VALUES(8,26);

let $query= SELECT * FROM t100 WHERE I <> 6 OR (I <> 8 AND J = 5);

#Verify that 'range' access will be used
--echo
--eval EXPLAIN $query

# Only row 101,8,26 should be returned
--echo
--eval $query

DROP TABLE t10,t100;

--echo #
--echo # BUG#12698916 - JOIN QUERY GIVES WRONG RESULT AT 2ND EXEC. OR
--echo # AFTER FLUSH TABLES [-INT VS NULL]
--echo #
CREATE TABLE t1 (col_int INT, pk INT) ENGINE=InnoDB STATS_PERSISTENT=0;

INSERT INTO t1 VALUES (-100,1),(1,6);

CREATE TABLE t2 (
  col_int_key INT,
  col_varchar VARCHAR(100) NOT NULL DEFAULT "DEFAULT",
  pk INT NOT NULL,
  PRIMARY KEY (pk),
  KEY (col_int_key)
) ENGINE=InnoDB STATS_PERSISTENT=0;

INSERT INTO t2 VALUES
(1,"GOOD",1),(100,"",2),(200,"",3),(300,"",4),(400,"",5),(500,"",8);

let $query=SELECT t1.*,t2.* FROM t1 straight_join t2
ON t2.col_int_key = t1.col_int WHERE t2.pk < t1.pk;

eval EXPLAIN $query;
eval EXPLAIN FORMAT=tree $query;
eval $query;
--echo # need FLUSH so that InnoDB statistics change and thus plan changes
FLUSH TABLES;
eval EXPLAIN $query;
eval EXPLAIN FORMAT=tree $query;  # Tests printing of DynamicRangeIterator.
eval $query;

DROP TABLE t1,t2;

--echo #
--echo # Bug#12694872 - 
--echo # VALGRIND: 18,816 BYTES IN 196 BLOCKS ARE DEFINITELY LOST IN UNIQUE::GET
--echo #

CREATE TABLE t1 (
  pk INTEGER AUTO_INCREMENT,
  col_int_nokey INTEGER NOT NULL,
  col_int_key INTEGER NOT NULL,

  col_date_key DATE NOT NULL,

  col_varchar_key VARCHAR(1) NOT NULL,
  col_varchar_nokey VARCHAR(1) NOT NULL,

  PRIMARY KEY (pk),
  KEY (col_int_key),
  KEY (col_date_key),
  KEY (col_varchar_key, col_int_key)
);

INSERT INTO t1 (
   col_int_key,
   col_int_nokey,
   col_date_key,
   col_varchar_key,
   col_varchar_nokey
) VALUES 
(0, 4, '2011-08-25', 'j', 'j'),
(8, 6, '2004-09-18', 'v', 'v'),
(1, 3, '2009-12-01', 'c', 'c'),
(8, 5, '2004-12-17', 'm', 'm'),
(9, 3, '2000-03-14', 'd', 'd'),
(6, 2, '2006-05-25', 'y', 'y'),
(1, 9, '2008-01-23', 't', 't'),
(6, 3, '2007-06-18', 'd', 'd'),
(2, 8, '2002-10-13', 's', 's'),
(4, 1, '1900-01-01', 'r', 'r'),
(8, 8, '1959-04-25', 'm', 'm'),
(4, 8, '2006-03-09', 'b', 'b'),
(4, 5, '2001-06-05', 'x', 'x'),
(7, 7, '2006-05-28', 'g', 'g'),
(4, 5, '2001-04-19', 'p', 'p'),
(1, 1, '1900-01-01', 'q', 'q'),
(9, 6, '2004-08-20', 'w', 'w'),
(4, 2, '2004-10-10', 'd', 'd'),
(8, 9, '2000-04-02', 'e', 'e')
;

let $query=
SELECT table2.col_date_key AS field1,
       CONCAT ( table2.col_varchar_nokey, table1.col_varchar_nokey ) AS field2
FROM ( t1 AS table1 INNER JOIN t1 AS table2
       ON (( table2.pk <> table1.pk ) AND
           ( table2.pk >= table1.col_int_nokey ) ) )
WHERE ( table1.pk > 226 AND
        table1.pk < ( 226 + 102 ) OR
        ( table1.col_int_key > 226 AND
          table1.col_int_key < ( 226 + 36 ) OR
          ( table1.col_varchar_key <= 'h' OR
            table1.col_int_key > 226 AND
            table1.col_int_key < ( 226 + 227 ) )
        ) 
      )
;

ALTER TABLE t1 DISABLE KEYS;
sorted_result;
eval $query;
ALTER TABLE t1 ENABLE KEYS;

eval CREATE TABLE t2 $query;

sorted_result;
eval SELECT * FROM t2
     WHERE (field1, field2) IN ($query);

DROP TABLE t1, t2;

--echo #
--echo # BUG#12912171 - ASSERTION FAILED: QUICK->HEAD->READ_SET ==
--echo # SAVE_READ_SET
--echo #
CREATE TABLE t1 (
 a INT,
 b INT,
 c INT,
 PRIMARY KEY (c,a), KEY (a),KEY (a)
) ENGINE=INNODB PARTITION BY KEY () PARTITIONS 2;
INSERT INTO t1 VALUES (1,5,1),(2,4,1),(3,3,1),(4,2,1),(5,1,1);
# Disabled by WL#5094
--disable_warnings
UPDATE t1 SET b = 0, c=1 WHERE a <=>0;
--enable_warnings
SELECT * FROM t1;
DROP TABLE t1;

--echo #
--echo # BUG#13256446 - ASSERTION QUICK->HEAD->READ_SET ==
--echo # SAVE_READ_SET' FAILED IN OPT_RANGE.CC:1606 
--echo #
CREATE TABLE t1 (
 f1 INT AUTO_INCREMENT,
 f2 INT,
 f3 INT,
 f4 INT,
 PRIMARY KEY (f1),KEY(f2)
) ENGINE=INNODB;
CREATE ALGORITHM=MERGE VIEW v2 AS SELECT f1,f2,f3,f4 FROM t1 WHERE f2=103;
INSERT INTO t1 VALUES (154,0,NULL,0),(0,NULL,9,0),
 (NULL,102,NULL,3),(0,3,NULL,0), (9,0,NULL,0),(0,9,NULL,157);
SELECT * FROM v2;
UPDATE v2 SET f4=0, f2=NULL, f1=NULL WHERE f1 > 16 ORDER BY f1;
SELECT * FROM v2;
DROP TABLE t1;
DROP VIEW v2;

CREATE TABLE t1 (
 f1 INT AUTO_INCREMENT,
 f2 INT,
 f3 INT,
 f4 INT,
 PRIMARY KEY (f1),KEY(f2)
) ENGINE=INNODB;
INSERT INTO t1 VALUES(1,NULL,NULL,0), (2,2,0,3), (9,0,107,18),
 (10,0,0,0), (231,0,0,0), (232,0,8,0), (234,0,0,NULL), (235,8,0,3);
CREATE ALGORITHM=MERGE VIEW v3 AS SELECT f1,f2,f3,f4 FROM t1 WHERE f1<=85 ;
SELECT * FROM v3;
UPDATE v3 SET f3=0, f4=4 WHERE f2=68 ORDER BY f1; 
SELECT * FROM v3;
DROP TABLE t1;
DROP VIEW v3;

--echo #
--echo # BUG#13731380: RANGE OPTIMIZER CALLS RECORDS_IN_RANGE() FOR OPEN RANGE
--echo #

CREATE TABLE t1 (pk INT PRIMARY KEY);
INSERT INTO t1 VALUES (1),(3),(5);
SELECT * FROM t1 WHERE pk <> 3 OR pk < 4;
DROP TABLE t1;

--echo #
--echo # BUG#13803810: TOO FEW ROWS RETURNED FOR RANGE ACCESS IN 
--echo #               VARCHAR INDEX USING DATETIME VALUE
--echo
CREATE TABLE t1 (a DATETIME);
INSERT INTO t1 VALUES ('2001-01-01 00:00:00');
INSERT INTO t1 VALUES ('2001-01-01 11:22:33');

CREATE TABLE t2 (b VARCHAR(64), KEY (b)) CHARSET utf8mb4;
INSERT INTO t2 VALUES ('2001-01-01');
INSERT INTO t2 VALUES ('2001.01.01');
INSERT INTO t2 VALUES ('2001#01#01');
INSERT INTO t2 VALUES ('2001-01-01 00:00:00');
INSERT INTO t2 VALUES ('2001-01-01 11:22:33');

--echo
--echo # range/ref access cannot be used for this query
--echo
EXPLAIN SELECT * FROM t2 WHERE b=CAST('2001-01-01' AS DATE);
SELECT * FROM t2 WHERE b=CAST('2001-01-01' AS DATE);

let $query_ab=SELECT * FROM t1, t2 WHERE a=b ORDER BY BINARY a, BINARY b;
let $query_ba=SELECT * FROM t1, t2 WHERE b=a ORDER BY BINARY a, BINARY b;

--echo
--echo # range/ref access cannot be used for any of the queries below.
--echo # See BUG#13814468 about 'Range checked for each record'
--echo
eval EXPLAIN $query_ab;
eval $query_ab;
--echo
eval EXPLAIN $query_ba;
eval $query_ba;

--echo
DROP TABLE t1,t2;

--echo #
--echo # WL#7019: Add support for row value constructors in in predicates to
--echo # range optimizer
--echo #
CREATE TABLE t1 (a INT, b INT, c INT, KEY x(a, b));

INSERT INTO t1 VALUES (2, 2, 2), (3, 3, 3), (4, 4, 4), (5, 5, 5), (6, 6, 6),
  (7, 7, 7), (8, 8, 8), (9, 9, 9);

INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;

INSERT INTO t1 VALUES (0, 0, 0), (1, 1, 1);

ANALYZE TABLE t1;

CREATE TABLE t2 (a INT, b INT, c INT, d INT, KEY x(a, b));

INSERT INTO t2 VALUES (2, 2, 2, 2), (3, 3, 3, 3), (4, 4, 4, 4), (5, 5, 5, 5),
  (6, 6, 6, 6), (7, 7, 7, 7), (8, 8, 8, 8), (9, 9, 9, 9);

INSERT INTO t2 SELECT * FROM t2;
INSERT INTO t2 SELECT * FROM t2;
INSERT INTO t2 SELECT * FROM t2;
INSERT INTO t2 SELECT * FROM t2;
INSERT INTO t2 SELECT * FROM t2;
INSERT INTO t2 SELECT * FROM t2;
INSERT INTO t2 SELECT * FROM t2;
INSERT INTO t2 SELECT * FROM t2;
INSERT INTO t2 SELECT * FROM t2;

INSERT INTO t2 VALUES (0, 0, 0, 0), (1, 1, 1, 1);

ANALYZE TABLE t2;

CREATE TABLE t3 (a INT, b INT, c INT, d INT, KEY x(a, b, c));

INSERT INTO t3 VALUES (2, 2, 2, 2), (3, 3, 3, 3), (4, 4, 4, 4), (5, 5, 5, 5),
  (6, 6, 6, 6), (7, 7, 7, 7), (8, 8, 8, 8), (9, 9, 9, 9);

INSERT INTO t3 SELECT * FROM t3;
INSERT INTO t3 SELECT * FROM t3;
INSERT INTO t3 SELECT * FROM t3;
INSERT INTO t3 SELECT * FROM t3;
INSERT INTO t3 SELECT * FROM t3;
INSERT INTO t3 SELECT * FROM t3;
INSERT INTO t3 SELECT * FROM t3;
INSERT INTO t3 SELECT * FROM t3;
INSERT INTO t3 SELECT * FROM t3;

INSERT INTO t3 VALUES (0, 0, 0, 0), (1, 1, 1, 1);

ANALYZE TABLE t3;

EXPLAIN SELECT a, b FROM t1 WHERE (a, b) IN ((NULL, NULL), (NULL, NULL));

let $ERC_query=
SELECT a, b FROM t1 WHERE (a = 0 AND b = 0) OR (a = 1 AND b = 1);
source include/explain_run_count.inc;

let $ERC_query= SELECT a, b FROM t1 WHERE (a, b) IN ((0, 0));
source include/explain_run_count.inc;

let $ERC_query= SELECT a, b FROM t1 WHERE (a, b) IN ((0, 0), (1, 1));
source include/explain_run_count.inc;

let $ERC_query=SELECT a, b, c FROM t2 WHERE (a, b, c) IN ((0, 0, 0), (1, 1, 1));
source include/explain_run_count.inc;

let $ERC_query=SELECT a, b, c FROM t3 WHERE (a, b, c) IN ((0, 0, 0), (1, 1, 1));
source include/explain_run_count.inc;

let $ERC_query= SELECT a, b FROM t1 WHERE a = 0 AND b = 0 OR (a, b) IN ((1, 1));
source include/explain_run_count.inc;

let $ERC_query= SELECT a, b FROM t1 WHERE (a, b) IN ((1, 1)) OR a = 0 AND b = 0;
source include/explain_run_count.inc;

let $ERC_query=
SELECT a, b FROM t2 WHERE (a, b, c) IN ((1, 1, 1)) OR a = 0 AND b = 0 AND c = 0;
source include/explain_run_count.inc;

let $ERC_query=
SELECT a, b FROM t3 WHERE (a, b, c) IN ((1, 1, 1)) OR a = 0 AND b = 0 AND c = 0;
source include/explain_run_count.inc;

let $ERC_query= SELECT a, b FROM t1 WHERE (a, b) IN (((SELECT 1), 1));
source include/explain_run_count.inc;

let $ERC_query= SELECT a, b FROM t1 WHERE (a, b) IN (('0', 0), (1, 1));
source include/explain_run_count.inc;

SET @v = 0;
let $ERC_query= SELECT a, b FROM t1 WHERE (a, b) IN ((@v, 0), (1, 1));
source include/explain_run_count.inc;

CREATE TABLE t4 ( a INT, b INT );

INSERT INTO t4 VALUES (1, 1), (2, 2), (3, 3), (4, 4), (5, 5);
INSERT INTO t4 SELECT a + 5, b + 5 FROM t4;
INSERT INTO t4 SELECT * FROM t4;
INSERT INTO t4 SELECT * FROM t4;
INSERT INTO t4 SELECT * FROM t4;
INSERT INTO t4 SELECT * FROM t4;
INSERT INTO t4 SELECT * FROM t4;
INSERT INTO t4 SELECT * FROM t4;

--echo # Inner table in a nested-loops join
EXPLAIN
SELECT t4.*, t1.a, t1.b
FROM t4 JOIN t1 USING(a, b)
WHERE (t4.a, t4.b) IN ((1, 1), (0, 0));

--echo # Join on IN
EXPLAIN
SELECT t4.*, t1.a, t1.b
FROM t4 JOIN t1
WHERE (t1.a, t1.b) IN ((t4.a, t4.b), (0, 0));

EXPLAIN
SELECT t4.*, t1.a, t1.b
FROM t4 JOIN t1
WHERE (t1.a, t1.b) IN ((t4.a, t4.b), (1, 1));


--echo #
--echo # Tests for non-deterministic functions.
--echo #
CREATE FUNCTION f1() RETURNS INT NOT DETERMINISTIC RETURN 1;

--echo # The statement immediately below should not use range access.
EXPLAIN SELECT a, b FROM t1 WHERE (a, b, c) IN ((0, 0, 0), (f1(), 1, 1));
EXPLAIN SELECT a, b FROM t1 WHERE (a, b, c) IN ((0, 0, 0), (1, f1(), 1));
EXPLAIN SELECT a, b FROM t1 WHERE (a, b, c) IN ((0, 0, 0), (1, 1, f1()));

--echo # The statement immediately below should not use range access.
EXPLAIN SELECT a, b FROM t1 WHERE (a, b) IN ((f1(), 1));

DROP FUNCTION f1;

--echo # The statement immediately below should not use range access.
EXPLAIN
SELECT a, b
FROM t1 AS t1o
WHERE EXISTS (SELECT /*+ NO_SEMIJOIN() */ 1
              FROM t1
              WHERE (a, b) IN ((t1o.a, t1o.b)) );

--echo #
--echo # Tests of dynamic range access
--echo #
CREATE TABLE t5 (a int, b int, KEY (a));
INSERT INTO t5 VALUES (1, 1), (2, 1), (3, 1), (4, 1), (5, 1), (6, 1);

CREATE TABLE t6 (a int, b int, KEY (a));
INSERT INTO t6 VALUES (3, 2), (4, 2), (100, 100), (101, 201), (102, 102);

CREATE TABLE t7 (a int, b int, KEY (a, b));
INSERT INTO t7 VALUES (1, 1), (2, 2), (1000, 1000), (1001, 1001), (1002, 1002),
(1003, 1003), (1004, 1004);

let $ERC_query=
SELECT *
FROM t5 JOIN t6 ON t5.a = t6.a JOIN t7
WHERE t7.a IN (t5.b, t6.b);
source include/explain_run_count.inc;

let $ERC_query=
SELECT *
FROM t5 JOIN t6 ON t5.a = t6.a JOIN t7
WHERE (t7.a, t7.b) IN ((t5.b, 1), (t6.b, 1));
source include/explain_run_count.inc;

--echo #
--echo # Regression tests of the worklog below.
--echo #

--echo # Make sure we process IN predicates only.
--echo # The code around this is very unclear.
EXPLAIN SELECT a, b FROM t1 WHERE (a, b) != (1, 1);

--echo # This should give us a ref scan, as it always did.
EXPLAIN SELECT a, b FROM t1 WHERE (a, b) IN ((0, 0));

EXPLAIN SELECT a, b FROM t1 WHERE (a, c) IN ((0, 0));

--echo # ref scan on x(a).
EXPLAIN SELECT * FROM t1 WHERE (a, c) IN ((0, 0), (1, 1));

DROP TABLE t1, t2, t3, t4, t5, t6, t7;

--echo #
--echo # Bug#17635476: CRASH IN GET_MM_PARTS() OR ASSERT IN
--echo # GET_FUNC_MM_TREE_FROM_IN_PREDICATE()
--echo #
CREATE TABLE t1 (
  a INT,
  b INT,
  KEY (a)
) ENGINE = INNODB;

SELECT DISTINCT a FROM t1 WHERE (a, b) IN ((0, 0), (1, 1));

INSERT INTO t1 VALUES (0, 0), (1, 1), (2, 2), (3, 3), (4, 4), (5, 5), (6, 6),
  (7, 7), (8, 8), (9, 9);
ANALYZE TABLE t1;

CREATE TABLE t2 (
  a INT,
  b INT,
  KEY (a, b)
);

INSERT INTO t2 SELECT * FROM t1;
ANALYZE TABLE t2;

let $ERC_query=
SELECT DISTINCT a FROM t1 WHERE (a, b) IN ((0, 0), (1, 1));
source include/explain_run_count.inc;

let $ERC_query=
SELECT DISTINCT a FROM t2 WHERE (a, b) IN ((0, 0), (1, 1));
source include/explain_run_count.inc;

--echo # Should not use range scan.
EXPLAIN SELECT DISTINCT a FROM t1 WHERE (NULL, b) IN ((0, 0), (1, 1));

--echo # Should not use range scan.
EXPLAIN SELECT DISTINCT a FROM t2 WHERE (NULL, b) IN ((0, 0), (1, 1));

DROP TABLE t1, t2;

--echo #
--echo # Bug#17755540 VALGRIND ERROR WHEN SETTING UP ROW COMPARATORS
--echo #

CREATE TABLE t2 (a INT, b INT, c INT, d INT, KEY x(a, b));

INSERT INTO t2 VALUES (2, 2, 2, 2), (3, 3, 3, 3), (4, 4, 4, 4), (5, 5, 5, 5),
  (6, 6, 6, 6), (7, 7, 7, 7), (8, 8, 8, 8), (9, 9, 9, 9);

INSERT INTO t2 SELECT * FROM t2;

INSERT INTO t2 VALUES (0, 0, 0, 0), (1, 1, 1, 1);

ANALYZE TABLE t2;

SELECT a, b FROM t2 WHERE (a, b) IN ((0, 0), (1, 1));
DROP TABLE t2;

--echo #
--echo # BUG#18364815: OPTIMIZER PREFERS TABLE SCAN WHEN 
--echo #               USING "IN" WITH VALUE OF DIFFERENT TYPE
--echo #

CREATE TABLE t1 (a INT PRIMARY KEY, b INT);

INSERT INTO t1 VALUES (1,1), (2,1), (3,1), (4,1), (5,1), (6,1);

EXPLAIN SELECT * FROM t1 WHERE a IN (1, 2);
EXPLAIN SELECT * FROM t1 WHERE a IN (1, "2");

SELECT * FROM t1 WHERE a IN (1, 2);
SELECT * FROM t1 WHERE a IN (1, "2");

DROP TABLE t1;

--echo # 
--echo # Bug#18715670
--echo # CRASH IN DECIMAL_ACTUAL_FRACTION COMPARING DECIMAL TO NULLS
--echo # 

CREATE TABLE t1(n DECIMAL(39,19) NOT NULL, KEY(n)) engine=innodb;
INSERT INTO t1 SET n=0;
SELECT 1 FROM t1 WHERE n NOT IN(NULL, NULL);
DROP TABLE t1;

--echo #
--echo # Bug#18759597 MISSING ROWS ON WHERE .. 
--echo #              IN QUERY WITH VARIABLES AND CONCAT
--echo #

CREATE TABLE t1 (
  col_varchar_key varchar(2),
  KEY col_varchar_key (col_varchar_key)
) ENGINE=InnoDB;

INSERT INTO t1 VALUES ('n'),('xm');

SET @var1 = 't', @var2 = 'him',
    @var3 = 'n', @var4 = 'n',
    @var5 = 'n', @var6 = 'g',
    @var7 = 'b', @var8 = 'o',
    @var9 = 'm', @var10 = 'xm', @var11 = 'u'
;

SELECT col_varchar_key AS field1
FROM t1
WHERE ( col_varchar_key, col_varchar_key ) IN (
  ('m', @var1  ),
  ('n', @var3  ),
  ('a', @var5  ),
  ('l', @var7  ),
  (CONCAT('x', @var9 ), @var10 )
);

DROP TABLE t1;

--echo #
--echo # Bug#18535226 	DEBUG CRASH ON QUICK_RANGE_SELECT::RESET
--echo #

SET @old_tmp_table_size=@@tmp_table_size;
SET tmp_table_size=1024;

CREATE TABLE t1 (
  pk INT NOT NULL,
  col_int_key INT,
  col_date_key date,
  col_date_nokey date,
  col_time_key time,
  col_time_nokey time,
  col_datetime_key datetime,
  col_datetime_nokey datetime,
  col_varchar_key varchar(1),
  col_varchar_nokey varchar(1),
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key)
) ENGINE=MyISAM;

INSERT INTO t1 VALUES
(5,0,'2001-05-06','2001-05-06','16:21:18','16:21:18','2001-11-08 21:02:12',
'2001-11-08 21:02:12','x','x'),(6,7,'2006-03-03','2006-03-03','18:56:33',
'18:56:33','2003-04-01 00:00:00','2003-04-01 00:00:00','i','i'),
(7,7,'2007-12-28','2007-12-28',NULL,NULL,'1900-01-01 00:00:00',
'1900-01-01 00:00:00','e','e'),(8,1,'2004-10-20','2004-10-20','09:29:08',
'09:29:08','2007-07-12 00:00:00','2007-07-12 00:00:00','p','p'),
(9,7,'2008-04-09','2008-04-09','19:11:10','19:11:10',
'2005-04-04 01:21:01','2005-04-04 01:21:01','s','s'),
(10,1,'2005-12-25','2005-12-25','11:57:26','11:57:26',
'1900-01-01 00:00:00','1900-01-01 00:00:00','j','j');

SELECT alias1.col_int_key
FROM
( SELECT SQ1_alias1.* FROM t1 AS SQ1_alias1, t1 AS SQ1_alias2 ) AS alias1,
(SELECT 7 AS col_int_nokey) AS alias2
WHERE
alias2.col_int_nokey = alias1.pk
AND alias1.col_varchar_nokey < alias1.col_varchar_key
ORDER BY alias1.col_varchar_key;

DROP TABLE t1;
SET tmp_table_size=@old_tmp_table_size;
SET sql_mode = default;

--echo #
--echo # Bug#19585938 Crash in get_full_func_mm_tree with null
--echo #              item_field->table_ref
--echo #

CREATE TABLE t1(id INTEGER, col1 INTEGER, col2 INTEGER, PRIMARY KEY(id));

INSERT INTO t1 VALUES (1,2,3), (3,2,1);

SELECT (SELECT 1
        FROM t1
        WHERE SUM(1) < id
       ) AS c
FROM t1
GROUP BY col1;

SELECT (SELECT 1
        FROM t1
        WHERE id > SUM(1)
       ) AS c
FROM t1
GROUP BY col1;

SELECT (SELECT 1
        FROM t1
        WHERE SUM(1) BETWEEN id AND id+1
       ) AS c
FROM t1
GROUP BY col1;

SELECT (SELECT 1
        FROM t1
        WHERE id BETWEEN SUM(1) AND SUM(5)
       ) AS c
FROM t1
GROUP BY col1;

SELECT (SELECT 1
        FROM t1
        WHERE SUM(1) BETWEEN COUNT(*) AND id
       ) AS c
FROM t1
GROUP BY col1;

DROP TABLE t1;

--echo #
--echo # Bug#21415791 VALGRIND ERROR (CONDITIONAL JUMP) AT KEY_AND
--echo #              (RANGE_OPT_PARAM*, SEL_ARG*, SEL_AR
--echo #
CREATE TABLE t1 (
  col_varchar_10 VARCHAR(10),
  pk INTEGER NOT NULL,
  col_int_key INTEGER,
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key),
  KEY col_varchar_10 (col_varchar_10)
) ENGINE=InnoDB;

INSERT INTO t1 (
pk, col_varchar_10, col_int_key) VALUES
(1, 'ttttt', 0), (2, 'zzzzz', 0), (3, 'ggggg', 0),
(4, 'hhhhh', 0), (5, 'kkkkk', 0), (6, 'lllll', 0);

CREATE TABLE t2 (
  pk INTEGER NOT NULL,
  col_varchar_10 VARCHAR(10),
  PRIMARY KEY (pk),
  KEY col_varchar_10 (col_varchar_10)
) ENGINE=InnoDB;

INSERT INTO t2 (
pk,  col_varchar_10) VALUES
(1, '00000'), (2, '00000'), (3, '44444'), (4, '00000'),
(5, NULL), (6, NULL), (7, NULL);

SELECT COUNT(t1.col_int_key)
FROM t2 RIGHT OUTER JOIN t1 ON t2.col_varchar_10 <= t1.col_varchar_10
WHERE t2.pk <> 4 OR t2.pk != t1.col_int_key AND t2.pk <> 1000;

DROP TABLE t1, t2;

--echo #
--echo # Bug #20229614: OR CONDITIONS ON MULTI-COLUMN INDEX MAY NOT USE ALL
--echo #                INDEX COLUMNS TO FILTER ROWS
--echo #

CREATE TABLE t1 (
  c1 INT,
  c2 INT,
  c3 INT,
  PRIMARY KEY(c1, c2, c3)
) ENGINE=INNODB;

INSERT INTO t1 VALUES (1, 1, 1), (1, 1, 2), (1, 1, 3),
                      (1, 1, 4), (1, 1, 5);
INSERT INTO t1 SELECT c1, 2, c3 FROM t1;
INSERT INTO t1 SELECT c1, 3, c3 FROM t1 WHERE c2 = 1;

ANALYZE TABLE t1;

SELECT COUNT(*) FROM t1;

EXPLAIN SELECT c1, c2, c3
        FROM t1
        WHERE (c1 = 1 AND c2 = 1 AND c3 = 1) OR
              (c1 = 1 AND c2 = 2 AND c3 = 2) OR
              (c1 = 1 AND c2 = 2 AND c3 = 3);
DROP TABLE t1;


--echo #
--echo # Bug#21139683: ASSERTION FAILED: TYPE_ARG == MAYBE_KEY ||
--echo #               TYPE_ARG == IMPOSSIBLE
--echo #

CREATE TABLE t1 (
  a BLOB,
  PRIMARY KEY(a(1)),
  KEY(a(1))
) ENGINE=INNODB;

SELECT 1 FROM t1 WHERE a <> 'a' OR a <> "";

DROP TABLE t1;

--echo #
--echo # Bug#19333852: RESULT DIFF IN QUERY HAVING DISTINCT
--echo #               WITH GROUP BY
--echo #

CREATE TABLE t1 (
  v1 VARCHAR(20) CHARACTER SET utf8 NOT NULL,
  pk INTEGER NOT NULL,
  PRIMARY KEY (pk),
  KEY v1_key (v1(10))
) ENGINE=InnoDB;

INSERT INTO t1 VALUES ('ABCDE',19), ('JLVGO',14);

Let query1= SELECT * FROM t1 WHERE t1.v1 < CHAR(128);
Let query2= SELECT * FROM t1 WHERE t1.v1 = CHAR(128);
Let query3= SELECT * FROM t1 WHERE t1.v1 BETWEEN 'f' AND CHAR(128);

eval EXPLAIN $query1;
eval EXPLAIN $query2;
eval EXPLAIN $query3;

eval $query1;
eval $query2;
eval $query3;

DROP TABLE t1;

--echo #
--echo # Bug #21761867: ASSERTION `TYPE_ARG == MAYBE_KEY ||
--echo #                TYPE_ARG == IMPOSSIBLE' FAILED.
--echo #

CREATE TABLE t1 (
  c10 INT NOT NULL,
  c12 INT NOT NULL,
  c18 INT,
  PRIMARY KEY (c10,c12),
  UNIQUE KEY key_c12(c12),
  KEY key_c18(c18));

INSERT INTO t1 VALUES(11,5,0), (12,6,1), (13,7,2), (14,8,3), (15,9,4);

CREATE TABLE t2 (
  c10 INT NOT NULL,
  c12 INT NOT NULL,
  PRIMARY KEY(c10,c12));

CREATE TABLE t3 (c10 INT NOT NULL);

SELECT t2.c10
FROM t1
     RIGHT JOIN t3
     LEFT JOIN t2
     ON t3.c10 = t2.c10
     ON t1.c12 > t2.c12
WHERE
     t1.c10 <= 25
     AND
     t1.c18 IS NOT NULL
     OR
     t1.c10 > 5
     AND
     t1.c18 IN (15,16,18);

DROP TABLE t1, t2, t3;

--echo #
--echo # Bug #21318711: WRONG RESULTS FOR TRUNCATED COLUMN AND AGGREGATION
--echo #

CREATE TABLE t1 (
  col1 VARCHAR(5),
  col2 INT NOT NULL,
  PRIMARY KEY (col1, col2)
) ENGINE=InnoDB, CHARSET utf8mb4;

INSERT INTO t1 VALUES ('abcde', 10);

let $query1= SELECT MAX(col2) FROM t1 WHERE col1 = 'abcdeaa';
let $query2= SELECT MAX(col2) FROM t1 WHERE col1 = 'abcde  ';

eval EXPLAIN $query1;
eval EXPLAIN $query2;

eval $query1;
eval $query2;

DROP TABLE t1;

--echo #
--echo # Bug# 22283790: RANGE OPTIMIZER UTILIZES TOO MUCH MEMORY WITH
--echo #                MANY OR CONDITIONS
--echo #

CREATE TABLE t1 (
 f1 INTEGER,
 KEY (f1)
);

INSERT INTO t1 VALUES (1),(2),(3),(4),(5),(6),(7),(8),(9),(10),(11),(12),(13),(14),(15);

SET @orig_range_optimizer_max_mem_size= @@range_optimizer_max_mem_size;
SET range_optimizer_max_mem_size= 5000;

EXPLAIN SELECT * FROM t1 WHERE f1=1 OR f1=2 OR f1=3 OR f1=4 OR f1=5
OR f1=6 OR f1=7 OR f1=8 OR f1=9 OR f1=10 OR f1=11;

SET range_optimizer_max_mem_size= @orig_range_optimizer_max_mem_size;

DROP TABLE t1;

--echo #
--echo # Bug #23623110: REFACTOR USE_COUNT IN THE RANGE OPTIMIZER
--echo #
--echo # Tests deleting nodes in tree_delete() not from the end of the
--echo # linked list.
--echo #

CREATE TABLE t1 (
 pk INTEGER,
 PRIMARY KEY (pk)
);

SELECT * FROM t1 WHERE pk IN (3, 8) OR pk NOT IN (2);

DROP TABLE t1;

--echo #
--echo # Bug #23623110: REFACTOR USE_COUNT IN THE RANGE OPTIMIZER
--echo #
--echo # Tests deleting nodes in tree_delete() not from the end of the
--echo # linked list.
--echo #
--echo # Indirectly tests ORing with a tree that has a nonzero refcount,
--echo # where nodes are also being skipped over. (There was a bug in
--echo # refcounting such trees earlier.)
--echo #

CREATE TABLE t1 (
 col_int_key int,
 pk int NOT NULL,
 col_int int,
 KEY test_idx (pk,col_int_key,col_int)
);

SELECT * FROM t1 WHERE
    (col_int_key >= 6 AND pk > 6) OR
    (pk > 2 AND col_int_key = 7 AND col_int > 6);

DROP TABLE t1;

--echo #

--echo #
--echo # Bug #23259872: OPTIMIZER CHOOSES TO USE NON PRIMARY
--echo #                INDEX, EVEN THOUGH COST IS HIGHER
--echo #

CREATE TABLE `giant_table` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `one_id` int(11) NOT NULL,
  `other_id` bigint(20) NOT NULL DEFAULT '0',
  `some_other_id` int(11) DEFAULT 0 NOT NULL,
  `something` double NOT NULL DEFAULT '0',
  `comment` text COLLATE utf8_unicode_ci,
  `flags` int(11) NOT NULL DEFAULT '0',
  `time_created` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `time_created` (`time_created`),
  KEY `some_other_id` (`some_other_id`),
  KEY `one_other_idx` (`one_id`,`other_id`),
  KEY `other_id` (`other_id`,`time_created`)
) ENGINE=InnoDB AUTO_INCREMENT=101651329
DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci ;

CREATE TABLE t1 (c1 INT);
INSERT INTO t1 VALUES (66136540), (68983250), (89627210), (77869520),
                      (82543190), (67538270), (77282760), (77908170),
                      (70923370), (68066360);
DELIMITER $;
CREATE PROCEDURE p()
BEGIN
SET @x = 1;
REPEAT

INSERT INTO giant_table(id,one_id)
  SELECT c1 + @x, 0
  FROM t1
  WHERE c1 IN (66136540, 68985250, 89627210, 77869520 , 82543190, 67538270,
               77282760, 77908170, 70923370, 68066360);
SET @x =  @x + 1;

UNTIL @x > 30 END REPEAT;
END $
DELIMITER ;$

CALL p();
SELECT count(*) FROM giant_table;

INSERT INTO giant_table (id,one_id) VALUES (66136539, 0), (68983258,1),
                                           (89628210,1), (77869520,2);
INSERT INTO giant_table (id,one_id, some_other_id) VALUES(84673401, 0, 1),
                                                         (61069031, 1, 1);

EXPLAIN SELECT id, something, comment, time_created, one_id, other_id,
               some_other_id, flags
FROM giant_table
WHERE id IN (66136539, 68983258, 89628210, 77869520, 82543198, 67538272,
             84673401, 61069031, 68214385, 77282865, 76991297, 64569216,
             89481638, 74534074, 70396537, 80076375, 63308530, 77908270,
             70923271, 68066180)
  AND (giant_table.flags & 0x01) = 0 AND giant_table.some_other_id = 0;

DROP PROCEDURE p;
DROP TABLE giant_table, t1;

--echo #
--echo # Bug#22671573: SIG11 IN SEL_ARG::RB_INSERT |SQL/OPT_RANGE.CC
--echo #
--echo # Tests creating an empty tree (impossible condition) and then ANDing with it.
--echo #

CREATE TABLE t1 (
 a INT,
 b INT,
 KEY test_idx (b, a)
);

SELECT * FROM t1 WHERE b BETWEEN 8 AND 8 AND a > 8 AND a <= 8;

DROP TABLE t1;

--echo #
--echo # Indirectly tests inserting a node into an empty (impossible) tree.
--echo #

CREATE TABLE t1 (
 pk INT,
 PRIMARY KEY ( pk )
);

SELECT * FROM t1 WHERE pk = 6 OR pk > 6;

DROP TABLE t1;

--echo #
--echo # Indirectly tests ORing with an empty (impossible) tree.
--echo #

CREATE TABLE t1 (
  col_int_key INT,
  KEY (col_int_key)
);

SELECT * FROM t1 WHERE col_int_key IS NULL OR col_int_key < 9 OR col_int_key = 9;

DROP TABLE t1;

--echo #
--echo # Indirectly tests deleting the last node from a tree
--echo # (which makes it IMPOSSIBLE) and then inserting more than one node.
--echo #

CREATE TABLE t1 (
  a VARCHAR(64),
  KEY a (a)
);

SELECT * FROM t1 WHERE a IS NULL OR a NOT IN ( 'foo', 'bar' );

DROP TABLE t1;

--echo #
--echo # Bug#25229315: ASSERTION TABLE->M_RECORD_BUFFER.RECORD_SIZE() ==
--echo #               RECORD_PREFIX_SIZE(TAB)' FAIL
--echo #

CREATE TABLE t1 (pk INT PRIMARY KEY, vc VARCHAR(1)) ENGINE=InnoDB;

INSERT INTO t1 VALUES (1, NULL), (2, 'w');

# The primary key needs to come after the other columns accessed by
# the query in order to reproduce the bug.
CREATE TABLE t2 (
  i INT,
  vc VARCHAR(1),
  pk INT PRIMARY KEY,
  KEY (i),
  KEY (vc)
) ENGINE=InnoDB;

INSERT INTO t2 VALUES (1, 'p', 1), (9, 'w', 2), (NULL, 'r', 3);

ANALYZE TABLE t1, t2;

# The query should use dynamic ranges on t2 ("Range checked for each record").
--let $query= SELECT MIN(t1.pk) FROM t1, t2 WHERE t2.vc = t1.vc OR t2.i = t1.pk
--eval EXPLAIN $query
--eval $query

DROP TABLE t1, t2;

--echo #
--echo # Bug #26727773: OPTIMIZER CHOSES COMPOSITE INDEX FOR REF OVER RANGE
--echo #

CREATE TABLE transactions (
 app_trans_id INT DEFAULT NULL,
 id INT NOT NULL,
 tbl INT NOT NULL DEFAULT 1,
 created TIMESTAMP NOT NULL DEFAULT '2017-01-01 01:01:01',
 trans_type INT NOT NULL,
 description BLOB,
 source_lvl1 INT DEFAULT NULL,
 source_lvl2 INT DEFAULT NULL,
 KEY tbl_id_idx (tbl,id),
 KEY created_idx (created),
 KEY trans_type_created_idx (trans_type,created),
 KEY app_trans_id_idx (app_trans_id)
) ENGINE=INNODB ;

CREATE TABLE t1 (c1 INT);
INSERT INTO t1 VALUES (1), (1000), (2000), (3000), (4000), (5000), (6000),
       (7000), (8000), (9000);

DELIMITER $;
CREATE PROCEDURE p()
BEGIN

SET @x = 1;
REPEAT

INSERT IGNORE INTO transactions(id,trans_type, description)
  SELECT c1 + @x, @x , 'abcd'
  FROM t1;

SET @x =  @x + 1;

UNTIL @x > 300 END REPEAT;
END $
DELIMITER ;$

CALL p();
SELECT count(*) FROM transactions;

#The qualifying row.
INSERT IGNORE INTO transactions(id,trans_type, description, created)
  SELECT 3, 3 , 'abcd', '2018-01-01 01:01:01'
  FROM dual;

let query1=
SELECT '2017-10-23 01:01:01', HOUR(created), source_lvl1, source_lvl2,
       COUNT(DISTINCT(app_trans_id))
FROM transactions
WHERE created > '2017-10-23 01:01:01' AND tbl = 1 AND trans_type in (3)
GROUP BY '2017-10-23 01:01:01', HOUR(created), source_lvl1, source_lvl2;


#UPDATE stats to produce a ref-access plan on 'trans_type_created_idx'
--disable_query_log
ALTER TABLE transactions STATS_PERSISTENT=1, STATS_AUTO_RECALC=0;

UPDATE mysql.innodb_index_stats
SET stat_value=3001
WHERE index_name ='trans_type_created_idx' AND TABLE_name='transactions';

UPDATE mysql.innodb_table_stats SET n_rows=3001 WHERE TABLE_name='transactions';

FLUSH TABLE transactions;
--enable_query_log

--replace_column 10 # 11 #
eval EXPLAIN $query1;

SET optimizer_trace="enabled=on";
eval $query1;
SELECT TRACE INTO @trace FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;
SELECT @trace RLIKE "range_uses_more_keyparts";
SET optimizer_trace="enabled=off";

DROP PROCEDURE p;
DROP TABLE t1, transactions;

--echo #
--echo # Bug #28086754: OPTIMIZER SKIP THE RANG SCAN ON SECOND COLUMN IN A
--echo #                 COMPOSITE INDEX
--echo #

CREATE TABLE test_ref (
   a INT PRIMARY KEY,
   b VARCHAR(20),
   c VARCHAR(20) DEFAULT NULL,
   d VARCHAR(3) DEFAULT NULL,
   id INT DEFAULT NULL,
   KEY idx1 (id, c),
   KEY idx2 (id, d)) ENGINE=INNODB ;

--disable_query_log
INSERT INTO test_ref VALUES
(   27699, 'D007', 'gheennse', 'S',    7),
(   34876, 'D003', 'gheennse', 'S',    3),
(   70714, 'D007', 'gheennse', 'S',    7),
(   71945, 'D005', 'gheennse', 'S',    5),
(   95929, 'D003', 'gheennse', 'D',    3),
(  129342, 'D002', 'gheennse', 'S',    2),
(  146471, 'D003', 'gheennse', 'S',    3),
(  186302, 'D003', 'gheennse', 'S',    3),
(  248098, 'D003', 'gheennse', 'S',    3),
(  308317, 'D003', 'gheennse', 'D',    3),
(  317360, 'D005', 'gheennse', 'D',    5),
(  444176, 'D003', 'gheennse', 'A',    3),
(  460871, 'D003', 'gheennse', 'D',    3),
(  462396, 'D002', 'gheennse', 'S',    2),
(  477701, 'D007', 'gheennse', 'D',    7),
(  503689, 'D003', 'gheennse', 'S',    3),
(  505492, 'D003', 'gheennse', 'DD',   3),
(  516238, 'D003', 'gheennse', 'S',    3),
(  534333, 'D002', 'gheennse', 'D',    2),
(  565439, 'D007', 'gheennse', 'D',    7),
(  577221, 'D002', 'gheennse', 'S',    2),
(  633762, 'D002', 'gheennse', 'S',    2),
(  670506, 'D002', 'gheennse', 'D',    2),
(  676921, 'D007', 'gheennse', 'D',    7),
(  680607, 'D002', 'gheennse', 'D',    2),
(  694397, 'D003', 'gheennse', 'S',    3),
(  714547, 'D003', 'gheennse', 'S',    3),
(  717856, 'D003', 'gheennse', 'D',    3),
(  808061, 'D003', 'gheennse', 'S',    3),
(  864526, 'D003', 'gheennse', 'S',    3),
(  890043, 'D007', 'gheennse', 'S',    7),
(  910633, 'D003', 'gheennse', 'S',    3),
(  948201, 'D002', 'gheennse', 'S',    2),
(  949220, 'D005', 'gheennse', 'S',    5),
(  960404, 'D003', 'gheennse', 'S',    3),
( 1092448, 'D007', 'gheennse', 'D',    7),
( 1104681, 'D002', 'gheennse', 'S',    2),
( 1123559, 'D003', 'gheennse', 'S',    3),
( 1129004, 'D003', 'gheennse', 'S',    3),
( 1158830, 'D005', 'gheennse', 'D',    5),
( 1181872, 'D003', 'gheennse', 'S',    3),
( 1244160, 'D005', 'gheennse', 'S',    5),
( 1248290, 'D005', 'gheennse', 'D',    5),
( 1271990, 'D005', 'gheennse', 'S',    5),
( 1292696, 'D003', 'gheennse', 'S',    3),
( 1334421, 'D002', 'gheennse', 'S',    2),
( 1386182, 'D007', 'gheennse', 'S',    7),
( 1432466, 'D007', 'gheennse', 'D',    7),
( 1604224, 'D003', 'gheennse', 'S',    3),
( 1635704, 'D007', 'gheennse', 'S',    7),
( 1762832, 'D003', 'gheennse', 'D',    3),
( 1770649, 'D003', 'gheennse', 'S',    3),
( 1896320, 'D005', 'gheennse', 'S',    5),
( 1946091, 'D007', 'gheennse', 'S',    7),
( 1955807, 'D007', 'gheennse', 'S',    7),
( 1981281, 'D002', 'gheennse', 'D',    2),
( 1984746, 'D002', 'gheennse', 'S',    2),
( 2011617, 'D002', 'gheennse', 'S',    2),
( 2021745, 'D005', 'gheennse', 'D',    5),
( 2058339, 'D002', 'cheennse', 'D',    2),
( 2088903, 'D002', 'cheennse', 'S',    2),
( 2170936, 'D003', 'cheennse', 'S',    3),
( 2191014, 'D007', 'cheennse', 'S',    7),
( 2359396, 'D003', 'cheennse', 'D',    3),
( 2392181, 'D003', 'cheennse', 'D',    3),
( 2509548, 'D002', 'cheennse', 'D',    2),
( 2524317, 'D002', 'cheennse', 'S',    2),
( 2532146, 'D003', 'cheennse', 'S',    3),
( 2554387, 'D003', 'cheennse', 'S',    3),
( 2615723, 'D003', 'cheennse', 'D',    3),
( 2769368, 'D007', 'cheennse', 'S',    7),
( 2788698, 'D002', 'cheennse', 'D',    2),
( 2900141, 'D003', 'cheennse', 'D',    3),
( 2916874, 'D003', 'cheennse', 'S',    3),
( 2917505, 'D005', 'cheennse', 'D',    5),
( 3049543, 'D007', 'cheennse', 'D',    7),
( 3054225, 'D005', 'cheennse', 'S',    5),
( 3127100, 'D005', 'cheennse', 'S',    5),
( 3231967, 'D003', 'cheennse', 'DD',   3),
( 3239080, 'D005', 'cheennse', 'DD',   5),
( 3245821, 'D003', 'cheennse', 'DD',   3),
( 3492641, 'D003', 'cheennse', 'DD',   3),
( 3502099, 'D003', 'cheennse', 'S',    3),
( 3502400, 'D005', 'cheennse', 'DD',   5),
( 3652408, 'D003', 'cheennse', 'DD',   3),
( 3671827, 'D003', 'cheennse', 'S',    3),
( 3739085, 'D007', 'cheennse', 'S',    7),
( 3856727, 'D007', 'cheennse', 'S',    7),
( 3871123, 'D003', 'cheennse', 'S',    3),
( 3955047, 'D002', 'cheennse', 'DD',   2),
( 3995249, 'D003', 'cheennse', 'S',    3),
( 3996816, 'D002', 'cheennse', 'S',    2),
( 4037006, 'D003', 'cheennse', 'S',    3),
( 4040042, 'D003', 'cheennse', 'S',    3),
( 4126972, 'D003', 'cheennse', 'S',    3),
( 4163290, 'D005', 'cheennse', 'DD',   5),
( 4170771, 'D002', 'cheennse', 'DD',   2),
( 4209586, 'D003', 'cheennse', 'S',    3),
( 4267486, 'D003', 'cheennse', 'S',    3),
( 4317590, 'D005', 'cheennse', 'S',    5),
( 4318938, 'D002', 'cheennse', 'S',    2),
( 4363730, 'D007', 'cheennse', 'D',    7),
( 4455095, 'D005', 'cheennse', 'S',    5);
--enable_query_log
ANALYZE TABLE t1;

let query1=
SELECT *
FROM test_ref
WHERE id=3 AND c LIKE 'gh%'
ORDER BY c
LIMIT 1;

# test with descending order
let query2=
SELECT *
FROM test_ref
WHERE id=3 AND c LIKE 'gh%'
ORDER BY c DESC
LIMIT 1;

eval EXPLAIN $query1;
eval $query1;

eval EXPLAIN $query2;
eval $query2;

DROP TABLE test_ref;
