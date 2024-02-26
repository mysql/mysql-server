#
# Bug with order by
#

--disable_warnings
drop table if exists t1,t2,t3;
--enable_warnings
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
CREATE TABLE t1 (
  id int(6) DEFAULT '0' NOT NULL,
  idservice int(5),
  clee char(20) NOT NULL,
  flag char(1),
  KEY id (id),
  PRIMARY KEY (clee)
);


INSERT INTO t1 VALUES (2,4,'6067169d','Y');
INSERT INTO t1 VALUES (2,5,'606716d1','Y');
INSERT INTO t1 VALUES (2,1,'606717c1','Y');
INSERT INTO t1 VALUES (3,1,'6067178d','Y');
INSERT INTO t1 VALUES (2,6,'60671515','Y');
INSERT INTO t1 VALUES (2,7,'60671569','Y');
INSERT INTO t1 VALUES (2,3,'dd','Y');

CREATE TABLE t2 (
  id int(6) NOT NULL auto_increment,
  description varchar(40) NOT NULL,
  idform varchar(40),
  ordre int(6) unsigned DEFAULT '0' NOT NULL,
  image varchar(60),
  PRIMARY KEY (id),
  KEY id (id,ordre)
);

#
# Dumping data for table 't2'
#

INSERT INTO t2 VALUES (1,'Emettre un appel d''offres','en_construction.html',10,'emettre.gif');
INSERT INTO t2 VALUES (2,'Emettre des soumissions','en_construction.html',20,'emettre.gif');
INSERT INTO t2 VALUES (7,'Liste des t2','t2_liste_form.phtml',51060,'link.gif');
INSERT INTO t2 VALUES (8,'Consulter les soumissions','consulter_soumissions.phtml',200,'link.gif');
INSERT INTO t2 VALUES (9,'Ajouter un type de materiel','typeMateriel_ajoute_form.phtml',51000,'link.gif');
INSERT INTO t2 VALUES (10,'Lister/modifier un type de materiel','typeMateriel_liste_form.phtml',51010,'link.gif');
INSERT INTO t2 VALUES (3,'Créer une fiche de client','clients_ajoute_form.phtml',40000,'link.gif');
INSERT INTO t2 VALUES (4,'Modifier des clients','en_construction.html',40010,'link.gif');
INSERT INTO t2 VALUES (5,'Effacer des clients','en_construction.html',40020,'link.gif');
INSERT INTO t2 VALUES (6,'Ajouter un service','t2_ajoute_form.phtml',51050,'link.gif');


select t1.id,t1.idservice,t2.ordre,t2.description  from t1, t2 where t1.id = 2   and t1.idservice = t2.id  order by t2.ordre;
 
drop table t1,t2;

#
# Test of ORDER BY on concat() result
#

create table t1 (first char(10),last char(10));
insert into t1 values ("Michael","Widenius");
insert into t1 values ("Allan","Larsson");
insert into t1 values ("David","Axmark");
select concat(first," ",last) as name from t1 order by name;
select concat(last," ",first) as name from t1 order by name;
drop table t1;

#
# bug in distinct + order by
#

create table t1 (i int);
insert into t1 values(1),(2),(1),(2),(1),(2),(3);
select distinct i from t1;
--sorted_result  # By nature nondeterministic.
select distinct i from t1 order by rand(5);
select distinct i from t1 order by i desc;
select distinct i from t1 order by 1-i;
select distinct i from t1 order by mod(i,2),i;
drop table t1;

#
# bug#3681
#

create table t1 ( pk     int primary key, name   varchar(255) not null, number varchar(255) not null);
insert into t1 values (1, 'Gamma',     '123'), (2, 'Gamma Ext', '123a'), (3, 'Alpha',     '001'), (4, 'Beta',      '200c');
select distinct t1.name as 'Building Name',t1.number as 'Building Number' from t1 order by t1.name asc;
drop table t1;


#
# Order by on first index part
#

create table t1 (id int not null,col1 int not null,col2 int not null,index(col1));
insert into t1 values(1,2,2),(2,2,1),(3,1,2),(4,1,1),(5,1,4),(6,2,3),(7,3,1),(8,2,4);
select * from t1 order by col1,col2;
select col1 from t1 order by id;
select col1 as id from t1 order by id;
select concat(col1) as id from t1 order by id;
drop table t1;

#
# Test of order by on field()
#

CREATE TABLE t1 (id int auto_increment primary key,aika varchar(40),aikakentta  timestamp);
insert into t1 (aika) values ('Keskiviikko');
insert into t1 (aika) values ('Tiistai');
insert into t1 (aika) values ('Maanantai');
insert into t1 (aika) values ('Sunnuntai');

SELECT FIELD(SUBSTRING(t1.aika,1,2),'Ma','Ti','Ke','To','Pe','La','Su') AS test FROM t1 ORDER by test;
drop table t1;

#
# Test of ORDER BY on IF
#

CREATE TABLE t1
(
  a          int unsigned       NOT NULL,
  b          int unsigned       NOT NULL,
  c          int unsigned       NOT NULL,
  UNIQUE(a),
  INDEX(b),
  INDEX(c)
);

CREATE TABLE t2
(
  c          int unsigned       NOT NULL,
  i          int unsigned       NOT NULL,
  INDEX(c)
);

CREATE TABLE t3
(
  c          int unsigned       NOT NULL,
  v          varchar(64),
  INDEX(c)
);

INSERT INTO t1 VALUES (1,1,1);
INSERT INTO t1 VALUES (2,1,2);
INSERT INTO t1 VALUES (3,2,1);
INSERT INTO t1 VALUES (4,2,2);
INSERT INTO t2 VALUES (1,50);
INSERT INTO t2 VALUES (2,25);
INSERT INTO t3 VALUES (1,'123 Park Place');
INSERT INTO t3 VALUES (2,'453 Boardwalk');

--sorted_result
SELECT    a,b,if(b = 1,i,if(b = 2,v,''))
FROM      t1
LEFT JOIN t2 USING(c)
LEFT JOIN t3 ON t3.c = t1.c;

--sorted_result
SELECT    a,b,if(b = 1,i,if(b = 2,v,''))
FROM      t1
LEFT JOIN t2 ON t1.c = t2.c
LEFT JOIN t3 ON t3.c = t1.c;

--sorted_result
SELECT    a,b,if(b = 1,i,if(b = 2,v,''))
FROM      t1
LEFT JOIN t2 USING(c)
LEFT JOIN t3 ON t3.c = t1.c
ORDER BY a;

--sorted_result
SELECT    a,b,if(b = 1,i,if(b = 2,v,''))
FROM      t1
LEFT JOIN t2 ON t1.c = t2.c
LEFT JOIN t3 ON t3.c = t1.c
ORDER BY a;

drop table t1,t2,t3;

#
# Test of ORDER BY (Bug found by Dean Edmonds)
#

create table t1 (ID int not null primary key, TransactionID int not null);
insert into t1 (ID, TransactionID) values  (1,  87), (2,  89), (3,  92), (4,  94), (5,  486), (6,  490), (7,  753), (9,  828), (10, 832), (11, 834), (12, 840);
create table t2 (ID int not null primary key, GroupID int not null);
 insert into t2 (ID, GroupID) values (87,  87), (89,  89), (92,  92), (94,  94), (486, 486), (490, 490),(753, 753), (828, 828), (832, 832), (834, 834), (840, 840);
create table t3 (ID int not null primary key, DateOfAction date not null);
insert into t3 (ID, DateOfAction) values  (87,  '1999-07-19'), (89,  '1999-07-19'), (92,  '1999-07-19'), (94,  '1999-07-19'), (486, '1999-07-18'), (490, '2000-03-27'), (753, '2000-03-28'), (828, '1999-07-27'), (832, '1999-07-27'),(834, '1999-07-27'), (840, '1999-07-27');
select t3.DateOfAction, t1.TransactionID from t1 join t2 join t3 where t2.ID = t1.TransactionID and t3.ID = t2.GroupID order by t3.DateOfAction, t1.TransactionID; 
select t3.DateOfAction, t1.TransactionID from t1 join t2 join t3 where t2.ID = t1.TransactionID and t3.ID = t2.GroupID order by t1.TransactionID,t3.DateOfAction; 
drop table t1,t2,t3;

#bug reported by Wouter de Jong

CREATE TABLE t1 (
  member_id int(11) NOT NULL auto_increment,
  inschrijf_datum varchar(20) NOT NULL default '',
  lastchange_datum varchar(20) NOT NULL default '',
  nickname varchar(20) NOT NULL default '',
  password varchar(8) NOT NULL default '',
  voornaam varchar(30) NOT NULL default '',
  tussenvoegsels varchar(10) NOT NULL default '',
  achternaam varchar(50) NOT NULL default '',
  straat varchar(100) NOT NULL default '',
  postcode varchar(10) NOT NULL default '',
  wijk varchar(40) NOT NULL default '',
  plaats varchar(50) NOT NULL default '',
  telefoon varchar(10) NOT NULL default '',
  geboortedatum date NOT NULL default '0000-00-00',
  geslacht varchar(5) NOT NULL default '',
  email varchar(80) NOT NULL default '',
  uin varchar(15) NOT NULL default '',
  homepage varchar(100) NOT NULL default '',
  internet varchar(15) NOT NULL default '',
  scherk varchar(30) NOT NULL default '',
  favo_boek varchar(50) NOT NULL default '',
  favo_tijdschrift varchar(50) NOT NULL default '',
  favo_tv varchar(50) NOT NULL default '',
  favo_eten varchar(50) NOT NULL default '',
  favo_muziek varchar(30) NOT NULL default '',
  info text NOT NULL default '',
  ipnr varchar(30) NOT NULL default '',
  PRIMARY KEY  (member_id)
) charset utf8mb4 ENGINE=MyISAM PACK_KEYS=1;

insert into t1 (member_id) values (1),(2),(3);
select member_id, nickname, voornaam FROM t1
ORDER by lastchange_datum DESC LIMIT 2;
drop table t1;

#
# Test optimization of ORDER BY DESC
#

create table t1 (a int not null, b int, c varchar(10), key (a, b, c)) charset utf8mb4;
insert into t1 values (1, NULL, NULL), (1, NULL, 'b'), (1, 1, NULL), (1, 1, 'b'), (1, 1, 'b'), (2, 1, 'a'), (2, 1, 'b'), (2, 2, 'a'), (2, 2, 'b'), (2, 3, 'c'),(1,3,'b');
ANALYZE TABLE t1;
explain select * from t1 where (a = 1 and b is null and c = 'b') or (a > 2) order by a desc;
select * from t1 where (a = 1 and b is null and c = 'b') or (a > 2) order by a desc;
explain select * from t1 where a >= 1 and a < 3 order by a desc;
--skip_if_hypergraph  # Depends on the query plan.
explain format=json select * from t1 where a >= 1 and a < 3 order by a desc;
--partially_sorted_result 1
select * from t1 where a >= 1 and a < 3 order by a desc;
explain select * from t1 where a = 1 order by a desc, b desc;
--partially_sorted_result 2
select * from t1 where a = 1 order by a desc, b desc;
explain select * from t1 where a = 1 and b is null order by a desc, b desc;
--partially_sorted_result 2
select * from t1 where a = 1 and b is null order by a desc, b desc;
explain select * from t1 where a >= 1 and a < 3 and b >0 order by a desc,b desc;
explain select * from t1 where a = 2 and b >0 order by a desc,b desc;
explain select * from t1 where a = 2 and b is null order by a desc,b desc;
explain select * from t1 where a = 2 and (b is null or b > 0) order by a
desc,b desc;
explain select * from t1 where a = 2 and b > 0 order by a desc,b desc;
explain select * from t1 where a = 2 and b < 2 order by a desc,b desc;
explain select * from t1 where a = 1 order by b desc;
--partially_sorted_result 2
select * from t1 where a = 1 order by b desc;
#
# Test things when we don't have NULL keys
#

alter table t1 modify b int not null, modify c varchar(10) not null;
ANALYZE TABLE t1;
explain select * from t1 order by a, b, c;
select * from t1 order by a, b, c;
explain select * from t1 order by a desc, b desc, c desc;
select * from t1 order by a desc, b desc, c desc;
# test multiple ranges, NO_MAX_RANGE and EQ_RANGE
explain select * from t1 where (a = 1 and b = 1 and c = 'b') or (a > 2) order by a desc;
select * from t1 where (a = 1 and b = 1 and c = 'b') or (a > 2) order by a desc;
# test NEAR_MAX, NO_MIN_RANGE
explain select * from t1 where a < 2 and b <= 1 order by a desc, b desc;
--partially_sorted_result 2
select * from t1 where a < 2 and b <= 1 order by a desc, b desc;
select count(*) from t1 where a < 5 and b > 0;
--partially_sorted_result 2
select * from t1 where a < 5 and b > 0 order by a desc,b desc;
# test HA_READ_AFTER_KEY (at the end of the file), NEAR_MIN
explain select * from t1 where a between 1 and 3 and b <= 1 order by a desc, b desc;
--partially_sorted_result 2
select * from t1 where a between 1 and 3 and b <= 1 order by a desc, b desc;
# test HA_READ_AFTER_KEY (in the middle of the file)
explain select * from t1 where a between 0 and 1 order by a desc, b desc;
--partially_sorted_result 2
select * from t1 where a between 0 and 1 order by a desc, b desc;
drop table t1;


CREATE TABLE t1 (
  gid int(10) unsigned NOT NULL auto_increment,
  cid smallint(5) unsigned NOT NULL default '0',
  PRIMARY KEY  (gid),
  KEY component_id (cid)
) ENGINE=MyISAM;
INSERT INTO t1 VALUES (103853,108),(103867,108),(103962,108),(104505,108),(104619,108),(104620,108);
ALTER TABLE t1 add skr int(10) not null;

CREATE TABLE t2 (
  gid int(10) unsigned NOT NULL default '0',
  uid smallint(5) unsigned NOT NULL default '1',
  sid tinyint(3) unsigned NOT NULL default '1',
  PRIMARY KEY  (gid),
  KEY uid (uid),
  KEY status_id (sid)
) ENGINE=MyISAM;
INSERT INTO t2 VALUES (103853,250,5),(103867,27,5),(103962,27,5),(104505,117,5),(104619,75,5),(104620,15,5);

CREATE TABLE t3 (
  uid smallint(6) NOT NULL auto_increment,
  PRIMARY KEY  (uid)
) ENGINE=MyISAM;
INSERT INTO t3 VALUES (1),(15),(27),(75),(117),(250);
ALTER TABLE t3 add skr int(10) not null;
ANALYZE TABLE t1, t2, t3;
select t1.gid, t2.sid, t3.uid from t2, t1, t3 where t2.gid = t1.gid and t2.uid = t3.uid order by t3.uid, t1.gid;
select t1.gid, t2.sid, t3.uid from t3, t2, t1 where t2.gid = t1.gid and t2.uid = t3.uid order by t3.uid, t1.gid;

# The following ORDER BY can be optimimized
EXPLAIN select t1.gid, t2.sid, t3.uid from t3, t2, t1 where t2.gid = t1.gid and t2.uid = t3.uid order by t1.gid, t3.uid;
EXPLAIN SELECT t1.gid, t3.uid from t1, t3 where t1.gid = t3.uid order by t1.gid,t3.skr;

# The following ORDER BY can't be optimimized
EXPLAIN SELECT t1.gid, t2.sid, t3.uid from t2, t1, t3 where t2.gid = t1.gid and t2.uid = t3.uid order by t3.uid, t1.gid;
EXPLAIN SELECT t1.gid, t3.uid from t1, t3 where t1.gid = t3.uid order by t3.skr,t1.gid;
EXPLAIN SELECT t1.gid, t3.uid from t1, t3 where t1.skr = t3.uid order by t1.gid,t3.skr;
drop table t1,t2,t3;

#
# Test of bug when doing an ORDER BY with const items
#

CREATE TABLE t1 (
  `titre` char(80) NOT NULL default '',
  `numeropost` mediumint(8) unsigned NOT NULL auto_increment,
  `date` datetime NOT NULL default '0000-00-00 00:00:00',
  `auteur` char(35) NOT NULL default '',
  `icone` tinyint(2) unsigned NOT NULL default '0',
  `lastauteur` char(35) NOT NULL default '',
  `nbrep` smallint(6) unsigned NOT NULL default '0',
  `dest` char(35) NOT NULL default '',
  `lu` tinyint(1) unsigned NOT NULL default '0',
  `vue` mediumint(8) unsigned NOT NULL default '0',
  `ludest` tinyint(1) unsigned NOT NULL default '0',
  `ouvert` tinyint(1) unsigned NOT NULL default '1',
  PRIMARY KEY  (`numeropost`),
  KEY `date` (`date`),
  KEY `dest` (`dest`,`ludest`),
  KEY `auteur` (`auteur`,`lu`),
  KEY `auteur_2` (`auteur`,`date`),
  KEY `dest_2` (`dest`,`date`)
) CHECKSUM=1;

CREATE TABLE t2 (
  `numeropost` mediumint(8) unsigned NOT NULL default '0',
  `pseudo` char(35) NOT NULL default '',
  PRIMARY KEY  (`numeropost`,`pseudo`),
  KEY `pseudo` (`pseudo`)
);

INSERT INTO t1 (titre,auteur,dest) VALUES ('test','joce','bug');
INSERT INTO t2 (numeropost,pseudo) VALUES (1,'joce'),(1,'bug');
SELECT titre,t1.numeropost,auteur,icone,nbrep,0,date,vue,ouvert,lastauteur,dest FROM t2 LEFT JOIN t1 USING(numeropost) WHERE t2.pseudo='joce' ORDER BY date DESC LIMIT 0,30;
SELECT titre,numeropost,auteur,icone,nbrep,0,date,vue,ouvert,lastauteur,dest FROM t2 LEFT JOIN t1 USING(numeropost) WHERE t2.pseudo='joce' ORDER BY date DESC LIMIT 0,30;
SELECT titre,t1.numeropost,auteur,icone,nbrep,'0',date,vue,ouvert,lastauteur,dest FROM t2 LEFT JOIN t1 USING(numeropost) WHERE t2.pseudo='joce' ORDER BY date DESC LIMIT 0,30;
SELECT titre,numeropost,auteur,icone,nbrep,'0',date,vue,ouvert,lastauteur,dest FROM t2 LEFT JOIN t1 USING(numeropost) WHERE t2.pseudo='joce' ORDER BY date DESC LIMIT 0,30;
drop table t1,t2;

#
# Test order by with NULL values
#
CREATE TABLE t1 (a int, b int);
INSERT INTO t1 VALUES (1, 2);
INSERT INTO t1 VALUES (3, 4);
INSERT INTO t1 VALUES (5, NULL);
SELECT * FROM t1 ORDER BY b;
SELECT * FROM t1 ORDER BY b DESC;
SELECT * FROM t1 ORDER BY (a + b);
SELECT * FROM t1 ORDER BY (a + b) DESC;
DROP TABLE t1;

#
# Test of FORCE INDEX ... ORDER BY
#

create table t1(id int not null auto_increment primary key, t char(12));
disable_query_log;
let $1 = 1000;
while ($1)
 {
  eval insert into t1(t) values ('$1'); 
  dec $1;
 }
enable_query_log;
ANALYZE TABLE t1;
explain select id,t from t1 order by id;
explain select id,t from t1 force index (primary) order by id;
drop table t1;

#
# Test of test_if_subkey() function
#
CREATE TABLE t1 (
  FieldKey varchar(36) NOT NULL default '',
  LongVal bigint(20) default NULL,
  StringVal mediumtext,
  KEY FieldKey (FieldKey),
  KEY LongField (FieldKey,LongVal),
  KEY StringField (FieldKey,StringVal(32))
) charset utf8mb4;
INSERT INTO t1 VALUES ('0',3,'0'),('0',2,'1'),('0',1,'2'),('1',2,'1'),('1',1,'3'), ('1',0,'2'),('2',3,'0'),('2',2,'1'),('2',1,'2'),('2',3,'0'),('2',2,'1'),('2',1,'2'),('3',2,'1'),('3',1,'2'),('3','3','3');
EXPLAIN SELECT * FROM t1 WHERE FieldKey = '1' ORDER BY LongVal;
SELECT * FROM t1 WHERE FieldKey = '1' ORDER BY LongVal;
EXPLAIN SELECT * FROM t1 ignore index (FieldKey, LongField) WHERE FieldKey > '2' ORDER BY LongVal;
SELECT * FROM t1 WHERE FieldKey > '2' ORDER BY LongVal;
EXPLAIN SELECT * FROM t1 WHERE FieldKey > '2' ORDER BY FieldKey, LongVal;
SELECT * FROM t1 WHERE FieldKey > '2' ORDER BY FieldKey, LongVal;
DROP TABLE t1;
#
# Bug #1945 - Crashing bug with bad User Variables in UPDATE ... ORDER BY ...
#
CREATE TABLE t1 (a INT, b INT);
SET @id=0;
UPDATE t1 SET a=0 ORDER BY (a=@id), b;
DROP TABLE t1;

#
# Bug when doing an order by on a 1 byte string (Bug #2147)
#

CREATE TABLE t1 (  id smallint(6) unsigned NOT NULL default '0',  menu tinyint(4) NOT NULL default '0',  KEY id (id),  KEY menu (menu)) ENGINE=MyISAM;
INSERT INTO t1 VALUES (11384, 2),(11392, 2);
SELECT id FROM t1 WHERE id <11984 AND menu =2 ORDER BY id DESC LIMIT 1 ;
drop table t1;

#
# REF_OR_NULL optimization + filesort (bug #2419)
# Also tests printing of RefOrNullIterator with index condition pushdown.
# Do note, however, that this given index condition pushdown is pretty
# nonsensical (the condition should have been removed when setting up
# the ref_or_null, so that there was nothing left to push); if that issue
# is fixed, you'll need to make a new test.
#

create table t1(a int, b int, index(b));
insert into t1 values (2, 1), (1, 1), (4, NULL), (3, NULL), (6, 2), (5, 2);
ANALYZE TABLE t1;
explain select * from t1 where b=1 or b is null order by a;
--skip_if_hypergraph  # Depends on the query plan.
explain format=tree select * from t1 where b=1 or b is null order by a;
select * from t1 where b=1 or b is null order by a;
explain select * from t1 where b=2 or b is null order by a;
select * from t1 where b=2 or b is null order by a;
drop table t1;

#
# Bug #3155 - Strange results with index (x, y) ... WHERE ... ORDER BY pk
#

create table t1 (a int not null auto_increment, b int not null, c int not null, d int not null,
key(a,b,d), key(c,b,a));
create table t2 like t1;
insert into t1 values (NULL, 1, 2, 0), (NULL, 2, 1, 1), (NULL, 3, 4, 2), (NULL, 4, 3, 3);
insert into t2 select null, b, c, d from t1;
insert into t1 select null, b, c, d from t2;
insert into t2 select null, b, c, d from t1;
insert into t1 select null, b, c, d from t2;
insert into t2 select null, b, c, d from t1;
insert into t1 select null, b, c, d from t2;
insert into t2 select null, b, c, d from t1;
insert into t1 select null, b, c, d from t2;
insert into t2 select null, b, c, d from t1;
insert into t1 select null, b, c, d from t2;
optimize table t1;
set @row=10;
insert into t1 select 1, b, c + (@row:=@row - 1) * 10, d - @row from t2 limit 10;
select * from t1 where a=1 and b in (1) order by c, b, a;
--sorted_result
select * from t1 where a=1 and b in (1);
drop table t1, t2;

#
# Bug #4302
# Ambiguos order by when renamed column is identical to another in result.
# Should not fail and prefer column from t1 for sorting.
#
create table t1 (col1 int, col int);
create table t2 (col2 int, col int);
insert into t1 values (1,1),(2,2),(3,3);
insert into t2 values (1,3),(2,2),(3,1);
select t1.* , t2.col as t2_col from t1 left join t2 on (t1.col1=t2.col2)
  order by col;

#
# Let us also test various ambiguos and potentially ambiguos cases 
# related to aliases
#
--error 1052
select col1 as col, col from t1 order by col;
--error 1052
select t1.col as c1, t2.col as c2 from t1, t2 where t1.col1=t2.col2
  order by col;
--error 1052
select t1.col as c1, t2.col as c2 from t1, t2 where t1.col1=t2.col2
  order by col;
--error 1052
select col1 from t1, t2 where t1.col1=t2.col2 order by col;
--error 1052
select t1.col as t1_col, t2.col2 from t1, t2 where t1.col1=t2.col2
  order by col;

select t1.col as t1_col, t2.col from t1, t2 where t1.col1=t2.col2
  order by col;
select col2 as c, col as c from t2 order by col;
select col2 as col, col as col2 from t2 order by col; 
select t2.col2, t2.col, t2.col from t2 order by col;

select t2.col2 as col from t2 order by t2.col;
select t2.col2 as col, t2.col from t2 order by t2.col;
select t2.col2, t2.col, t2.col from t2 order by t2.col;

drop table t1, t2;

#
# Bug #5428: a problem with small max_sort_length value
#

# max_sort_length is in number of bytes, use latin1 to avoid strnxfrm.
create table t1 (a char(25)) charset latin1;
insert into t1 set a = repeat('x', 20);
insert into t1 set a = concat(repeat('x', 19), 'z');
insert into t1 set a = concat(repeat('x', 19), 'ab');
insert into t1 set a = concat(repeat('x', 19), 'aa');
set max_sort_length=20;
select a from t1 order by a;
drop table t1;

#
# Bug #7331
#

create table t1 (
  `sid` decimal(8,0) default null,
  `wnid` varchar(11) not null default '',
  key `wnid14` (`wnid`(4)),
  key `wnid` (`wnid`)
) engine=myisam default charset=latin1;

insert into t1 (`sid`, `wnid`) values
('10100','01019000000'),('37986','01019000000'),('37987','01019010000'),
('39560','01019090000'),('37989','01019000000'),('37990','01019011000'),
('37991','01019011000'),('37992','01019019000'),('37993','01019030000'),
('37994','01019090000'),('475','02070000000'),('25253','02071100000'),
('25255','02071100000'),('25256','02071110000'),('25258','02071130000'),
('25259','02071190000'),('25260','02071200000'),('25261','02071210000'),
('25262','02071290000'),('25263','02071300000'),('25264','02071310000'),
('25265','02071310000'),('25266','02071320000'),('25267','02071320000'),
('25269','02071330000'),('25270','02071340000'),('25271','02071350000'),
('25272','02071360000'),('25273','02071370000'),('25281','02071391000'),
('25282','02071391000'),('25283','02071399000'),('25284','02071400000'),
('25285','02071410000'),('25286','02071410000'),('25287','02071420000'),
('25288','02071420000'),('25291','02071430000'),('25290','02071440000'),
('25292','02071450000'),('25293','02071460000'),('25294','02071470000'),
('25295','02071491000'),('25296','02071491000'),('25297','02071499000');

explain select * from t1 where wnid like '0101%' order by wnid;

select * from t1 where wnid like '0101%' order by wnid;

drop table t1;

#
# Bug #7672 - a wrong result for a select query in braces followed by order by
#

CREATE TABLE t1 (a int);
INSERT INTO t1 VALUES (2), (1), (1), (2), (1);
SELECT a FROM t1 ORDER BY a;
(SELECT a FROM t1) ORDER BY a;
DROP TABLE t1;

#
# Bug #18767: global ORDER BY applied to a SELECT with ORDER BY either was
#             ignored or 'concatened' to the latter. 

CREATE TABLE t1 (a int, b int);
INSERT INTO t1 VALUES (1,30), (2,20), (1,10), (2,30), (1,20), (2,10);

(SELECT b,a FROM t1 ORDER BY a,b) ORDER BY b,a;
(SELECT b FROM t1 ORDER BY b DESC) ORDER BY b ASC;
(SELECT b,a FROM t1 ORDER BY b,a) ORDER BY a,b;
(SELECT b,a FROM t1 ORDER by b,a LIMIT 3) ORDER by a,b;

DROP TABLE t1;

#
# Bug #22457: Column alias in ORDER BY works, but not if in an expression
#

CREATE TABLE t1 (a INT); INSERT INTO t1 VALUES (1),(2);
SELECT a + 1 AS num FROM t1 ORDER BY 30 - num;
SELECT CONCAT('test', a) AS str FROM t1 ORDER BY UPPER(str);
--source include/turn_off_only_full_group_by.inc
--sorted_result
SELECT a + 1 AS num FROM t1 GROUP BY 30 - num;
--source include/restore_sql_mode_after_turn_off_only_full_group_by.inc
--sorted_result
SELECT a + 1 AS num FROM t1 HAVING 30 - num;
--error ER_BAD_FIELD_ERROR
SELECT a + 1 AS num, num + 1 FROM t1;
--sorted_result
SELECT a + 1 AS num, (select num + 2 FROM t1 LIMIT 1) FROM t1;
--error ER_BAD_FIELD_ERROR
SELECT a.a + 1 AS num FROM t1 a JOIN t1 b ON num = b.a;
DROP TABLE t1;

#
# Bug#25126: Reference to non-existant column in UPDATE...ORDER BY... 
#       crashes server
#
CREATE TABLE bug25126 (
  val int unsigned NOT NULL AUTO_INCREMENT PRIMARY KEY
);
--error 1054
UPDATE bug25126 SET MissingCol = MissingCol;
--error 1054
UPDATE bug25126 SET val = val ORDER BY MissingCol;
UPDATE bug25126 SET val = val ORDER BY val;
UPDATE bug25126 SET val = 1 ORDER BY val;
--error 1054
UPDATE bug25126 SET val = 1 ORDER BY MissingCol;
--error 1054
UPDATE bug25126 SET val = 1 ORDER BY val, MissingCol;
--error 1054
UPDATE bug25126 SET val = MissingCol ORDER BY MissingCol;
--error 1054
UPDATE bug25126 SET MissingCol = 1 ORDER BY val, MissingCol;
--error 1054
UPDATE bug25126 SET MissingCol = 1 ORDER BY MissingCol;
--error 1054
UPDATE bug25126 SET MissingCol = val ORDER BY MissingCol;
--error 1054
UPDATE bug25126 SET MissingCol = MissingCol ORDER BY MissingCol;
DROP TABLE bug25126;

#
# Bug #25427: crash when order by expression contains a name
#             that cannot be resolved unambiguously               
#

CREATE TABLE t1 (a int);

SELECT p.a AS val, q.a AS val1 FROM t1 p, t1 q ORDER BY val > 1;
--error 1052
SELECT p.a AS val, q.a AS val FROM t1 p, t1 q ORDER BY val;
--error 1052
SELECT p.a AS val, q.a AS val FROM t1 p, t1 q ORDER BY val > 1;

DROP TABLE t1;

#
# Bug #27532: ORDER/GROUP BY expressions with IN/BETWEEN and NOT IN/BETWEEN
#                          

CREATE TABLE t1 (a int);
INSERT INTO t1 VALUES (3), (2), (4), (1);

SELECT a, IF(a IN (2,3), a, a+10) FROM t1
  ORDER BY IF(a IN (2,3), a, a+10);
SELECT a, IF(a NOT IN (2,3), a, a+10) FROM t1 
  ORDER BY IF(a NOT IN (2,3), a, a+10);
SELECT a, IF(a IN (2,3), a, a+10) FROM t1 
  ORDER BY IF(a NOT IN (2,3), a, a+10);

SELECT a, IF(a BETWEEN 2 AND 3, a, a+10) FROM t1
  ORDER BY IF(a BETWEEN 2 AND 3, a, a+10);
SELECT a, IF(a NOT BETWEEN 2 AND 3, a, a+10) FROM t1 
  ORDER BY IF(a NOT BETWEEN 2 AND 3, a, a+10);
SELECT a, IF(a BETWEEN 2 AND 3, a, a+10) FROM t1 
  ORDER BY IF(a NOT BETWEEN 2 AND 3, a, a+10);

--sorted_result
SELECT IF(a IN (1,2), a, '') as x1, IF(a NOT IN (1,2), a, '') as x2
  FROM t1 GROUP BY x1, x2;
--sorted_result
SELECT IF(a IN (1,2), a, '') as x1, IF(a NOT IN (1,2), a, '') as x2
  FROM t1 GROUP BY x1, IF(a NOT IN (1,2), a, '');

# The remaining queries are for better coverage
SELECT a, a IN (1,2) FROM t1 ORDER BY a IN (1,2);
SELECT a FROM t1 ORDER BY a IN (1,2);
SELECT a+10 FROM t1 ORDER BY a IN (1,2);
SELECT a, IF(a IN (1,2), a, a+10) FROM t1
  ORDER BY IF(a IN (3,4), a, a+10);   
DROP TABLE t1;

# End of 4.1
create table t1 (a int not null, b  int not null, c int not null);
insert t1 values (1,1,1),(1,1,2),(1,2,1);
select a, b from t1 group by a, b order by sum(c);
drop table t1;

#
# Bug#21302: Result not properly sorted when using an ORDER BY on a second 
#             table in a join
#
CREATE TABLE t1 (a int, b int, PRIMARY KEY  (a));
INSERT INTO t1 VALUES (1,1), (2,2), (3,3);
ANALYZE TABLE t1, t2;
explain SELECT t1.b as a, t2.b as c FROM 
 t1 LEFT JOIN t1 t2 ON (t1.a = t2.a AND t2.a = 2) 
ORDER BY c;
SELECT t2.b as c FROM 
 t1 LEFT JOIN t1 t2 ON (t1.a = t2.a AND t2.a = 2) 
ORDER BY c;

# check that it still removes sort of const table
explain SELECT t1.b as a, t2.b as c FROM 
 t1 JOIN t1 t2 ON (t1.a = t2.a AND t2.a = 2)  
ORDER BY c;

CREATE TABLE t2 LIKE t1;
INSERT INTO t2 SELECT * from t1;
CREATE TABLE t3 LIKE t1;
INSERT INTO t3 SELECT * from t1;
CREATE TABLE t4 LIKE t1;
INSERT INTO t4 SELECT * from t1;
INSERT INTO t1 values (0,0),(4,4);

SELECT t2.b FROM t1 LEFT JOIN (t2, t3 LEFT JOIN t4 ON t3.a=t4.a)
ON (t1.a=t2.a AND t1.b=t3.b) order by t2.b;

DROP TABLE t1,t2,t3,t4;

#
# Bug#25376: Incomplete setup of ORDER BY clause results in a wrong result.
#
create table t1 (a int, b int, c int);
insert into t1 values (1,2,3), (9,8,3), (19,4,3), (1,4,9);
select a,(sum(b)/sum(c)) as ratio from t1 group by a order by sum(b)/sum(c) asc;
drop table t1;

#
# Bug#26672: Incorrect SEC_TO_TIME() casting in ORDER BY
#
CREATE TABLE t1 (a INT UNSIGNED NOT NULL, b TIME);
INSERT INTO t1 (a) VALUES (100000), (0), (100), (1000000),(10000), (1000), (10);
UPDATE t1 SET b = SEC_TO_TIME(a);

# Correct ORDER
SELECT a, b FROM t1 ORDER BY b DESC;

# must be ordered as the above
SELECT a, b FROM t1 ORDER BY SEC_TO_TIME(a) DESC;

DROP TABLE t1;

#
# BUG#16590: Optimized does not do right "const" table pre-read
#
CREATE TABLE t1 (a INT, b INT, PRIMARY KEY (a), UNIQUE KEY b (b));
INSERT INTO t1 VALUES (1,1),(2,2);

CREATE TABLE t2 (a INT, b INT, KEY a (a,b));
INSERT INTO t2 VALUES (1,1),(1,2),(2,1),(2,2);

EXPLAIN SELECT 1 FROM t1,t2 WHERE t1.b=2 AND t1.a=t2.a ORDER BY t2.b;

DROP TABLE t1,t2;

# End of 5.0

#
# Bug #28404: query with ORDER BY and ref access
#

CREATE TABLE t1(
  id int auto_increment PRIMARY KEY, c2 int, c3 int, INDEX k2(c2), INDEX k3(c3));

INSERT INTO t1 (c2,c3) VALUES
 (31,34),(35,38),(34,31),(32,35),(31,39),
 (11,14),(15,18),(14,11),(12,15),(11,19);

INSERT INTO t1 (c2,c3) SELECT c2,c3 FROM t1;
INSERT INTO t1 (c2,c3) SELECT c2,c3 FROM t1;
INSERT INTO t1 (c2,c3) SELECT c2,c3 FROM t1;
INSERT INTO t1 (c2,c3) SELECT c2,c3 FROM t1;
INSERT INTO t1 (c2,c3) SELECT c2,c3 FROM t1;
INSERT INTO t1 (c2,c3) SELECT c2,c3 FROM t1;
INSERT INTO t1 (c2,c3) SELECT c2,c3 FROM t1;
INSERT INTO t1 (c2,c3) SELECT c2,c3 FROM t1;
INSERT INTO t1 (c2,c3) SELECT c2,c3 FROM t1;
INSERT INTO t1 (c2,c3) SELECT c2,c3 FROM t1;
INSERT INTO t1 (c2,c3) SELECT c2,c3 FROM t1;
INSERT INTO t1 (c2,c3) SELECT c2,c3 FROM t1;
UPDATE t1 SET c2=20 WHERE id%100 = 0;
SELECT COUNT(*) FROM t1;

CREATE TABLE t2 LIKE t1;
INSERT INTO t2 SELECT * FROM t1 ORDER BY id;
ANALYZE TABLE t1, t2;
--replace_column 10 # 11 #
EXPLAIN SELECT id,c3 FROM t2 WHERE c2=11 ORDER BY c3 LIMIT 20;
EXPLAIN SELECT id,c3 FROM t2 WHERE c2=11 ORDER BY c3 LIMIT 4000;
EXPLAIN SELECT id,c3 FROM t2 WHERE c2 BETWEEN 10 AND 12 ORDER BY c3 LIMIT 20;
EXPLAIN SELECT id,c3 FROM t2 WHERE c2 BETWEEN 20 AND 30 ORDER BY c3 LIMIT 4000;

--partially_sorted_result 1
SELECT c3,id FROM t2 WHERE c2=11 ORDER BY c3 LIMIT 20;

DROP TABLE t1,t2;

# 
# Bug #30665: Inconsistent optimization of IGNORE INDEX FOR {ORDER BY|GROUP BY}
#
CREATE TABLE t1 (
  a INT,
  b INT,
  PRIMARY KEY (a),
  KEY ab(a, b)
);
INSERT INTO t1 VALUES (1,1),(2,2),(3,3),(4,4);
INSERT INTO t1 SELECT a + 4, b + 4 FROM t1;
INSERT INTO t1 SELECT a + 8, b + 8 FROM t1;
INSERT INTO t1 SELECT a +16, b +16 FROM t1;
INSERT INTO t1 SELECT a +32, b +32 FROM t1;
INSERT INTO t1 SELECT a +64, b +64 FROM t1;
ANALYZE TABLE t1;
let $query=
SELECT a FROM t1 IGNORE INDEX FOR GROUP BY (PRIMARY, ab) GROUP BY a;

eval EXPLAIN $query;
flush status;
--sorted_result
eval $query;
--skip_if_hypergraph  # Depends on the query plan.
show status like 'Created_tmp_tables';

let $query=
SELECT a FROM t1 IGNORE INDEX FOR ORDER BY (PRIMARY, ab) ORDER BY a;
ANALYZE TABLE t1;
eval EXPLAIN $query;
flush status;
eval $query;
--skip_if_hypergraph  # Depends on the query plan.
show status like 'Created_tmp_tables';

flush status;
ANALYZE TABLE t1;
let $query=
SELECT a FROM t1 IGNORE INDEX (PRIMARY, ab) GROUP BY a;
eval EXPLAIN $query;
--sorted_result
eval $query;
ANALYZE TABLE t1;
let $query=
SELECT a FROM t1 IGNORE INDEX (PRIMARY, ab) ORDER BY a;
eval EXPLAIN $query;
eval $query;
--skip_if_hypergraph  # Depends on the query plan.
show status like 'Created_tmp_tables';

DROP TABLE t1;
--echo #
--echo # Bug#31590: Wrong error message on sort buffer being too small.
--echo #
create table t1(a int, b text);
insert into t1 values (1,REPEAT('x', 60000)),(3,REPEAT('y', 60000));
set session sort_buffer_size= 32768;
CALL mtr.add_suppression("Out of sort memory");
--error ER_OUT_OF_SORTMEMORY
select * from t1 order by b;
drop table t1;
call mtr.add_suppression("Out of sort memory; increase server sort buffer size");
--echo #
--echo # Bug #39844: Query Crash Mysql Server 5.0.67
--echo #

CREATE TABLE t1 (a INT PRIMARY KEY);
CREATE TABLE t2 (a INT PRIMARY KEY, b INT);
CREATE TABLE t3 (c INT);

INSERT INTO t1 (a) VALUES (1), (2);
INSERT INTO t2 (a,b) VALUES (1,2), (2,3);
INSERT INTO t3 (c) VALUES (1), (2);

SELECT
  (SELECT t1.a FROM t1, t2 WHERE t1.a = t2.b AND t2.a = t3.c ORDER BY t1.a)
  FROM t3;

DROP TABLE t1, t2, t3;


--echo #
--echo # Bug #42760: Select doesn't return desired results when we have null
--echo # values
--echo #

CREATE TABLE t1 (
  a INT,
  c INT,
  UNIQUE KEY a_c (a,c),
  KEY (a));

INSERT INTO t1 VALUES (1, 10), (2, NULL);

--echo # Must use ref-or-null on the a_c index
EXPLAIN
SELECT 1 AS col FROM t1 WHERE a=2 AND (c=10 OR c IS NULL) ORDER BY c;
--echo # Must return 1 row
SELECT 1 AS col FROM t1 WHERE a=2 AND (c=10 OR c IS NULL) ORDER BY c;

# part 2 of the problem : DESC test cases
--echo # Must use ref-or-null on the a_c index
--replace_column 1 x 2 x 3 x 7 x 8 x 9 x 10 x 12 x
EXPLAIN
SELECT 1 AS col FROM t1 WHERE a=2 AND (c=10 OR c IS NULL) ORDER BY c DESC;
--echo # Must return 1 row
SELECT 1 AS col FROM t1 WHERE a=2 AND (c=10 OR c IS NULL) ORDER BY c DESC;


DROP TABLE t1;


--echo End of 5.0 tests


#
# Bug #35206: select query result different if the key is indexed or not
#

CREATE TABLE t2 (a varchar(32), b int(11), c float, d double, 
  UNIQUE KEY a (a,b,c), KEY b (b), KEY c (c)) charset utf8mb4;

CREATE TABLE t1 (a varchar(32), b char(3), UNIQUE KEY a (a,b), KEY b (b))
charset utf8mb4;
CREATE TABLE t3 (a varchar(32), b char(3), UNIQUE KEY a (a,b))
charset utf8mb4;

--disable_query_log
INSERT INTO t1 (a, b) VALUES
('domestic', 'CH'), ('domestic', 'LI'), ('plfcz1', 'FR'), ('all', 'AD'), 
('all', 'AE'), ('all', 'AF'), ('all', 'AG'), ('all', 'AI'), ('all', 'AL'), 
('all', 'AM'), ('all', 'AN'), ('all', 'AO'), ('all', 'AP'), ('all', 'AQ'), 
('all', 'AR'), ('all', 'AS'), ('all', 'AT'), ('all', 'AU'), ('all', 'AW'),
('all', 'AZ'), ('all', 'BA'), ('all', 'BB'), ('all', 'BD'), ('all', 'BE'),
('all', 'BF'), ('all', 'BG'), ('all', 'BH'), ('all', 'BI'), ('all', 'BJ'),
('all', 'BM'), ('all', 'BN'), ('all', 'BO'), ('all', 'BR'), ('all', 'BS'),
('all', 'BT'), ('all', 'BV'), ('all', 'BW'), ('all', 'BY'), ('all', 'BZ'),
('all', 'CA'), ('all', 'CC'), ('all', 'CD'), ('all', 'CF'), ('all', 'CG'),
('all', 'CH'), ('all', 'CI'), ('all', 'CK'), ('all', 'CL'), ('all', 'CM'),
('all', 'CN'), ('all', 'CO'), ('all', 'CR'), ('all', 'CU'), ('all', 'CV'),
('all', 'CX'), ('all', 'CY'), ('all', 'CZ'), ('all', 'DE'), ('all', 'DJ'),
('all', 'DK'), ('all', 'DM'), ('all', 'DO'), ('all', 'DZ'), ('all', 'EC'),
('all', 'EE'), ('all', 'EG'), ('all', 'EH'), ('all', 'EI'), ('all', 'ER'),
('all', 'ES'), ('all', 'ET'), ('all', 'FI'), ('all', 'FJ'), ('all', 'FK'),
('all', 'FM'), ('all', 'FO'), ('all', 'FR'), ('all', 'FX'), ('all', 'GA'),
('all', 'GB'), ('all', 'GD'), ('all', 'GE'), ('all', 'GF'), ('all', 'GH'),
('all', 'GI'), ('all', 'GL'), ('all', 'GM'), ('all', 'GN'), ('all', 'GP'),
('all', 'GQ'), ('all', 'GR'), ('all', 'GS'), ('all', 'GT'), ('all', 'GU'),
('all', 'GW'), ('all', 'GY'), ('all', 'HK'), ('all', 'HM'), ('all', 'HN'),
( 'all', 'HR'), ( 'all', 'HT'), ( 'all', 'HU'), ( 'all', 'ID'), ( 'all', 'IE'),
( 'all', 'IL'), ( 'all', 'IN'), ( 'all', 'IO'), ( 'all', 'IQ'), ( 'all', 'IR'),
( 'all', 'IS'), ( 'all', 'IT'), ( 'all', 'JM'), ( 'all', 'JO'), ( 'all', 'JP'),
( 'all', 'KE'), ( 'all', 'KG'), ( 'all', 'KH'), ( 'all', 'KI'), ( 'all', 'KM'),
( 'all', 'KN'), ( 'all', 'KP'), ( 'all', 'KR'), ( 'all', 'KW'), ( 'all', 'KY'),
( 'all', 'KZ'), ( 'all', 'LA'), ( 'all', 'LB'), ( 'all', 'LC'), ( 'all', 'LI'),
( 'all', 'LK'), ( 'all', 'LR'), ( 'all', 'LS'), ( 'all', 'LT'), ( 'all', 'LU'),
( 'all', 'LV'), ( 'all', 'LY'), ( 'all', 'MA'), ( 'all', 'MC'), ( 'all', 'MD'),
( 'all', 'ME'), ( 'all', 'MG'), ( 'all', 'MH'), ( 'all', 'MK'), ( 'all', 'ML'),
( 'all', 'MM'), ( 'all', 'MN'), ( 'all', 'MO'), ( 'all', 'MP'), ( 'all', 'MQ'),
( 'all', 'MR'), ( 'all', 'MS'), ( 'all', 'MT'), ( 'all', 'MU'), ( 'all', 'MV'),
( 'all', 'MW'), ( 'all', 'MX'), ( 'all', 'MY'), ( 'all', 'MZ'), ( 'all', 'NA'),
( 'all', 'NC'), ( 'all', 'NE'), ( 'all', 'NF'), ( 'all', 'NG'), ( 'all', 'NI'),
( 'all', 'NL'), ( 'all', 'NO'), ( 'all', 'NP'), ( 'all', 'NR'), ( 'all', 'NU'),
( 'all', 'NV'), ( 'all', 'NZ'), ( 'all', 'OM'), ( 'all', 'PA'), ( 'all', 'PE'),
( 'all', 'PF'), ( 'all', 'PG'), ( 'all', 'PH'), ( 'all', 'PK'), ( 'all', 'PL'),
( 'all', 'PM'), ( 'all', 'PN'), ( 'all', 'PR'), ( 'all', 'PS'), ( 'all', 'PT'),
( 'all', 'PW'), ( 'all', 'PY'), ( 'all', 'QA'), ( 'all', 'RE'), ( 'all', 'RO'),
( 'all', 'RU'), ( 'all', 'RW'), ( 'all', 'SA'), ( 'all', 'SB'), ( 'all', 'SC'),
( 'all', 'SD'), ( 'all', 'SE'), ( 'all', 'SG'), ( 'all', 'SH'), ( 'all', 'SI'),
( 'all', 'SJ'), ( 'all', 'SK'), ( 'all', 'SL'), ( 'all', 'SM'), ( 'all', 'SN'),
( 'all', 'SO'), ( 'all', 'SR'), ( 'all', 'ST'), ( 'all', 'SV'), ( 'all', 'SY'),
( 'all', 'SZ'), ( 'all', 'TA'), ( 'all', 'TC'), ( 'all', 'TD'), ( 'all', 'TF'),
( 'all', 'TG'), ( 'all', 'TH'), ( 'all', 'TJ'), ( 'all', 'TK'), ( 'all', 'TM'),
( 'all', 'TN'), ( 'all', 'TO'), ( 'all', 'TP'), ( 'all', 'TR'), ( 'all', 'TT'),
( 'all', 'TV'), ( 'all', 'TW'), ( 'all', 'TZ'), ( 'all', 'UA'), ( 'all', 'UG'),
( 'all', 'UM'), ( 'all', 'US'), ( 'all', 'UY'), ( 'all', 'UZ'), ( 'all', 'VA'),
( 'all', 'VC'), ( 'all', 'VE'), ( 'all', 'VG'), ( 'all', 'VI'), ( 'all', 'VN'),
( 'all', 'VU'), ( 'all', 'WF'), ( 'all', 'WS'), ( 'plfcz1', 'FI'), 
( 'all', 'XE'), ( 'all', 'XS'), ( 'all', 'XU'), ( 'plfcz1', 'XE'), 
( 'all', 'YE'), ( 'all', 'YT'), ( 'all', 'YU'), ( 'all', 'ZA'), ( 'all', 'ZM'), 
( 'all', 'ZR'), ( 'all', 'ZW'), ( 'foreign', 'AD'), ( 'foreign', 'AE'), 
( 'foreign', 'AF'), ( 'foreign', 'AG'), ( 'foreign', 'AI'),
( 'foreign', 'AL'), ( 'foreign', 'AM'), ( 'foreign', 'AN'), ( 'foreign', 'AO'), 
( 'foreign', 'AP'), ( 'foreign', 'AQ'), ( 'foreign', 'AR'), ( 'foreign', 'AS'),
( 'foreign', 'AT'), ( 'foreign', 'AU'), ( 'foreign', 'AW'), ( 'foreign', 'AZ'),
( 'foreign', 'BA'), ( 'foreign', 'BB'), ( 'foreign', 'BD'), ( 'foreign', 'BE'),
( 'foreign', 'BF'), ( 'foreign', 'BG'), ( 'foreign', 'BH'), ( 'foreign', 'BI'),
( 'foreign', 'BJ'), ( 'foreign', 'BM'), ( 'foreign', 'BN'), ( 'foreign', 'BO'),
( 'foreign', 'BR'), ( 'foreign', 'BS'), ( 'foreign', 'BT'), ( 'foreign', 'BV'),
( 'foreign', 'BW'), ( 'foreign', 'BY'), ( 'foreign', 'BZ'), ( 'foreign', 'CA'),
( 'foreign', 'CC'), ( 'foreign', 'CD'), ( 'foreign', 'CF'), ( 'foreign', 'CG'),
( 'foreign', 'CI'), ( 'foreign', 'CK'), ( 'foreign', 'CL'), ( 'foreign', 'CM'),
( 'foreign', 'CN'), ( 'foreign', 'CO'), ( 'foreign', 'CR'), ( 'foreign', 'CU'),
( 'foreign', 'CV'), ( 'foreign', 'CX'), ( 'foreign', 'CY'), ( 'foreign', 'CZ'),
( 'foreign', 'DE'), ( 'foreign', 'DJ'), ( 'foreign', 'DK'), ( 'foreign', 'DM'),
( 'foreign', 'DO'), ( 'foreign', 'DZ'), ( 'foreign', 'EC'), ( 'foreign', 'EE'),
( 'foreign', 'EG'), ( 'foreign', 'EH'), ( 'foreign', 'EI'), ( 'foreign', 'ER'),
( 'foreign', 'ES'), ( 'foreign', 'ET'), ( 'foreign', 'FI'), ( 'foreign', 'FJ'),
( 'foreign', 'FK'), ( 'foreign', 'FM'), ( 'foreign', 'FO'), ( 'foreign', 'FR'),
( 'foreign', 'FX'), ( 'foreign', 'GA'), ( 'foreign', 'GB'), ( 'foreign', 'GD'),
( 'foreign', 'GE'), ( 'foreign', 'GF'), ( 'foreign', 'GH'), ( 'foreign', 'GI'),
( 'foreign', 'GL'), ( 'foreign', 'GM'), ( 'foreign', 'GN'), ( 'foreign', 'GP'),
( 'foreign', 'GQ'), ( 'foreign', 'GR'), ( 'foreign', 'GS'), ( 'foreign', 'GT'),
( 'foreign', 'GU'), ( 'foreign', 'GW'), ( 'foreign', 'GY'), ( 'foreign', 'HK'),
( 'foreign', 'HM'), ( 'foreign', 'HN'), ( 'foreign', 'HR'), ( 'foreign', 'HT'),
( 'foreign', 'HU'), ( 'foreign', 'ID'), ( 'foreign', 'IE'), ( 'foreign', 'IL'),
( 'foreign', 'IN'), ( 'foreign', 'IO'), ( 'foreign', 'IQ'), ( 'foreign', 'IR'),
( 'foreign', 'IS'), ( 'foreign', 'IT'), ( 'foreign', 'JM'), ( 'foreign', 'JO'),
( 'foreign', 'JP'), ( 'foreign', 'KE'), ( 'foreign', 'KG'), ( 'foreign', 'KH'),
( 'foreign', 'KI'), ( 'foreign', 'KM'), ( 'foreign', 'KN'), ( 'foreign', 'KP'),
( 'foreign', 'KR'), ( 'foreign', 'KW'), ( 'foreign', 'KY'), ( 'foreign', 'KZ'),
( 'foreign', 'LA'), ( 'foreign', 'LB'), ( 'foreign', 'LC'), ( 'foreign', 'LK'),
( 'foreign', 'LR'), ( 'foreign', 'LS'), ( 'foreign', 'LT'), ( 'foreign', 'LU'),
( 'foreign', 'LV'), ( 'foreign', 'LY'), ( 'foreign', 'MA'), ( 'foreign', 'MC'),
( 'foreign', 'MD'), ( 'foreign', 'ME'), ( 'foreign', 'MG'), ( 'foreign', 'MH'),
( 'foreign', 'MK'), ( 'foreign', 'ML'), ( 'foreign', 'MM'), ( 'foreign', 'MN'),
( 'foreign', 'MO'), ( 'foreign', 'MP'), ( 'foreign', 'MQ'), ( 'foreign', 'MR'),
( 'foreign', 'MS'), ( 'foreign', 'MT'), ( 'foreign', 'MU'), ( 'foreign', 'MV'),
( 'foreign', 'MW'), ( 'foreign', 'MX'), ( 'foreign', 'MY'), ( 'foreign', 'MZ'),
( 'foreign', 'NA'), ( 'foreign', 'NC'), ( 'foreign', 'NE'), ( 'foreign', 'NF'),
( 'foreign', 'NG'), ( 'foreign', 'NI'), ( 'foreign', 'NL'), ( 'foreign', 'NO'),
( 'foreign', 'NP'), ( 'foreign', 'NR'), ( 'foreign', 'NU'), ( 'foreign', 'NV'),
( 'foreign', 'NZ'), ( 'foreign', 'OM'), ( 'foreign', 'PA'), ( 'foreign', 'PE'),
( 'foreign', 'PF'), ( 'foreign', 'PG'), ( 'foreign', 'PH'), ( 'foreign', 'PK'),
( 'foreign', 'PL'), ( 'foreign', 'PM'), ( 'foreign', 'PN'), ( 'foreign', 'PR'),
( 'foreign', 'PS'), ( 'foreign', 'PT'), ( 'foreign', 'PW'), ( 'foreign', 'PY'),
( 'foreign', 'QA'), ( 'foreign', 'RE'), ( 'foreign', 'RO'), ( 'foreign', 'RU'),
( 'foreign', 'RW'), ( 'foreign', 'SA'), ( 'foreign', 'SB'), ( 'foreign', 'SC'),
( 'foreign', 'SD'), ( 'foreign', 'SE'), ( 'foreign', 'SG'), ( 'foreign', 'SH'),
( 'foreign', 'SI'), ( 'foreign', 'SJ'), ( 'foreign', 'SK'), ( 'foreign', 'SL'),
( 'foreign', 'SM'), ( 'foreign', 'SN'), ( 'foreign', 'SO'), ( 'foreign', 'SR'),
( 'foreign', 'ST'), ( 'foreign', 'SV'), ( 'foreign', 'SY'), ( 'foreign', 'SZ'),
( 'foreign', 'TA'), ( 'foreign', 'TC'), ( 'foreign', 'TD'), ( 'foreign', 'TF'),
( 'foreign', 'TG'), ( 'foreign', 'TH'), ( 'foreign', 'TJ'), ( 'foreign', 'TK'),
( 'foreign', 'TM'), ( 'foreign', 'TN'), ( 'foreign', 'TO'), ( 'foreign', 'TP'),
( 'foreign', 'TR'), ( 'foreign', 'TT'), ( 'foreign', 'TV'), ( 'foreign', 'TW'),
( 'foreign', 'TZ'), ( 'foreign', 'UA'), ( 'foreign', 'UG'), ( 'foreign', 'UM'),
( 'foreign', 'US'), ( 'foreign', 'UY'), ( 'foreign', 'UZ'), ( 'foreign', 'VA'),
( 'foreign', 'VC'), ( 'foreign', 'VE'), ( 'foreign', 'VG'), ( 'foreign', 'VI'),
( 'foreign', 'VN'), ( 'foreign', 'VU'), ( 'foreign', 'WF'), ( 'foreign', 'WS'),
( 'plfcz1', 'DK'), ( 'foreign', 'XE'), ( 'foreign', 'XS'), ( 'foreign', 'XU'),
( 'plfcz1', 'BE'), ( 'foreign', 'YE'), ( 'foreign', 'YT'), ( 'foreign', 'YU'),
( 'foreign', 'ZA'), ( 'foreign', 'ZM'), ( 'foreign', 'ZR'), ( 'foreign', 'ZW'),
( 'plfcz1', 'DE'), ( 'plfcz1', 'GI'), ( 'plfcz1', 'GR'), ( 'plfcz1', 'IS'),
( 'plfcz1', 'EI'), ( 'plfcz1', 'IT'), ( 'plfcz1', 'LU'), ( 'plfcz1', 'NL'),
( 'plfcz1', 'NO'), ( 'plfcz1', 'ES'), ( 'plfcz1', 'SE'), ( 'plfcz1', 'AL'),
( 'plfcz1', 'AD'), ( 'plfcz1', 'BY'), ( 'plfcz1', 'BA'), ( 'plfcz1', 'BG'),
( 'plfcz1', 'EE'), ( 'plfcz1', 'FO'), ( 'plfcz1', 'GL'), ( 'plfcz1', 'GB'),
( 'plfcz1', 'HR'), ( 'plfcz1', 'LV'), ( 'plfcz1', 'LT'), ( 'plfcz1', 'MT'),
( 'plfcz1', 'MK'), ( 'plfcz1', 'MD'), ( 'plfcz1', 'MC'), ( 'plfcz1', 'AT'),
( 'plfcz1', 'PL'), ( 'plfcz1', 'PT'), ( 'plfcz1', 'RO'), ( 'plfcz1', 'RU'),
( 'plfcz1', 'SM'), ( 'plfcz1', 'XS'), ( 'plfcz1', 'SK'), ( 'plfcz1', 'SI'),
( 'plfcz1', 'CZ'), ( 'plfcz1', 'TR'), ( 'plfcz1', 'UA'), ( 'plfcz1', 'HU'),
( 'plfcz1', 'VA'), ( 'plfcz1', 'CY'), ( 'plfcz2', 'AF'), ( 'plfcz2', 'DZ'),
( 'plfcz2', 'AS'), ( 'plfcz2', 'AO'), ( 'plfcz2', 'AI'), ( 'plfcz2', 'AQ'),
( 'plfcz2', 'AG'), ( 'plfcz2', 'AR'), ( 'plfcz2', 'AM'), ( 'plfcz2', 'AW'),
( 'plfcz2', 'AU'), ( 'plfcz2', 'AZ'), ( 'plfcz2', 'AP'), ( 'plfcz2', 'BS'),
( 'plfcz2', 'BH'), ( 'plfcz2', 'BD'), ( 'plfcz2', 'BB'), ( 'plfcz2', 'BZ'),
( 'plfcz2', 'BJ'), ( 'plfcz2', 'BM'), ( 'plfcz2', 'BT'), ( 'plfcz2', 'BO'),
( 'plfcz2', 'BW'), ( 'plfcz2', 'BV'), ( 'plfcz2', 'BR'), ( 'plfcz2', 'IO'),
( 'plfcz2', 'VG'), ( 'plfcz2', 'BN'), ( 'plfcz2', 'BF'), ( 'plfcz2', 'BI'),
( 'plfcz2', 'KH'), ( 'plfcz2', 'CM'), ( 'plfcz2', 'CA'), ( 'plfcz2', 'CV'),
( 'plfcz2', 'KY'), ( 'plfcz2', 'CF'), ( 'plfcz2', 'TD'), ( 'plfcz2', 'CL'),
( 'plfcz2', 'CN'), ( 'plfcz2', 'CX'), ( 'plfcz2', 'CC'), ( 'plfcz2', 'CO'),
( 'plfcz2', 'KM'), ( 'plfcz2', 'CG'), ( 'plfcz2', 'CD'), ( 'plfcz2', 'CK'),
( 'plfcz2', 'CR'), ( 'plfcz2', 'CI'), ( 'plfcz2', 'CU'), ( 'plfcz2', 'DJ'),
( 'plfcz2', 'DM'), ( 'plfcz2', 'DO'), ( 'plfcz2', 'TP'), ( 'plfcz2', 'EC'),
( 'plfcz2', 'EG'), ( 'plfcz2', 'SV'), ( 'plfcz2', 'GQ'), ( 'plfcz2', 'ER'),
( 'plfcz2', 'ET'), ( 'plfcz2', 'FK'), ( 'plfcz2', 'FJ'), ( 'plfcz2', 'FX'),
( 'plfcz2', 'GF'), ( 'plfcz2', 'PF'), ( 'plfcz2', 'TA'), ( 'plfcz2', 'TF'),
( 'plfcz2', 'GA'), ( 'plfcz2', 'GM'), ( 'plfcz2', 'GE'), ( 'plfcz2', 'GH'),
( 'plfcz2', 'GD'), ( 'plfcz2', 'GP'), ( 'plfcz2', 'GU'), ( 'plfcz2', 'GT'),
( 'plfcz2', 'GN'), ( 'plfcz2', 'GW'), ( 'plfcz2', 'GY'), ( 'plfcz2', 'HT'),
( 'plfcz2', 'HM'), ( 'plfcz2', 'HN'), ( 'plfcz2', 'HK'), ( 'plfcz2', 'IN'),
( 'plfcz2', 'ID'), ( 'plfcz2', 'IR'), ( 'plfcz2', 'IQ'), ( 'plfcz2', 'IE'),
( 'plfcz2', 'IL'), ( 'plfcz2', 'JM'), ( 'plfcz2', 'JP'), ( 'plfcz2', 'JO'),
( 'plfcz2', 'KZ'), ( 'plfcz2', 'KE'), ( 'plfcz2', 'KI'), ( 'plfcz2', 'KP'),
( 'plfcz2', 'KW'), ( 'plfcz2', 'KG'), ( 'plfcz2', 'LA'), ( 'plfcz2', 'LB'),
( 'plfcz2', 'LS'), ( 'plfcz2', 'LR'), ( 'plfcz2', 'LY'), ( 'plfcz2', 'MO'),
( 'plfcz2', 'MG'), ( 'plfcz2', 'ME'), ( 'plfcz2', 'MW'), ( 'plfcz2', 'MY'),
( 'plfcz2', 'MV'), ( 'plfcz2', 'ML'), ( 'plfcz2', 'MH'), ( 'plfcz2', 'MQ'),
( 'plfcz2', 'MR'), ( 'plfcz2', 'MU'), ( 'plfcz2', 'YT'), ( 'plfcz2', 'MX'),
( 'plfcz2', 'FM'), ( 'plfcz2', 'MN'), ( 'plfcz2', 'MS'), ( 'plfcz2', 'MA'),
( 'plfcz2', 'MZ'), ( 'plfcz2', 'MM'), ( 'plfcz2', 'NA'), ( 'plfcz2', 'NR'),
( 'plfcz2', 'NP'), ( 'plfcz2', 'AN'), ( 'plfcz2', 'NC'), ( 'plfcz2', 'NZ'),
( 'plfcz2', 'NI'), ( 'plfcz2', 'NE'), ( 'plfcz2', 'NG'), ( 'plfcz2', 'NU'),
( 'plfcz2', 'NF'), ( 'plfcz2', 'MP'), ( 'plfcz2', 'OM'), ( 'plfcz2', 'PK'),
( 'plfcz2', 'PW'), ( 'plfcz2', 'PS'), ( 'plfcz2', 'PA'), ( 'plfcz2', 'PG'),
( 'plfcz2', 'PY'), ( 'plfcz2', 'PE'), ( 'plfcz2', 'PH'), ( 'plfcz2', 'PN'),
( 'plfcz2', 'PR'), ( 'plfcz2', 'QA'), ( 'plfcz2', 'RE'), ( 'plfcz2', 'RW'),
( 'plfcz2', 'KN'), ( 'plfcz2', 'ST'), ( 'plfcz2', 'SA'), ( 'plfcz2', 'SN'),
( 'plfcz2', 'SC'), ( 'plfcz2', 'SL'), ( 'plfcz2', 'SG'), ( 'plfcz2', 'SB'),
( 'plfcz2', 'SO'), ( 'plfcz2', 'ZA'), ( 'plfcz2', 'GS'), ( 'plfcz2', 'KR'),
( 'plfcz2', 'LK'), ( 'plfcz2', 'NV'), ( 'plfcz2', 'SH'), ( 'plfcz2', 'LC'),
( 'plfcz2', 'PM'), ( 'plfcz2', 'VC'), ( 'plfcz2', 'SD'), ( 'plfcz2', 'SR'),
( 'plfcz2', 'SJ'), ( 'plfcz2', 'SZ'), ( 'plfcz2', 'SY'), ( 'plfcz2', 'TW'),
( 'plfcz2', 'TJ'), ( 'plfcz2', 'TZ'), ( 'plfcz2', 'TH'), ( 'plfcz2', 'TG'),
( 'plfcz2', 'TK'), ( 'plfcz2', 'TO'), ( 'plfcz2', 'TT'), ( 'plfcz2', 'XU'),
( 'plfcz2', 'TN'), ( 'plfcz2', 'TM'), ( 'plfcz2', 'TC'), ( 'plfcz2', 'TV'),
( 'plfcz2', 'UG'), ( 'plfcz2', 'AE'), ( 'plfcz2', 'US'), ( 'plfcz2', 'UM'),
( 'plfcz2', 'UY'), ( 'plfcz2', 'UZ'), ( 'plfcz2', 'VU'), ( 'plfcz2', 'VE'),
( 'plfcz2', 'VN'), ( 'plfcz2', 'VI'), ( 'plfcz2', 'WF'), ( 'plfcz2', 'EH'),
( 'plfcz2', 'WS'), ( 'plfcz2', 'YE'), ( 'plfcz2', 'YU'), ( 'plfcz2', 'ZR'),
( 'plfcz2', 'ZM'), ( 'plfcz2', 'ZW'), ( 'ppfcz1', 'AT'), ( 'ppfcz1', 'BE'),
( 'ppfcz1', 'DE'), ( 'ppfcz1', 'FR'), ( 'ppfcz1', 'FX'), ( 'ppfcz1', 'IT'),
( 'ppfcz1', 'LU'), ( 'ppfcz1', 'MC'), ( 'ppfcz1', 'NL'), ( 'ppfcz1', 'SM'),
( 'ppfcz1', 'VA'), ( 'ppfcz1', 'XE'), ( 'ppfcz2', 'AD'), ( 'ppfcz2', 'AL'),
( 'ppfcz2', 'BA'), ( 'ppfcz2', 'BG'), ( 'ppfcz2', 'BY'), ( 'ppfcz2', 'CY'),
( 'ppfcz2', 'CZ'), ( 'ppfcz2', 'DK'), ( 'ppfcz2', 'EE'), ( 'ppfcz2', 'EI'),
( 'ppfcz2', 'ES'), ( 'ppfcz2', 'FI'), ( 'ppfcz2', 'FO'), ( 'ppfcz2', 'GB'),
( 'ppfcz2', 'GI'), ( 'ppfcz2', 'GL'), ( 'ppfcz2', 'GR'), ( 'ppfcz2', 'HR'),
( 'ppfcz2', 'HU'), ( 'ppfcz2', 'IE'), ( 'ppfcz2', 'IS'), ( 'ppfcz2', 'LT'),
( 'ppfcz2', 'LV'), ( 'ppfcz2', 'MD'), ( 'ppfcz2', 'MK'), ( 'ppfcz2', 'MT'),
( 'ppfcz2', 'NO'), ( 'ppfcz2', 'PL'), ( 'ppfcz2', 'PT'), ( 'ppfcz2', 'RO'),
( 'ppfcz2', 'RU'), ( 'ppfcz2', 'SE'), ( 'ppfcz2', 'SI'), ( 'ppfcz2', 'SK'),
( 'ppfcz2', 'TR'), ( 'ppfcz2', 'UA'), ( 'ppfcz2', 'XS'), ( 'ppfcz2', 'YU'),
( 'ppfcz3', 'CA'), ( 'ppfcz3', 'DZ'), ( 'ppfcz3', 'EG'), ( 'ppfcz3', 'IL'),
( 'ppfcz3', 'JO'), ( 'ppfcz3', 'LB'), ( 'ppfcz3', 'LY'), ( 'ppfcz3', 'MA'),
( 'ppfcz3', 'MX'), ( 'ppfcz3', 'PM'), ( 'ppfcz3', 'SY'), ( 'ppfcz3', 'TN'),
( 'ppfcz3', 'US'), ( 'ppfcz4', 'AE'), ( 'ppfcz4', 'AF'), ( 'ppfcz4', 'AM'),
( 'ppfcz4', 'AO'), ( 'ppfcz4', 'AZ'), ( 'ppfcz4', 'BD'), ( 'ppfcz4', 'BF'),
( 'ppfcz4', 'BH'), ( 'ppfcz4', 'BI'), ( 'ppfcz4', 'BJ'), ( 'ppfcz4', 'BT'),
( 'ppfcz4', 'BV'), ( 'ppfcz4', 'BW'), ( 'ppfcz4', 'CF'), ( 'ppfcz4', 'CG'),
( 'ppfcz4', 'CI'), ( 'ppfcz4', 'CM'), ( 'ppfcz4', 'CN'), ( 'ppfcz4', 'DJ'),
( 'ppfcz4', 'DO'), ( 'ppfcz4', 'ER'), ( 'ppfcz4', 'ET'), ( 'ppfcz4', 'GA'),
( 'ppfcz4', 'GE'), ( 'ppfcz4', 'GH'), ( 'ppfcz4', 'GM'), ( 'ppfcz4', 'GN'),
( 'ppfcz4', 'GQ'), ( 'ppfcz4', 'GW'), ( 'ppfcz4', 'HK'), ( 'ppfcz4', 'IN'),
( 'ppfcz4', 'IQ'), ( 'ppfcz4', 'IR'), ( 'ppfcz4', 'JP'), ( 'ppfcz4', 'KE'),
( 'ppfcz4', 'KG'), ( 'ppfcz4', 'KH'), ( 'ppfcz4', 'KP'), ( 'ppfcz4', 'KW'),
( 'ppfcz4', 'KZ'), ( 'ppfcz4', 'LA'), ( 'ppfcz4', 'LK'), ( 'ppfcz4', 'LR'),
( 'ppfcz4', 'LS'), ( 'ppfcz4', 'MG'), ( 'ppfcz4', 'ML'), ( 'ppfcz4', 'MM'),
( 'ppfcz4', 'MN'), ( 'ppfcz4', 'MO'), ( 'ppfcz4', 'MR'), ( 'ppfcz4', 'MU'),
( 'ppfcz4', 'MV'), ( 'ppfcz4', 'MW'), ( 'ppfcz4', 'MY'), ( 'ppfcz4', 'MZ'),
( 'ppfcz4', 'NA'), ( 'ppfcz4', 'NE'), ( 'ppfcz4', 'NG'), ( 'ppfcz4', 'NP'),
( 'ppfcz4', 'OM'), ( 'ppfcz4', 'PK'), ( 'ppfcz4', 'QA'), ( 'ppfcz4', 'RE'),
( 'ppfcz4', 'RW'), ( 'ppfcz4', 'SA'), ( 'ppfcz4', 'SC'), ( 'ppfcz4', 'SD'),
( 'ppfcz4', 'SG'), ( 'ppfcz4', 'SH'), ( 'ppfcz4', 'SL'), ( 'ppfcz4', 'SN'),
( 'ppfcz4', 'SO'), ( 'ppfcz4', 'SZ'), ( 'ppfcz4', 'TD'), ( 'ppfcz4', 'TG'),
( 'ppfcz4', 'TH'), ( 'ppfcz4', 'TJ'), ( 'ppfcz4', 'TM'), ( 'ppfcz4', 'TW'),
( 'ppfcz4', 'TZ'), ( 'ppfcz4', 'UG'), ( 'ppfcz4', 'UZ'), ( 'ppfcz4', 'VN'),
( 'ppfcz4', 'XU'), ( 'ppfcz4', 'YT'), ( 'ppfcz4', 'ZA'), ( 'ppfcz4', 'ZW'),
( 'ppfcz5', 'AG'), ( 'ppfcz5', 'AI'), ( 'ppfcz5', 'AN'), ( 'ppfcz5', 'AP'),
( 'ppfcz5', 'AQ'), ( 'ppfcz5', 'AR'), ( 'ppfcz5', 'AS'), ( 'ppfcz5', 'AU'),
( 'ppfcz5', 'AW'), ( 'ppfcz5', 'BB'), ( 'ppfcz5', 'BM'), ( 'ppfcz5', 'BN'),
( 'ppfcz5', 'BO'), ( 'ppfcz5', 'BR'), ( 'ppfcz5', 'BS'), ( 'ppfcz5', 'BZ'),
( 'ppfcz5', 'CC'), ( 'ppfcz5', 'CD'), ( 'ppfcz5', 'CK'), ( 'ppfcz5', 'CL'),
( 'ppfcz5', 'CO'), ( 'ppfcz5', 'CR'), ( 'ppfcz5', 'CU'), ( 'ppfcz5', 'CV'),
( 'ppfcz5', 'CX'), ( 'ppfcz5', 'DM'), ( 'ppfcz5', 'EC'), ( 'ppfcz5', 'EH'),
( 'ppfcz5', 'FJ'), ( 'ppfcz5', 'FK'), ( 'ppfcz5', 'FM'), ( 'ppfcz5', 'GD'),
( 'ppfcz5', 'GF'), ( 'ppfcz5', 'GP'), ( 'ppfcz5', 'GS'), ( 'ppfcz5', 'GT'),
( 'ppfcz5', 'GU'), ( 'ppfcz5', 'GY'), ( 'ppfcz5', 'HM'), ( 'ppfcz5', 'HN'),
( 'ppfcz5', 'HT'), ( 'ppfcz5', 'ID'), ( 'ppfcz5', 'IO'), ( 'ppfcz5', 'JM'),
( 'ppfcz5', 'KI'), ( 'ppfcz5', 'KM'), ( 'ppfcz5', 'KN'), ( 'ppfcz5', 'KR'),
( 'ppfcz5', 'KY'), ( 'ppfcz5', 'LC'), ( 'ppfcz5', 'ME'), ( 'ppfcz5', 'MH'),
( 'ppfcz5', 'MP'), ( 'ppfcz5', 'MQ'), ( 'ppfcz5', 'MS'), ( 'ppfcz5', 'NC'),
( 'ppfcz5', 'NF'), ( 'ppfcz5', 'NI'), ( 'ppfcz5', 'NR'), ( 'ppfcz5', 'NU'),
( 'ppfcz5', 'NZ'), ( 'ppfcz5', 'PA'), ( 'ppfcz5', 'PE'), ( 'ppfcz5', 'PF'),
( 'ppfcz5', 'PG'), ( 'ppfcz5', 'PH'), ( 'ppfcz5', 'PN'), ( 'ppfcz5', 'PR'),
( 'ppfcz5', 'PS'), ( 'ppfcz5', 'PW'), ( 'ppfcz5', 'PY'), ( 'ppfcz5', 'SB'),
( 'ppfcz5', 'SJ'), ( 'ppfcz5', 'SR'), ( 'ppfcz5', 'ST'), ( 'ppfcz5', 'SV'),
( 'ppfcz5', 'TA'), ( 'ppfcz5', 'TC'), ( 'ppfcz5', 'TF'), ( 'ppfcz5', 'TK'),
( 'ppfcz5', 'TO'), ( 'ppfcz5', 'TP'), ( 'ppfcz5', 'TT'), ( 'ppfcz5', 'TV'),
( 'ppfcz5', 'UM'), ( 'ppfcz5', 'UY'), ( 'ppfcz5', 'VC'), ( 'ppfcz5', 'VE'),
( 'ppfcz5', 'VG'), ( 'ppfcz5', 'VI'), ( 'ppfcz5', 'VU'), ( 'ppfcz5', 'WF'),
( 'ppfcz5', 'WS'), ( 'ppfcz5', 'YE'), ( 'ppfcz5', 'ZM'), ( 'ppfcz5', 'ZR');

INSERT INTO t2 (a, b, c, d) VALUES
('domestic', 26, 0.25, 4.7), ('domestic', 27, 0.25, 6),
('domestic', 19, 2, 6.3), ('domestic', 19, 5, 7.77),
('domestic', 19, 10, 10.3), ('domestic', 19, 20, 14.83),
('domestic', 19, 30, 20.88), ('domestic', 20, 2, 7.3),
('domestic', 20, 5, 8.77), ('domestic', 20, 10, 11.3),
('domestic', 20, 20, 15.83), ('domestic', 20, 30, 21.88),
('domestic', 23, 2, 18.8), ('domestic', 23, 5, 20.8),
('domestic', 23, 10, 24.8), ('domestic', 23, 20, 27.8),
('domestic', 23, 30, 30.8), ('domestic', 24, 2, 21.1405),
('domestic', 24, 5, 22.3705), ('domestic', 24, 10, 25.0905),
('domestic', 24, 20, 29.7705), ('domestic', 24, 30, 35.9605),
('domestic', 17, 2, 7.2), ('domestic', 17, 5, 8.43),
('domestic', 17, 10, 11.15), ('domestic', 17, 20, 15.83),
('domestic', 17, 30, 22.02), ('domestic', 18, 2, 8.2),
('domestic', 18, 5, 9.43), ('domestic', 18, 10, 12.15),
('domestic', 18, 20, 16.83), ('domestic', 18, 30, 23.02),
('domestic', 28, 2, 17), ('domestic', 28, 5, 19),
('domestic', 28, 10, 22), ('domestic', 28, 20, 28),
('domestic', 28, 30, 35), ('domestic', 29, 30, 29.5),
('foreign', 25, 200, 0), ('domestic', 3, 100, 59),
('foreign', 10, 30, 0), ('foreign', 22, 0, 0),
('foreign', 11, 30, 0), ('foreign', 12, 30, 0),
('all', 1, 10000, 0), ('all', 2, 10000, 0),
('domestic', 9, 10000, 0), ('domestic', 4, 500, 0),
('domestic', 5, 500, 0), ('domestic', 6, 500, 0),
('domestic', 7, 500, 0), ('domestic', 8, 500, 0),
('domestic', 21, 3.9, 10.8), ('domestic', 21, 4.9, 12.2),
('domestic', 21, 9.9, 15.3), ('domestic', 21, 19.9, 20.6),
('domestic', 21, 30, 28.1), ('plfcz1', 16, 0.5, 19),
('plfcz2', 16, 0.5, 25), ( 'ppfcz2', 15, 16, 76.5),
( 'ppfcz2', 15, 15, 75.5), ( 'ppfcz2', 15, 14, 73.5),
( 'ppfcz2', 15, 13, 71.5), ( 'ppfcz2', 15, 12, 69.5),
( 'ppfcz2', 15, 11, 67.5), ( 'ppfcz2', 15, 10, 65.5),
( 'ppfcz2', 15, 9, 62.5), ( 'ppfcz2', 15, 8, 59.5),
( 'ppfcz2', 15, 7, 56.5), ( 'ppfcz2', 15, 6, 53.5),
( 'ppfcz2', 15, 5, 50.5), ( 'ppfcz2', 15, 4, 46.5),
( 'ppfcz2', 15, 3, 42.5), ( 'ppfcz2', 15, 2, 38.5),
('ppfcz1', 15, 2, 33.5), ('ppfcz1', 15, 3, 36.5),
('ppfcz1', 15, 4, 39.5), ('ppfcz1', 15, 5, 41.5),
('ppfcz1', 15, 6, 42.5), ('ppfcz1', 15, 7, 43.5),
('ppfcz1', 15, 8, 44.5), ('ppfcz1', 15, 9, 45.5),
('ppfcz1', 15, 10, 46.5), ('ppfcz1', 15, 11, 47.5),
( 'ppfcz1', 15, 12, 48.5), ( 'ppfcz1', 15, 13, 49.5), ( 'ppfcz1', 15, 14, 50.5),
( 'ppfcz1', 15, 15, 51.5), ( 'ppfcz1', 15, 16, 52.5), ( 'ppfcz1', 15, 17, 53.5),
( 'ppfcz1', 15, 18, 54.5), ( 'ppfcz1', 15, 19, 55.5), ( 'ppfcz1', 15, 20, 56.5),
( 'ppfcz1', 15, 21, 57.5), ( 'ppfcz1', 15, 22, 58.5), ( 'ppfcz1', 15, 23, 59.5),
( 'ppfcz1', 15, 24, 60.5), ( 'ppfcz1', 15, 25, 61.5), ( 'ppfcz1', 15, 26, 62.5),
( 'ppfcz1', 15, 27, 63.5), ( 'ppfcz1', 15, 28, 64.5), ( 'ppfcz1', 15, 29, 65.5),
( 'ppfcz1', 15, 30, 66.5), ( 'ppfcz2', 15, 17, 77.5), ( 'ppfcz2', 15, 18, 78.5),
( 'ppfcz2', 15, 19, 79.5), ( 'ppfcz2', 15, 20, 80.5), ( 'ppfcz2', 15, 21, 81.5),
( 'ppfcz2', 15, 22, 82.5), ( 'ppfcz2', 15, 23, 83.5), ( 'ppfcz2', 15, 24, 84.5),
( 'ppfcz2', 15, 25, 85.5), ( 'ppfcz2', 15, 26, 86.5), ( 'ppfcz2', 15, 27, 87.5),
( 'ppfcz2', 15, 28, 88.5), ( 'ppfcz2', 15, 29, 89.5), ( 'ppfcz2', 15, 30, 90.5),
( 'ppfcz3', 15, 2, 39.5), ( 'ppfcz3', 15, 3, 45.5), ( 'ppfcz3', 15, 4, 51.5),
( 'ppfcz3', 15, 5, 57.5), ( 'ppfcz3', 15, 6, 63.5), ( 'ppfcz3', 15, 7, 69.5),
( 'ppfcz3', 15, 8, 75.5), ( 'ppfcz3', 15, 9, 81.5), ( 'ppfcz3', 15, 10, 87.5),
( 'ppfcz3', 15, 11, 93.5), ( 'ppfcz3', 15, 12, 99.5), ( 'ppfcz3', 15, 13, 105.5),
( 'ppfcz3', 15, 14, 111.5), ( 'ppfcz3', 15, 15, 117.5), ( 'ppfcz3', 15, 16, 122.5),
( 'ppfcz3', 15, 17, 127.5), ( 'ppfcz3', 15, 18, 132.5), ( 'ppfcz3', 15, 19, 137.5),
( 'ppfcz3', 15, 20, 142.5), ( 'ppfcz3', 15, 21, 146.5), ( 'ppfcz3', 15, 22, 150.5),
( 'ppfcz3', 15, 23, 154.5), ( 'ppfcz3', 15, 24, 158.5), ( 'ppfcz3', 15, 25, 162.5),
( 'ppfcz3', 15, 26, 166.5), ( 'ppfcz3', 15, 27, 170.5), ( 'ppfcz3', 15, 28, 174.5),
( 'ppfcz3', 15, 29, 178.5), ( 'ppfcz3', 15, 30, 182.5), ( 'ppfcz4', 15, 2, 44.5),
( 'ppfcz4', 15, 3, 51.5), ( 'ppfcz4', 15, 4, 58.5), ( 'ppfcz4', 15, 5, 65.5),
( 'ppfcz4', 15, 6, 72.5), ( 'ppfcz4', 15, 7, 79.5), ( 'ppfcz4', 15, 8, 86.5),
( 'ppfcz4', 15, 9, 93.5), ( 'ppfcz4', 15, 10, 100.5), ( 'ppfcz4', 15, 11, 105.5),
( 'ppfcz4', 15, 12, 110.5), ( 'ppfcz4', 15, 13, 115.5), ( 'ppfcz4', 15, 14, 120.5),
( 'ppfcz4', 15, 15, 125.5), ( 'ppfcz4', 15, 16, 130.5), ( 'ppfcz4', 15, 17, 135.5),
( 'ppfcz4', 15, 18, 140.5), ( 'ppfcz4', 15, 19, 145.5), ( 'ppfcz4', 15, 20, 150.5),
( 'ppfcz4', 15, 21, 154.5), ( 'ppfcz4', 15, 22, 158.5), ( 'ppfcz4', 15, 23, 162.5),
( 'ppfcz4', 15, 24, 166.5), ( 'ppfcz4', 15, 25, 170.5), ( 'ppfcz4', 15, 26, 174.5),
( 'ppfcz4', 15, 27, 178.5), ( 'ppfcz4', 15, 28, 182.5), ( 'ppfcz4', 15, 29, 186.5),
( 'ppfcz4', 15, 30, 190.5), ( 'ppfcz5', 15, 2, 48.5), ( 'ppfcz5', 15, 3, 56.5),
( 'ppfcz5', 15, 4, 64.5), ( 'ppfcz5', 15, 5, 72.5), ( 'ppfcz5', 15, 6, 80.5),
( 'ppfcz5', 15, 7, 88.5), ( 'ppfcz5', 15, 8, 96.5), ( 'ppfcz5', 15, 9, 104.5),
( 'ppfcz5', 15, 10, 112.5), ( 'ppfcz5', 15, 11, 119.5), ( 'ppfcz5', 15, 12, 126.5),
( 'ppfcz5', 15, 13, 133.5), ( 'ppfcz5', 15, 14, 140.5), ( 'ppfcz5', 15, 15, 147.5),
( 'ppfcz5', 15, 16, 153.5), ( 'ppfcz5', 15, 17, 161.5), ( 'ppfcz5', 15, 18, 167.5),
( 'ppfcz5', 15, 19, 173.5), ( 'ppfcz5', 15, 20, 179.5), ( 'ppfcz5', 15, 21, 185.5),
( 'ppfcz5', 15, 22, 191.5), ( 'ppfcz5', 15, 23, 197.5), ( 'ppfcz5', 15, 24, 203.5),
( 'ppfcz5', 15, 25, 207.5), ( 'ppfcz5', 15, 26, 212.5), ( 'ppfcz5', 15, 27, 217.5),
( 'ppfcz5', 15, 28, 222.5), ( 'ppfcz5', 15, 29, 227.5), ( 'ppfcz5', 15, 30, 232.5),
( 'ppfcz1', 14, 2, 37.5), ( 'ppfcz1', 14, 3, 41.5), ( 'ppfcz1', 14, 4, 45.5),
( 'ppfcz1', 14, 5, 48.5), ( 'ppfcz1', 14, 6, 52.5), ( 'ppfcz1', 14, 7, 55.5),
( 'ppfcz1', 14, 8, 57.5), ( 'ppfcz1', 14, 9, 59.5), ( 'ppfcz1', 14, 10, 61.5),
( 'ppfcz1', 14, 11, 62.5), ( 'ppfcz1', 14, 12, 63.5), ( 'ppfcz1', 14, 13, 64.5),
( 'ppfcz1', 14, 14, 65.5), ( 'ppfcz1', 14, 15, 66.5), ( 'ppfcz1', 14, 16, 67.5),
( 'ppfcz1', 14, 17, 68.5), ( 'ppfcz1', 14, 18, 69.5), ( 'ppfcz1', 14, 19, 70.5),
( 'ppfcz1', 14, 20, 71.5), ( 'ppfcz1', 14, 21, 72.5), ( 'ppfcz1', 14, 22, 73.5),
( 'ppfcz1', 14, 23, 74.5), ( 'ppfcz1', 14, 24, 75.5), ( 'ppfcz1', 14, 25, 76.5),
( 'ppfcz1', 14, 26, 77.5), ( 'ppfcz1', 14, 27, 78.5), ( 'ppfcz1', 14, 28, 79.5),
( 'ppfcz1', 14, 29, 80.5), ( 'ppfcz1', 14, 30, 81.5), ( 'ppfcz2', 14, 2, 43.5),
( 'ppfcz2', 14, 3, 48.5), ( 'ppfcz2', 14, 4, 53.5), ( 'ppfcz2', 14, 5, 57.5),
( 'ppfcz2', 14, 6, 61.5), ( 'ppfcz2', 14, 7, 65.5), ( 'ppfcz2', 14, 8, 69.5),
( 'ppfcz2', 14, 9, 73.5), ( 'ppfcz2', 14, 10, 77.5), ( 'ppfcz2', 14, 11, 80.5),
( 'ppfcz2', 14, 12, 83.5), ( 'ppfcz2', 14, 13, 86.5), ( 'ppfcz2', 14, 14, 89.5),
( 'ppfcz2', 14, 15, 92.5), ( 'ppfcz2', 14, 16, 94.5), ( 'ppfcz2', 14, 17, 96.5),
( 'ppfcz2', 14, 18, 98.5), ( 'ppfcz2', 14, 19, 99.5), ( 'ppfcz2', 14, 20, 100.5),
( 'ppfcz2', 14, 21, 101.5), ( 'ppfcz2', 14, 22, 102.5), ( 'ppfcz2', 14, 23, 103.5),
( 'ppfcz2', 14, 24, 104.5), ( 'ppfcz2', 14, 25, 105.5), ( 'ppfcz2', 14, 26, 106.5),
( 'ppfcz2', 14, 27, 107.5), ( 'ppfcz2', 14, 28, 108.5), ( 'ppfcz2', 14, 29, 109.5),
( 'ppfcz2', 14, 30, 110.5), ( 'ppfcz3', 14, 2, 47.5), ( 'ppfcz3', 14, 3, 56.5),
( 'ppfcz3', 14, 4, 67.5), ( 'ppfcz3', 14, 5, 78.5), ( 'ppfcz3', 14, 6, 87.5),
( 'ppfcz3', 14, 7, 96.5), ( 'ppfcz3', 14, 8, 105.5), ( 'ppfcz3', 14, 9, 114.5),
( 'ppfcz3', 14, 10, 123.5), ( 'ppfcz3', 14, 11, 131.5), ( 'ppfcz3', 14, 12, 139.5),
( 'ppfcz3', 14, 13, 147.5), ( 'ppfcz3', 14, 14, 155.5), ( 'ppfcz3', 14, 15, 163.5),
( 'ppfcz3', 14, 16, 171.5), ( 'ppfcz3', 14, 17, 179.5), ( 'ppfcz3', 14, 18, 187.5),
( 'ppfcz3', 14, 19, 195.5), ( 'ppfcz3', 14, 20, 203.5), ( 'ppfcz3', 14, 21, 210.5),
( 'ppfcz3', 14, 22, 217.5), ( 'ppfcz3', 14, 23, 224.5), ( 'ppfcz3', 14, 24, 231.5),
( 'ppfcz3', 14, 25, 238.5), ( 'ppfcz3', 14, 26, 245.5), ( 'ppfcz3', 14, 27, 252.5),
( 'ppfcz3', 14, 28, 259.5), ( 'ppfcz3', 14, 29, 266.5), ( 'ppfcz3', 14, 30, 273.5),
( 'ppfcz4', 14, 2, 54.5), ( 'ppfcz4', 14, 3, 68.5), ( 'ppfcz4', 14, 4, 81.5),
( 'ppfcz4', 14, 5, 95.5), ( 'ppfcz4', 14, 6, 108.5), ( 'ppfcz4', 14, 7, 121.5),
( 'ppfcz4', 14, 8, 134.5), ( 'ppfcz4', 14, 9, 147.5), ( 'ppfcz4', 14, 10, 160.5),
( 'ppfcz4', 14, 11, 168.5), ( 'ppfcz4', 14, 12, 178.5), ( 'ppfcz4', 14, 13, 188.5),
( 'ppfcz4', 14, 14, 198.5), ( 'ppfcz4', 14, 15, 208.5), ( 'ppfcz4', 14, 16, 216.5),
( 'ppfcz4', 14, 17, 224.5), ( 'ppfcz4', 14, 18, 232.5), ( 'ppfcz4', 14, 19, 240.5),
( 'ppfcz4', 14, 20, 248.5), ( 'ppfcz4', 14, 21, 256.5), ( 'ppfcz4', 14, 22, 264.5),
( 'ppfcz4', 14, 23, 272.5), ( 'ppfcz4', 14, 24, 280.5), ( 'ppfcz4', 14, 25, 288.5),
( 'ppfcz4', 14, 26, 296.5), ( 'ppfcz4', 14, 27, 304.5), ( 'ppfcz4', 14, 28, 312.5),
( 'ppfcz4', 14, 29, 320.5), ( 'ppfcz4', 14, 30, 328.5), ( 'ppfcz5', 14, 2, 66.5),
( 'ppfcz5', 14, 3, 84.5), ( 'ppfcz5', 14, 4, 102.5), ( 'ppfcz5', 14, 5, 120.5),
( 'ppfcz5', 14, 6, 137.5), ( 'ppfcz5', 14, 7, 154.5), ( 'ppfcz5', 14, 8, 171.5),
( 'ppfcz5', 14, 9, 188.5), ( 'ppfcz5', 14, 10, 205.5), ( 'ppfcz5', 14, 11, 220.5),
( 'ppfcz5', 14, 12, 235.5), ( 'ppfcz5', 14, 13, 250.5), ( 'ppfcz5', 14, 14, 265.5),
( 'ppfcz5', 14, 15, 280.5), ( 'ppfcz5', 14, 16, 295.5), ( 'ppfcz5', 14, 17, 310.5),
( 'ppfcz5', 14, 18, 325.5), ( 'ppfcz5', 14, 19, 340.5), ( 'ppfcz5', 14, 20, 355.5),
( 'ppfcz5', 14, 21, 368.5), ( 'ppfcz5', 14, 22, 381.5), ( 'ppfcz5', 14, 23, 394.5),
( 'ppfcz5', 14, 24, 407.5), ( 'ppfcz5', 14, 25, 420.5), ( 'ppfcz5', 14, 26, 433.5),
( 'ppfcz5', 14, 27, 446.5), ( 'ppfcz5', 14, 28, 459.5), ( 'ppfcz5', 14, 29, 472.5),
( 'ppfcz5', 14, 30, 485.5), ( 'ppfcz1', 30, 0.5, 56.5), ( 'ppfcz1', 30, 1, 63.5),
( 'ppfcz1', 30, 1.5, 69.5), ( 'ppfcz1', 30, 2, 75.5), ( 'ppfcz1', 30, 2.5, 80.5),
( 'ppfcz1', 30, 3, 86.5), ( 'ppfcz1', 30, 3.5, 92.5), ( 'ppfcz1', 30, 4, 99.5),
( 'ppfcz1', 30, 4.5, 105.5), ( 'ppfcz1', 30, 5, 111.5), ( 'ppfcz1', 30, 6, 118.5),
( 'ppfcz1', 30, 7, 126.5), ( 'ppfcz1', 30, 8, 133.5), ( 'ppfcz1', 30, 9, 141.5),
( 'ppfcz1', 30, 10, 148.5), ( 'ppfcz1', 30, 11, 156.5), ( 'ppfcz1', 30, 12, 163.5),
( 'ppfcz1', 30, 13, 171.5), ( 'ppfcz1', 30, 14, 178.5), ( 'ppfcz1', 30, 15, 186.5),
( 'ppfcz1', 30, 16, 193.5), ( 'ppfcz1', 30, 17, 201.5), ( 'ppfcz1', 30, 18, 209.5),
( 'ppfcz1', 30, 19, 216.5), ( 'ppfcz1', 30, 20, 224.5), ( 'ppfcz1', 30, 21, 231.5),
( 'ppfcz1', 30, 22, 239.5), ( 'ppfcz1', 30, 23, 246.5), ( 'ppfcz1', 30, 24, 254.5),
( 'ppfcz1', 30, 25, 261.5), ( 'ppfcz1', 30, 26, 269.5), ( 'ppfcz1', 30, 27, 276.5),
( 'ppfcz1', 30, 28, 284.5), ( 'ppfcz1', 30, 29, 291.5), ( 'ppfcz1', 30, 30, 299.5),
( 'ppfcz2', 30, 0.5, 61.5), ( 'ppfcz2', 30, 1, 65.5), ( 'ppfcz2', 30, 1.5, 75.5),
( 'ppfcz2', 30, 2, 80.5), ( 'ppfcz2', 30, 2.5, 86.5), ( 'ppfcz2', 30, 3, 99.5),
( 'ppfcz2', 30, 3.5, 109.5), ( 'ppfcz2', 30, 4, 113.5), ( 'ppfcz2', 30, 4.5, 121.5),
( 'ppfcz2', 30, 5, 129.5), ( 'ppfcz2', 30, 6, 139.5), ( 'ppfcz2', 30, 7, 149.5),
( 'ppfcz2', 30, 8, 159.5), ( 'ppfcz2', 30, 9, 169.5), ( 'ppfcz2', 30, 10, 180.5),
( 'ppfcz2', 30, 11, 189.5), ( 'ppfcz2', 30, 12, 199.5), ( 'ppfcz2', 30, 13, 210.5),
( 'ppfcz2', 30, 14, 219.5), ( 'ppfcz2', 30, 15, 229.5), ( 'ppfcz2', 30, 16, 240.5),
( 'ppfcz2', 30, 17, 249.5), ( 'ppfcz2', 30, 18, 259.5), ( 'ppfcz2', 30, 19, 270.5),
( 'ppfcz2', 30, 20, 280.5), ( 'ppfcz2', 30, 21, 289.5), ( 'ppfcz2', 30, 22, 300.5),
( 'ppfcz2', 30, 23, 310.5), ( 'ppfcz2', 30, 24, 320.5), ( 'ppfcz2', 30, 25, 330.5),
( 'ppfcz2', 30, 26, 340.5), ( 'ppfcz2', 30, 27, 350.5), ( 'ppfcz2', 30, 28, 360.5),
( 'ppfcz2', 30, 29, 370.5), ( 'ppfcz2', 30, 30, 381.5), ( 'ppfcz3', 30, 0.5, 74.5),
( 'ppfcz3', 30, 1, 83.5), ( 'ppfcz3', 30, 1.5, 90.5), ( 'ppfcz3', 30, 2, 99.5),
( 'ppfcz3', 30, 2.5, 107.5), ( 'ppfcz3', 30, 3, 114.5), ( 'ppfcz3', 30, 3.5, 122.5),
( 'ppfcz3', 30, 4, 130.5), ( 'ppfcz3', 30, 4.5, 140.5), ( 'ppfcz3', 30, 5, 147.5),
( 'ppfcz3', 30, 6, 162.5), ( 'ppfcz3', 30, 7, 174.5), ( 'ppfcz3', 30, 8, 188.5),
( 'ppfcz3', 30, 9, 201.5), ( 'ppfcz3', 30, 10, 213.5), ( 'ppfcz3', 30, 11, 227.5),
( 'ppfcz3', 30, 12, 240.5), ( 'ppfcz3', 30, 13, 252.5), ( 'ppfcz3', 30, 14, 266.5),
( 'ppfcz3', 30, 15, 278.5), ( 'ppfcz3', 30, 16, 290.5), ( 'ppfcz3', 30, 17, 304.5),
( 'ppfcz3', 30, 18, 317.5), ( 'ppfcz3', 30, 19, 330.5), ( 'ppfcz3', 30, 20, 343.5),
( 'ppfcz3', 30, 21, 354.5), ( 'ppfcz3', 30, 22, 363.5), ( 'ppfcz3', 30, 23, 375.5),
( 'ppfcz3', 30, 24, 385.5), ( 'ppfcz3', 30, 25, 396.5), ( 'ppfcz3', 30, 26, 405.5),
( 'ppfcz3', 30, 27, 417.5), ( 'ppfcz3', 30, 28, 428.5), ( 'ppfcz3', 30, 29, 438.5),
( 'ppfcz3', 30, 30, 448.5), ( 'ppfcz4', 30, 0.5, 90.5), ( 'ppfcz4', 30, 1, 104.5),
( 'ppfcz4', 30, 1.5, 118.5), ( 'ppfcz4', 30, 2, 134.5), ( 'ppfcz4', 30, 2.5, 146.5),
( 'ppfcz4', 30, 3, 163.5), ( 'ppfcz4', 30, 3.5, 179.5), ( 'ppfcz4', 30, 4, 195.5),
( 'ppfcz4', 30, 4.5, 211.5), ( 'ppfcz4', 30, 5, 232.5), ( 'ppfcz4', 30, 6, 257.5),
( 'ppfcz4', 30, 7, 278.5), ( 'ppfcz4', 30, 8, 300.5), ( 'ppfcz4', 30, 9, 321.5),
( 'ppfcz4', 30, 10, 343.5), ( 'ppfcz4', 30, 11, 364.5), ( 'ppfcz4', 30, 12, 386.5),
( 'ppfcz4', 30, 13, 407.5), ( 'ppfcz4', 30, 14, 429.5), ( 'ppfcz4', 30, 15, 450.5),
( 'ppfcz4', 30, 16, 472.5), ( 'ppfcz4', 30, 17, 493.5), ( 'ppfcz4', 30, 18, 515.5),
( 'ppfcz4', 30, 19, 536.5), ( 'ppfcz4', 30, 20, 558.5), ( 'ppfcz4', 30, 21, 579.5),
( 'ppfcz4', 30, 22, 601.5), ( 'ppfcz4', 30, 23, 622.5), ( 'ppfcz4', 30, 24, 644.5),
( 'ppfcz4', 30, 25, 665.5), ( 'ppfcz4', 30, 26, 687.5), ( 'ppfcz4', 30, 27, 708.5),
( 'ppfcz4', 30, 28, 730.5), ( 'ppfcz4', 30, 29, 751.5), ( 'ppfcz4', 30, 30, 773.5),
( 'ppfcz5', 30, 0.5, 97.5), ( 'ppfcz5', 30, 1, 114.5), ( 'ppfcz5', 30, 1.5, 131.5),
( 'ppfcz5', 30, 2, 148.5), ( 'ppfcz5', 30, 2.5, 165.5), ( 'ppfcz5', 30, 3, 183.5),
( 'ppfcz5', 30, 3.5, 200.5), ( 'ppfcz5', 30, 4, 221.5), ( 'ppfcz5', 30, 4.5, 243.5),
( 'ppfcz5', 30, 5, 264.5), ( 'ppfcz5', 30, 6, 289.5), ( 'ppfcz5', 30, 7, 313.5),
( 'ppfcz5', 30, 8, 336.5), ( 'ppfcz5', 30, 9, 360.5), ( 'ppfcz5', 30, 10, 384.5),
( 'ppfcz5', 30, 11, 407.5), ( 'ppfcz5', 30, 12, 431.5), ( 'ppfcz5', 30, 13, 455.5),
( 'ppfcz5', 30, 14, 478.5), ( 'ppfcz5', 30, 15, 502.5), ( 'ppfcz5', 30, 16, 526.5),
( 'ppfcz5', 30, 17, 549.5), ( 'ppfcz5', 30, 18, 573.5), ( 'ppfcz5', 30, 19, 597.5),
( 'ppfcz5', 30, 20, 620.5), ( 'ppfcz5', 30, 21, 644.5), ( 'ppfcz5', 30, 22, 668.5),
( 'ppfcz5', 30, 23, 691.5), ( 'ppfcz5', 30, 24, 715.5), ( 'ppfcz5', 30, 25, 738.5),
( 'ppfcz5', 30, 26, 762.5), ( 'ppfcz5', 30, 27, 786.5), ( 'ppfcz5', 30, 28, 809.5),
( 'ppfcz5', 30, 29, 833.5), ( 'ppfcz5', 30, 30, 857.5), ( 'foreign', 13, 30, 0),
( 'all', 32, 10000, 23.2342007434944);

--enable_query_log

INSERT INTO t3 SELECT * FROM t1;
ANALYZE TABLE t1, t2, t3;
EXPLAIN
SELECT d FROM t1, t2
WHERE t2.b=14 AND t2.a=t1.a AND 5.1<t2.c AND t1.b='DE'
ORDER BY t2.c LIMIT 1;
SELECT d FROM t1, t2
WHERE t2.b=14 AND t2.a=t1.a AND 5.1<t2.c AND t1.b='DE'
ORDER BY t2.c LIMIT 1;

EXPLAIN
SELECT d FROM t3 AS t1, t2 AS t2 
WHERE t2.b=14 AND t2.a=t1.a AND 5.1<t2.c AND t1.b='DE'
ORDER BY t2.c LIMIT 1;
SELECT d FROM t3 AS t1, t2 AS t2 
WHERE t2.b=14 AND t2.a=t1.a AND 5.1<t2.c AND t1.b='DE'
ORDER BY t2.c LIMIT 1;

DROP TABLE t1,t2,t3;

--echo #
--echo # WL#1393 - Optimizing filesort with small limit
--echo #

CREATE TABLE t1(f0 int auto_increment primary key, f1 int, f2 varchar(200)) charset latin1;
INSERT INTO t1(f1, f2) VALUES 
(0,"0"),(1,"1"),(2,"2"),(3,"3"),(4,"4"),(5,"5"),
(6,"6"),(7,"7"),(8,"8"),(9,"9"),(10,"10"),
(11,"11"),(12,"12"),(13,"13"),(14,"14"),(15,"15"),
(16,"16"),(17,"17"),(18,"18"),(19,"19"),(20,"20"),
(21,"21"),(22,"22"),(23,"23"),(24,"24"),(25,"25"),
(26,"26"),(27,"27"),(28,"28"),(29,"29"),(30,"30"),
(31,"31"),(32,"32"),(33,"33"),(34,"34"),(35,"35"),
(36,"36"),(37,"37"),(38,"38"),(39,"39"),(40,"40"),
(41,"41"),(42,"42"),(43,"43"),(44,"44"),(45,"45"),
(46,"46"),(47,"47"),(48,"48"),(49,"49"),(50,"50"),
(51,"51"),(52,"52"),(53,"53"),(54,"54"),(55,"55"),
(56,"56"),(57,"57"),(58,"58"),(59,"59"),(60,"60"),
(61,"61"),(62,"62"),(63,"63"),(64,"64"),(65,"65"),
(66,"66"),(67,"67"),(68,"68"),(69,"69"),(70,"70"),
(71,"71"),(72,"72"),(73,"73"),(74,"74"),(75,"75"),
(76,"76"),(77,"77"),(78,"78"),(79,"79"),(80,"80"),
(81,"81"),(82,"82"),(83,"83"),(84,"84"),(85,"85"),
(86,"86"),(87,"87"),(88,"88"),(89,"89"),(90,"90"),
(91,"91"),(92,"92"),(93,"93"),(94,"94"),(95,"95"),
(96,"96"),(97,"97"),(98,"98"),(99,"99");

################
## Test sort when source data fits in memory

SELECT * FROM t1 ORDER BY f1 ASC, f0 LIMIT 100;
SELECT * FROM t1 ORDER BY f1 ASC, f0 LIMIT 30;
SELECT * FROM t1 ORDER BY f1 ASC, f0 LIMIT 0;
SELECT * FROM t1 ORDER BY f2 DESC, f0 LIMIT 30;
SELECT * FROM t1 ORDER BY f2 DESC, f0 LIMIT 0;
SELECT * FROM t1 WHERE f1>10 ORDER BY f2, f0 LIMIT 20;
SELECT * FROM t1 WHERE f1>10 ORDER BY f2, f0 LIMIT 0;
SELECT * FROM t1 WHERE f1>10 ORDER BY f2, f0 LIMIT 10 OFFSET 10;
SELECT * FROM t1 WHERE f1>10 ORDER BY f2, f0 LIMIT 0 OFFSET 10;

################
## Test sort when source data does not fit in memory
set sort_buffer_size= 32768;
CREATE TEMPORARY TABLE tmp (f1 int, f2 varchar(20));
INSERT INTO tmp SELECT f1, f2 FROM t1;
INSERT INTO t1(f1, f2) SELECT * FROM tmp;
INSERT INTO tmp SELECT f1, f2 FROM t1;
INSERT INTO t1(f1, f2) SELECT * FROM tmp;

SELECT * FROM t1 ORDER BY f1 ASC, f0 LIMIT 30;
SELECT * FROM t1 ORDER BY f1 ASC, f0 LIMIT 0;
SELECT * FROM t1 ORDER BY f2 DESC, f0 LIMIT 30;
SELECT * FROM t1 ORDER BY f2 DESC, f0 LIMIT 0;
SELECT * FROM t1 WHERE f1>10 ORDER BY f2, f0 LIMIT 20;
SELECT * FROM t1 WHERE f1>10 ORDER BY f2, f0 LIMIT 0;
SELECT * FROM t1 WHERE f1>10 ORDER BY f2, f0 LIMIT 10 OFFSET 10;
SELECT * FROM t1 WHERE f1>10 ORDER BY f2, f0 LIMIT 0 OFFSET 10;

################
## Test with SQL_CALC_FOUND_ROWS
set sort_buffer_size= 32768;
SELECT SQL_CALC_FOUND_ROWS * FROM t1
ORDER BY f1, f0 LIMIT 30;
SELECT FOUND_ROWS();

SELECT SQL_CALC_FOUND_ROWS * FROM t1
ORDER BY f1, f0 LIMIT 0;
SELECT FOUND_ROWS();

SELECT SQL_CALC_FOUND_ROWS * FROM t1 WHERE f1>10
ORDER BY f2, f0 LIMIT 20;
SELECT FOUND_ROWS();

SELECT SQL_CALC_FOUND_ROWS * FROM t1 WHERE f1>10
ORDER BY f2, f0 LIMIT 0;
SELECT FOUND_ROWS();

SELECT SQL_CALC_FOUND_ROWS * FROM t1 WHERE f1>10
ORDER BY f2, f0 LIMIT 10 OFFSET 10;
SELECT FOUND_ROWS();

SELECT SQL_CALC_FOUND_ROWS * FROM t1 WHERE f1>10
ORDER BY f2, f0 LIMIT 0 OFFSET 10;
SELECT FOUND_ROWS();

################
## Test sorting with join
## These are re-written to use PQ during execution.
set sort_buffer_size= 327680;

SELECT * FROM t1 JOIN tmp on t1.f2=tmp.f2
ORDER BY tmp.f1, f0 LIMIT 30;

SELECT * FROM t1 JOIN tmp on t1.f2=tmp.f2
ORDER BY tmp.f1, f0 LIMIT 30 OFFSET 30;

SELECT SQL_CALC_FOUND_ROWS * FROM t1 JOIN tmp on t1.f2=tmp.f2
ORDER BY tmp.f1, f0 LIMIT 30 OFFSET 30;
SELECT FOUND_ROWS();

SELECT SQL_CALC_FOUND_ROWS * FROM t1 JOIN tmp on t1.f2=tmp.f2
WHERE t1.f2>20
ORDER BY tmp.f1, f0 LIMIT 30 OFFSET 30;
SELECT FOUND_ROWS();

################
## Test views
CREATE VIEW v1 as SELECT * FROM t1 ORDER BY f1, f0 LIMIT 30;
SELECT * FROM v1;
drop view v1;

CREATE VIEW v1 as SELECT * FROM t1 ORDER BY f1, f0 LIMIT 100;
SELECT * FROM v1 ORDER BY f2, f0 LIMIT 30;

CREATE VIEW v2 as SELECT * FROM t1 ORDER BY f2, f0 LIMIT 100;
SELECT * FROM v1 JOIN v2 on v1.f1=v2.f1 ORDER BY v1.f2,v1.f0,v2.f0
LIMIT 30;

################
## Test group & having
SELECT floor(f1/10) f3, count(f2) FROM t1
GROUP BY 1 ORDER BY 2,1 LIMIT 5;

SELECT floor(f1/10) f3, count(f2) FROM t1
GROUP BY 1 ORDER BY 2,1 LIMIT 0;

################
## Test SP
delimiter |;
CREATE PROCEDURE wl1393_sp_test()
BEGIN
SELECT * FROM t1 WHERE f1>10 ORDER BY f2, f0 LIMIT 30;
SELECT * FROM t1 WHERE f1>10 ORDER BY f2, f0 LIMIT 15 OFFSET 15;
SELECT SQL_CALC_FOUND_ROWS * FROM t1 WHERE f1>10
ORDER BY f2, f0 LIMIT 15 OFFSET 15;
SELECT FOUND_ROWS();
SELECT * FROM v1 ORDER BY f2, f0 LIMIT 30;
END|
CALL wl1393_sp_test()|
DROP PROCEDURE wl1393_sp_test|
delimiter ;|

################
## Test with subqueries
SELECT d1.f1, d1.f2 FROM t1
LEFT JOIN (SELECT * FROM t1 ORDER BY f1 LIMIT 30) d1 on t1.f1=d1.f1
ORDER BY d1.f2 DESC LIMIT 30;

SELECT * FROM t1 WHERE f1 = (SELECT f1 FROM t1 ORDER BY 1 LIMIT 1);

--error ER_SUBQUERY_NO_1_ROW
SELECT * FROM t1 WHERE f1 = (SELECT f1 FROM t1 ORDER BY 1 LIMIT 2);

DROP TABLE t1, tmp;
DROP VIEW v1, v2;

--echo # end of WL#1393 - Optimizing filesort with small limit

--echo #
--echo # Bug #58761
--echo # Crash in Field::is_null in field.h on subquery in WHERE clause
--echo #

CREATE TABLE t1 (
  pk INT NOT NULL AUTO_INCREMENT,
  col_int_key INT DEFAULT NULL,
  col_varchar_key VARCHAR(1) DEFAULT NULL,
  PRIMARY KEY (pk),
  KEY col_varchar_key (col_varchar_key,col_int_key)
);

INSERT INTO t1 VALUES (27,7,'x');
INSERT INTO t1 VALUES (28,6,'m');
INSERT INTO t1 VALUES (29,4,'c');

CREATE TABLE where_subselect
  SELECT DISTINCT `pk` AS field1 , `pk` AS field2 
  FROM t1 AS alias1 
  WHERE alias1 . `col_int_key` > 229 
    OR alias1 . `col_varchar_key` IS NOT NULL
  GROUP BY field1, field2
;

--sorted_result
SELECT * 
FROM where_subselect
WHERE (field1, field2) IN (  
  SELECT DISTINCT `pk` AS field1 , `pk` AS field2 
  FROM t1 AS alias1 
  WHERE alias1 . `col_int_key` > 229 
    OR alias1 . `col_varchar_key` IS NOT NULL
  GROUP BY field1, field2
);

DROP TABLE t1;
DROP TABLE where_subselect;

--echo # End of Bug #58761

#
# Bug#35844: Covering index for ref access not compatible with ORDER BY list
#

CREATE TABLE t1 (
  id1 INT NOT NULL,
  id2 INT  NOT NULL,
  junk INT NOT NULL,
  PRIMARY KEY (id1, id2, junk),
  INDEX id2_j_id1 (id2, junk, id1)
);

INSERT INTO t1 VALUES (1, 1, 1), (2, 1, 2), (3, 1, 3), (4, 1, 4);
INSERT INTO t1 VALUES (5, 2, 1), (6, 2, 2), (7, 2, 3), (8, 2, 4);
INSERT INTO t1 VALUES (9, 3, 1), (10, 3, 2), (11, 3, 3), (12, 3, 4);
INSERT INTO t1 VALUES (13, 4, 1), (14, 4, 2), (15, 4, 3), (16, 4, 4);
INSERT INTO t1 VALUES (17, 5, 1), (18, 5, 2), (19, 5, 3), (20, 5, 4);
INSERT INTO t1 VALUES (21, 6, 1), (22, 6, 2), (23, 6, 3), (24, 6, 4);
INSERT INTO t1 VALUES (25, 7, 1), (26, 7, 2), (27, 7, 3), (28, 7, 4);
INSERT INTO t1 VALUES (29, 8, 1), (30, 8, 2), (31, 8, 3), (32, 8, 4);
INSERT INTO t1 VALUES (33, 9, 1), (34, 9, 2), (35, 9, 3), (36, 9, 4);

EXPLAIN SELECT id1 FROM t1 WHERE id2 = 4 ORDER BY id1;

SELECT id1 FROM t1 WHERE id2 = 4 ORDER BY id1;

DROP TABLE t1;



#
# Bug#46454: MySQL wrong index optimisation leads to incorrect result & crashes 
#
CREATE TABLE t1 (
  a INT,
  b INT NOT NULL,
  c char(100),
  KEY (b, c),
  KEY (b, a, c)
)
DEFAULT CHARSET = utf8;

INSERT INTO t1 VALUES 
(1,  1, 1),
(2,  2, 2),
(3,  3, 3),
(4,  4, 4),
(5,  5, 5),
(6,  6, 6),
(7,  7, 7),
(8,  8, 8),
(9,  9, 9);

INSERT INTO t1 SELECT a + 10,  b, c  + 10   FROM t1;
INSERT INTO t1 SELECT a + 20,  b, c  + 20   FROM t1;
INSERT INTO t1 SELECT a + 40,  b, c  + 40   FROM t1;
INSERT INTO t1 SELECT a + 80,  b, c  + 80   FROM t1;
INSERT INTO t1 SELECT a + 160, b, c  + 160  FROM t1;
INSERT INTO t1 SELECT a + 320, b, c  + 320  FROM t1;
INSERT INTO t1 SELECT a + 640, b, c  + 640  FROM t1;
INSERT INTO t1 SELECT a + 1280, b, c + 1280 FROM t1 LIMIT 80;
ANALYZE TABLE t1;
EXPLAIN
SELECT a FROM t1 WHERE b = 1 ORDER BY c DESC LIMIT 9;
SELECT a FROM t1 WHERE b = 1 ORDER BY c DESC LIMIT 9;

--source include/turn_off_only_full_group_by.inc
EXPLAIN
SELECT DISTINCT a FROM t1 WHERE b = 1 ORDER BY c DESC LIMIT 0, 9;
SELECT DISTINCT a FROM t1 WHERE b = 1 ORDER BY c DESC LIMIT 0, 9;
--source include/restore_sql_mode_after_turn_off_only_full_group_by.inc

DROP TABLE t1;

--echo #
--echo # Bug #43029: FORCE INDEX FOR ORDER BY is ignored when join buffering 
--echo #   is used
--echo #

CREATE TABLE t1 (a INT, b INT, KEY (a));

INSERT INTO t1 VALUES (0, NULL), (1, NULL), (2, NULL), (3, NULL);
INSERT INTO t1 SELECT a+4, b FROM t1;
INSERT INTO t1 SELECT a+8, b FROM t1;

CREATE TABLE t2 (a INT, b INT);

INSERT INTO t2 VALUES (0,NULL), (1,NULL), (2,NULL), (3,NULL), (4,NULL);
INSERT INTO t2 SELECT a+4, b FROM t2;

--echo # shouldn't have "using filesort"
EXPLAIN 
SELECT * FROM t1 FORCE INDEX FOR ORDER BY (a), t2 WHERE t1.a < 2 ORDER BY t1.a;

--echo # should have "using filesort"
EXPLAIN 
SELECT * FROM t1 USE INDEX FOR ORDER BY (a), t2 WHERE t1.a < 2 ORDER BY t1.a;

--echo # should have "using filesort"
EXPLAIN 
SELECT * FROM t1 FORCE INDEX FOR JOIN (a), t2 WHERE t1.a < 2 ORDER BY t1.a;

DROP TABLE t1, t2;

--echo #
--echo # Bug #50394: Regression in EXPLAIN with index scan, LIMIT, GROUP BY and
--echo # ORDER BY computed col
--echo #
CREATE TABLE t1 ( a INT NOT NULL, b INT NOT NULL, KEY( a, b ) );

INSERT INTO t1 VALUES (1, 1), (2, 2), (3, 3), (4, 4), (5, 5);
INSERT INTO t1 SELECT a + 5, b + 5 FROM t1;

CREATE TABLE t2( a INT PRIMARY KEY, b INT );

INSERT INTO t2 VALUES (1, 1), (2, 2), (3, 3), (4, 4), (5, 5);
INSERT INTO t2 SELECT a + 5, b + 5 FROM t2;
ANALYZE TABLE t1, t2;
EXPLAIN
SELECT count(*) AS c, t1.a
FROM t1 JOIN t2 ON t1.b = t2.a
WHERE t2.b = 1
GROUP BY t1.a
ORDER by c
LIMIT 2;

DROP TABLE t1, t2;


--echo #
--echo # Bug #59110: Memory leak of QUICK_SELECT_I allocated memory 
--echo #  and
--echo # Bug #59308: Incorrect result for 
--echo               SELECT DISTINCT <col>... ORDER BY <col> DESC 
--echo
--echo # Use Valgrind to detect #59110!
--echo #

CREATE TABLE t1 (a INT,KEY (a));
INSERT INTO t1 VALUES (1),(2),(3),(4),(5),(6),(7),(8),(9),(10);
ANALYZE TABLE t1;
EXPLAIN SELECT DISTINCT a,1 FROM t1 WHERE a <> 1 ORDER BY a DESC;
SELECT DISTINCT a,1 FROM t1 WHERE a <> 1 ORDER BY a DESC;

DROP TABLE t1;

--echo #
--echo # Bug#11765255 58201:
--echo # VALGRIND/CRASH WHEN ORDERING BY MULTIPLE AGGREGATE FUNCTIONS
--echo #

select count(*) order by max(1) + min(1);

--echo End of 5.1 tests


--echo #
--echo # Bug #38745: MySQL 5.1 optimizer uses filesort for ORDER BY
--echo #             when it should use index
--echo #

CREATE TABLE t1 (i1 integer NOT NULL PRIMARY KEY);
CREATE TABLE t2 (i2 integer NOT NULL PRIMARY KEY);
CREATE TABLE t3 (i3 integer);

INSERT INTO t1 VALUES (1), (2), (3), (4), (5), (6), (7), (8), (9), (10), (11), (12);
INSERT INTO t2 SELECT * FROM t1;
ANALYZE TABLE t1, t2, t3;
EXPLAIN
SELECT t1.*, t2.* FROM t1 JOIN t2 ON t1.i1 = t2.i2 
                     LEFT JOIN t3 ON t2.i2 = t3.i3
  ORDER BY t1.i1 LIMIT 5;

SELECT t1.*, t2.* FROM t1 JOIN t2 ON t1.i1 = t2.i2 
                     LEFT JOIN t3 ON t2.i2 = t3.i3
  ORDER BY t1.i1 LIMIT 5;

DROP TABLE t1, t2, t3;

--echo #
--echo # Bug #11885377 VOID JOIN_READ_KEY_UNLOCK_ROW(ST_JOIN_TABLE*): ASSERTION
--echo # `TAB->REF.USE_COUNT'
--echo #

CREATE TABLE t1(a INT PRIMARY KEY);
CREATE TABLE t2(b INT,c INT);
INSERT INTO t1 VALUES (1), (2);
INSERT INTO t2 VALUES (1,2), (2,3);
SELECT (SELECT 1 FROM t1 WHERE a=b AND c=1 ORDER BY a DESC) FROM t2;
DROP TABLE t1, t2;

--echo #
--echo # Bug #13531865
--echo # TEST_IF_SKIP_SORT_ORDER() INCORRECTLY SKIP FILESORT IF
--echo # 'TYPE' IS REF_OR_NULL 
--echo # 
--echo #

CREATE TABLE t1 (
  a INT,
  c INT,
  UNIQUE KEY a_c (a,c),
  KEY (a)) engine=myisam;

INSERT INTO t1 VALUES (1,10), (2,NULL), (2,10);
ANALYZE TABLE t1;

--echo # Using 'KEY a_c' for order-by opt, would have required 
--echo # REF_OR_NULL access which never can be order_by skipped.
--echo # -> Keep initial REF on 'KEY a' selected by cond. optimizer
EXPLAIN
SELECT c FROM t1 WHERE a=2 AND (c=10 OR c IS NULL);
EXPLAIN
SELECT c FROM t1 WHERE a=2 AND (c=10 OR c IS NULL) ORDER BY c;
SELECT c FROM t1 WHERE a=2 AND (c=10 OR c IS NULL) ORDER BY c;

EXPLAIN
SELECT c FROM t1 WHERE a=2 AND (c=10 OR c IS NULL) ORDER BY c DESC;
SELECT c FROM t1 WHERE a=2 AND (c=10 OR c IS NULL) ORDER BY c DESC;

DROP TABLE t1;

--echo #
--echo # Bug #13528826 
--echo # TEST_IF_CHEAPER_ORDERING(): CALCULATES INCORRECT 'SELECT_LIMIT'
--echo # 
--echo #

CREATE TABLE t1(a int PRIMARY KEY, b int) ENGINE=myisam;
INSERT INTO t1 VALUES
  (5, 10), (2, 70), (7, 80), (6, 20), (1, 50), (9, 40), (8, 30), (3, 60);
CREATE TABLE t2 (p int, a int, INDEX i_a(a)) ENGINE=myisam;
INSERT INTO t2 VALUES
  (103, 7), (109, 3), (102, 3), (108, 1), (106, 3),
  (107, 7), (105, 1), (101, 3), (100, 7), (110, 1);

--echo # number of rows in t1 was incorrectly used as an 
--echo # implicit limit-clause if not explicit specified
EXPLAIN
SELECT t1.a FROM t1 LEFT JOIN t2 ON t1.a=t2.a ORDER BY t1.a;

--echo # Query above used to be explained identical to this:
EXPLAIN
SELECT t1.a FROM t1 LEFT JOIN t2 ON t1.a=t2.a ORDER BY t1.a LIMIT 8;

-- echo # A really high limit was required to give the correct explain
EXPLAIN
SELECT t1.a FROM t1 LEFT JOIN t2 ON t1.a=t2.a ORDER BY t1.a LIMIT 1000;

DROP TABLE t1, t2;

--echo #
--echo # Bug #13949068 ASSERT TAB->REF.KEY == REF_KEY IN
--echo # PLAN_CHANGE_WATCHDOG::~PLAN_CHANGE_WATCHDOG
--echo #
CREATE TABLE t1 (a INT, b INT, KEY(b), KEY(b,a)) ENGINE=INNODB;
INSERT INTO t1 VALUES (0,0);
let $query=SELECT DISTINCT a FROM t1 WHERE b=1 ORDER BY 1;
eval EXPLAIN $query;
eval $query;
DROP TABLE t1;

--echo #
--echo # Bug#18636076 DEBUG CRASH ON 
--echo #              PLAN_CHANGE_WATCHDOG::~PLAN_CHANGE_WATCHDOG
--echo #

# Both tables must be created using MyISAM in order to reproduce assert
CREATE TABLE t1 (
  pk INTEGER NOT NULL,
  PRIMARY KEY (pk)
) ENGINE=MyISAM;

INSERT INTO t1 VALUES (1), (2), (3);

CREATE VIEW view_t1 AS SELECT * FROM t1;

CREATE TABLE t2 (
  i1 INTEGER NOT NULL
) ENGINE=MyISAM;

let query=
SELECT DISTINCT t2.i1, view_t1.pk
FROM view_t1 LEFT JOIN t2
ON  view_t1.pk = t2.i1
WHERE (view_t1.pk <= 204 AND t2.i1 != 3)
  OR view_t1.pk BETWEEN 1 AND 7
ORDER BY t2.i1;

eval EXPLAIN $query;
eval $query;

DROP VIEW view_t1;
DROP TABLE t1, t2;

SET sql_mode = default;

--echo #
--echo # Bug#20455386 INVALID WRITE IN COPY_FIELD::SET CAUSE CRASH WITH TMP TABLES
--echo #

CREATE TABLE t1(a INT) ENGINE=INNODB;
CREATE TABLE t2(c INT) ENGINE=INNODB;
CREATE TABLE t3(b BLOB NOT NULL) ENGINE=INNODB;
SELECT
 (SELECT 1 FROM t1
  WHERE (SELECT c FROM t2 ORDER BY COUNT(t3.b ))
 )
FROM t3;
DROP TABLE t1,t2,t3;

--echo #
--echo # Bug#16833464: Presence of ORDER BY changes data type of column in
--echo #               CREATE TABLE SELECT

CREATE TABLE z1 (c05 TIME);
(SELECT * FROM z1 ORDER BY '1') ORDER BY '1';

CREATE TABLE z2 (SELECT * FROM z1 ORDER BY '1') ORDER BY '1';
CREATE TABLE z3 (SELECT * FROM z1) ORDER BY '1';

#Column in z2 and z3 should be of same type.
SHOW CREATE TABLE z2;
SHOW CREATE TABLE z3;

DROP TABLE z1, z2, z3;

--echo #
--echo # Bug#32695670: ASSERTION `M_CURRENT_ROLLUP_LEVEL >= 0 && M_CURRENT_ROLLUP_LEVEL < M_NUM_LEVELS'
--echo #
CREATE TABLE t(
  m MEDIUMTEXT,   # must be blob, CHAR(1), TEXT works....
  pk INT PRIMARY KEY
);

INSERT INTO t VALUES ('s',1);

# Used to hit assert in debug mode
SELECT MIN(alias2.m) AS field2,
       alias2.pk AS field3
FROM t AS alias1
     LEFT JOIN
     t AS alias2
     ON alias1.m = alias2.m
GROUP BY field3 WITH ROLLUP
ORDER BY UPPER(field2); # The alias must be a function argument

DROP TABLE t;

CREATE TABLE t1 ( a INTEGER, b LONGTEXT );
INSERT INTO t1 VALUES (1,'z');
INSERT INTO t1 VALUES (2,'y');
INSERT INTO t1 VALUES (3,'x');

# Used to give correct result
SELECT MIN(b) AS min_b FROM t1 GROUP BY a ORDER BY COALESCE(MIN(b), 'a');
# Used to give wrong result
SELECT MIN(b) AS min_b FROM t1 GROUP BY a ORDER BY COALESCE(min_b, 'a');

SELECT MIN(b) AS min_b FROM t1 GROUP BY a ORDER BY COALESCE(MIN(b), 'a') DESC;
SELECT MIN(b) AS min_b FROM t1 GROUP BY a ORDER BY COALESCE(min_b, 'a') DESC;

DROP TABLE t1;
