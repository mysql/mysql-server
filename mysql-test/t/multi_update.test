#
# Test of update statement that uses many tables.
#

source include/have_log_bin.inc;

CALL mtr.add_suppression("Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT.");
CALL mtr.add_suppression("REVOKE/GRANT failed while storing table level "
                         "and column level grants in the privilege "
                         "tables. An incident event has been written to "
                         "the binary log which will stop the slaves.");
CALL mtr.add_suppression("REVOKE/GRANT failed while granting/revoking "
                         "privileges in databases. An incident event "
                         "has been written to the binary log which "
                         "will stop the slaves.");

--disable_warnings
drop table if exists t1,t2,t3;
drop database if exists mysqltest;
drop view if exists v1;
--error 0,ER_NONEXISTING_GRANT,ER_NONEXISTING_TABLE_GRANT
revoke all privileges on mysqltest.t1 from mysqltest_1@localhost;
--error 0,ER_NONEXISTING_GRANT,ER_NONEXISTING_TABLE_GRANT
revoke all privileges on mysqltest.* from mysqltest_1@localhost;
delete from mysql.user where user=_binary'mysqltest_1';
--enable_warnings

create table t1(id1 int not null auto_increment primary key, t char(12));
create table t2(id2 int not null, t char(12));
create table t3(id3 int not null, t char(12), index(id3));
disable_query_log;
let $1 = 100;
while ($1)
 {
  let $2 = 5;
  eval insert into t1(t) values ('$1');
  while ($2)
   {
     eval insert into t2(id2,t) values ($1,'$2');
     let $3 = 10;
     while ($3)
     {
       eval insert into t3(id3,t) values ($1,'$2');
       dec $3;
     }
     dec $2;
   }
  dec $1;
 }
enable_query_log;

select count(*) from t1 where id1 > 95;
select count(*) from t2 where id2 > 95;
select count(*) from t3 where id3 > 95;

update t1,t2,t3 set t1.t="aaa", t2.t="bbb", t3.t="cc" where  t1.id1 = t2.id2 and t2.id2 = t3.id3  and t1.id1 > 90;
select count(*) from t1 where t = "aaa";
select count(*) from t1 where id1 > 90;
select count(*) from t2 where t = "bbb";
select count(*) from t2 where id2 > 90;
select count(*) from t3 where t = "cc";
select count(*) from t3 where id3 > 90;
delete t1.*, t2.*, t3.*  from t1,t2,t3 where t1.id1 = t2.id2 and t2.id2 = t3.id3  and t1.id1 > 95;

check table t1, t2, t3;

select count(*) from t1 where id1 > 95;
select count(*) from t2 where id2 > 95;
select count(*) from t3 where id3 > 95;

delete t1, t2, t3  from t1,t2,t3 where t1.id1 = t2.id2 and t2.id2 = t3.id3  and t1.id1 > 5;
select count(*) from t1 where id1 > 5;
select count(*) from t2 where id2 > 5;
select count(*) from t3 where id3 > 5;

delete from t1, t2, t3  using t1,t2,t3 where t1.id1 = t2.id2 and t2.id2 = t3.id3  and t1.id1 > 0;

# These queries will force a scan of the table
select count(*) from t1 where id1;
select count(*) from t2 where id2;
select count(*) from t3 where id3;
drop table t1,t2,t3;

create table t1(id1 int not null  primary key, t varchar(100)) pack_keys = 1;
create table t2(id2 int not null, t varchar(100), index(id2)) pack_keys = 1;
disable_query_log;
let $1 = 1000;
while ($1)
 {
  let $2 = 5;
  eval insert into t1 values ($1,'aaaaaaaaaaaaaaaaaaaa');
  while ($2)
   {
     eval insert into t2(id2,t) values ($1,'bbbbbbbbbbbbbbbbb');
     dec $2;
   }
  dec $1;
 }
enable_query_log;
delete t1  from t1,t2 where t1.id1 = t2.id2 and t1.id1 > 500;
drop table t1,t2;

CREATE TABLE t1 (
  id int(11) NOT NULL default '0',
  name varchar(10) default NULL,
  PRIMARY KEY  (id)
);
INSERT INTO t1 VALUES (1,'aaa'),(2,'aaa'),(3,'aaa');
CREATE TABLE t2 (
  id int(11) NOT NULL default '0',
  name varchar(10) default NULL,
  PRIMARY KEY  (id)
);
INSERT INTO t2 VALUES (2,'bbb'),(3,'bbb'),(4,'bbb');
CREATE TABLE t3 (
  id int(11) NOT NULL default '0',
  mydate datetime default NULL,
  PRIMARY KEY  (id)
);
INSERT INTO t3 VALUES (1,'2002-02-04 00:00:00'),(3,'2002-05-12 00:00:00'),(5,'2002-05-12 00:00:00'),(6,'2002-06-22
00:00:00'),(7,'2002-07-22 00:00:00');
delete t1,t2,t3 from t1,t2,t3 where to_days(now())-to_days(t3.mydate)>=30 and t3.id=t1.id and t3.id=t2.id;
select * from t3;
DROP TABLE t1,t2,t3;

CREATE TABLE IF NOT EXISTS `t1` (
  `id` int(11) NOT NULL auto_increment,
  `tst` text,
  `tst1` text,
  PRIMARY KEY  (`id`)
);

CREATE TABLE IF NOT EXISTS `t2` (
  `ID` int(11) NOT NULL auto_increment,
  `ParId` int(11) default NULL,
  `tst` text,
  `tst1` text,
  PRIMARY KEY  (`ID`),
  KEY `IX_ParId_t2` (`ParId`),
  FOREIGN KEY (`ParId`) REFERENCES `t1` (`id`)
);

INSERT INTO t1(tst,tst1) VALUES("MySQL","MySQL AB"), ("MSSQL","Microsoft"), ("ORACLE","ORACLE");

INSERT INTO t2(ParId) VALUES(1), (2), (3);

select * from t2;

UPDATE t2, t1 SET t2.tst = t1.tst, t2.tst1 = t1.tst1 WHERE t2.ParId = t1.Id;

select * from t2;
drop table t2, t1 ;

create table t1 (n numeric(10));
create table t2 (n numeric(10));
insert into t2 values (1),(2),(4),(8),(16),(32);
select * from t2 left outer join t1  using (n);
delete  t1,t2 from t2 left outer join t1  using (n);
select * from t2 left outer join t1  using (n);
drop table t1,t2 ;

#
# Test with locking
#

create table t1 (n int(10) not null primary key, d int(10));
create table t2 (n int(10) not null primary key, d int(10));
insert into t1 values(1,1);
insert into t2 values(1,10),(2,20);
LOCK TABLES t1 write, t2 read;
--error ER_TABLE_NOT_LOCKED_FOR_WRITE
DELETE t1.*, t2.* FROM t1,t2 where t1.n=t2.n;
--error ER_TABLE_NOT_LOCKED_FOR_WRITE
UPDATE t1,t2 SET t1.d=t2.d,t2.d=30 WHERE t1.n=t2.n;
UPDATE t1,t2 SET t1.d=t2.d WHERE t1.n=t2.n;
unlock tables;
LOCK TABLES t1 write, t2 write;
UPDATE t1,t2 SET t1.d=t2.d WHERE t1.n=t2.n;
--skip_if_hypergraph  # Result depends on previous plan choice.
select * from t1;
DELETE t1.*, t2.* FROM t1,t2 where t1.n=t2.n;
--skip_if_hypergraph  # Result depends on previous plan choice.
select * from t1;
--skip_if_hypergraph  # Result depends on previous plan choice.
select * from t2;
unlock tables;
drop table t1,t2;

#
# Test safe updates and timestamps
#
set sql_safe_updates=1;
create table t1 (n int(10), d int(10));
create table t2 (n int(10), d int(10));
insert into t1 values(1,1);
insert into t2 values(1,10),(2,20);
--error ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE
UPDATE t1,t2 SET t1.d=t2.d WHERE t1.n=t2.n;
set sql_safe_updates=0;
drop table t1,t2;
set timestamp=1038401397;
create table t1 (n int(10) not null primary key, d int(10), t timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP);
create table t2 (n int(10) not null primary key, d int(10), t timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP);
insert into t1(n,d) values(1,1);
insert into t2(n,d) values(1,10),(2,20);
set timestamp=1038000000;
UPDATE t1,t2 SET t1.d=t2.d WHERE t1.n=t2.n;
select n,d,unix_timestamp(t) from t1;
select n,d,unix_timestamp(t) from t2;
--error ER_PARSE_ERROR
UPDATE t1,t2 SET 1=2 WHERE t1.n=t2.n;
drop table t1,t2;
set timestamp=0;
set sql_safe_updates=0;
create table t1 (n int(10) not null primary key, d int(10));
create table t2 (n int(10) not null primary key, d int(10));
insert into t1 values(1,1), (3,3);
insert into t2 values(1,10),(2,20);
UPDATE t2 left outer join t1 on t1.n=t2.n  SET t1.d=t2.d;
select * from t1;
select * from t2;
drop table t1,t2;
create table t1 (n int(10), d int(10));
create table t2 (n int(10), d int(10));
insert into t1 values(1,1),(1,2);
insert into t2 values(1,10),(2,20);
UPDATE t1,t2 SET t1.d=t2.d,t2.d=30 WHERE t1.n=t2.n;
--skip_if_hypergraph  # Result depends on previous plan choice.
select * from t1;
--skip_if_hypergraph  # Result depends on previous plan choice.
select * from t2;
drop table t1,t2;
create table t1 (n int(10), d int(10));
create table t2 (n int(10), d int(10));
insert into t1 values(1,1),(3,2);
insert into t2 values(1,10),(1,20);
UPDATE t1,t2 SET t1.d=t2.d,t2.d=30 WHERE t1.n=t2.n;
# It is unspecified which order the assignments are performed in, and
# in which order the rows from t2 are read, so for n=1 the value of d
# can end up as 10, 20 or 30, depending on the plan chosen.
--replace_result 20 10 30 10
--skip_if_hypergraph  # Result depends on previous plan choice.
select * from t1;
--skip_if_hypergraph  # Result depends on previous plan choice.
select * from t2;
UPDATE t1 a ,t2 b SET a.d=b.d,b.d=30 WHERE a.n=b.n;
--skip_if_hypergraph  # Result depends on previous plan choice.
select * from t1;
--skip_if_hypergraph  # Result depends on previous plan choice.
select * from t2;
DELETE a, b  FROM t1 a,t2 b where a.n=b.n;
--skip_if_hypergraph  # Result depends on previous plan choice.
select * from t1;
--skip_if_hypergraph  # Result depends on previous plan choice.
select * from t2;
drop table t1,t2;

CREATE TABLE t1 ( broj int(4) unsigned NOT NULL default '0',  naziv char(25) NOT NULL default 'NEPOZNAT',  PRIMARY KEY  (broj));
INSERT INTO t1 VALUES (1,'jedan'),(2,'dva'),(3,'tri'),(4,'xxxxxxxxxx'),(5,'a'),(10,''),(11,''),(12,''),(13,'');
CREATE TABLE t2 ( broj int(4) unsigned NOT NULL default '0',  naziv char(25) NOT NULL default 'NEPOZNAT',  PRIMARY KEY  (broj));
INSERT INTO t2 VALUES (1,'jedan'),(2,'dva'),(3,'tri'),(4,'xxxxxxxxxx'),(5,'a');
CREATE TABLE t3 ( broj int(4) unsigned NOT NULL default '0',  naziv char(25) NOT NULL default 'NEPOZNAT',  PRIMARY KEY  (broj));
INSERT INTO t3 VALUES (1,'jedan'),(2,'dva');
update t1,t2 set t1.naziv="aaaa" where t1.broj=t2.broj;
update t1,t2,t3 set t1.naziv="bbbb", t2.naziv="aaaa" where t1.broj=t2.broj and t2.broj=t3.broj;
drop table t1,t2,t3;

#
# Test multi update with different join methods
#

CREATE TABLE t1 (a int not null primary key, b int not null, key (b));
CREATE TABLE t2 (a int not null primary key, b int not null, key (b));
INSERT INTO t1 values (1,1),(2,2),(3,3),(4,4),(5,5),(6,6),(7,7),(8,8),(9,9);
INSERT INTO t2 values (1,1),(2,2),(3,3),(4,4),(5,5),(6,6),(7,7),(8,8),(9,9);

# Full join, without key
update t1,t2 set t1.a=t1.a+100;
--sorted_result
select * from t1;

# unique key
update t1,t2 set t1.a=t1.a+100 where t1.a=101;
--sorted_result
select * from t1;

# ref key
update t1,t2 set t1.b=t1.b+10 where t1.b=2;
--sorted_result
select * from t1;

# Range key (in t1)
update t1,t2 set t1.b=t1.b+2,t2.b=t1.b+10 where t1.b between 3 and 5 and t2.a=t1.a-100;
--sorted_result
select * from t1;
# It is unspecified whether the assignment of t2.b sees the
# modification of t1.b on the same row. The hypergraph optimizer
# chooses a different plan (not using range scan on t1.b, as that
# prevents immediate update) and does the assignments in a different
# order.
--skip_if_hypergraph
--sorted_result
select * from t2;

# test for non-updating table which is also used in sub-select

update t1,t2 set t1.b=t2.b, t1.a=t2.a where t1.a=t2.a and not exists (select * from t2 where t2.a > 10);

drop table t1,t2;
CREATE TABLE t3 (  KEY1 varchar(50) NOT NULL default '',  PARAM_CORR_DISTANCE_RUSH double default NULL,  PARAM_CORR_DISTANCE_GEM double default NULL,  PARAM_AVG_TARE double default NULL,  PARAM_AVG_NB_DAYS double default NULL,  PARAM_DEFAULT_PROP_GEM_SRVC varchar(50) default NULL,  PARAM_DEFAULT_PROP_GEM_NO_ETIK varchar(50) default NULL,  PARAM_SCENARIO_COSTS varchar(50) default NULL,  PARAM_DEFAULT_WAGON_COST double default NULL,  tmp int(11) default NULL,  PRIMARY KEY  (KEY1));
INSERT INTO t3 VALUES ('A',1,1,22,3.2,'R','R','BASE2',0.24,NULL);
create table t1 (A varchar(1));
insert into t1 values  ("A") ,("B"),("C"),("D");
create table t2(Z varchar(15));
insert into t2(Z)  select concat(a.a,b.a,c.a,d.a) from t1 as a, t1 as b, t1 as c, t1 as d;
update t2,t3 set Z =param_scenario_costs;
drop table t1,t2,t3;
create table t1 (a int, b int);
create table t2 (a int, b int);
insert into t1 values (1,1),(2,1),(3,1);
insert into t2 values (1,1), (3,1);
update t1 left join t2  on t1.a=t2.a set t1.b=2, t2.b=2 where t1.b=1 and t2.b=1 or t2.a is NULL;
select t1.a, t1.b,t2.a, t2.b from t1 left join t2  on t1.a=t2.a where t1.b=1 and t2.b=1 or t2.a is NULL;
drop table t1,t2;

#
# Test reuse of same table
#

create table t1 (a int not null auto_increment primary key, b int not null);
insert into t1 (b) values (1),(2),(3),(4);
update t1, t1 as t2 set t1.b=t2.b+1 where t1.a=t2.a;
select * from t1;
drop table t1;

# Test multi-update and multi-delete with impossible where

create table t1(id1 smallint(5), field char(5));
create table t2(id2 smallint(5), field char(5));

insert into t1 values (1, 'a'), (2, 'aa');
insert into t2 values (1, 'b'), (2, 'bb');

select * from t1;
select * from t2;

update t2 inner join t1 on t1.id1=t2.id2
  set t2.field=t1.field
  where 0=1;
update t2, t1 set t2.field=t1.field
  where t1.id1=t2.id2 and 0=1;

delete t1, t2 from t2 inner join t1 on t1.id1=t2.id2
  where 0=1;
delete t1, t2 from t2,t1
  where t1.id1=t2.id2 and 0=1;

drop table t1,t2;

#
# Test alias (this is not correct in 4.0)
#

CREATE TABLE t1 ( a int );
CREATE TABLE t2 ( a int );
DELETE t1 FROM t1, t2 AS t3;
DELETE t4 FROM t1, t1 AS t4;
DELETE t3 FROM t1 AS t3, t1 AS t4;
--error ER_UNKNOWN_TABLE
DELETE t1 FROM t1 AS t3, t2 AS t4;
INSERT INTO t1 values (1),(2);
INSERT INTO t2 values (1),(2);
DELETE t1 FROM t1 AS t2, t2 AS t1 where t1.a=t2.a and t1.a=1;
SELECT * from t1;
SELECT * from t2;
DELETE t2 FROM t1 AS t2, t2 AS t1 where t1.a=t2.a and t1.a=2;
SELECT * from t1;
SELECT * from t2;
DROP TABLE t1,t2;

#
# Test update with const tables
#
create table `t1` (`p_id` int(10) unsigned NOT NULL auto_increment, `p_code` varchar(20) NOT NULL default '', `p_active` tinyint(1) unsigned NOT NULL default '1', PRIMARY KEY (`p_id`) );
create table `t2` (`c2_id` int(10) unsigned NOT NULL auto_increment, `c2_p_id` int(10) unsigned NOT NULL default '0', `c2_note` text NOT NULL, `c2_active` tinyint(1) unsigned NOT NULL default '1', PRIMARY KEY (`c2_id`), KEY `c2_p_id` (`c2_p_id`) );
insert into t1 values (0,'A01-Comp',1);
insert into t1 values (0,'B01-Comp',1);
insert into t2 values (0,1,'A Note',1);
update t1 left join t2 on p_id = c2_p_id set c2_note = 'asdf-1' where p_id = 2;
select * from t1;
select * from t2;
drop table t1, t2;

#
# privilege check for multiupdate with other tables
#

connect (root,localhost,root,,test,$MASTER_MYPORT,$MASTER_MYSOCK);
connection root;
--disable_warnings
create database mysqltest;
--enable_warnings
create table mysqltest.t1 (a int, b int, primary key (a));
create table mysqltest.t2 (a int, b int, primary key (a));
create table mysqltest.t3 (a int, b int, primary key (a));
create user mysqltest_1@localhost;
grant select on mysqltest.* to mysqltest_1@localhost;
grant update on mysqltest.t1 to mysqltest_1@localhost;
connect (user1,localhost,mysqltest_1,,mysqltest,$MASTER_MYPORT,$MASTER_MYSOCK);
connection user1;
update t1, t2 set t1.b=1 where t1.a=t2.a;
update t1, t2 set t1.b=(select t3.b from t3 where t1.a=t3.a) where t1.a=t2.a;
connection root;
revoke all privileges on mysqltest.t1 from mysqltest_1@localhost;
revoke all privileges on mysqltest.* from mysqltest_1@localhost;
delete from mysql.user where user=_binary'mysqltest_1';
drop database mysqltest;
connection default;
disconnect user1;
disconnect root;

#
# multi delete wrong table check
#
create table t1 (a int, primary key (a));
create table t2 (a int, primary key (a));
create table t3 (a int, primary key (a));
-- error ER_UNKNOWN_TABLE
delete t1,t3 from t1,t2 where t1.a=t2.a and t2.a=(select t3.a from t3 where t1.a=t3.a);
drop table t1, t2, t3;

#
# multi* unique updating table check
#
create table t1 (col1 int);
create table t2 (col1 int);
-- error ER_UPDATE_TABLE_USED
update t1,t2 set t1.col1 = (select max(col1) from t1) where t1.col1 = t2.col1;
-- error ER_UPDATE_TABLE_USED
delete t1 from t1,t2 where t1.col1 < (select max(col1) from t1) and t1.col1 = t2.col1;
drop table t1,t2;

# Test for Bug#5837 delete with outer join and const tables
--disable_warnings
create table t1 (
  aclid bigint not null primary key,
  status tinyint(1) not null
) engine = innodb;

create table t2 (
  refid bigint not null primary key,
  aclid bigint, index idx_acl(aclid)
) engine = innodb;
--enable_warnings
insert into t2 values(1,null);
delete t2, t1 from t2 left join t1 on (t2.aclid=t1.aclid) where t2.refid='1';
drop table t1, t2;

#
# Bug#19225 unchecked error leads to server crash
#
create table t1(a int);
create table t2(a int);
--error ER_UPDATE_TABLE_USED
delete from t1,t2 using t1,t2 where t1.a=(select a from t1);
drop table t1, t2;
# End of 4.1 tests

#
# Test for Bug#1980.
#
--disable_warnings
create table t1 ( c char(8) not null ) engine=innodb;
--enable_warnings

insert into t1 values ('0'),('1'),('2'),('3'),('4'),('5'),('6'),('7'),('8'),('9');
insert into t1 values ('A'),('B'),('C'),('D'),('E'),('F');

alter table t1 add b char(8) not null;
alter table t1 add a char(8) not null;
alter table t1 add primary key (a,b,c);
update t1 set a=c, b=c;

create table t2 like t1;
insert into t2 select * from t1;

delete t1,t2 from t2,t1 where t1.a<'B' and t2.b=t1.b;

drop table t1,t2;

--disable_warnings
create table t1 ( c char(8) not null ) engine=innodb;
--enable_warnings

insert into t1 values ('0'),('1'),('2'),('3'),('4'),('5'),('6'),('7'),('8'),('9');
insert into t1 values ('A'),('B'),('C'),('D'),('E'),('F');

alter table t1 add b char(8) not null;
alter table t1 add a char(8) not null;
alter table t1 add primary key (a,b,c);
update t1 set a=c, b=c;

create table t2 like t1;
insert into t2 select * from t1;

delete t1,t2 from t2,t1 where t1.a<'B' and t2.b=t1.b;

drop table t1,t2;

#
# Test alter table and a concurrent multi update
# (Before we have introduced data-lock-aware metadata locks
#  this test case forced update to reopen tables).
#

create table t1 (a int, b int);
insert into t1 values (1, 2), (2, 3), (3, 4);
create table t2 (a int);
insert into t2 values (10), (20), (30);
create view v1 as select a as b, a/10 as a from t2;

connect (locker,localhost,root,,test);
connection locker;
lock table t1 write;

connect (changer,localhost,root,,test);
connection changer;
send alter table t1 add column c int default 100 after a;

connect (updater,localhost,root,,test);
connection updater;
# Wait till "alter table t1 ..." of session changer is in work.
# = There is one session waiting.
let $wait_condition= select count(*)= 1 from information_schema.processlist
                     where state= 'Waiting for table metadata lock';
--source include/wait_condition.inc
send update t1, v1 set t1.b=t1.a+t1.b+v1.b where t1.a=v1.a;

connection locker;
# Wait till
# - "alter table t1 ..." of session changer and
# - "update t1, v1 ..." of session updater
# are in work.
# = There are two session waiting.
let $wait_condition= select count(*)= 2 from information_schema.processlist
                     where state= 'Waiting for table metadata lock';
--source include/wait_condition.inc
unlock tables;

connection changer;
reap;

connection updater;
reap;
select * from t1;
select * from t2;
drop view v1;
drop table t1, t2;

connection default;
disconnect locker;
disconnect changer;
disconnect updater;

#
# Test multi updates and deletes using primary key and without.
#
create table t1 (i1 int, i2 int, i3 int);
create table t2 (id int, c1 varchar(20), c2 varchar(20));
insert into t1 values (1,5,10),(3,7,12),(4,5,2),(9,10,15),(2,2,2);
insert into t2 values (9,"abc","def"),(5,"opq","lmn"),(2,"test t","t test");
select * from t1 order by i1;
select * from t2;
update t1,t2 set t1.i2=15, t2.c2="ppc" where t1.i1=t2.id;
select * from t1 order by i1;
select * from t2 order by id;
delete t1.*,t2.* from t1,t2 where t1.i2=t2.id;
select * from t1 order by i1;
select * from t2 order by id;
drop table t1, t2;
create table t1 (i1 int auto_increment not null, i2 int, i3 int, primary key (i1));
create table t2 (id int auto_increment not null, c1 varchar(20), c2 varchar(20), primary key(id));
insert into t1 values (1,5,10),(3,7,12),(4,5,2),(9,10,15),(2,2,2);
insert into t2 values (9,"abc","def"),(5,"opq","lmn"),(2,"test t","t test");
select * from t1 order by i1;
select * from t2 order by id;
update t1,t2 set t1.i2=15, t2.c2="ppc" where t1.i1=t2.id;
select * from t1 order by i1;
select * from t2 order by id;
delete t1.*,t2.* from t1,t2 where t1.i2=t2.id;
select * from t1 order by i1;
select * from t2 order by id;
drop table t1, t2;

#
# Bug#29136 erred multi-delete on trans table does not rollback
#

# prepare
--disable_warnings
drop table if exists t1, t2, t3;
--enable_warnings
CREATE TABLE t1 (a int, PRIMARY KEY (a));
CREATE TABLE t2 (a int, PRIMARY KEY (a));
CREATE TABLE t3 (a int, PRIMARY KEY (a));
create trigger trg_del_t3 before  delete on t3 for each row insert into t1 values (1);

insert into t2 values (1),(2);
insert into t3 values (1),(2);
reset binary logs and gtids;

# exec cases B, A - see innodb.test

# B. send_eof() and send_error() afterward

--error ER_DUP_ENTRY
delete t3.* from t2,t3 where t2.a=t3.a;

# check
select count(*) from t1 /* must be 0 */;
select count(*) from t3 /* must be 2 */;

# cleanup
drop table t1, t2, t3;

#
# Add further tests from here
#

--echo #
--echo # Bug#49534: multitable IGNORE update with sql_safe_updates error
--echo # causes debug assertion
--echo #
CREATE TABLE t1( a INT, KEY( a ) );
INSERT INTO t1 VALUES (1), (2), (3);
SET SESSION sql_safe_updates = 1;
--echo # Must not cause failed assertion
--error ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE
UPDATE IGNORE t1, t1 t1a SET t1.a = 1 WHERE t1a.a = 1;
DROP TABLE t1;

--echo #
--echo # Bug#54543: update ignore with incorrect subquery leads to assertion
--echo # failure: inited==INDEX
--echo #
set @optimizer_switch_saved=@@optimizer_switch;
set optimizer_switch='derived_merge=off';
SET SESSION sql_safe_updates = 0;
CREATE TABLE t1 ( a INT );
INSERT INTO t1 VALUES (1), (2);

CREATE TABLE t2 ( a INT );
INSERT INTO t2 VALUES (1), (2);

CREATE TABLE t3 ( a INT );
INSERT INTO t3 VALUES (1), (2);

--echo # This test has lost its original coverage for Bug#54543 now
--echo # that ER_SUBQUERY_NO_1_ROW is no longer ignored
--error ER_SUBQUERY_NO_1_ROW
UPDATE IGNORE
  ( SELECT ( SELECT COUNT(*) FROM t1 GROUP BY a, @v ) a FROM t2 ) x, t3
SET t3.a = 0;

DROP TABLE t1, t2, t3;
SET SESSION sql_safe_updates = DEFAULT;
set @@optimizer_switch=@optimizer_switch_saved;

--echo #
--echo # Bug#52157 various crashes and assertions with multi-table update, stored function
--echo #

set @optimizer_switch_saved=@@optimizer_switch;
set optimizer_switch='derived_merge=off';

# Updated the test case with another one provided in the bug report
# as the previous case returns error now, due to invalid date

CREATE FUNCTION f1 () RETURNS BLOB RETURN '2011-01-01';
CREATE TABLE t1 (f1 DATE);
INSERT INTO t1 VALUES('2001-01-01');
UPDATE IGNORE (SELECT 1 FROM t1 WHERE f1 = (SELECT f1() FROM t1)) x, t1 SET f1 = '2011-01-01';
DROP FUNCTION f1;
DROP TABLE t1;

CREATE TABLE t1(a TEXT, FULLTEXT(a)) engine = blackhole;
--disable_warnings
UPDATE IGNORE (SELECT 1 FROM t1 WHERE(MATCH(a) AGAINST(''))) `x`,`t1` SET a = 1;
--enable_warnings
DROP TABLE t1;

set @@optimizer_switch=@optimizer_switch_saved;

--echo # Bug#13256831 - ERROR 1032 (HY000): CAN'T FIND RECORD

CREATE TABLE t1 (f1 INT PRIMARY KEY, f2 INT) ENGINE=InnoDB;
CREATE TABLE t2 (f1 INT PRIMARY KEY, f2 INT) ENGINE=InnoDB;
INSERT INTO t1 VALUES (5, 7);
INSERT INTO t2 VALUES (6, 97);

CREATE ALGORITHM = MERGE VIEW v1 AS 
SELECT a2.f1 AS f1, a2.f2 AS f2
FROM t1 AS a1 JOIN t2 AS a2 ON a1.f2 > a2.f1 
WITH LOCAL CHECK OPTION; 

SELECT * FROM v1;
UPDATE v1 SET f1 = 1;
SELECT * FROM v1;

DROP TABLE t1, t2;
DROP VIEW v1;

--echo #
--echo # BUG #11766576 - 59715: UPDATE IGNORE, 1 ROW AFFECTED
--echo #

--disable_warnings
DROP TABLE IF EXISTS t1, t2;
--enable_warnings

CREATE TABLE t1 (id INT PRIMARY KEY);
INSERT INTO t1 VALUES (1), (2);

--enable_info
UPDATE IGNORE t1, (SELECT 1 AS duplicate_id) AS t2 SET t1.id=t2.duplicate_id;
--disable_info

--echo # Check that no rows changed.
SELECT * FROM t1;
DROP TABLE t1;

--echo #
--echo # Bug #18352634 	"UPDATE ORDER BY" OF MULTI-TABLE VIEW DOES NOT WORK
--echo #

CREATE TABLE t1 (a INT) ENGINE=INNODB;
CREATE VIEW v AS SELECT t1.a FROM t1,t1 q;
# different errors in ps / non-ps mode:
-- error ER_WRONG_USAGE, ER_VIEW_PREVENT_UPDATE
UPDATE v SET a=1 ORDER BY a;
-- error ER_WRONG_USAGE, ER_VIEW_PREVENT_UPDATE
UPDATE v SET a=1 LIMIT 3;
DROP TABLE t1;
DROP VIEW v;

--echo #
--echo # WL#5275: Multi-table update, view with subquery in CHECK OPTION
--echo #

CREATE TABLE t1 (f1 INTEGER PRIMARY KEY) ENGINE=InnoDB;
CREATE TABLE t2 (f1 INTEGER PRIMARY KEY, f2 INTEGER) ENGINE=InnoDB;
CREATE TABLE t3 (f1 INTEGER) ENGINE=INNODB;

INSERT INTO t1 VALUES (1), (2);
INSERT INTO t2 VALUES (1, 1), (2, 2);
INSERT INTO t3 VALUES (1), (2);

CREATE VIEW v2 AS 
SELECT * FROM t2 WHERE f2 IN (SELECT f1 FROM t3)
WITH CHECK OPTION; 

SELECT * FROM t1 JOIN v2 ON t1.f1=v2.f1;

UPDATE t1 JOIN v2 ON t1.f1=v2.f1
SET f2 = f2 + 1
WHERE t1.f1=1;

--error ER_VIEW_CHECK_FAILED
UPDATE t1 JOIN v2 ON t1.f1=v2.f1
SET f2 = f2 + 1
WHERE t1.f1=2;

SELECT * FROM t1 JOIN v2 ON t1.f1=v2.f1;

DROP VIEW v2;
DROP TABLE t1, t2, t3;

--echo #
--echo # Bug 18449085: WRONG VALUE AFTER MULTI UPDATE
--echo #

CREATE TABLE t1 (c1 INTEGER, c2 INTEGER, KEY(c1));
CREATE TABLE t2 (c1 INTEGER, c2 INTEGER);
CREATE TABLE t3 (c1 INTEGER, c2 INTEGER);
INSERT INTO t1 VALUES(1,1),(2,2),(3,3),(4,4),(5,5);
INSERT INTO t2 VALUES(11,1),(12,1),(13,1),(14,2),(15,6);
INSERT INTO t3 VALUES(21,11),(22,11),(23,13),(24,14),(25,15);
ANALYZE TABLE t1, t2, t3;
let $my_stmt= UPDATE t2 straight_join t3 straight_join t1 SET t1.c2 = 30, t2.c2 = 40, t3.c2=50
              WHERE t1.c1=t2.c2 AND t2.c1=t3.c2;
eval $my_stmt;
eval EXPLAIN $my_stmt;
SELECT * FROM t3 ORDER BY c1;
DROP TABLE t1, t2, t3;

--echo #
--echo # Bug#98692 multi-table UPDATE has problem when updating a table in Performance Schema
--echo #

let $check_query=
  SELECT enabled FROM performance_schema.setup_instruments
    WHERE name = 'wait/lock/metadata/sql/mdl';
eval $check_query;
let $query=
  UPDATE performance_schema.setup_instruments, (SELECT 1) AS dt
    SET enabled = 'NO' WHERE name = 'wait/lock/metadata/sql/mdl';
eval EXPLAIN $query;
eval $query;
# Not testing DELETE, as it's not possible on
# performance_schema.setup_instruments.
eval $check_query;
let $query=
  UPDATE (VALUES ROW(1),ROW(2)) AS dt(a)
    LEFT JOIN performance_schema.setup_instruments ON dt.a=enabled
    SET enabled = 'YES' WHERE name = 'wait/lock/metadata/sql/mdl';
eval EXPLAIN $query;
eval $query;
eval $check_query;

# Now update an InnoDB const table, and check that it does get updated.
CREATE TABLE t1 (a INT PRIMARY KEY, b INT);
INSERT INTO t1 VALUES(1,1);

let $query=
  UPDATE t1 STRAIGHT_JOIN performance_schema.setup_instruments
    SET t1.b = 2 WHERE t1.a = 1 AND name = 'wait/lock/metadata/sql/mdl';
eval EXPLAIN $query;
eval $query;
SELECT * FROM t1;

let $query=
  UPDATE performance_schema.setup_instruments STRAIGHT_JOIN t1
    SET t1.b = 3 WHERE t1.a = 1 AND name = 'wait/lock/metadata/sql/mdl';
eval EXPLAIN $query;
eval $query;
SELECT * FROM t1;

DROP TABLE t1;

--echo # Bug#31640267: Assertion `trans_safe || updated_rows == 0 || thd->get_transaction()

CREATE TABLE t1(pk INTEGER PRIMARY KEY, a INTEGER);
INSERT INTO t1 VALUES(1, 10), (2, 20);

PREPARE s FROM 'UPDATE t1, (SELECT 1 FROM DUAL) AS dt SET a=a+1';
EXECUTE s;
SELECT ROW_COUNT();
EXECUTE s;
SELECT ROW_COUNT();
DEALLOCATE PREPARE s;

DROP TABLE t1;
