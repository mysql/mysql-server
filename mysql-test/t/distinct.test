--source include/elide_costs.inc

#
# Bug with distinct and INSERT INTO
# Bug with group by and not used fields
#

CREATE TABLE t1 (id int,facility char(20));
CREATE TABLE t2 (facility char(20));
INSERT INTO t1 VALUES (NULL,NULL);
INSERT INTO t1 VALUES (-1,'');
INSERT INTO t1 VALUES (0,'');
INSERT INTO t1 VALUES (1,'/L');
INSERT INTO t1 VALUES (2,'A01');
INSERT INTO t1 VALUES (3,'ANC');
INSERT INTO t1 VALUES (4,'F01');
INSERT INTO t1 VALUES (5,'FBX');
INSERT INTO t1 VALUES (6,'MT');
INSERT INTO t1 VALUES (7,'P');
INSERT INTO t1 VALUES (8,'RV');
INSERT INTO t1 VALUES (9,'SRV');
INSERT INTO t1 VALUES (10,'VMT');
INSERT INTO t2 SELECT DISTINCT FACILITY FROM t1;

select id from t1 group by id;
select * from t1 order by id;
select id-5,facility from t1 order by "id-5";
--source include/turn_off_only_full_group_by.inc
select id,concat(facility) from t1 group by id ;
select id+0 as a,max(id),concat(facility) as b from t1 group by a order by b desc,a;
--source include/restore_sql_mode_after_turn_off_only_full_group_by.inc
select id >= 0 and id <= 5 as grp,count(*) from t1 group by grp;

SELECT DISTINCT FACILITY FROM t1;
SELECT FACILITY FROM t2;
SELECT count(*) from t1,t2 where t1.facility=t2.facility;
select count(facility) from t1;
select count(*) from t1;
select count(*) from t1 where facility IS NULL;
select count(*) from t1 where facility = NULL;
select count(*) from t1 where facility IS NOT NULL;
select count(*) from t1 where id IS NULL;
select count(*) from t1 where id IS NOT NULL;

drop table t1,t2;

#
# Problem with distinct without results
#
CREATE TABLE t1 (UserId int(11) DEFAULT '0' NOT NULL);
INSERT INTO t1 VALUES (20);
INSERT INTO t1 VALUES (27);

SELECT UserId FROM t1 WHERE Userid=22;
SELECT UserId FROM t1 WHERE UserId=22 group by Userid;
SELECT DISTINCT UserId FROM t1 WHERE UserId=22 group by Userid;
SELECT DISTINCT UserId FROM t1 WHERE UserId=22;
drop table t1;

#
# Test of distinct
#

CREATE TABLE t1 (a int(10) unsigned not null primary key,b int(10) unsigned);
INSERT INTO t1 VALUES (1,1),(2,1),(3,1),(4,1);
CREATE TABLE t2 (a int(10) unsigned not null, key (A));
INSERT INTO t2 VALUES (1),(2);
CREATE TABLE t3 (a int(10) unsigned, key(A), b text);
INSERT INTO t3 VALUES (1,'1'),(2,'2');
SELECT DISTINCT t3.b FROM t3,t2,t1 WHERE t3.a=t1.b AND t1.a=t2.a;
INSERT INTO t2 values (1),(2),(3);
INSERT INTO t3 VALUES (1,'1'),(2,'2'),(1,'1'),(2,'2');
ANALYZE TABLE t1, t2, t3;
explain SELECT distinct t3.a FROM t3,t2,t1 WHERE t3.a=t1.b AND t1.a=t2.a;
SELECT distinct t3.a FROM t3,t2,t1 WHERE t3.a=t1.b AND t1.a=t2.a;

# Create a lot of data into t3;
create temporary table t4 select * from t3;
insert into t3 select * from t4;
insert into t4 select * from t3;
insert into t3 select * from t4;
insert into t4 select * from t3;
insert into t3 select * from t4;
insert into t4 select * from t3;
insert into t3 select * from t4;

ANALYZE TABLE t1,t2,t3;

explain select distinct t1.a from t1,t3 where t1.a=t3.a;

# This query uses the "not used in distinct" optimization (search for
# not_used_in_distinct in code); it means that when we have a partial
# record for t1, and we find a match in t3, we have a combination
# rec_t1|rec_t3_a, and thus (DISTINCT is used and the SELECT list does
# not contain columns of t2) we have rec_t1 in the result set; we don't
# need to explore more records of t3, as all it would do is producing
# more rec_t1|rec_t3_x, which would all be eliminated (due to
# DISTINCT).
# Without this optimization, the first query does ~200 read_next instead
# of 4.
flush status;
select distinct t1.a from t1,t3 where t1.a=t3.a;
--skip_if_hypergraph  # Does not support rewriting DISTINCT to antijoins.
show status like 'Handler_read%';
flush status;
select distinct 1 from t1,t3 where t1.a=t3.a;
--skip_if_hypergraph  # Does not support rewriting DISTINCT to antijoins.
show status like 'Handler_read%';

let $iteration=2;
let $q_type= explain;
# First EXPLAIN query, then execute query
while($iteration)
{
  eval $q_type SELECT distinct t1.a from t1;
  eval $q_type SELECT distinct t1.a from t1 order by a desc;
  eval $q_type SELECT t1.a from t1 group by a order by a desc;
  eval $q_type SELECT distinct t1.a from t1 order by a desc limit 1;
  eval $q_type SELECT distinct a from t3 order by a desc limit 2;
  eval $q_type SELECT distinct a,b from t3 order by a+1;
  eval $q_type SELECT distinct a,b from t3 order by a limit 2;
  eval $q_type SELECT a,b from t3 group by a,b order by a+1;

  let $q_type=;
  dec $iteration;
}

drop table t1,t2,t3,t4;

CREATE TABLE t1 (name varchar(255));
INSERT INTO t1 VALUES ('aa'),('ab'),('ac'),('ad'),('ae');
SELECT DISTINCT * FROM t1 LIMIT 2;
SELECT DISTINCT name FROM t1 LIMIT 2;
SELECT DISTINCT 1 FROM t1 LIMIT 2;
drop table t1;

CREATE TABLE t1 (
  ID int(11) NOT NULL auto_increment,
  NAME varchar(75) DEFAULT '' NOT NULL,
  LINK_ID int(11) DEFAULT '0' NOT NULL,
  PRIMARY KEY (ID),
  KEY NAME (NAME),
  KEY LINK_ID (LINK_ID)
);

INSERT INTO t1 (ID, NAME, LINK_ID) VALUES (1,'Mike',0),(2,'Jack',0),(3,'Bill',0);

CREATE TABLE t2 (
  ID int(11) NOT NULL auto_increment,
  NAME varchar(150) DEFAULT '' NOT NULL,
  PRIMARY KEY (ID),
  KEY NAME (NAME)
);

# We see the functional dependency implied by ON condition

SELECT DISTINCT
    t2.id AS key_link_id,
    t2.name AS link
FROM t1
LEFT JOIN t2 ON t1.link_id=t2.id
GROUP BY t1.id
ORDER BY link;

drop table t1,t2;

#
# Problem with table dependencies
#

create table t1 (
    id		int not null,
    name	tinytext not null,
    unique	(id)
);
create table t2 (
    id		int not null,
    idx		int not null,
    unique	(id, idx)
);
create table t3 (
    id		int not null,
    idx		int not null,
    unique	(id, idx)
);
insert into t1 values (1,'yes'), (2,'no');
insert into t2 values (1,1);
insert into t3 values (1,1);
EXPLAIN
SELECT DISTINCT
    t1.id
from
    t1
    straight_join
    t2
    straight_join
    t3
    straight_join
    t1 as j_lj_t2 left join t2 as t2_lj
        on j_lj_t2.id=t2_lj.id
    straight_join
    t1 as j_lj_t3 left join t3 as t3_lj
        on j_lj_t3.id=t3_lj.id
WHERE
    ((t1.id=j_lj_t2.id AND t2_lj.id IS NULL) OR (t1.id=t2.id AND t2.idx=2))
    AND ((t1.id=j_lj_t3.id AND t3_lj.id IS NULL) OR (t1.id=t3.id AND t3.idx=2));
SELECT DISTINCT
    t1.id
from
    t1
    straight_join
    t2
    straight_join
    t3
    straight_join
    t1 as j_lj_t2 left join t2 as t2_lj
        on j_lj_t2.id=t2_lj.id
    straight_join
    t1 as j_lj_t3 left join t3 as t3_lj
        on j_lj_t3.id=t3_lj.id
WHERE
    ((t1.id=j_lj_t2.id AND t2_lj.id IS NULL) OR (t1.id=t2.id AND t2.idx=2))
    AND ((t1.id=j_lj_t3.id AND t3_lj.id IS NULL) OR (t1.id=t3.id AND t3.idx=2));
drop table t1,t2,t3;

#
# Test using DISTINCT on a function that contains a group function
# This also test the case when one doesn't use all fields in GROUP BY.
#

create table t1 (a int not null, b int not null, t time);
insert into t1 values (1,1,"00:06:15"),(1,2,"00:06:15"),(1,2,"00:30:15"),(1,3,"00:06:15"),(1,3,"00:30:15");
select a,sec_to_time(sum(time_to_sec(t))) from t1 group by a,b;
select distinct a,sec_to_time(sum(time_to_sec(t))) from t1 group by a,b;
create table t2 (a int not null primary key, b int);
insert into t2 values (1,1),(2,2),(3,3);
select t1.a,sec_to_time(sum(time_to_sec(t))) from t1 left join t2 on (t1.b=t2.a) group by t1.a,t2.b;
select distinct t1.a,sec_to_time(sum(time_to_sec(t))) from t1 left join t2 on (t1.b=t2.a) group by t1.a,t2.b;
drop table t1,t2;

#
# Test problem with DISTINCT and HAVING
#
create table t1 (a int not null,b char(5), c text);
insert into t1 (a) values (1),(2),(3),(4),(1),(2),(3),(4);
select distinct a from t1 group by b,a having a > 2 order by a desc;
select distinct a,c from t1 group by b,c,a having a > 2 order by a desc;
drop table t1;

#
# Test problem with DISTINCT and ORDER BY DESC
#

create table t1 (a char(1), key(a)) engine=myisam;
insert into t1 values('1'),('1');
select * from t1 where a >= '1'; 
select distinct a from t1 order by a desc;
select distinct a from t1 where a >= '1' order by a desc;
drop table t1;

#
# Test when using a not previously used column in ORDER BY
#

CREATE TABLE t1 (email varchar(50), infoID BIGINT, dateentered DATETIME);
CREATE TABLE t2 (infoID BIGINT, shipcode varchar(10));

INSERT INTO t1 (email, infoID, dateentered) VALUES
      ('test1@testdomain.com', 1, '2002-07-30 22:56:38'),
      ('test1@testdomain.com', 1, '2002-07-27 22:58:16'),
      ('test2@testdomain.com', 1, '2002-06-19 15:22:19'),
      ('test2@testdomain.com', 2, '2002-06-18 14:23:47'),
      ('test3@testdomain.com', 1, '2002-05-19 22:17:32');

INSERT INTO t2(infoID, shipcode) VALUES
      (1, 'Z001'),
      (2, 'R002');

--sorted_result
SELECT DISTINCTROW email, shipcode FROM t1, t2 WHERE t1.infoID=t2.infoID;
--source include/turn_off_only_full_group_by.inc
SELECT DISTINCTROW email FROM t1 ORDER BY dateentered DESC;
SELECT DISTINCTROW email, shipcode FROM t1, t2 WHERE t1.infoID=t2.infoID ORDER BY dateentered DESC;
--source include/restore_sql_mode_after_turn_off_only_full_group_by.inc
drop table t1,t2;

#
# test with table.* in DISTINCT
#
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
CREATE TABLE t1 (privatemessageid int(10) unsigned NOT NULL auto_increment,  folderid smallint(6) NOT NULL default '0',  userid int(10) unsigned NOT NULL default '0',  touserid int(10) unsigned NOT NULL default '0',  fromuserid int(10) unsigned NOT NULL default '0',  title varchar(250) NOT NULL default '',  message mediumtext NOT NULL,  dateline int(10) unsigned NOT NULL default '0',  showsignature smallint(6) NOT NULL default '0',  iconid smallint(5) unsigned NOT NULL default '0',  messageread smallint(6) NOT NULL default '0',  readtime int(10) unsigned NOT NULL default '0',  receipt smallint(6) unsigned NOT NULL default '0',  deleteprompt smallint(6) unsigned NOT NULL default '0',  multiplerecipients smallint(6) unsigned NOT NULL default '0',  PRIMARY KEY  (privatemessageid),  KEY userid (userid)) ENGINE=MyISAM;
INSERT INTO t1 VALUES (128,0,33,33,8,':D','',996121863,1,0,2,996122850,2,0,0);
CREATE TABLE t2 (userid int(10) unsigned NOT NULL auto_increment,  usergroupid smallint(5) unsigned NOT NULL default '0',  username varchar(50) NOT NULL default '',  password varchar(50) NOT NULL default '',  email varchar(50) NOT NULL default '',  styleid smallint(5) unsigned NOT NULL default '0',  parentemail varchar(50) NOT NULL default '',  coppauser smallint(6) NOT NULL default '0',  homepage varchar(100) NOT NULL default '',  icq varchar(20) NOT NULL default '',  aim varchar(20) NOT NULL default '',  yahoo varchar(20) NOT NULL default '',  signature mediumtext NOT NULL,  adminemail smallint(6) NOT NULL default '0',  showemail smallint(6) NOT NULL default '0',  invisible smallint(6) NOT NULL default '0',  usertitle varchar(250) NOT NULL default '',  customtitle smallint(6) NOT NULL default '0',  joindate int(10) unsigned NOT NULL default '0',  cookieuser smallint(6) NOT NULL default '0',  daysprune smallint(6) NOT NULL default '0',  lastvisit int(10) unsigned NOT NULL default '0',  lastactivity int(10) unsigned NOT NULL default '0',  lastpost int(10) unsigned NOT NULL default '0',  posts smallint(5) unsigned NOT NULL default '0',  timezoneoffset varchar(4) NOT NULL default '',  emailnotification smallint(6) NOT NULL default '0',  buddylist mediumtext NOT NULL,  ignorelist mediumtext NOT NULL,  pmfolders mediumtext NOT NULL,  receivepm smallint(6) NOT NULL default '0',  emailonpm smallint(6) NOT NULL default '0',  pmpopup smallint(6) NOT NULL default '0',  avatarid smallint(6) NOT NULL default '0',  avatarrevision int(6) unsigned NOT NULL default '0',  options smallint(6) NOT NULL default '15',  birthday date NOT NULL default '0000-00-00',  maxposts smallint(6) NOT NULL default '-1',  startofweek smallint(6) NOT NULL default '1',  ipaddress varchar(20) NOT NULL default '',  referrerid int(10) unsigned NOT NULL default '0',  nosessionhash smallint(6) NOT NULL default '0',  autorefresh smallint(6) NOT NULL default '-1',  messagepopup tinyint(2) NOT NULL default '0',  inforum smallint(5) unsigned NOT NULL default '0',  ratenum smallint(5) unsigned NOT NULL default '0',  ratetotal smallint(5) unsigned NOT NULL default '0',  allowrate smallint(5) unsigned NOT NULL default '1',  PRIMARY KEY  (userid),  KEY usergroupid (usergroupid),  KEY username (username),  KEY inforum (inforum)) ENGINE=MyISAM;
INSERT INTO t2 VALUES (33,6,'Kevin','0','kevin@stileproject.com',1,'',0,'http://www.stileproject.com','','','','',1,1,0,'Administrator',0,996120694,1,-1,1030996168,1031027028,1030599436,36,'-6',0,'','','',1,0,1,0,0,15,'0000-00-00',-1,1,'64.0.0.0',0,1,-1,0,0,4,19,1);
SELECT DISTINCT t1.*, t2.* FROM t1 LEFT JOIN t2 ON (t2.userid = t1.touserid);
DROP TABLE t1,t2;
SET sql_mode = default;
#
# test with const_item in ORDER BY
#

CREATE TABLE t1 (a int primary key, b int, c int);
INSERT t1 VALUES (1,2,3);
CREATE TABLE t2 (a int primary key, b int, c int);
INSERT t2 VALUES (3,4,5);
--source include/turn_off_only_full_group_by.inc
SELECT DISTINCT t1.a, t2.b FROM t1, t2 WHERE t1.a=1 ORDER BY t2.c;
--source include/restore_sql_mode_after_turn_off_only_full_group_by.inc
DROP TABLE t1,t2;

#
# Test of LEFT() with distinct
#

CREATE table t1 (  `id` int(11) NOT NULL auto_increment,  `name` varchar(50) NOT NULL default '',  PRIMARY KEY  (`id`)) ENGINE=MyISAM AUTO_INCREMENT=3 ;
INSERT INTO t1 VALUES (1, 'aaaaa');
INSERT INTO t1 VALUES (3, 'aaaaa');
INSERT INTO t1 VALUES (2, 'eeeeeee');
select distinct left(name,1) as name from t1;
drop  table t1; 

#
# Test case from sel000100
#

CREATE TABLE t1 (
  ID int(11) NOT NULL auto_increment,
  NAME varchar(75) DEFAULT '' NOT NULL,
  LINK_ID int(11) DEFAULT '0' NOT NULL,
  PRIMARY KEY (ID),
  KEY NAME (NAME),
  KEY LINK_ID (LINK_ID)
);

INSERT INTO t1 (ID, NAME, LINK_ID) VALUES (1,'Mike',0);
INSERT INTO t1 (ID, NAME, LINK_ID) VALUES (2,'Jack',0);
INSERT INTO t1 (ID, NAME, LINK_ID) VALUES (3,'Bill',0);

CREATE TABLE t2 (
  ID int(11) NOT NULL auto_increment,
  NAME varchar(150) DEFAULT '' NOT NULL,
  PRIMARY KEY (ID),
  KEY NAME (NAME)
);

SELECT DISTINCT
    t2.id AS key_link_id,
    t2.name AS link
FROM t1
LEFT JOIN t2 ON t1.link_id=t2.id
GROUP BY t1.id
ORDER BY link;

drop table t1,t2;

#
# test case for #674
#

CREATE TABLE t1 (
  html varchar(5) default NULL,
  rin int(11) default '0',
  rout int(11) default '0'
) ENGINE=MyISAM;

INSERT INTO t1 VALUES ('1',1,0);
--source include/turn_off_only_full_group_by.inc
SELECT DISTINCT html,SUM(rout)/(SUM(rin)+1) as 'prod' FROM t1 GROUP BY rin;
--source include/restore_sql_mode_after_turn_off_only_full_group_by.inc
drop table t1;

#
# Test cases for #12625: DISTINCT for a list with constants
#

CREATE TABLE t1 (a int);
INSERT INTO t1 VALUES (1),(2),(3),(4),(5);
SELECT DISTINCT a, 1 FROM t1;
SELECT DISTINCT 1, a FROM t1;

CREATE TABLE t2 (a int, b int); 
INSERT INTO t2 VALUES (1,1),(2,2),(2,3),(2,4),(3,5);
SELECT DISTINCT a, b, 2 FROM t2;
SELECT DISTINCT 2, a, b FROM t2;
SELECT DISTINCT a, 2, b FROM t2;

DROP TABLE t1,t2;
#
# Bug#16458: Simple SELECT FOR UPDATE causes "Result Set not updatable" 
#   error.
#
CREATE TABLE t1(a INT PRIMARY KEY, b INT);
INSERT INTO t1 VALUES (1,1), (2,1), (3,1);
ANALYZE TABLE t1;
EXPLAIN SELECT DISTINCT a FROM t1;
EXPLAIN SELECT DISTINCT a,b FROM t1;
EXPLAIN SELECT DISTINCT t1_1.a, t1_1.b FROM t1 t1_1, t1 t1_2;
EXPLAIN SELECT DISTINCT t1_1.a, t1_1.b FROM t1 t1_1, t1 t1_2
  WHERE t1_1.a = t1_2.a;
EXPLAIN SELECT a FROM t1 GROUP BY a;
EXPLAIN SELECT a,b FROM t1 GROUP BY a,b;
EXPLAIN SELECT DISTINCT a,b FROM t1 GROUP BY a,b;

CREATE TABLE t2(a INT, b INT NOT NULL, c INT NOT NULL, d INT, 
                PRIMARY KEY (a,b));
INSERT INTO t2 VALUES (1,1,1,50), (1,2,3,40), (2,1,3,4);
ANALYZE TABLE t2;
EXPLAIN SELECT DISTINCT a FROM t2;
EXPLAIN SELECT DISTINCT a,a FROM t2;
EXPLAIN SELECT DISTINCT b,a FROM t2;
EXPLAIN SELECT DISTINCT a,c FROM t2;
EXPLAIN SELECT DISTINCT c,a,b FROM t2;

--source include/turn_off_only_full_group_by.inc
EXPLAIN SELECT DISTINCT a,b,d FROM t2 GROUP BY c,b,d;
# After adding unique constraint, GROUP BY is ok.
--source include/restore_sql_mode_after_turn_off_only_full_group_by.inc
CREATE UNIQUE INDEX c_b_unq ON t2 (c,b);
ANALYZE TABLE t2;
EXPLAIN SELECT DISTINCT a,b,d FROM t2 GROUP BY c,b,d;

DROP TABLE t1,t2;

# Bug 9784 DISTINCT IFNULL truncates data
#
create table t1 (id int, dsc varchar(50));
insert into t1 values (1, "line number one"), (2, "line number two"), (3, "line number three");
select distinct id, IFNULL(dsc, '-') from t1;
drop table t1;

#
# Bug 21456: SELECT DISTINCT(x) produces incorrect results when using order by
#
CREATE TABLE t1 (a int primary key, b int);

INSERT INTO t1 (a,b) values (1,1), (2,3), (3,2);
ANALYZE TABLE t1;
explain SELECT DISTINCT a, b FROM t1 ORDER BY b;
SELECT DISTINCT a, b FROM t1 ORDER BY b;
DROP TABLE t1;

# End of 4.1 tests


#
# Bug #15745 ( COUNT(DISTINCT CONCAT(x,y)) returns wrong result)
#
CREATE TABLE t1 (
  ID int(11) NOT NULL auto_increment,
  x varchar(20) default NULL,
  y decimal(10,0) default NULL,
  PRIMARY KEY  (ID),
  KEY (y)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

INSERT INTO t1 VALUES
(1,'ba','-1'),
(2,'ba','1150'),
(306,'ba','-1'),
(307,'ba','1150'),
(611,'ba','-1'),
(612,'ba','1150');

select count(distinct x,y) from t1;
select count(distinct concat(x,y)) from t1;
drop table t1;

#
# Bug #18068: SELECT DISTINCT
#
CREATE TABLE t1 (a INT, b INT, PRIMARY KEY (a,b));

INSERT INTO t1 VALUES (1, 101);
INSERT INTO t1 SELECT a + 1, a + 101 FROM t1;
INSERT INTO t1 SELECT a + 2, a + 102 FROM t1;
INSERT INTO t1 SELECT a + 4, a + 104 FROM t1;
INSERT INTO t1 SELECT a + 8, a + 108 FROM t1;
ANALYZE TABLE t1;
EXPLAIN SELECT DISTINCT a,a FROM t1 WHERE b < 12 ORDER BY a;
SELECT DISTINCT a,a FROM t1 WHERE b < 12 ORDER BY a;

DROP TABLE t1;

#Bug #20836: Selecting into variables results in wrong results being returned
--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

CREATE TABLE t1 (id INT NOT NULL, fruit_id INT NOT NULL, fruit_name varchar(20)
default NULL);

INSERT INTO t1 VALUES (1,1,'ORANGE');
INSERT INTO t1 VALUES (2,2,'APPLE');
INSERT INTO t1 VALUES (3,2,'APPLE');
INSERT INTO t1 VALUES (4,3,'PEAR');

SELECT DISTINCT fruit_id, fruit_name INTO @v1, @v2 FROM t1 WHERE fruit_name = 
'APPLE';
SELECT @v1, @v2;

SELECT DISTINCT fruit_id, fruit_name INTO @v3, @v4 FROM t1 GROUP BY fruit_id, 
fruit_name HAVING fruit_name = 'APPLE';
SELECT @v3, @v4;

SELECT DISTINCT @v5:= fruit_id, @v6:= fruit_name INTO @v7, @v8 FROM t1 WHERE 
fruit_name = 'APPLE';
SELECT @v5, @v6, @v7, @v8;

SELECT DISTINCT @v5 + fruit_id, CONCAT(@v6, fruit_name) INTO @v9, @v10 FROM t1 
WHERE fruit_name = 'APPLE';
SELECT @v5, @v6, @v7, @v8, @v9, @v10;

SELECT DISTINCT @v11:= @v5 + fruit_id, @v12:= CONCAT(@v6, fruit_name) INTO 
@v13, @v14 FROM t1 WHERE fruit_name = 'APPLE';
SELECT @v11, @v12, @v13, @v14;

SELECT DISTINCT @v13, @v14 INTO @v15, @v16 FROM t1 WHERE fruit_name = 'APPLE';
SELECT @v15, @v16;

SELECT DISTINCT 2 + 2, 'Bob' INTO @v17, @v18 FROM t1 WHERE fruit_name = 
'APPLE';
SELECT @v17, @v18;

--disable_warnings
DROP TABLE IF EXISTS t2;
--enable_warnings

CREATE TABLE t2 (fruit_id INT NOT NULL, fruit_name varchar(20)
default NULL);

SELECT DISTINCT fruit_id, fruit_name INTO OUTFILE 
'../../tmp/data1.tmp' FROM t1 WHERE fruit_name = 'APPLE';
LOAD DATA INFILE '../../tmp/data1.tmp' INTO TABLE t2;
--error 0,1
--remove_file $MYSQLTEST_VARDIR/tmp/data1.tmp

SELECT DISTINCT @v19:= fruit_id, @v20:= fruit_name INTO OUTFILE 
'../../tmp/data2.tmp' FROM t1 WHERE fruit_name = 'APPLE';
LOAD DATA INFILE '../../tmp/data2.tmp' INTO TABLE t2;
--remove_file $MYSQLTEST_VARDIR/tmp/data2.tmp

SELECT @v19, @v20;
SELECT * FROM t2;

DROP TABLE t1;
DROP TABLE t2;

#
# Bug #15881: cast problems
#
CREATE TABLE t1 (a CHAR(1)); INSERT INTO t1 VALUES('A'), (0);
SELECT a FROM t1 WHERE a=0;
--sorted_result
SELECT DISTINCT a FROM t1 WHERE a=0;
DROP TABLE t1;
CREATE TABLE t1 (a DATE);
INSERT INTO t1 VALUES ('1972-07-29'), ('1972-02-06');
ANALYZE TABLE t1;
EXPLAIN SELECT (SELECT DISTINCT a FROM t1 WHERE a = '2002-08-03');
EXPLAIN SELECT (SELECT DISTINCT ADDDATE(a,1) FROM t1
                WHERE ADDDATE(a,1) = '2002-08-03');
CREATE TABLE t2 (a CHAR(5) CHARACTER SET latin1 COLLATE latin1_general_ci);
INSERT INTO t2 VALUES (0xf6);
INSERT INTO t2 VALUES ('oe');

SELECT COUNT(*) FROM (SELECT DISTINCT a FROM t2) dt;

set names latin1;
SELECT COUNT(*) FROM 
  (SELECT DISTINCT a FROM t2 WHERE a='oe' COLLATE latin1_german2_ci) dt;
set names utf8mb4;
DROP TABLE t1, t2;

#
# Bug #25551: inconsistent behaviour in grouping NULL, depending on index type
#
CREATE TABLE t1 (a INT, UNIQUE (a));
INSERT INTO t1 VALUES (4),(null),(2),(1),(null),(3);
ANALYZE TABLE t1;
EXPLAIN SELECT DISTINCT a FROM t1;
#result must have one row with NULL
SELECT DISTINCT a FROM t1;
EXPLAIN SELECT a FROM t1 GROUP BY a;
#result must have one row with NULL
SELECT a FROM t1 GROUP BY a;

DROP TABLE t1;

#
#Bug #27659: SELECT DISTINCT returns incorrect result set when field is
#repeated
#
#
CREATE TABLE t1 (a INT, b INT);
INSERT INTO t1 VALUES(1,1),(1,2),(1,3);
SELECT DISTINCT a, b FROM t1;
SELECT DISTINCT a, a, b FROM t1;
DROP TABLE t1;

--echo End of 5.0 tests

#
# Bug #34928: Confusion by having Primary Key and Index
#
CREATE TABLE t1(a INT, b INT, c INT, d INT, e INT,
                PRIMARY KEY(a,b,c,d,e),
                KEY(a,b,d,c)
);

INSERT IGNORE INTO t1(a, b, c) VALUES (1, 1, 1),
                                      (1, 1, 2),
                                      (1, 1, 3),
                                      (1, 2, 1),
                                      (1, 2, 2),
                                      (1, 2, 3);

ANALYZE TABLE t1;
EXPLAIN SELECT DISTINCT a, b, d, c FROM t1;

SELECT DISTINCT a, b, d, c FROM t1;

DROP TABLE t1;

--echo #
--echo # Bug #46159: simple query that never returns
--echo #

# Set max_heap_table_size to the minimum value so that GROUP BY table in the
# SELECT query below gets converted to MyISAM
SET @old_max_heap_table_size = @@max_heap_table_size;
SET @@max_heap_table_size = 16384;

# Set sort_buffer_size to the mininum value so that remove_duplicates() calls
# remove_dup_with_compare()
SET @old_sort_buffer_size = @@sort_buffer_size;
SET @@sort_buffer_size = 32804;

CREATE TABLE t1(c1 int, c2 VARCHAR(20));
INSERT INTO t1 VALUES (1, '1'), (1, '1'), (2, '2'), (3, '1'), (3, '1'), (4, '4');
# Now we just need to pad the table with random data so we have enough unique
# values to force conversion of the GROUP BY table to MyISAM
INSERT INTO t1 SELECT 5 + 10000 * RAND(), '5' FROM t1;
INSERT INTO t1 SELECT 5 + 10000 * RAND(), '5' FROM t1;
INSERT INTO t1 SELECT 5 + 10000 * RAND(), '5' FROM t1;
INSERT INTO t1 SELECT 5 + 10000 * RAND(), '5' FROM t1;
INSERT INTO t1 SELECT 5 + 10000 * RAND(), '5' FROM t1;
INSERT INTO t1 SELECT 5 + 10000 * RAND(), '5' FROM t1;
INSERT INTO t1 SELECT 5 + 10000 * RAND(), '5' FROM t1;
INSERT INTO t1 SELECT 5 + 10000 * RAND(), '5' FROM t1;
--source include/turn_off_only_full_group_by.inc
ANALYZE TABLE t1;
# First rows of the GROUP BY table that will be processed by 
# remove_dup_with_compare()
SELECT c1, c2, COUNT(*) FROM t1 GROUP BY c1 LIMIT 4;

# The actual test case
--skip_if_hypergraph  # Chooses a different query plan.
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT DISTINCT c2 FROM t1 GROUP BY c1 HAVING COUNT(*) > 1;
SELECT DISTINCT c2 FROM t1 GROUP BY c1 HAVING COUNT(*) > 1;

# Cleanup

--source include/restore_sql_mode_after_turn_off_only_full_group_by.inc
DROP TABLE t1;
SET @@sort_buffer_size = @old_sort_buffer_size;
SET @@max_heap_table_size = @old_max_heap_table_size;

--echo End of 5.1 tests


--echo #
--echo # Bug #11744875: 4082: integer lengths cause truncation with distinct concat and innodb
--echo #

CREATE TABLE t1 (a INT(1), b INT(1));
INSERT INTO t1 VALUES (1111, 2222), (3333, 4444);
SELECT DISTINCT CONCAT(a,b) AS c FROM t1 ORDER BY 1;
DROP TABLE t1;

--echo #
--echo # Bug#16539979 BASIC SELECT COUNT(DISTINCT ID) IS BROKEN.
--echo # Bug#17867117 ERROR RESULT WHEN "COUNT + DISTINCT + CASE WHEN" NEED MERGE_WALK 
--echo #

SET @tmp_table_size_save= @@tmp_table_size;
SET @@tmp_table_size= 1024;

CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1),(2),(3),(4),(5),(6),(7),(8);
INSERT INTO t1 SELECT a+8 FROM t1;
INSERT INTO t1 SELECT a+16 FROM t1;
INSERT INTO t1 SELECT a+32 FROM t1;
INSERT INTO t1 SELECT a+64 FROM t1;
INSERT INTO t1 VALUE(NULL);
SELECT COUNT(DISTINCT a) FROM t1;
SELECT COUNT(DISTINCT (a+0)) FROM t1;
DROP TABLE t1;

create table tb(
id int auto_increment primary key,
v varchar(32))
engine=myisam charset=gbk;
insert into tb(v) values("aaa");
insert into tb(v) (select v from tb);
insert into tb(v) (select v from tb);
insert into tb(v) (select v from tb);
insert into tb(v) (select v from tb);
insert into tb(v) (select v from tb);
insert into tb(v) (select v from tb);

update tb set v=concat(v, id);
select count(distinct case when id<=64 then id end) from tb;
select count(distinct case when id<=63 then id end) from tb;
drop table tb;

SET @@tmp_table_size= @tmp_table_size_save;

--echo End of 5.5 tests

--echo #
--echo # BUG#13540692: WRONG NULL HANDLING WITH RIGHT JOIN + 
--echo #               DISTINCT OR ORDER BY
--echo #

CREATE TABLE t1 (
  a INT,
  b INT NOT NULL
);
INSERT INTO t1 VALUES (1,2), (3,3);

SET @save_optimizer_switch= @@optimizer_switch;
SET @@SESSION.optimizer_switch="derived_merge=off";
ANALYZE TABLE t1;
let $query=
SELECT DISTINCT subselect.b
FROM t1 LEFT JOIN 
     (SELECT it_b.* FROM t1 as it_a LEFT JOIN t1 as it_b ON true) AS subselect 
     ON t1.a = subselect.b
;

--echo
eval EXPLAIN $query;
eval $query;

SET @@SESSION.optimizer_switch= @save_optimizer_switch;

DROP TABLE t1;

--echo #
--echo # BUG#13538387: WRONG RESULT ON SELECT DISTINCT + LEFT JOIN + 
--echo #               LIMIT + MIX OF MYISAM AND INNODB
--echo #

CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (2),(3);

CREATE TABLE t2 (b INT);

CREATE TABLE t3 (
  a INT,
  b INT,
  PRIMARY KEY (b)
);
INSERT INTO t3 VALUES (2001,1), (2007,2);

let $query=
SELECT DISTINCT t3.a AS t3_date
FROM t1
     LEFT JOIN t2 ON false
     LEFT JOIN t3 ON t2.b = t3.b
LIMIT 1;
ANALYZE TABLE t1, t2, t3;
eval EXPLAIN $query;
eval $query;

DROP TABLE t1,t2,t3;

--echo #
--echo # Bug#22686994 REGRESSION FOR A GROUPING QUERY WITH DISTINCT
--echo #

CREATE TABLE t1 (a INTEGER, b INTEGER);

INSERT INTO t1 VALUES (1,3), (2,4), (1,5),
(1,3), (2,1), (1,5), (1,7), (3,1),
(3,2), (3,1), (2,4);

--sorted_result
SELECT DISTINCT (COUNT(DISTINCT b) + 1) AS c FROM t1 GROUP BY a;
DROP TABLE t1;

--echo # End of test for Bug#22686994

--echo #
--echo # Bug #25740550: DISTINCT OPERATIONS ON TEMP TABLES ALLOCATE TOO LITTLE
--echo #                MEMORY FOR SORT KEYS
--echo #
--echo # A test for remove_dup_with_hash_index() where sort keys are significantly
--echo # longer than the row itself.
--echo #

--source include/turn_off_only_full_group_by.inc

CREATE TABLE t1(c1 int, c2 VARCHAR(1) COLLATE utf8mb4_0900_as_cs);
INSERT INTO t1 VALUES (1, 'a');
INSERT INTO t1 VALUES (2, 'A');
SELECT DISTINCT c2 FROM t1 GROUP BY c1;
DROP TABLE t1;

--source include/restore_sql_mode_after_turn_off_only_full_group_by.inc

--echo #
--echo # Bug#20692219 INCORRECT RESULT FOR DISTINCT SELECT
--echo #

CREATE TABLE t(a INT, b INT);
INSERT INTO t VALUES(1,2);
INSERT INTO t VALUES(2,4);
SELECT  LEAST(1,COUNT(DISTINCT a)) FROM t GROUP BY a;
SELECT DISTINCT LEAST(1,COUNT(DISTINCT a)) FROM t GROUP BY a;
DROP TABLE t;

--echo #
--echo # Bug #28974762: WL#12074: RESULT DIFFS WITH SOME SELECT DISTINCT QUERIES
--echo #

SET @old_optimizer_switch=@@optimizer_switch;
SET optimizer_switch='block_nested_loop=off';

CREATE TABLE t1 (
  pk integer NOT NULL,
  f1 integer,
  f2 varchar(10),
  f3 varchar(255)
);

INSERT INTO t1 VALUES (14,7,'G','W');
INSERT INTO t1 VALUES (16,8,'W','p');
INSERT INTO t1 VALUES (23,NULL,'q','w');
ANALYZE TABLE t1;
# Verify that we don't add LIMIT 1 on the scan of table2, since the filter is
# pushed up above it.

SELECT DISTINCT table1.pk FROM t1 AS table1 LEFT JOIN t1 AS table2 ON table1.f2=table2.f3 WHERE table2.f1 IS NULL;
--skip_if_hypergraph  # Chooses a different query plan.
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT DISTINCT table1.pk FROM t1 AS table1 LEFT JOIN t1 AS table2 ON table1.f2=table2.f3 WHERE table2.f1 IS NULL;

DROP TABLE t1;

# Verify that we only put the LIMIT on the rightmost table in a multi-table join.
# Putting limit on t3 here (the second table in the join order) would cause
# wrong results, as the first row would pass the filter on t3, but only the second
# row would also match the filter in the upper join.

CREATE TABLE t1 (
  pk integer
);

INSERT INTO t1 VALUES (3);

CREATE TABLE t2 (
  pk integer,
  f1 integer
);

INSERT INTO t2 VALUES (12,4);

CREATE TABLE t3 (
  pk integer,
  f2 integer,
  f3 integer
);

INSERT INTO t3 VALUES (56,0,4);  # Two different rows that both match t2.pk=4...
INSERT INTO t3 VALUES (97,3,4);  # ...but only the second one also matches t1.pk=3.
ANALYZE TABLE t1, t2, t3;
SELECT /*+JOIN_ORDER(t2,t3,t1) */ DISTINCT t2.pk FROM t1 LEFT JOIN t2 RIGHT OUTER JOIN t3 ON t2.f1 = t3.f3 ON t1.pk = t3.f2 WHERE t3.pk <> t2.pk;

# Demonstrate that we get the expected plan, with only one LIMIT 1.
--skip_if_hypergraph  # Doesn't do const tables, so gets a fairly different query plan.
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT /*+JOIN_ORDER(t2,t3,t1) */ DISTINCT t2.pk FROM t1 LEFT JOIN t2 RIGHT OUTER JOIN t3 ON t2.f1 = t3.f3 ON t1.pk = t3.f2 WHERE t3.pk <> t2.pk;

DROP TABLE t1, t2, t3;

SET optimizer_switch=@old_optimizer_switch;

--echo #
--echo # Test that DISTINCT-by-filesort manages to deduplicate across sort chunks.
--echo #

SET @old_sort_buffer_size = @@sort_buffer_size;
SET @@sort_buffer_size = 32768;
CREATE TABLE t1 ( f FLOAT );
INSERT INTO t1 VALUES (0.0);
INSERT INTO t1 SELECT RAND() FROM t1 AS t1, t1 AS t2;  # 3 rows.
INSERT INTO t1 SELECT RAND() FROM t1 AS t1, t1 AS t2;  # 6 rows.
INSERT INTO t1 SELECT RAND() FROM t1 AS t1, t1 AS t2;  # 42 rows.
INSERT INTO t1 SELECT RAND() FROM t1 AS t1, t1 AS t2;  # 1806 rows.
INSERT INTO t1 SELECT RAND() FROM t1;  # 3612 rows.
INSERT INTO t1 SELECT RAND() FROM t1;  # 7224 rows; enough to fill a few sort chunks.
ANALYZE TABLE t1;

--skip_if_hypergraph  # Chooses a different query plan.
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT DISTINCT COUNT(*) AS num FROM t1 GROUP BY f HAVING num=1;
SELECT DISTINCT COUNT(*) AS num FROM t1 GROUP BY f HAVING num=1;

DROP TABLE t1;
SET @@sort_buffer_size = @old_sort_buffer_size;

--echo #
--echo # Bug #29486219: RECENT REGRESSION: CRASH IN FILESORT::MAKE_SORTORDER()
--echo #

CREATE TABLE t1 (
  a INTEGER NOT NULL,
  b BIT(62) NOT NULL
);
SELECT DISTINCT b FROM t1 GROUP BY a, b;
DROP TABLE t1;

--echo #
--echo # Bug 30760534: THE "ORDER BY DESC" OPTION DOES NOT WORK
--echo #

CREATE TABLE t1 (a INTEGER, b INTEGER);
INSERT INTO t1 VALUES(1, 1), (2, 2), (3, 3);
ANALYZE TABLE t1;
let $query = SELECT DISTINCT SQL_BIG_RESULT a FROM t1 GROUP BY a,b ORDER BY a DESC;
--skip_if_hypergraph  # Doesn't get the redundant sort!
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;  # Verify that we have a sort, with DESC. Note that we currently have two, which is redundant.
eval $query;
DROP TABLE t1;

--echo #
--echo # Bug#30761372: THE "ORDER BY" OPTION CAUSE WRONG RESULT
--echo #

CREATE TABLE t1 (f1 INTEGER, f2 VARCHAR(10));
INSERT INTO t1 VALUES (1,"aaa"), (1,"bbb"),(1,"aaaa"),(1,"bbbb");

SELECT DISTINCT f1, LENGTH(F2) FROM t1 GROUP BY f1,LENGTH(F2) ORDER BY 1,2;

DROP TABLE t1;

--echo #
--echo # Bug #33148369: HYPERGRAPH-OPTIMIZER: HIT ASSERT(COUNT>0) IN MAKE_SORTORDER() WHILE FINALIZEPLAN
--echo #

CREATE TABLE t1 ( a INTEGER );
CREATE TABLE t2 ( a INTEGER );
CREATE TABLE t3 ( a INTEGER );

SELECT DISTINCT t1.a
  FROM t1, t2, t3
  WHERE t1.a = t2.a AND t2.a IS NULL
  HAVING t1.a = 8
  ORDER BY t1.a;

DROP TABLE t1, t2, t3;

--echo #
--echo # Bug#33914410: Hypergraph: Duplicate rows in query with DISTINCT, ROLLUP and ORDER BY
--echo #

CREATE TABLE t1(f1 INTEGER);
INSERT INTO t1 VALUES (1),(NULL);
SELECT DISTINCT f1 FROM t1 GROUP BY f1 WITH ROLLUP ORDER BY f1, ANY_VALUE(GROUPING(f1));
DROP TABLE t1;

--echo # Bug#34039275: Hypergraph: SELECT DISTINCT on self join gives dups

CREATE TABLE t1(f1 integer,f2 integer);
INSERT INTO t1 VALUES (1,100000), (2,100000);

SELECT SQL_SMALL_RESULT DISTINCT t1.f1/t1.f2 FROM t1;

SELECT SQL_BIG_RESULT DISTINCT t1.f1/t1.f2 FROM t1;

DROP TABLE t1;

--echo # Bug#34167787: Result mismatch with integer and cast as float

CREATE TABLE t1(v INTEGER);

INSERT INTO t1 VALUES
 (-2007568257), (-2007568260), (-2007570000), (-2007567234), (-2007567230),
 (2007568257), (2007568260), (2007570000), (2007567234), (2007567230);

SELECT SQL_SMALL_RESULT DISTINCT CAST(v AS FLOAT) FROM t1;
SELECT SQL_BIG_RESULT DISTINCT CAST(v AS FLOAT) FROM t1;

DROP TABLE t1;

--echo #
--echo # Bug#36496160 Incorrect results when using distinct and order by and derived table
--echo #
CREATE TABLE tt (
  id BIGINT UNSIGNED NOT NULL,
  name VARCHAR(20),
  time VARCHAR(20),
  PRIMARY KEY (id)
);

INSERT INTO tt VALUES (0, 'kk', '20230312'),
                      (1, 'kk', '20230309'),
                      (2, 'kk', '20230314'),
                      (3, 'kk', '20230320'),
                      (4, 'ss', '20230305'),
                      (5, 'ss', '20230304'),
                      (6, 'ss', '20230320');

SELECT DISTINCT t0.name, STR_TO_DATE(t0.time,'%Y%m%d')
FROM (SELECT * FROM tt) AS t0
ORDER BY t0.name, STR_TO_DATE(t0.time,'%Y%m%d');

DROP TABLE tt;
