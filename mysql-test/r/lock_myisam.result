CREATE TABLE t1 (  `id` int(11) NOT NULL default '0', `id2` int(11) NOT NULL default '0', `id3` int(11) NOT NULL default '0', `dummy1` char(30) default NULL, PRIMARY KEY  (`id`,`id2`), KEY `index_id3` (`id3`)) ENGINE=MyISAM;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
insert into t1 (id,id2) values (1,1),(1,2),(1,3);
LOCK TABLE t1 WRITE;
select dummy1,count(distinct id) from t1 group by dummy1;
dummy1	count(distinct id)
NULL	1
update t1 set id=-1 where id=1;
LOCK TABLE t1 READ;
update t1 set id=1 where id=1;
ERROR HY000: Table 't1' was locked with a READ lock and can't be updated
create table t2 SELECT * from t1;
ERROR HY000: Table 't2' was not locked with LOCK TABLES
create temporary table t2 SELECT * from t1;
drop table if exists t2;
unlock tables;
create table t2 SELECT * from t1;
LOCK TABLE t1 WRITE,t2 write;
insert into t2 SELECT * from t1;
update t1 set id=1 where id=-1;
drop table t1,t2;
CREATE TABLE t1 (
index1 smallint(6) default NULL,
nr smallint(6) default NULL,
KEY index1(index1)
) ENGINE=MyISAM;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
CREATE TABLE t2 (
nr smallint(6) default NULL,
name varchar(20) default NULL
) ENGINE=MyISAM;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
INSERT INTO t2 VALUES (1,'item1');
INSERT INTO t2 VALUES (2,'item2');
lock tables t1 write, t2 read;
insert into t1 select 1,nr from t2 where name='item1';
insert into t1 select 2,nr from t2 where name='item2';
unlock tables;
check table t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
lock tables t1 write;
check table t2;
Table	Op	Msg_type	Msg_text
test.t2	check	Error	Table 't2' was not locked with LOCK TABLES
test.t2	check	status	Operation failed
insert into t1 select index1,nr from t1;
ERROR HY000: Table 't1' was not locked with LOCK TABLES
unlock tables;
lock tables t1 write, t1 as t1_alias read;
insert into t1 select index1,nr from t1 as t1_alias;
drop table t1,t2;
ERROR HY000: Table 't2' was not locked with LOCK TABLES
unlock tables;
drop table t1,t2;
#
# Ensure that REPAIR .. USE_FRM works under LOCK TABLES.
#
create table t1 (a int) engine=myisam;
create table t2 (a int) engine=myisam;
lock table t1 write, t2 write;
repair table t1 use_frm;
Table	Op	Msg_type	Msg_text
test.t1	repair	status	OK
repair table t1 use_frm;
Table	Op	Msg_type	Msg_text
test.t1	repair	status	OK
select * from t1;
a
select * from t2;
a
repair table t2 use_frm;
Table	Op	Msg_type	Msg_text
test.t2	repair	status	OK
repair table t2 use_frm;
Table	Op	Msg_type	Msg_text
test.t2	repair	status	OK
select * from t1;
a
unlock tables;
drop table t1, t2;
#
# Additional test coverage for WL#5534 - Online ALTER phase 1.
#
CREATE TABLE t1(a INT) ENGINE = MYISAM;
CREATE TABLE m1(a INT) engine=merge union(t1);
LOCK TABLES m1 WRITE;
ALTER TABLE m1 COMMENT 'test';
# This triggered an assert in MDL_ticket::downgrade_lock
FLUSH TABLE m1;
UNLOCK TABLES;
DROP TABLE m1, t1;
