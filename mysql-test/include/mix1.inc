# include/mix1.inc
#
# The variables
#     $engine_type       -- storage engine to be tested
#     $other_engine_type -- storage engine <> $engine_type
#                           $other_engine_type must point to an all
#                           time available storage engine
#                              2006-08 MySQL 5.1 MyISAM and MEMORY only
#    $test_foreign_keys -- 0, skip foreign key tests
#                       -- 1, do not skip foreign key tests
# have to be set before sourcing this script.
#
# Note: The comments/expectations refer to InnoDB.
#       They might be not valid for other storage engines.
#
# Last update:
# 2006-08-15 ML refactoring of t/innodb_mysql.test
#               - shift main code of t/innodb_mysql.test to include/mix1.inc
#               - replace hardcoded assignment of storage engine by
#                 use of $engine_type and $other_engine_type variables
#               - remove redundant replay testcase of
#                 Bug#12882 min/max inconsistent on empty table
#               - corrected analyze table t1; to analyze table t4;
#                 Much older versions of this test show that the table
#                 where just some indexes have been created must be used.
#

eval SET SESSION STORAGE_ENGINE = $engine_type;

--disable_warnings
drop table if exists t1,t2,t3,t1m,t1i,t2m,t2i,t4;
drop procedure if exists p1;
--enable_warnings


# BUG#16798: Uninitialized row buffer reads in ref-or-null optimizer
# (repeatable only w/innodb).
create table t1 (
  c_id int(11) not null default '0',
  org_id int(11) default null,
  unique key contacts$c_id (c_id),
  key contacts$org_id (org_id)
);
insert into t1 values
  (2,null),(120,null),(141,null),(218,7), (128,1),
  (151,2),(234,2),(236,2),(243,2),(255,2),(259,2),(232,3),(235,3),(238,3),
  (246,3),(253,3),(269,3),(285,3),(291,3),(293,3),(131,4),(230,4),(231,4);

create table t2 (
  slai_id int(11) not null default '0',
  owner_tbl int(11) default null,
  owner_id int(11) default null,
  sla_id int(11) default null,
  inc_web int(11) default null,
  inc_email int(11) default null,
  inc_chat int(11) default null,
  inc_csr int(11) default null,
  inc_total int(11) default null,
  time_billed int(11) default null,
  activedate timestamp null default null,
  expiredate timestamp null default null,
  state int(11) default null,
  sla_set int(11) default null,
  unique key t2$slai_id (slai_id),
  key t2$owner_id (owner_id),
  key t2$sla_id (sla_id)
);
insert into t2(slai_id, owner_tbl, owner_id, sla_id) values
  (1,3,1,1), (3,3,10,2), (4,3,3,6), (5,3,2,5), (6,3,8,3), (7,3,9,7),
  (8,3,6,8), (9,3,4,9), (10,3,5,10), (11,3,11,11), (12,3,7,12);

flush tables;
select si.slai_id
from t1 c join t2 si on
  ((si.owner_tbl = 3 and si.owner_id = c.org_id) or
   ( si.owner_tbl = 2 and si.owner_id = c.c_id))
where
  c.c_id = 218 and expiredate is null;

select * from t1 where org_id is null;
select si.slai_id
from t1 c join t2 si on
  ((si.owner_tbl = 3 and si.owner_id = c.org_id) or
   ( si.owner_tbl = 2 and si.owner_id = c.c_id))
where
  c.c_id = 218 and expiredate is null;

drop table t1, t2;

#
# Bug#17212: results not sorted correctly by ORDER BY when using index
# (repeatable only w/innodb because of index props)
#
CREATE TABLE t1 (a int, b int, KEY b (b));
CREATE TABLE t2 (a int, b int, PRIMARY KEY  (a,b));
CREATE TABLE t3 (a int, b int, c int, PRIMARY KEY  (a),
  UNIQUE KEY b (b,c), KEY a (a,b,c));

INSERT INTO t1 VALUES (1, 1);
INSERT INTO t1 SELECT a + 1, b + 1 FROM t1;
INSERT INTO t1 SELECT a + 2, b + 2 FROM t1;

INSERT INTO t2 VALUES (1,1),(1,2),(1,3),(1,4),(1,5),(1,6),(1,7),(1,8);
INSERT INTO t2 SELECT a + 1, b FROM t2;
DELETE FROM t2 WHERE a = 1 AND b < 2;

INSERT INTO t3 VALUES (1,1,1),(2,1,2);
INSERT INTO t3 SELECT a + 2, a + 2, 3 FROM t3;
INSERT INTO t3 SELECT a + 4, a + 4, 3 FROM t3;

# demonstrate a problem when a must-use-sort table flag
# (sort_by_table=1) is being neglected.
SELECT STRAIGHT_JOIN SQL_NO_CACHE t1.b, t1.a FROM t1, t3, t2 WHERE
  t3.a = t2.a AND t2.b = t1.a AND t3.b = 1 AND t3.c IN (1, 2)
  ORDER BY t1.b LIMIT 2;

# demonstrate the problem described in the bug report
SELECT STRAIGHT_JOIN SQL_NO_CACHE t1.b, t1.a FROM t1, t3, t2 WHERE
  t3.a = t2.a AND t2.b = t1.a AND t3.b = 1 AND t3.c IN (1, 2)
  ORDER BY t1.b LIMIT 5;
DROP TABLE t1, t2, t3;


# BUG#21077 (The testcase is not deterministic so correct execution doesn't
# prove anything) For proof one should track if sequence of ha_innodb::* func
# calls is correct.
CREATE TABLE `t1` (`id1` INT) ;
INSERT INTO `t1` (`id1`) VALUES (1),(5),(2);

CREATE TABLE `t2` (
  `id1` INT,
  `id2` INT NOT NULL,
  `id3` INT,
  `id4` INT NOT NULL,
  UNIQUE (`id2`,`id4`),
  KEY (`id1`)
);

INSERT INTO `t2`(`id1`,`id2`,`id3`,`id4`) VALUES
(1,1,1,0),
(1,1,2,1),
(5,1,2,2),
(6,1,2,3),
(1,2,2,2),
(1,2,1,1);

SELECT `id1` FROM `t1` WHERE `id1` NOT IN (SELECT `id1` FROM `t2` WHERE `id2` = 1 AND `id3` = 2);
DROP TABLE t1, t2;

#
# Bug #22728 - Handler_rollback value is growing
#

let $before= `show /*!50002 GLOBAL */ status like 'Handler_rollback'`;
create table t1 (c1 int) engine=innodb;
connect (con1,localhost,root,,);
connect (con2,localhost,root,,);
connection con2;
handler t1 open;
handler t1 read first;
disconnect con2;
connection con1;
let $after= `show /*!50002 GLOBAL */ status like 'Handler_rollback'`;
# Compare the before and after value, it should be equal
--disable_query_log
eval select STRCMP("$before", "$after") as "Before and after comparison";
--enable_query_log
connection default;
drop table t1;
disconnect con1;

#
# Bug #13191: INSERT...ON DUPLICATE KEY UPDATE of UTF-8 string fields
# used in partial unique indices.
#

CREATE TABLE t1(c1 TEXT, UNIQUE (c1(1)), cnt INT DEFAULT 1)
  ENGINE=INNODB CHARACTER SET UTF8;
INSERT INTO t1 (c1) VALUES ('1a');
SELECT * FROM t1;
INSERT INTO t1 (c1) VALUES ('1b') ON DUPLICATE KEY UPDATE cnt=cnt+1;
SELECT * FROM t1;
DROP TABLE t1;

CREATE TABLE t1(c1 VARCHAR(2), UNIQUE (c1(1)), cnt INT DEFAULT 1)
  ENGINE=INNODB CHARACTER SET UTF8;
INSERT INTO t1 (c1) VALUES ('1a');
SELECT * FROM t1;
INSERT INTO t1 (c1) VALUES ('1b') ON DUPLICATE KEY UPDATE cnt=cnt+1;
SELECT * FROM t1;
DROP TABLE t1;

CREATE TABLE t1(c1 CHAR(2), UNIQUE (c1(1)), cnt INT DEFAULT 1)
  ENGINE=INNODB CHARACTER SET UTF8;
INSERT INTO t1 (c1) VALUES ('1a');
SELECT * FROM t1;
INSERT INTO t1 (c1) VALUES ('1b') ON DUPLICATE KEY UPDATE cnt=cnt+1;
SELECT * FROM t1;
DROP TABLE t1;

#
# Bug #28272: EXPLAIN for SELECT from an empty InnoDB table
#

CREATE TABLE t1 (
  a1 decimal(10,0) DEFAULT NULL,
  a2 blob,
  a3 time DEFAULT NULL,
  a4 blob,
  a5 char(175) DEFAULT NULL,
  a6 timestamp NOT NULL DEFAULT '0000-00-00 00:00:00',
  a7 tinyblob,
  INDEX idx (a6,a7(239),a5)
) ENGINE=InnoDB;

EXPLAIN SELECT a4 FROM t1 WHERE
a6=NULL AND
a4='UNcT5pIde4I6c2SheTo4gt92OV1jgJCVkXmzyf325R1DwLURkbYHwhydANIZMbKTgdcR5xS';

EXPLAIN SELECT t1.a4 FROM t1, t1 t WHERE
t.a6=t.a6 AND t1.a6=NULL AND
t1.a4='UNcT5pIde4I6c2SheTo4gt92OV1jgJCVkXmzyf325R1DwLURkbYHwhydANIZMbKTgdcR5xS';

DROP TABLE t1;

#
# Bug #12882  	min/max inconsistent on empty table
#

--disable_warnings
eval create table t1m (a int) engine = $other_engine_type;
create table t1i (a int);
eval create table t2m (a int) engine = $other_engine_type;
create table t2i (a int);
--enable_warnings
insert into t2m values (5);
insert into t2i values (5);

# test with $engine_type
select min(a) from t1i;
select min(7) from t1i;
select min(7) from DUAL;
explain select min(7) from t2i join t1i;
select min(7) from t2i join t1i;

select max(a) from t1i;
select max(7) from t1i;
select max(7) from DUAL;
explain select max(7) from t2i join t1i;
select max(7) from t2i join t1i;

select 1, min(a) from t1i where a=99;
select 1, min(a) from t1i where 1=99;
select 1, min(1) from t1i where a=99;
select 1, min(1) from t1i where 1=99;

select 1, max(a) from t1i where a=99;
select 1, max(a) from t1i where 1=99;
select 1, max(1) from t1i where a=99;
select 1, max(1) from t1i where 1=99;

# mixed $engine_type/$other_engine_type test
explain select count(*), min(7), max(7) from t1m, t1i;
select count(*), min(7), max(7) from t1m, t1i;

explain select count(*), min(7), max(7) from t1m, t2i;
select count(*), min(7), max(7) from t1m, t2i;

explain select count(*), min(7), max(7) from t2m, t1i;
select count(*), min(7), max(7) from t2m, t1i;

drop table t1m, t1i, t2m, t2i;

#
# Bug #12882: primary key implcitly included in every innodb index
# (was part of group_min_max.test)
#

eval create table t1 (
  a1 char(64), a2 char(64), b char(16), c char(16) not null, d char(16), dummy char(64) default ' '
) ENGINE = $other_engine_type;

insert into t1 (a1, a2, b, c, d) values
('a','a','a','a111','xy1'),('a','a','a','b111','xy2'),('a','a','a','c111','xy3'),('a','a','a','d111','xy4'),
('a','a','b','e112','xy1'),('a','a','b','f112','xy2'),('a','a','b','g112','xy3'),('a','a','b','h112','xy4'),
('a','b','a','i121','xy1'),('a','b','a','j121','xy2'),('a','b','a','k121','xy3'),('a','b','a','l121','xy4'),
('a','b','b','m122','xy1'),('a','b','b','n122','xy2'),('a','b','b','o122','xy3'),('a','b','b','p122','xy4'),
('b','a','a','a211','xy1'),('b','a','a','b211','xy2'),('b','a','a','c211','xy3'),('b','a','a','d211','xy4'),
('b','a','b','e212','xy1'),('b','a','b','f212','xy2'),('b','a','b','g212','xy3'),('b','a','b','h212','xy4'),
('b','b','a','i221','xy1'),('b','b','a','j221','xy2'),('b','b','a','k221','xy3'),('b','b','a','l221','xy4'),
('b','b','b','m222','xy1'),('b','b','b','n222','xy2'),('b','b','b','o222','xy3'),('b','b','b','p222','xy4'),
('c','a','a','a311','xy1'),('c','a','a','b311','xy2'),('c','a','a','c311','xy3'),('c','a','a','d311','xy4'),
('c','a','b','e312','xy1'),('c','a','b','f312','xy2'),('c','a','b','g312','xy3'),('c','a','b','h312','xy4'),
('c','b','a','i321','xy1'),('c','b','a','j321','xy2'),('c','b','a','k321','xy3'),('c','b','a','l321','xy4'),
('c','b','b','m322','xy1'),('c','b','b','n322','xy2'),('c','b','b','o322','xy3'),('c','b','b','p322','xy4'),
('d','a','a','a411','xy1'),('d','a','a','b411','xy2'),('d','a','a','c411','xy3'),('d','a','a','d411','xy4'),
('d','a','b','e412','xy1'),('d','a','b','f412','xy2'),('d','a','b','g412','xy3'),('d','a','b','h412','xy4'),
('d','b','a','i421','xy1'),('d','b','a','j421','xy2'),('d','b','a','k421','xy3'),('d','b','a','l421','xy4'),
('d','b','b','m422','xy1'),('d','b','b','n422','xy2'),('d','b','b','o422','xy3'),('d','b','b','p422','xy4'),
('a','a','a','a111','xy1'),('a','a','a','b111','xy2'),('a','a','a','c111','xy3'),('a','a','a','d111','xy4'),
('a','a','b','e112','xy1'),('a','a','b','f112','xy2'),('a','a','b','g112','xy3'),('a','a','b','h112','xy4'),
('a','b','a','i121','xy1'),('a','b','a','j121','xy2'),('a','b','a','k121','xy3'),('a','b','a','l121','xy4'),
('a','b','b','m122','xy1'),('a','b','b','n122','xy2'),('a','b','b','o122','xy3'),('a','b','b','p122','xy4'),
('b','a','a','a211','xy1'),('b','a','a','b211','xy2'),('b','a','a','c211','xy3'),('b','a','a','d211','xy4'),
('b','a','b','e212','xy1'),('b','a','b','f212','xy2'),('b','a','b','g212','xy3'),('b','a','b','h212','xy4'),
('b','b','a','i221','xy1'),('b','b','a','j221','xy2'),('b','b','a','k221','xy3'),('b','b','a','l221','xy4'),
('b','b','b','m222','xy1'),('b','b','b','n222','xy2'),('b','b','b','o222','xy3'),('b','b','b','p222','xy4'),
('c','a','a','a311','xy1'),('c','a','a','b311','xy2'),('c','a','a','c311','xy3'),('c','a','a','d311','xy4'),
('c','a','b','e312','xy1'),('c','a','b','f312','xy2'),('c','a','b','g312','xy3'),('c','a','b','h312','xy4'),
('c','b','a','i321','xy1'),('c','b','a','j321','xy2'),('c','b','a','k321','xy3'),('c','b','a','l321','xy4'),
('c','b','b','m322','xy1'),('c','b','b','n322','xy2'),('c','b','b','o322','xy3'),('c','b','b','p322','xy4'),
('d','a','a','a411','xy1'),('d','a','a','b411','xy2'),('d','a','a','c411','xy3'),('d','a','a','d411','xy4'),
('d','a','b','e412','xy1'),('d','a','b','f412','xy2'),('d','a','b','g412','xy3'),('d','a','b','h412','xy4'),
('d','b','a','i421','xy1'),('d','b','a','j421','xy2'),('d','b','a','k421','xy3'),('d','b','a','l421','xy4'),
('d','b','b','m422','xy1'),('d','b','b','n422','xy2'),('d','b','b','o422','xy3'),('d','b','b','p422','xy4');
--disable_warnings
create table t4 (
  pk_col int auto_increment primary key, a1 char(64), a2 char(64), b char(16), c char(16) not null, d char(16), dummy char(64) default ' '
);
--enable_warnings
insert into t4 (a1, a2, b, c, d, dummy) select * from t1;

create index idx12672_0 on t4 (a1);
create index idx12672_1 on t4 (a1,a2,b,c);
create index idx12672_2 on t4 (a1,a2,b);
analyze table t4;

select distinct a1 from t4 where pk_col not in (1,2,3,4);

drop table t1,t4;


#
# BUG#18819: DELETE IGNORE hangs on foreign key parent delete
#
# The bug itself does not relate to InnoDB, but we have to use foreign
# keys to reproduce it.
#
--disable_warnings
DROP TABLE IF EXISTS t2, t1;
--enable_warnings

CREATE TABLE t1 (i INT NOT NULL PRIMARY KEY) ENGINE= InnoDB;
CREATE TABLE t2 (
  i INT NOT NULL,
  FOREIGN KEY (i) REFERENCES t1 (i) ON DELETE NO ACTION
) ENGINE= InnoDB;

INSERT INTO t1 VALUES (1);
INSERT INTO t2 VALUES (1);

DELETE IGNORE FROM t1 WHERE i = 1;

SELECT * FROM t1, t2;

DROP TABLE t2, t1;


--echo End of 4.1 tests.


#
# Bug #6142: a problem with the empty innodb table
# (was part of group_min_max.test)
#

--disable_warnings
create table t1 (
  a varchar(30), b varchar(30), primary key(a), key(b)
);
--enable_warnings
select distinct a from t1;
drop table t1;

#
# Bug #9798: group by with rollup
# (was part of group_min_max.test)
#

--disable_warnings
create table t1(a int, key(a));
--enable_warnings
insert into t1 values(1);
select a, count(a) from t1 group by a with rollup;
drop table t1;

#
# Bug #13293 Wrongly used index results in endless loop.
# (was part of group_min_max.test)
#
create table t1 (f1 int, f2 char(1), primary key(f1,f2));
insert into t1 values ( 1,"e"),(2,"a"),( 3,"c"),(4,"d");
alter table t1 drop primary key, add primary key (f2, f1);
explain select distinct f1 a, f1 b from t1;
explain select distinct f1, f2 from t1;
drop table t1;

#
# Test for bug #17164: ORed FALSE blocked conversion of outer join into join
#

CREATE TABLE t1 (id int(11) NOT NULL PRIMARY KEY, name varchar(20),
                 INDEX (name));
CREATE TABLE t2 (id int(11) NOT NULL PRIMARY KEY, fkey int(11));
# CREATE TABLE t2 (id int(11) NOT NULL PRIMARY KEY, fkey int(11),
#                  FOREIGN KEY (fkey) REFERENCES t2(id));
if ($test_foreign_keys)
{
   ALTER TABLE t2 ADD FOREIGN KEY (fkey) REFERENCES t2(id);
}
INSERT INTO t1 VALUES (1,'A1'),(2,'A2'),(3,'B');
INSERT INTO t2 VALUES (1,1),(2,2),(3,2),(4,3),(5,3);

EXPLAIN
SELECT COUNT(*) FROM t2 LEFT JOIN t1 ON t2.fkey = t1.id
  WHERE t1.name LIKE 'A%';

EXPLAIN
SELECT COUNT(*) FROM t2 LEFT JOIN t1 ON t2.fkey = t1.id
  WHERE t1.name LIKE 'A%' OR FALSE;

DROP TABLE t1,t2;

#
# Bug#26159: crash for a loose scan of a table that has been emptied 
#

CREATE TABLE t1 (
  id int NOT NULL,
  name varchar(20) NOT NULL,
  dept varchar(20) NOT NULL,
  age tinyint(3) unsigned NOT NULL,
  PRIMARY KEY (id),
  INDEX (name,dept)
) ENGINE=InnoDB;
INSERT INTO t1(id, dept, age, name) VALUES
  (3987, 'cs1', 10, 'rs1'), (3988, 'cs2', 20, 'rs1'), (3995, 'cs3', 10, 'rs2'),
  (3996, 'cs4', 20, 'rs2'), (4003, 'cs5', 10, 'rs3'), (4004, 'cs6', 20, 'rs3'),
  (4011, 'cs7', 10, 'rs4'), (4012, 'cs8', 20, 'rs4'), (4019, 'cs9', 10, 'rs5'),
  (4020, 'cs10', 20, 'rs5'),(4027, 'cs11', 10, 'rs6'),(4028, 'cs12', 20, 'rs6');

EXPLAIN SELECT DISTINCT t1.name, t1.dept FROM t1 WHERE t1.name='rs5';
SELECT DISTINCT t1.name, t1.dept FROM t1 WHERE t1.name='rs5';
DELETE FROM t1;
EXPLAIN SELECT DISTINCT t1.name, t1.dept FROM t1 WHERE t1.name='rs5';
SELECT DISTINCT t1.name, t1.dept FROM t1 WHERE t1.name='rs5';

DROP TABLE t1;

--source include/innodb_rollback_on_timeout.inc

#
# Bug #27210: INNODB ON DUPLICATE KEY UPDATE
#

set @save_qcache_size=@@global.query_cache_size;
set @save_qcache_type=@@global.query_cache_type;
set global query_cache_size=10*1024*1024;
set global query_cache_type=1;
connect (con1,localhost,root,,);
connection con1;
drop table if exists `test`;
CREATE TABLE `test` (`test1` varchar(3) NOT NULL,
  `test2` varchar(4) NOT NULL,PRIMARY KEY  (`test1`))
  ENGINE=InnoDB DEFAULT CHARSET=latin1;
INSERT INTO `test` (`test1`, `test2`) VALUES ('tes', '5678');
disconnect con1;
connect (con2,localhost,root,,);
connection con2;
select * from test;
INSERT INTO `test` (`test1`, `test2`) VALUES ('tes', '1234')
  ON DUPLICATE KEY UPDATE `test2` = '1234';
select * from test;
flush tables;
select * from test;
disconnect con2;
connection default;
drop table test;
set global query_cache_type=@save_qcache_type;
set global query_cache_size=@save_qcache_size;

--source include/innodb_rollback_on_timeout.inc

#
# Bug #27650: INSERT fails after multi-row INSERT of the form:
# INSERT INTO t (id...) VALUES (NULL...) ON DUPLICATE KEY UPDATE id=VALUES(id)
#

create table t1(
id int auto_increment,
c char(1) not null,
counter int not null default 1,
primary key (id),
unique key (c)
) engine=innodb;

insert into t1 (id, c) values
(NULL, 'a'),
(NULL, 'a')
on duplicate key update id = values(id), counter = counter + 1;

select * from t1;

insert into t1 (id, c) values
(NULL, 'b')
on duplicate key update id = values(id), counter = counter + 1;

select * from t1;

truncate table t1;

insert into t1 (id, c) values (NULL, 'a');

select * from t1;

insert into t1 (id, c) values (NULL, 'b'), (NULL, 'b')
on duplicate key update id = values(id), c = values(c), counter = counter + 1;

select * from t1;

insert into t1 (id, c) values (NULL, 'a')
on duplicate key update id = values(id), c = values(c), counter = counter + 1;

select * from t1;

drop table t1;

#
# Bug #28189: optimizer erroniously prefers ref access to range access 
#             for an InnoDB table
#

CREATE TABLE t1(
  id int AUTO_INCREMENT PRIMARY KEY,
  stat_id int NOT NULL,
  acct_id int DEFAULT NULL,
  INDEX idx1 (stat_id, acct_id),
  INDEX idx2 (acct_id)
) ENGINE=MyISAM;

CREATE TABLE t2(
  id int AUTO_INCREMENT PRIMARY KEY,
  stat_id int NOT NULL,
  acct_id int DEFAULT NULL,
  INDEX idx1 (stat_id, acct_id),
  INDEX idx2 (acct_id)
) ENGINE=InnoDB;

INSERT INTO t1(stat_id,acct_id) VALUES
  (1,759), (2,831), (3,785), (4,854), (1,921),
  (1,553), (2,589), (3,743), (2,827), (2,545),
  (4,779), (4,783), (1,597), (1,785), (4,832),
  (1,741), (1,833), (3,788), (2,973), (1,907);

INSERT INTO t1(stat_id,acct_id) SELECT stat_id, mod(id+100000, acct_id) FROM t1;
INSERT INTO t1(stat_id,acct_id) SELECT stat_id, mod(id+100000, acct_id) FROM t1;
INSERT INTO t1(stat_id,acct_id) SELECT stat_id, mod(id+100000, acct_id) FROM t1;
INSERT INTO t1(stat_id,acct_id) SELECT stat_id, mod(id+100000, acct_id) FROM t1;
INSERT INTO t1(stat_id,acct_id) SELECT stat_id, mod(id+100000, acct_id) FROM t1;
INSERT INTO t1(stat_id,acct_id) SELECT stat_id, mod(id+100000, acct_id) FROM t1;
INSERT INTO t1(stat_id,acct_id) SELECT stat_id, mod(id+100000, acct_id) FROM t1;
INSERT INTO t1(stat_id,acct_id) SELECT stat_id, mod(id+100000, acct_id) FROM t1;
INSERT INTO t1(stat_id,acct_id) SELECT stat_id, mod(id+100000, acct_id) FROM t1;
INSERT INTO t1(stat_id,acct_id) SELECT stat_id, mod(id+100000, acct_id) FROM t1;
INSERT INTO t1(stat_id,acct_id) SELECT stat_id, mod(id+100000, acct_id) FROM t1;
UPDATE t1 SET acct_id=785 
  WHERE MOD(stat_id,2)=0 AND MOD(id,stat_id)=MOD(acct_id,stat_id);
OPTIMIZE TABLE t1;

SELECT COUNT(*) FROM t1;
SELECT COUNT(*) FROM t1 WHERE acct_id=785;

EXPLAIN SELECT COUNT(*) FROM t1 WHERE stat_id IN (1,3) AND acct_id=785; 

INSERT INTO t2 SELECT * FROM t1;
OPTIMIZE TABLE t2;

EXPLAIN SELECT COUNT(*) FROM t2 WHERE stat_id IN (1,3) AND acct_id=785;

DROP TABLE t1,t2; 

#
# Bug #28652: assert when alter innodb table operation
#
create table t1(a int) engine=innodb;
alter table t1 comment '123';
show create table t1;
drop table t1;

#
# Bug #25866: Getting "#HY000 Can't find record in..." on and INSERT
#
CREATE TABLE t1 (a CHAR(2), KEY (a)) ENGINE = InnoDB DEFAULT CHARSET=UTF8;
INSERT INTO t1 VALUES ('uk'),('bg');
SELECT * FROM t1 WHERE a = 'uk';
DELETE FROM t1 WHERE a = 'uk';
SELECT * FROM t1 WHERE a = 'uk';
UPDATE t1 SET a = 'us' WHERE a = 'uk';
SELECT * FROM t1 WHERE a = 'uk';

CREATE TABLE t2 (a CHAR(2), KEY (a)) ENGINE = InnoDB;
INSERT INTO t2 VALUES ('uk'),('bg');
SELECT * FROM t2 WHERE a = 'uk';
DELETE FROM t2 WHERE a = 'uk';
SELECT * FROM t2 WHERE a = 'uk';
INSERT INTO t2 VALUES ('uk');
UPDATE t2 SET a = 'us' WHERE a = 'uk';
SELECT * FROM t2 WHERE a = 'uk';

CREATE TABLE t3 (a CHAR(2), KEY (a)) ENGINE = MyISAM;
INSERT INTO t3 VALUES ('uk'),('bg');
SELECT * FROM t3 WHERE a = 'uk';
DELETE FROM t3 WHERE a = 'uk';
SELECT * FROM t3 WHERE a = 'uk';
INSERT INTO t3 VALUES ('uk');
UPDATE t3 SET a = 'us' WHERE a = 'uk';
SELECT * FROM t3 WHERE a = 'uk';

DROP TABLE t1,t2,t3;

#
# Test bug when trying to drop data file which no InnoDB directory entry
#

create table t1 (a int) engine=innodb;
copy_file $MYSQLTEST_VARDIR/master-data/test/t1.frm $MYSQLTEST_VARDIR/master-data/test/bug29807.frm;
--error 1146
select * from bug29807;
drop table t1;
--error 1051
drop table bug29807;
create table bug29807 (a int);
drop table bug29807;


#
# Bug #29154: LOCK TABLES is not atomic when >1 InnoDB tables are locked
#

CREATE TABLE t1 (a INT) ENGINE=InnoDB; 
CREATE TABLE t2 (a INT) ENGINE=InnoDB; 

CONNECT (c1,localhost,root,,);
CONNECT (c2,localhost,root,,);

--echo switch to connection c1
CONNECTION c1;
SET AUTOCOMMIT=0;
INSERT INTO t2 VALUES (1);

--echo switch to connection c2
CONNECTION c2;
SET AUTOCOMMIT=0;
--error ER_LOCK_WAIT_TIMEOUT
LOCK TABLES t1 READ, t2 READ;

--echo switch to connection c1
CONNECTION c1;
COMMIT;
INSERT INTO t1 VALUES (1);

--echo switch to connection default
CONNECTION default;
SET AUTOCOMMIT=default;
DISCONNECT c1;
DISCONNECT c2;
DROP TABLE t1,t2;

#
# Bug #25798: a query with forced index merge returns wrong result 
#

CREATE TABLE t1 (
  id int NOT NULL auto_increment PRIMARY KEY,
  b int NOT NULL,
  c datetime NOT NULL,
  INDEX idx_b(b),
  INDEX idx_c(c)
) ENGINE=InnoDB;

CREATE TABLE t2 (
  b int NOT NULL auto_increment PRIMARY KEY,
  c datetime NOT NULL
) ENGINE= MyISAM;

INSERT INTO t2(c) VALUES ('2007-01-01');
INSERT INTO t2(c) SELECT c FROM t2;
INSERT INTO t2(c) SELECT c FROM t2;
INSERT INTO t2(c) SELECT c FROM t2;
INSERT INTO t2(c) SELECT c FROM t2;
INSERT INTO t2(c) SELECT c FROM t2;
INSERT INTO t2(c) SELECT c FROM t2;
INSERT INTO t2(c) SELECT c FROM t2;
INSERT INTO t2(c) SELECT c FROM t2;
INSERT INTO t2(c) SELECT c FROM t2;
INSERT INTO t2(c) SELECT c FROM t2;

INSERT INTO t1(b,c) SELECT b,c FROM t2;
UPDATE t2 SET c='2007-01-02';
INSERT INTO t1(b,c) SELECT b,c FROM t2;
UPDATE t2 SET c='2007-01-03';
INSERT INTO t1(b,c) SELECT b,c FROM t2;

set @@sort_buffer_size=8192;

SELECT COUNT(*) FROM t1;

--replace_column 9 #
EXPLAIN 
SELECT COUNT(*) FROM t1 
  WHERE (c >= '2007-01-02' AND c <= '2007-01-03') OR b >= 1;
SELECT COUNT(*) FROM t1 
  WHERE (c >= '2007-01-02' AND c <= '2007-01-03') OR b >= 1;

--replace_column 9 #
EXPLAIN 
SELECT COUNT(*) FROM t1 FORCE INDEX(idx_b, idx_c) 
  WHERE (c >= '2007-01-02' AND c <= '2007-01-03') OR b >= 1;
SELECT COUNT(*) FROM t1 FORCE INDEX(idx_b, idx_c)
  WHERE (c >= '2007-01-02' AND c <= '2007-01-03') OR b >= 1;

set @@sort_buffer_size=default;

DROP TABLE t1,t2;

#
# Test of behaviour with CREATE ... SELECT
#

CREATE TABLE t1 (a int, b int);
insert into t1 values (1,1),(1,2);
--error ER_DUP_ENTRY
CREATE TABLE t2 (primary key (a)) select * from t1;
# This should give warning
drop table if exists t2;
--error ER_DUP_ENTRY
CREATE TEMPORARY TABLE t2 (primary key (a)) select * from t1;
# This should give warning
drop table if exists t2;
CREATE TABLE t2 (a int, b int, primary key (a));
BEGIN;
INSERT INTO t2 values(100,100);
--error ER_DUP_ENTRY
CREATE TABLE IF NOT EXISTS t2 (primary key (a)) select * from t1;
SELECT * from t2;
ROLLBACK;
SELECT * from t2;
TRUNCATE table t2;
--error ER_DUP_ENTRY
INSERT INTO t2 select * from t1;
SELECT * from t2;
drop table t2;

CREATE TEMPORARY TABLE t2 (a int, b int, primary key (a));
BEGIN;
INSERT INTO t2 values(100,100);
--error ER_DUP_ENTRY
CREATE TEMPORARY TABLE IF NOT EXISTS t2 (primary key (a)) select * from t1;
SELECT * from t2;
COMMIT;
BEGIN;
INSERT INTO t2 values(101,101);
--error ER_DUP_ENTRY
CREATE TEMPORARY TABLE IF NOT EXISTS t2 (primary key (a)) select * from t1;
SELECT * from t2;
ROLLBACK;
SELECT * from t2;
TRUNCATE table t2;
--error ER_DUP_ENTRY
INSERT INTO t2 select * from t1;
SELECT * from t2;
drop table t1,t2;

#
# Bug#17530: Incorrect key truncation on table creation caused server crash.
#
create table t1(f1 varchar(800) binary not null, key(f1))
  character set utf8 collate utf8_general_ci;
insert into t1 values('aaa');
drop table t1;


#
# Bug#22781: SQL_BIG_RESULT fails to influence sort plan
#
CREATE TABLE t1 (a INT PRIMARY KEY, b INT, c FLOAT, KEY b(b)) ENGINE = INNODB;

INSERT INTO t1 VALUES (    1 , 1              , 1);
INSERT INTO t1 SELECT  a + 1 , MOD(a + 1 , 20), 1 FROM t1;
INSERT INTO t1 SELECT  a + 2 , MOD(a + 2 , 20), 1 FROM t1;
INSERT INTO t1 SELECT  a + 4 , MOD(a + 4 , 20), 1 FROM t1;
INSERT INTO t1 SELECT  a + 8 , MOD(a + 8 , 20), 1 FROM t1;
INSERT INTO t1 SELECT  a + 16, MOD(a + 16, 20), 1 FROM t1;
INSERT INTO t1 SELECT  a + 32, MOD(a + 32, 20), 1 FROM t1;
INSERT INTO t1 SELECT  a + 64, MOD(a + 64, 20), 1 FROM t1;

EXPLAIN SELECT b, SUM(c) FROM t1 GROUP BY b;
EXPLAIN SELECT SQL_BIG_RESULT b, SUM(c) FROM t1 GROUP BY b;
DROP TABLE t1;

--source include/innodb_rollback_on_timeout.inc

#
# Bug#27296 Assertion in ALTER TABLE SET DEFAULT in Linux Debug build
# (possible deadlock).
#
# The bug is applicable only to a transactoinal table.
# Cover with tests behavior that no longer causes an
# assertion.
#
--disable_warnings
drop table if exists t1;
--enable_warnings
create table t1 (a int) engine=innodb;
alter table t1 alter a set default 1;
drop table t1;

--echo
--echo Bug#24918 drop table and lock / inconsistent between 
--echo perm and temp tables
--echo
--echo Check transactional tables under LOCK TABLES
--echo
--disable_warnings
drop table if exists t24918, t24918_tmp, t24918_trans, t24918_trans_tmp, 
t24918_access;
--enable_warnings
create table t24918_access (id int);
create table t24918 (id int) engine=myisam;
create temporary table t24918_tmp (id int) engine=myisam;
create table t24918_trans (id int) engine=innodb;
create temporary table t24918_trans_tmp (id int) engine=innodb;

lock table t24918 write, t24918_tmp write, t24918_trans write, t24918_trans_tmp write;
drop table t24918;
--error ER_TABLE_NOT_LOCKED
select * from t24918_access;
drop table t24918_trans;
--error ER_TABLE_NOT_LOCKED
select * from t24918_access;
drop table t24918_trans_tmp;
--error ER_TABLE_NOT_LOCKED
select * from t24918_access;
drop table t24918_tmp;
--error ER_TABLE_NOT_LOCKED
select * from t24918_access;
unlock tables;

drop table t24918_access;
#
# Bug #28591: MySQL need not sort the records in case of ORDER BY
# primary_key on InnoDB table
#

CREATE TABLE t1 (a int, b int, PRIMARY KEY (a), KEY bkey (b)) ENGINE=InnoDB;
INSERT INTO t1 VALUES (1,2),(3,2),(2,2),(4,2),(5,2),(6,2),(7,2),(8,2);
INSERT INTO t1 SELECT a + 8, 2 FROM t1;
INSERT INTO t1 SELECT a + 16, 1 FROM t1;
query_vertical EXPLAIN SELECT * FROM t1 WHERE b=2 ORDER BY a;
SELECT * FROM t1 WHERE b=2 ORDER BY a;
query_vertical EXPLAIN SELECT * FROM t1 WHERE b BETWEEN 1 AND 2 ORDER BY a;
SELECT * FROM t1 WHERE b BETWEEN 1 AND 2 ORDER BY a;
query_vertical EXPLAIN SELECT * FROM t1 WHERE b BETWEEN 1 AND 2 ORDER BY b,a;
SELECT * FROM t1 WHERE b BETWEEN 1 AND 2 ORDER BY b,a;

CREATE TABLE t2 (a int, b int, c int, PRIMARY KEY (a), KEY bkey (b,c))
  ENGINE=InnoDB;
INSERT INTO t2 VALUES (1,1,1),(3,1,1),(2,1,1),(4,1,1);
INSERT INTO t2 SELECT a + 4, 1, 1 FROM t2;
INSERT INTO t2 SELECT a + 8, 1, 1 FROM t2;

query_vertical EXPLAIN SELECT * FROM t2 WHERE b=1 ORDER BY a;
SELECT * FROM t2 WHERE b=1 ORDER BY a;
query_vertical EXPLAIN SELECT * FROM t2 WHERE b=1 AND c=1 ORDER BY a;
SELECT * FROM t2 WHERE b=1 AND c=1 ORDER BY a;
query_vertical EXPLAIN SELECT * FROM t2 WHERE b=1 AND c=1 ORDER BY b,c,a;
SELECT * FROM t2 WHERE b=1 AND c=1 ORDER BY b,c,a;
query_vertical EXPLAIN SELECT * FROM t2 WHERE b=1 AND c=1 ORDER BY c,a;
SELECT * FROM t2 WHERE b=1 AND c=1 ORDER BY c,a;

DROP TABLE t1,t2;


#
# Bug #29644: alter table hangs if records locked in share mode by long
# running transaction
#

CREATE TABLE t1 (a INT, PRIMARY KEY (a)) ENGINE=InnoDB;

INSERT INTO t1 VALUES (1),(2),(3),(4),(5),(6),(7),(8);
INSERT INTO t1 SELECT a + 8  FROM t1;
INSERT INTO t1 SELECT a + 16 FROM t1;

DELIMITER |;
CREATE PROCEDURE p1 ()
BEGIN
  DECLARE i INT DEFAULT 50;
  DECLARE cnt INT;
  START TRANSACTION;
    ALTER TABLE t1 ENGINE=InnoDB;
  COMMIT;
  START TRANSACTION;
  WHILE (i > 0) DO
    SET i = i - 1;
    SELECT COUNT(*) INTO cnt FROM t1 LOCK IN SHARE MODE;
  END WHILE;
  COMMIT;
END;|

DELIMITER ;|

CONNECT (con1,localhost,root,,);
CONNECT (con2,localhost,root,,);

CONNECTION con1;
SEND CALL p1();
CONNECTION con2;
SEND CALL p1();
CONNECTION default;
CALL p1();

CONNECTION con1;
REAP;
CONNECTION con2;
REAP;
CONNECTION default;
DISCONNECT con1;
DISCONNECT con2;

DROP PROCEDURE p1;
DROP TABLE t1;

#
# Bug #28125: ERROR 2013 when adding index.
#
create table t1(a text) engine=innodb default charset=utf8; 
insert into t1 values('aaa');
alter table t1 add index(a(1024));
show create table t1;
drop table t1;

#
# Bug #28570: handler::index_read() is called with different find_flag when 
# ORDER BY is used
#

CREATE TABLE t1 (
  a INT,
  b INT,
  KEY (b)
) ENGINE=InnoDB;

INSERT INTO t1 VALUES (1,10), (2,10), (2,20), (3,30);

START TRANSACTION;
SELECT * FROM t1 WHERE b=20 FOR UPDATE;

--connect (conn2, localhost, root,,test)

# This statement gives a "failed: 1205: Lock wait timeout exceeded; try 
# restarting transaction" message when the bug is present.
START TRANSACTION;
SELECT * FROM t1 WHERE b=10 ORDER BY A FOR UPDATE;
ROLLBACK;

--disconnect conn2
--connection default

ROLLBACK;
DROP TABLE t1;

#
# Bug#30596: GROUP BY optimization gives wrong result order
#  
CREATE TABLE t1(
  a INT, 
  b INT NOT NULL, 
  c INT NOT NULL, 
  d INT, 
  UNIQUE KEY (c,b)
) engine=innodb;

INSERT INTO t1 VALUES (1,1,1,50), (1,2,3,40), (2,1,3,4);

EXPLAIN SELECT c,b,d FROM t1 GROUP BY c,b,d;
SELECT c,b,d FROM t1 GROUP BY c,b,d;
EXPLAIN SELECT c,b,d FROM t1 GROUP BY c,b,d ORDER BY NULL;
SELECT c,b,d FROM t1 GROUP BY c,b,d ORDER BY NULL;
EXPLAIN SELECT c,b,d FROM t1 ORDER BY c,b,d;
SELECT c,b,d FROM t1 ORDER BY c,b,d;

EXPLAIN SELECT c,b,d FROM t1 GROUP BY c,b;
SELECT c,b,d FROM t1 GROUP BY c,b;
EXPLAIN SELECT c,b   FROM t1 GROUP BY c,b;
SELECT c,b   FROM t1 GROUP BY c,b;

DROP TABLE t1;

#
# Bug #31001: ORDER BY DESC in InnoDB not working
#
CREATE TABLE t1 (a INT, b INT, PRIMARY KEY (a), INDEX b (b)) ENGINE=InnoDB;
INSERT INTO t1(a,b) VALUES (1,1), (2,2), (3,2);

#The two queries below should produce different results, but they don't.
query_vertical EXPLAIN SELECT * FROM t1 WHERE b=2 ORDER BY a ASC;
SELECT * FROM t1 WHERE b=2 ORDER BY a ASC;
query_vertical EXPLAIN SELECT * FROM t1 WHERE b=2 ORDER BY a DESC;
SELECT * FROM t1 WHERE b=2 ORDER BY a DESC;

query_vertical EXPLAIN SELECT * FROM t1 ORDER BY b ASC, a ASC;
SELECT * FROM t1 ORDER BY b ASC, a ASC;
query_vertical EXPLAIN SELECT * FROM t1 ORDER BY b DESC, a DESC;
SELECT * FROM t1 ORDER BY b DESC, a DESC;
query_vertical EXPLAIN SELECT * FROM t1 ORDER BY b ASC, a DESC;
SELECT * FROM t1 ORDER BY b ASC, a DESC;
query_vertical EXPLAIN SELECT * FROM t1 ORDER BY b DESC, a ASC;
SELECT * FROM t1 ORDER BY b DESC, a ASC;

DROP TABLE t1;

--echo End of 5.0 tests

# Fix for BUG#19243 "wrong LAST_INSERT_ID() after ON DUPLICATE KEY
# UPDATE": if the row is updated, it's like a regular UPDATE:
# LAST_INSERT_ID() is not affected.
CREATE TABLE `t2` (
  `k` int(11) NOT NULL auto_increment,
  `a` int(11) default NULL,
  `c` int(11) default NULL,
  PRIMARY KEY  (`k`),
  UNIQUE KEY `idx_1` (`a`)
);
insert into t2 ( a ) values ( 6 ) on duplicate key update c =
ifnull( c,
0 ) + 1;
insert into t2 ( a ) values ( 7 ) on duplicate key update c =
ifnull( c,
0 ) + 1;
select last_insert_id();
select * from t2;
insert into t2 ( a ) values ( 6 ) on duplicate key update c =
ifnull( c,
0 ) + 1;
select last_insert_id();
# test again when last_insert_id() is 0 initially
select last_insert_id(0);
insert into t2 ( a ) values ( 6 ) on duplicate key update c =
ifnull( c,
0 ) + 1;
select last_insert_id();
select * from t2;

# Test of LAST_INSERT_ID() when autogenerated will fail:
# last_insert_id() should not change
insert ignore into t2 values (null,6,1),(10,8,1);
select last_insert_id();
# First and second autogenerated will fail, last_insert_id() should
# point to third
insert ignore into t2 values (null,6,1),(null,8,1),(null,15,1),(null,20,1);
select last_insert_id();
select * from t2;

# Test of the workaround which enables people to know the id of the
# updated row in INSERT ON DUPLICATE KEY UPDATE, by using
# LAST_INSERT_ID(autoinc_col) in the UPDATE clause.

insert into t2 ( a ) values ( 6 ) on duplicate key update c =
ifnull( c,
0 ) + 1, k=last_insert_id(k);
select last_insert_id();
select * from t2;

drop table t2;


#
# Tests for bug #28415 "Some ALTER TABLE statements no longer work
# under LOCK TABLES" and some aspects of fast ALTER TABLE behaviour
# for transactional tables.
#
--disable_warnings
drop table if exists t1, t2;
--enable_warnings
create table t1 (i int);
alter table t1 modify i int default 1;
alter table t1 modify i int default 2, rename t2;
lock table t2 write;
alter table t2 modify i int default 3;
unlock tables;
lock table t2 write;
alter table t2 modify i int default 4, rename t1;
unlock tables;
drop table t1;


# 
# Some more tests for ALTER TABLE and LOCK TABLES for transactional tables.
#
# Table which is altered under LOCK TABLES should stay in list of locked
# tables and be available after alter takes place unless ALTER contains
# RENAME clause. We should see the new definition of table, of course.
# Before 5.1 this behavior was inconsistent across the platforms and
# different engines. See also tests in alter_table.test
#
--disable_warnings
drop table if exists t1;
--enable_warnings
create table t1 (i int);
insert into t1 values ();
lock table t1 write;
# Example of so-called 'fast' ALTER TABLE
alter table t1 modify i int default 1;
insert into t1 values ();
select * from t1;
# And now full-blown ALTER TABLE
alter table t1 change i c char(10) default "Two";
insert into t1 values ();
select * from t1;
unlock tables;
select * from t1;
drop tables t1;

#
# Bug#29310: An InnoDB table was updated when the data wasn't actually changed.
#
create table t1(f1 varchar(5) unique, f2 timestamp NOT NULL DEFAULT
  CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP);
insert into t1(f1) values(1);
--replace_column 1 #
select @a:=f2 from t1;
--sleep 5
update t1 set f1=1;
--replace_column 1 #
select @b:=f2 from t1;
select if(@a=@b,"ok","wrong");
--sleep 5
insert into t1(f1) values (1) on duplicate key update f1="1";
--replace_column 1 #
select @b:=f2 from t1;
select if(@a=@b,"ok","wrong");
--sleep 5
insert into t1(f1) select f1 from t1 on duplicate key update f1="1";
--replace_column 1 #
select @b:=f2 from t1;
select if(@a=@b,"ok","wrong");
drop table t1;

#
# Bug #31310: Locked rows silently skipped in read-committed isolation level.
#

connect (con1,localhost,root,,);
connect (con2,localhost,root,,);
SET SESSION AUTOCOMMIT = 0;
SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED;
--echo # Switch to connection con1
connection con1;

eval
CREATE TABLE t1 (a INT PRIMARY KEY, b VARCHAR(256))
ENGINE = $engine_type;
INSERT INTO t1 VALUES (1,2);

--echo # 1. test for locking:

BEGIN;
--enable_info
UPDATE t1 SET b = 12 WHERE a = 1;
--disable_info
SELECT * FROM t1;

--echo # Switch to connection con2
connection con2;

--enable_info
--disable_abort_on_error
--error ER_LOCK_WAIT_TIMEOUT
UPDATE t1 SET b = 21 WHERE a = 1;
--disable_info

--echo # Switch to connection con1
connection con1;
SELECT * FROM t1;
ROLLBACK;

--echo # 2. test for serialized update:

CREATE TABLE t2 (a INT);

TRUNCATE t1;
INSERT INTO t1 VALUES (1,'init');

DELIMITER |;
CREATE PROCEDURE p1()
BEGIN
  UPDATE t1 SET b = CONCAT(b, '+con2')  WHERE a = 1;
  INSERT INTO t2 VALUES ();
END|
DELIMITER ;|

BEGIN;
--enable_info
UPDATE t1 SET b = CONCAT(b, '+con1') WHERE a = 1;
--disable_info
SELECT * FROM t1;

--echo # Switch to connection con2
connection con2;

--send CALL p1;

--echo # Switch to connection con1
connection con1;
SELECT * FROM t1;
COMMIT;

let $bug31310 = 1;
while ($bug31310)
{
  let $bug31310= `SELECT 1 - COUNT(*) FROM t2`;
}

SELECT * FROM t1;

--echo # Switch to connection con2
connection con2;
SELECT * FROM t1;

--echo # Switch to connection con1
connection con1;

--echo # 3. test for updated key column:

TRUNCATE t1;
TRUNCATE t2;

INSERT INTO t1 VALUES (1,'init');

BEGIN;
--enable_info
UPDATE t1 SET a = 2, b = CONCAT(b, '+con1') WHERE a = 1;
--disable_info
SELECT * FROM t1;

--echo # Switch to connection con2
connection con2;

--send CALL p1;

--echo # Switch to connection con1
connection con1;
SELECT * FROM t1;
COMMIT;

let $bug31310 = 1;
while ($bug31310)
{
  let $bug31310= `SELECT 1 - COUNT(*) FROM t2`;
}

SELECT * FROM t1;

--echo # Switch to connection con2
connection con2;
SELECT * FROM t1;

connection default;
disconnect con1;
disconnect con2;
DROP PROCEDURE p1;
DROP TABLE t1, t2;
# Bug#30747 Create table with identical constraint names behaves incorrectly
#

if ($test_foreign_keys)
{
  CREATE TABLE t1 (a INT NOT NULL, b INT NOT NULL, PRIMARY KEY (a,b)) engine=innodb;
  --error ER_WRONG_FK_DEF
  CREATE TABLE t2 (c INT NOT NULL, d INT NOT NULL, PRIMARY KEY (c,d),
                  CONSTRAINT c2 FOREIGN KEY f2 (c) REFERENCES t1 (a,b) ON UPDATE NO ACTION) engine=innodb;
  --error ER_WRONG_FK_DEF
  CREATE TABLE t2 (c INT NOT NULL, d INT NOT NULL, PRIMARY KEY (c,d),
                  CONSTRAINT c2 FOREIGN KEY (c) REFERENCES t1 (a,b) ON UPDATE NO ACTION) engine=innodb;
  CREATE TABLE t2 (c INT NOT NULL, d INT NOT NULL, PRIMARY KEY (c,d),
                  CONSTRAINT c1 FOREIGN KEY c2 (c) REFERENCES t1 (a) ON DELETE NO ACTION,
                  CONSTRAINT c2 FOREIGN KEY (c) REFERENCES t1 (a) ON UPDATE NO ACTION) engine=innodb;
  ALTER TABLE t2 DROP FOREIGN KEY c2;
  DROP TABLE t2;
  --error ER_WRONG_FK_DEF
  CREATE TABLE t2 (c INT NOT NULL, d INT NOT NULL, PRIMARY KEY (c,d),
                  FOREIGN KEY (c) REFERENCES t1 (a,k) ON UPDATE NO ACTION) engine=innodb;
  --error ER_WRONG_FK_DEF
  CREATE TABLE t2 (c INT NOT NULL, d INT NOT NULL, PRIMARY KEY (c,d),
                  FOREIGN KEY f1 (c) REFERENCES t1 (a,k) ON UPDATE NO ACTION) engine=innodb;
  CREATE TABLE t2 (c INT NOT NULL, d INT NOT NULL, PRIMARY KEY (c,d),
                  CONSTRAINT c1 FOREIGN KEY f1 (c) REFERENCES t1 (a) ON DELETE NO ACTION,
                  CONSTRAINT c2 FOREIGN KEY (c) REFERENCES t1 (a) ON UPDATE NO ACTION,
                  FOREIGN KEY f3 (c) REFERENCES t1 (a) ON UPDATE NO ACTION,
                  FOREIGN KEY (c) REFERENCES t1 (a) ON UPDATE NO ACTION) engine=innodb;
  SHOW CREATE TABLE t2;
  DROP TABLE t2;
  DROP TABLE t1;
}

--echo End of 5.1 tests
