drop table if exists t1,t2,t3,t4;
drop database if exists mysqltest;
create table t1 (id int unsigned not null auto_increment, code tinyint unsigned not null, name char(20) not null, primary key (id), key (code), unique (name)) engine=innodb;
insert into t1 (code, name) values (1, 'Tim'), (1, 'Monty'), (2, 'David'), (2, 'Erik'), (3, 'Sasha'), (3, 'Jeremy'), (4, 'Matt');
select id, code, name from t1 order by id;
id	code	name
1	1	Tim
2	1	Monty
3	2	David
4	2	Erik
5	3	Sasha
6	3	Jeremy
7	4	Matt
update ignore t1 set id = 8, name = 'Sinisa' where id < 3;
select id, code, name from t1 order by id;
id	code	name
2	1	Monty
3	2	David
4	2	Erik
5	3	Sasha
6	3	Jeremy
7	4	Matt
8	1	Sinisa
update ignore t1 set id = id + 10, name = 'Ralph' where id < 4;
select id, code, name from t1 order by id;
id	code	name
3	2	David
4	2	Erik
5	3	Sasha
6	3	Jeremy
7	4	Matt
8	1	Sinisa
12	1	Ralph
drop table t1;
CREATE TABLE t1 (
id int(11) NOT NULL auto_increment,
parent_id int(11) DEFAULT '0' NOT NULL,
level tinyint(4) DEFAULT '0' NOT NULL,
PRIMARY KEY (id),
KEY parent_id (parent_id),
KEY level (level)
) engine=innodb;
INSERT INTO t1 VALUES (1,0,0),(3,1,1),(4,1,1),(8,2,2),(9,2,2),(17,3,2),(22,4,2),(24,4,2),(28,5,2),(29,5,2),(30,5,2),(31,6,2),(32,6,2),(33,6,2),(203,7,2),(202,7,2),(20,3,2),(157,0,0),(193,5,2),(40,7,2),(2,1,1),(15,2,2),(6,1,1),(34,6,2),(35,6,2),(16,3,2),(7,1,1),(36,7,2),(18,3,2),(26,5,2),(27,5,2),(183,4,2),(38,7,2),(25,5,2),(37,7,2),(21,4,2),(19,3,2),(5,1,1),(179,5,2);
update t1 set parent_id=parent_id+100;
select * from t1 where parent_id=102;
id	parent_id	level
8	102	2
9	102	2
15	102	2
update t1 set id=id+1000;
update t1 set id=1024 where id=1009;
Got one of the listed errors
select * from t1;
id	parent_id	level
1001	100	0
1002	101	1
1003	101	1
1004	101	1
1005	101	1
1006	101	1
1007	101	1
1008	102	2
1009	102	2
1015	102	2
1016	103	2
1017	103	2
1018	103	2
1019	103	2
1020	103	2
1021	104	2
1022	104	2
1024	104	2
1025	105	2
1026	105	2
1027	105	2
1028	105	2
1029	105	2
1030	105	2
1031	106	2
1032	106	2
1033	106	2
1034	106	2
1035	106	2
1036	107	2
1037	107	2
1038	107	2
1040	107	2
1157	100	0
1179	105	2
1183	104	2
1193	105	2
1202	107	2
1203	107	2
update ignore t1 set id=id+1;
select * from t1;
id	parent_id	level
1001	100	0
1002	101	1
1003	101	1
1004	101	1
1005	101	1
1006	101	1
1007	101	1
1008	102	2
1010	102	2
1015	102	2
1016	103	2
1017	103	2
1018	103	2
1019	103	2
1020	103	2
1021	104	2
1023	104	2
1024	104	2
1025	105	2
1026	105	2
1027	105	2
1028	105	2
1029	105	2
1030	105	2
1031	106	2
1032	106	2
1033	106	2
1034	106	2
1035	106	2
1036	107	2
1037	107	2
1039	107	2
1041	107	2
1158	100	0
1180	105	2
1184	104	2
1194	105	2
1202	107	2
1204	107	2
update ignore t1 set id=1023 where id=1010;
select * from t1 where parent_id=102;
id	parent_id	level
1008	102	2
1010	102	2
1015	102	2
explain select level from t1 where level=1;
id	select_type	table	type	possible_keys	key	key_len	ref	rows	Extra
1	SIMPLE	t1	ref	level	level	1	const	#	Using where; Using index
explain select level,id from t1 where level=1;
id	select_type	table	type	possible_keys	key	key_len	ref	rows	Extra
1	SIMPLE	t1	ref	level	level	1	const	#	Using where; Using index
explain select level,id,parent_id from t1 where level=1;
id	select_type	table	type	possible_keys	key	key_len	ref	rows	Extra
1	SIMPLE	t1	ref	level	level	1	const	#	Using where
select level,id from t1 where level=1;
level	id
1	1002
1	1003
1	1004
1	1005
1	1006
1	1007
select level,id,parent_id from t1 where level=1;
level	id	parent_id
1	1002	101
1	1003	101
1	1004	101
1	1005	101
1	1006	101
1	1007	101
optimize table t1;
Table	Op	Msg_type	Msg_text
test.t1	optimize	status	OK
show keys from t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment
t1	0	PRIMARY	1	id	A	#	NULL	NULL		BTREE	
t1	1	parent_id	1	parent_id	A	#	NULL	NULL		BTREE	
t1	1	level	1	level	A	#	NULL	NULL		BTREE	
drop table t1;
CREATE TABLE t1 (
gesuchnr int(11) DEFAULT '0' NOT NULL,
benutzer_id int(11) DEFAULT '0' NOT NULL,
PRIMARY KEY (gesuchnr,benutzer_id)
) engine=innodb;
replace into t1 (gesuchnr,benutzer_id) values (2,1);
replace into t1 (gesuchnr,benutzer_id) values (1,1);
replace into t1 (gesuchnr,benutzer_id) values (1,1);
select * from t1;
gesuchnr	benutzer_id
1	1
2	1
drop table t1;
create table t1 (a int) engine=innodb;
insert into t1 values (1), (2);
optimize table t1;
Table	Op	Msg_type	Msg_text
test.t1	optimize	status	OK
delete from t1 where a = 1;
select * from t1;
a
2
check table t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
drop table t1;
create table t1 (a int,b varchar(20)) engine=innodb;
insert into t1 values (1,""), (2,"testing");
delete from t1 where a = 1;
select * from t1;
a	b
2	testing
create index skr on t1 (a);
insert into t1 values (3,""), (4,"testing");
analyze table t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
show keys from t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment
t1	1	skr	1	a	A	#	NULL	NULL	YES	BTREE	
drop table t1;
create table t1 (a int,b varchar(20),key(a)) engine=innodb;
insert into t1 values (1,""), (2,"testing");
select * from t1 where a = 1;
a	b
1	
drop table t1;
create table t1 (n int not null primary key) engine=innodb;
set autocommit=0;
insert into t1 values (4);
rollback;
select n, "after rollback" from t1;
n	after rollback
insert into t1 values (4);
commit;
select n, "after commit" from t1;
n	after commit
4	after commit
commit;
insert into t1 values (5);
insert into t1 values (4);
ERROR 23000: Duplicate entry '4' for key 1
commit;
select n, "after commit" from t1;
n	after commit
4	after commit
5	after commit
set autocommit=1;
insert into t1 values (6);
insert into t1 values (4);
ERROR 23000: Duplicate entry '4' for key 1
select n from t1;
n
4
5
6
rollback;
drop table t1;
create table t1 (n int not null primary key) engine=innodb;
start transaction;
insert into t1 values (4);
flush tables with read lock;
commit;
unlock tables;
commit;
select * from t1;
n
4
drop table t1;
create table t1 ( id int NOT NULL PRIMARY KEY, nom varchar(64)) engine=innodb;
begin;
insert into t1 values(1,'hamdouni');
select id as afterbegin_id,nom as afterbegin_nom from t1;
afterbegin_id	afterbegin_nom
1	hamdouni
rollback;
select id as afterrollback_id,nom as afterrollback_nom from t1;
afterrollback_id	afterrollback_nom
set autocommit=0;
insert into t1 values(2,'mysql');
select id as afterautocommit0_id,nom as afterautocommit0_nom from t1;
afterautocommit0_id	afterautocommit0_nom
2	mysql
rollback;
select id as afterrollback_id,nom as afterrollback_nom from t1;
afterrollback_id	afterrollback_nom
set autocommit=1;
drop table t1;
CREATE TABLE t1 (id char(8) not null primary key, val int not null) engine=innodb;
insert into t1 values ('pippo', 12);
insert into t1 values ('pippo', 12);
ERROR 23000: Duplicate entry 'pippo' for key 1
delete from t1;
delete from t1 where id = 'pippo';
select * from t1;
id	val
insert into t1 values ('pippo', 12);
set autocommit=0;
delete from t1;
rollback;
select * from t1;
id	val
pippo	12
delete from t1;
commit;
select * from t1;
id	val
drop table t1;
create table t1 (a integer) engine=innodb;
start transaction;
rename table t1 to t2;
create table t1 (b integer) engine=innodb;
insert into t1 values (1);
rollback;
drop table t1;
rename table t2 to t1;
drop table t1;
set autocommit=1;
CREATE TABLE t1 (ID INTEGER NOT NULL PRIMARY KEY, NAME VARCHAR(64)) ENGINE=innodb;
INSERT INTO t1 VALUES (1, 'Jochen');
select * from t1;
ID	NAME
1	Jochen
drop table t1;
CREATE TABLE t1 ( _userid VARCHAR(60) NOT NULL PRIMARY KEY) ENGINE=innodb;
set autocommit=0;
INSERT INTO t1  SET _userid='marc@anyware.co.uk';
COMMIT;
SELECT * FROM t1;
_userid
marc@anyware.co.uk
SELECT _userid FROM t1 WHERE _userid='marc@anyware.co.uk';
_userid
marc@anyware.co.uk
drop table t1;
set autocommit=1;
CREATE TABLE t1 (
user_id int(10) DEFAULT '0' NOT NULL,
name varchar(100),
phone varchar(100),
ref_email varchar(100) DEFAULT '' NOT NULL,
detail varchar(200),
PRIMARY KEY (user_id,ref_email)
)engine=innodb;
INSERT INTO t1 VALUES (10292,'sanjeev','29153373','sansh777@hotmail.com','xxx'),(10292,'shirish','2333604','shirish@yahoo.com','ddsds'),(10292,'sonali','323232','sonali@bolly.com','filmstar');
select * from t1 where user_id=10292;
user_id	name	phone	ref_email	detail
10292	sanjeev	29153373	sansh777@hotmail.com	xxx
10292	shirish	2333604	shirish@yahoo.com	ddsds
10292	sonali	323232	sonali@bolly.com	filmstar
INSERT INTO t1 VALUES (10291,'sanjeev','29153373','sansh777@hotmail.com','xxx'),(10293,'shirish','2333604','shirish@yahoo.com','ddsds');
select * from t1 where user_id=10292;
user_id	name	phone	ref_email	detail
10292	sanjeev	29153373	sansh777@hotmail.com	xxx
10292	shirish	2333604	shirish@yahoo.com	ddsds
10292	sonali	323232	sonali@bolly.com	filmstar
select * from t1 where user_id>=10292;
user_id	name	phone	ref_email	detail
10292	sanjeev	29153373	sansh777@hotmail.com	xxx
10292	shirish	2333604	shirish@yahoo.com	ddsds
10292	sonali	323232	sonali@bolly.com	filmstar
10293	shirish	2333604	shirish@yahoo.com	ddsds
select * from t1 where user_id>10292;
user_id	name	phone	ref_email	detail
10293	shirish	2333604	shirish@yahoo.com	ddsds
select * from t1 where user_id<10292;
user_id	name	phone	ref_email	detail
10291	sanjeev	29153373	sansh777@hotmail.com	xxx
drop table t1;
CREATE TABLE t1 (a int not null, b int not null,c int not null,
key(a),primary key(a,b), unique(c),key(a),unique(b));
show index from t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment
t1	0	PRIMARY	1	a	A	#	NULL	NULL		BTREE	
t1	0	PRIMARY	2	b	A	#	NULL	NULL		BTREE	
t1	0	c	1	c	A	#	NULL	NULL		BTREE	
t1	0	b	1	b	A	#	NULL	NULL		BTREE	
t1	1	a	1	a	A	#	NULL	NULL		BTREE	
t1	1	a_2	1	a	A	#	NULL	NULL		BTREE	
drop table t1;
create table t1 (col1 int not null, col2 char(4) not null, primary key(col1));
alter table t1 engine=innodb;
insert into t1 values ('1','1'),('5','2'),('2','3'),('3','4'),('4','4');
select * from t1;
col1	col2
1	1
2	3
3	4
4	4
5	2
update t1 set col2='7' where col1='4';
select * from t1;
col1	col2
1	1
2	3
3	4
4	7
5	2
alter table t1 add co3 int not null;
select * from t1;
col1	col2	co3
1	1	0
2	3	0
3	4	0
4	7	0
5	2	0
update t1 set col2='9' where col1='2';
select * from t1;
col1	col2	co3
1	1	0
2	9	0
3	4	0
4	7	0
5	2	0
drop table t1;
create table t1 (a int not null , b int, primary key (a)) engine = innodb;
create table t2 (a int not null , b int, primary key (a)) engine = myisam;
insert into t1 VALUES (1,3) , (2,3), (3,3);
select * from t1;
a	b
1	3
2	3
3	3
insert into t2 select * from t1;
select * from t2;
a	b
1	3
2	3
3	3
delete from t1 where b = 3;
select * from t1;
a	b
insert into t1 select * from t2;
select * from t1;
a	b
1	3
2	3
3	3
select * from t2;
a	b
1	3
2	3
3	3
drop table t1,t2;
CREATE TABLE t1 (
id int(11) NOT NULL auto_increment,
ggid varchar(32) binary DEFAULT '' NOT NULL,
email varchar(64) DEFAULT '' NOT NULL,
passwd varchar(32) binary DEFAULT '' NOT NULL,
PRIMARY KEY (id),
UNIQUE ggid (ggid)
) ENGINE=innodb;
insert into t1 (ggid,passwd) values ('test1','xxx');
insert into t1 (ggid,passwd) values ('test2','yyy');
insert into t1 (ggid,passwd) values ('test2','this will fail');
ERROR 23000: Duplicate entry 'test2' for key 2
insert into t1 (ggid,id) values ('this will fail',1);
ERROR 23000: Duplicate entry '1' for key 1
select * from t1 where ggid='test1';
id	ggid	email	passwd
1	test1		xxx
select * from t1 where passwd='xxx';
id	ggid	email	passwd
1	test1		xxx
select * from t1 where id=2;
id	ggid	email	passwd
2	test2		yyy
replace into t1 (ggid,id) values ('this will work',1);
replace into t1 (ggid,passwd) values ('test2','this will work');
update t1 set id=100,ggid='test2' where id=1;
ERROR 23000: Duplicate entry 'test2' for key 2
select * from t1;
id	ggid	email	passwd
1	this will work		
3	test2		this will work
select * from t1 where id=1;
id	ggid	email	passwd
1	this will work		
select * from t1 where id=999;
id	ggid	email	passwd
drop table t1;
CREATE TABLE t1 (
user_name varchar(12),
password text,
subscribed char(1),
user_id int(11) DEFAULT '0' NOT NULL,
quota bigint(20),
weight double,
access_date date,
access_time time,
approved datetime,
dummy_primary_key int(11) NOT NULL auto_increment,
PRIMARY KEY (dummy_primary_key)
) ENGINE=innodb;
INSERT INTO t1 VALUES ('user_0','somepassword','N',0,0,0,'2000-09-07','23:06:59','2000-09-07 23:06:59',1);
INSERT INTO t1 VALUES ('user_1','somepassword','Y',1,1,1,'2000-09-07','23:06:59','2000-09-07 23:06:59',2);
INSERT INTO t1 VALUES ('user_2','somepassword','N',2,2,1.4142135623731,'2000-09-07','23:06:59','2000-09-07 23:06:59',3);
INSERT INTO t1 VALUES ('user_3','somepassword','Y',3,3,1.7320508075689,'2000-09-07','23:06:59','2000-09-07 23:06:59',4);
INSERT INTO t1 VALUES ('user_4','somepassword','N',4,4,2,'2000-09-07','23:06:59','2000-09-07 23:06:59',5);
select  user_name, password , subscribed, user_id, quota, weight, access_date, access_time, approved, dummy_primary_key from t1 order by user_name;
user_name	password	subscribed	user_id	quota	weight	access_date	access_time	approved	dummy_primary_key
user_0	somepassword	N	0	0	0	2000-09-07	23:06:59	2000-09-07 23:06:59	1
user_1	somepassword	Y	1	1	1	2000-09-07	23:06:59	2000-09-07 23:06:59	2
user_2	somepassword	N	2	2	1.4142135623731	2000-09-07	23:06:59	2000-09-07 23:06:59	3
user_3	somepassword	Y	3	3	1.7320508075689	2000-09-07	23:06:59	2000-09-07 23:06:59	4
user_4	somepassword	N	4	4	2	2000-09-07	23:06:59	2000-09-07 23:06:59	5
drop table t1;
CREATE TABLE t1 (
id int(11) NOT NULL auto_increment,
parent_id int(11) DEFAULT '0' NOT NULL,
level tinyint(4) DEFAULT '0' NOT NULL,
KEY (id),
KEY parent_id (parent_id),
KEY level (level)
) engine=innodb;
INSERT INTO t1 VALUES (1,0,0),(3,1,1),(4,1,1),(8,2,2),(9,2,2),(17,3,2),(22,4,2),(24,4,2),(28,5,2),(29,5,2),(30,5,2),(31,6,2),(32,6,2),(33,6,2),(203,7,2),(202,7,2),(20,3,2),(157,0,0),(193,5,2),(40,7,2),(2,1,1),(15,2,2),(6,1,1),(34,6,2),(35,6,2),(16,3,2),(7,1,1),(36,7,2),(18,3,2),(26,5,2),(27,5,2),(183,4,2),(38,7,2),(25,5,2),(37,7,2),(21,4,2),(19,3,2),(5,1,1);
INSERT INTO t1 values (179,5,2);
update t1 set parent_id=parent_id+100;
select * from t1 where parent_id=102;
id	parent_id	level
8	102	2
9	102	2
15	102	2
update t1 set id=id+1000;
update t1 set id=1024 where id=1009;
select * from t1;
id	parent_id	level
1001	100	0
1003	101	1
1004	101	1
1008	102	2
1024	102	2
1017	103	2
1022	104	2
1024	104	2
1028	105	2
1029	105	2
1030	105	2
1031	106	2
1032	106	2
1033	106	2
1203	107	2
1202	107	2
1020	103	2
1157	100	0
1193	105	2
1040	107	2
1002	101	1
1015	102	2
1006	101	1
1034	106	2
1035	106	2
1016	103	2
1007	101	1
1036	107	2
1018	103	2
1026	105	2
1027	105	2
1183	104	2
1038	107	2
1025	105	2
1037	107	2
1021	104	2
1019	103	2
1005	101	1
1179	105	2
update ignore t1 set id=id+1;
select * from t1;
id	parent_id	level
1002	100	0
1004	101	1
1005	101	1
1009	102	2
1025	102	2
1018	103	2
1023	104	2
1025	104	2
1029	105	2
1030	105	2
1031	105	2
1032	106	2
1033	106	2
1034	106	2
1204	107	2
1203	107	2
1021	103	2
1158	100	0
1194	105	2
1041	107	2
1003	101	1
1016	102	2
1007	101	1
1035	106	2
1036	106	2
1017	103	2
1008	101	1
1037	107	2
1019	103	2
1027	105	2
1028	105	2
1184	104	2
1039	107	2
1026	105	2
1038	107	2
1022	104	2
1020	103	2
1006	101	1
1180	105	2
update ignore t1 set id=1023 where id=1010;
select * from t1 where parent_id=102;
id	parent_id	level
1009	102	2
1025	102	2
1016	102	2
explain select level from t1 where level=1;
id	select_type	table	type	possible_keys	key	key_len	ref	rows	Extra
1	SIMPLE	t1	ref	level	level	1	const	#	Using where; Using index
select level,id from t1 where level=1;
level	id
1	1004
1	1005
1	1003
1	1007
1	1008
1	1006
select level,id,parent_id from t1 where level=1;
level	id	parent_id
1	1004	101
1	1005	101
1	1003	101
1	1007	101
1	1008	101
1	1006	101
select level,id from t1 where level=1 order by id;
level	id
1	1003
1	1004
1	1005
1	1006
1	1007
1	1008
delete from t1 where level=1;
select * from t1;
id	parent_id	level
1002	100	0
1009	102	2
1025	102	2
1018	103	2
1023	104	2
1025	104	2
1029	105	2
1030	105	2
1031	105	2
1032	106	2
1033	106	2
1034	106	2
1204	107	2
1203	107	2
1021	103	2
1158	100	0
1194	105	2
1041	107	2
1016	102	2
1035	106	2
1036	106	2
1017	103	2
1037	107	2
1019	103	2
1027	105	2
1028	105	2
1184	104	2
1039	107	2
1026	105	2
1038	107	2
1022	104	2
1020	103	2
1180	105	2
drop table t1;
CREATE TABLE t1 (
sca_code char(6) NOT NULL,
cat_code char(6) NOT NULL,
sca_desc varchar(50),
lan_code char(2) NOT NULL,
sca_pic varchar(100),
sca_sdesc varchar(50),
sca_sch_desc varchar(16),
PRIMARY KEY (sca_code, cat_code, lan_code),
INDEX sca_pic (sca_pic)
) engine = innodb ;
INSERT INTO t1 ( sca_code, cat_code, sca_desc, lan_code, sca_pic, sca_sdesc, sca_sch_desc) VALUES ( 'PD', 'J', 'PENDANT', 'EN', NULL, NULL, 'PENDANT'),( 'RI', 'J', 'RING', 'EN', NULL, NULL, 'RING'),( 'QQ', 'N', 'RING', 'EN', 'not null', NULL, 'RING');
select count(*) from t1 where sca_code = 'PD';
count(*)
1
select count(*) from t1 where sca_code <= 'PD';
count(*)
1
select count(*) from t1 where sca_pic is null;
count(*)
2
alter table t1 drop index sca_pic, add index sca_pic (cat_code, sca_pic);
select count(*) from t1 where sca_code='PD' and sca_pic is null;
count(*)
1
select count(*) from t1 where cat_code='E';
count(*)
0
alter table t1 drop index sca_pic, add index (sca_pic, cat_code);
select count(*) from t1 where sca_code='PD' and sca_pic is null;
count(*)
1
select count(*) from t1 where sca_pic >= 'n';
count(*)
1
select sca_pic from t1 where sca_pic is null;
sca_pic
NULL
NULL
update t1 set sca_pic="test" where sca_pic is null;
delete from t1 where sca_code='pd';
drop table t1;
set @a:=now();
CREATE TABLE t1 (a int not null, b timestamp not null, primary key (a)) engine=innodb;
insert into t1 (a) values(1),(2),(3);
select t1.a from t1 natural join t1 as t2 where t1.b >= @a order by t1.a;
a
1
2
3
update t1 set a=5 where a=1;
select a from t1;
a
2
3
5
drop table t1;
create table t1 (a varchar(100) not null, primary key(a), b int not null) engine=innodb;
insert into t1 values("hello",1),("world",2);
select * from t1 order by b desc;
a	b
world	2
hello	1
optimize table t1;
Table	Op	Msg_type	Msg_text
test.t1	optimize	status	OK
show keys from t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment
t1	0	PRIMARY	1	a	A	#	NULL	NULL		BTREE	
drop table t1;
create table t1 (i int, j int ) ENGINE=innodb;
insert into t1 values (1,2);
select * from t1 where i=1 and j=2;
i	j
1	2
create index ax1 on t1 (i,j);
select * from t1 where i=1 and j=2;
i	j
1	2
drop table t1;
CREATE TABLE t1 (
a int3 unsigned NOT NULL,
b int1 unsigned NOT NULL,
UNIQUE (a, b)
) ENGINE = innodb;
INSERT INTO t1 VALUES (1, 1);
SELECT MIN(B),MAX(b) FROM t1 WHERE t1.a = 1;
MIN(B)	MAX(b)
1	1
drop table t1;
CREATE TABLE t1 (a int unsigned NOT NULL) engine=innodb;
INSERT INTO t1 VALUES (1);
SELECT * FROM t1;
a
1
DROP TABLE t1;
create table t1 (a int  primary key,b int, c int, d int, e int, f int, g int, h int, i int, j int, k int, l int, m int, n int, o int, p int, q int, r int, s int, t int, u int, v int, w int, x int, y int, z int, a1 int, a2 int, a3 int, a4 int, a5 int, a6 int, a7 int, a8 int, a9 int, b1 int, b2 int, b3 int, b4 int, b5 int, b6 int) engine = innodb;
insert into t1 values (1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1);
explain select * from t1 where a > 0 and a < 50;
id	select_type	table	type	possible_keys	key	key_len	ref	rows	Extra
1	SIMPLE	t1	range	PRIMARY	PRIMARY	4	NULL	#	Using where
drop table t1;
create table t1 (id int NOT NULL,id2 int NOT NULL,id3 int NOT NULL,dummy1 char(30),primary key (id,id2),index index_id3 (id3)) engine=innodb;
insert into t1 values (0,0,0,'ABCDEFGHIJ'),(2,2,2,'BCDEFGHIJK'),(1,1,1,'CDEFGHIJKL');
LOCK TABLES t1 WRITE;
insert into t1 values (99,1,2,'D'),(1,1,2,'D');
ERROR 23000: Duplicate entry '1-1' for key 1
select id from t1;
id
0
1
2
select id from t1;
id
0
1
2
UNLOCK TABLES;
DROP TABLE t1;
create table t1 (id int NOT NULL,id2 int NOT NULL,id3 int NOT NULL,dummy1 char(30),primary key (id,id2),index index_id3 (id3)) engine=innodb;
insert into t1 values (0,0,0,'ABCDEFGHIJ'),(2,2,2,'BCDEFGHIJK'),(1,1,1,'CDEFGHIJKL');
LOCK TABLES t1 WRITE;
begin;
insert into t1 values (99,1,2,'D'),(1,1,2,'D');
ERROR 23000: Duplicate entry '1-1' for key 1
select id from t1;
id
0
1
2
insert ignore into t1 values (100,1,2,'D'),(1,1,99,'D');
commit;
select id,id3 from t1;
id	id3
0	0
1	1
2	2
100	2
UNLOCK TABLES;
DROP TABLE t1;
create table t1 (a char(20), unique (a(5))) engine=innodb;
drop table t1;
create table t1 (a char(20), index (a(5))) engine=innodb;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` char(20) default NULL,
  KEY `a` (`a`(5))
) ENGINE=InnoDB DEFAULT CHARSET=latin1
drop table t1;
create temporary table t1 (a int not null auto_increment, primary key(a)) engine=innodb;
insert into t1 values (NULL),(NULL),(NULL);
delete from t1 where a=3;
insert into t1 values (NULL);
select * from t1;
a
1
2
4
alter table t1 add b int;
select * from t1;
a	b
1	NULL
2	NULL
4	NULL
drop table t1;
create table t1
(
id int auto_increment primary key,
name varchar(32) not null,
value text not null,
uid int not null,
unique key(name,uid)
) engine=innodb;
insert into t1 values (1,'one','one value',101),
(2,'two','two value',102),(3,'three','three value',103);
set insert_id=5;
replace into t1 (value,name,uid) values ('other value','two',102);
delete from t1 where uid=102;
set insert_id=5;
replace into t1 (value,name,uid) values ('other value','two',102);
set insert_id=6;
replace into t1 (value,name,uid) values ('other value','two',102);
select * from t1;
id	name	value	uid
1	one	one value	101
3	three	three value	103
6	two	other value	102
drop table t1;
create database mysqltest;
create table mysqltest.t1 (a int not null) engine= innodb;
insert into mysqltest.t1 values(1);
create table mysqltest.t2 (a int not null) engine= myisam;
insert into mysqltest.t2 values(1);
create table mysqltest.t3 (a int not null) engine= heap;
insert into mysqltest.t3 values(1);
commit;
drop database mysqltest;
show tables from mysqltest;
Got one of the listed errors
set autocommit=0;
create table t1 (a int not null) engine= innodb;
insert into t1 values(1),(2);
truncate table t1;
ERROR HY000: Can't execute the given command because you have active locked tables or an active transaction
commit;
truncate table t1;
select * from t1;
a
insert into t1 values(1),(2);
delete from t1;
select * from t1;
a
commit;
drop table t1;
set autocommit=1;
create table t1 (a int not null) engine= innodb;
insert into t1 values(1),(2);
truncate table t1;
insert into t1 values(1),(2);
select * from t1;
a
1
2
truncate table t1;
insert into t1 values(1),(2);
delete from t1;
select * from t1;
a
drop table t1;
create table t1 (a int not null, b int not null, c int not null, primary key (a),key(b)) engine=innodb;
insert into t1 values (3,3,3),(1,1,1),(2,2,2),(4,4,4);
explain select * from t1 order by a;
id	select_type	table	type	possible_keys	key	key_len	ref	rows	Extra
1	SIMPLE	t1	index	NULL	PRIMARY	4	NULL	#	
explain select * from t1 order by b;
id	select_type	table	type	possible_keys	key	key_len	ref	rows	Extra
1	SIMPLE	t1	index	NULL	b	4	NULL	#	
explain select * from t1 order by c;
id	select_type	table	type	possible_keys	key	key_len	ref	rows	Extra
1	SIMPLE	t1	ALL	NULL	NULL	NULL	NULL	#	Using filesort
explain select a from t1 order by a;
id	select_type	table	type	possible_keys	key	key_len	ref	rows	Extra
1	SIMPLE	t1	index	NULL	PRIMARY	4	NULL	#	Using index
explain select b from t1 order by b;
id	select_type	table	type	possible_keys	key	key_len	ref	rows	Extra
1	SIMPLE	t1	index	NULL	b	4	NULL	#	Using index
explain select a,b from t1 order by b;
id	select_type	table	type	possible_keys	key	key_len	ref	rows	Extra
1	SIMPLE	t1	index	NULL	b	4	NULL	#	Using index
explain select a,b from t1;
id	select_type	table	type	possible_keys	key	key_len	ref	rows	Extra
1	SIMPLE	t1	index	NULL	b	4	NULL	#	Using index
explain select a,b,c from t1;
id	select_type	table	type	possible_keys	key	key_len	ref	rows	Extra
1	SIMPLE	t1	ALL	NULL	NULL	NULL	NULL	#	
drop table t1;
create table t1 (t int not null default 1, key (t)) engine=innodb;
desc t1;
Field	Type	Null	Key	Default	Extra
t	int(11)		MUL	1	
drop table t1;
CREATE TABLE t1 (
number bigint(20) NOT NULL default '0',
cname char(15) NOT NULL default '',
carrier_id smallint(6) NOT NULL default '0',
privacy tinyint(4) NOT NULL default '0',
last_mod_date timestamp(14) NOT NULL,
last_mod_id smallint(6) NOT NULL default '0',
last_app_date timestamp(14) NOT NULL,
last_app_id smallint(6) default '-1',
version smallint(6) NOT NULL default '0',
assigned_scps int(11) default '0',
status tinyint(4) default '0'
) ENGINE=InnoDB;
INSERT INTO t1 VALUES (4077711111,'SeanWheeler',90,2,20020111112846,500,00000000000000,-1,2,3,1);
INSERT INTO t1 VALUES (9197722223,'berry',90,3,20020111112809,500,20020102114532,501,4,10,0);
INSERT INTO t1 VALUES (650,'San Francisco',0,0,20011227111336,342,00000000000000,-1,1,24,1);
INSERT INTO t1 VALUES (302467,'Sue\'s Subshop',90,3,20020109113241,500,20020102115111,501,7,24,0);
INSERT INTO t1 VALUES (6014911113,'SudzCarwash',520,1,20020102115234,500,20020102115259,501,33,32768,0);
INSERT INTO t1 VALUES (333,'tubs',99,2,20020109113440,501,20020109113440,500,3,10,0);
CREATE TABLE t2 (
number bigint(20) NOT NULL default '0',
cname char(15) NOT NULL default '',
carrier_id smallint(6) NOT NULL default '0',
privacy tinyint(4) NOT NULL default '0',
last_mod_date timestamp(14) NOT NULL,
last_mod_id smallint(6) NOT NULL default '0',
last_app_date timestamp(14) NOT NULL,
last_app_id smallint(6) default '-1',
version smallint(6) NOT NULL default '0',
assigned_scps int(11) default '0',
status tinyint(4) default '0'
) ENGINE=InnoDB;
INSERT INTO t2 VALUES (4077711111,'SeanWheeler',0,2,20020111112853,500,00000000000000,-1,2,3,1);
INSERT INTO t2 VALUES (9197722223,'berry',90,3,20020111112818,500,20020102114532,501,4,10,0);
INSERT INTO t2 VALUES (650,'San Francisco',90,0,20020109113158,342,00000000000000,-1,1,24,1);
INSERT INTO t2 VALUES (333,'tubs',99,2,20020109113453,501,20020109113453,500,3,10,0);
select * from t1;
number	cname	carrier_id	privacy	last_mod_date	last_mod_id	last_app_date	last_app_id	version	assigned_scps	status
4077711111	SeanWheeler	90	2	2002-01-11 11:28:46	500	0000-00-00 00:00:00	-1	2	3	1
9197722223	berry	90	3	2002-01-11 11:28:09	500	2002-01-02 11:45:32	501	4	10	0
650	San Francisco	0	0	2001-12-27 11:13:36	342	0000-00-00 00:00:00	-1	1	24	1
302467	Sue's Subshop	90	3	2002-01-09 11:32:41	500	2002-01-02 11:51:11	501	7	24	0
6014911113	SudzCarwash	520	1	2002-01-02 11:52:34	500	2002-01-02 11:52:59	501	33	32768	0
333	tubs	99	2	2002-01-09 11:34:40	501	2002-01-09 11:34:40	500	3	10	0
select * from t2;
number	cname	carrier_id	privacy	last_mod_date	last_mod_id	last_app_date	last_app_id	version	assigned_scps	status
4077711111	SeanWheeler	0	2	2002-01-11 11:28:53	500	0000-00-00 00:00:00	-1	2	3	1
9197722223	berry	90	3	2002-01-11 11:28:18	500	2002-01-02 11:45:32	501	4	10	0
650	San Francisco	90	0	2002-01-09 11:31:58	342	0000-00-00 00:00:00	-1	1	24	1
333	tubs	99	2	2002-01-09 11:34:53	501	2002-01-09 11:34:53	500	3	10	0
delete t1, t2 from t1 left join t2 on t1.number=t2.number where (t1.carrier_id=90 and t1.number=t2.number) or (t2.carrier_id=90 and t1.number=t2.number) or  (t1.carrier_id=90 and t2.number is null);
select * from t1;
number	cname	carrier_id	privacy	last_mod_date	last_mod_id	last_app_date	last_app_id	version	assigned_scps	status
6014911113	SudzCarwash	520	1	2002-01-02 11:52:34	500	2002-01-02 11:52:59	501	33	32768	0
333	tubs	99	2	2002-01-09 11:34:40	501	2002-01-09 11:34:40	500	3	10	0
select * from t2;
number	cname	carrier_id	privacy	last_mod_date	last_mod_id	last_app_date	last_app_id	version	assigned_scps	status
333	tubs	99	2	2002-01-09 11:34:53	501	2002-01-09 11:34:53	500	3	10	0
select * from t2;
number	cname	carrier_id	privacy	last_mod_date	last_mod_id	last_app_date	last_app_id	version	assigned_scps	status
333	tubs	99	2	2002-01-09 11:34:53	501	2002-01-09 11:34:53	500	3	10	0
drop table t1,t2;
create table t1 (id int unsigned not null auto_increment, code tinyint unsigned not null, name char(20) not null, primary key (id), key (code), unique (name)) engine=innodb;
BEGIN;
SET SESSION TRANSACTION ISOLATION LEVEL SERIALIZABLE;
SELECT @@tx_isolation,@@global.tx_isolation;
@@tx_isolation	@@global.tx_isolation
SERIALIZABLE	REPEATABLE-READ
insert into t1 (code, name) values (1, 'Tim'), (1, 'Monty'), (2, 'David');
select id, code, name from t1 order by id;
id	code	name
1	1	Tim
2	1	Monty
3	2	David
COMMIT;
BEGIN;
SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ;
insert into t1 (code, name) values (2, 'Erik'), (3, 'Sasha');
select id, code, name from t1 order by id;
id	code	name
1	1	Tim
2	1	Monty
3	2	David
4	2	Erik
5	3	Sasha
COMMIT;
BEGIN;
SET SESSION TRANSACTION ISOLATION LEVEL READ UNCOMMITTED;
insert into t1 (code, name) values (3, 'Jeremy'), (4, 'Matt');
select id, code, name from t1 order by id;
id	code	name
1	1	Tim
2	1	Monty
3	2	David
4	2	Erik
5	3	Sasha
6	3	Jeremy
7	4	Matt
COMMIT;
DROP TABLE t1;
create table t1 (n int(10), d int(10)) engine=innodb;
create table t2 (n int(10), d int(10)) engine=innodb;
insert into t1 values(1,1),(1,2);
insert into t2 values(1,10),(2,20);
UPDATE t1,t2 SET t1.d=t2.d,t2.d=30 WHERE t1.n=t2.n;
select * from t1;
n	d
1	10
1	10
select * from t2;
n	d
1	30
2	20
drop table t1,t2;
create table t1 (a int, b int) engine=innodb;
insert into t1 values(20,null);
select t2.b, ifnull(t2.b,"this is null") from t1 as t2 left join t1 as t3 on
t2.b=t3.a;
b	ifnull(t2.b,"this is null")
NULL	this is null
select t2.b, ifnull(t2.b,"this is null") from t1 as t2 left join t1 as t3 on
t2.b=t3.a order by 1;
b	ifnull(t2.b,"this is null")
NULL	this is null
insert into t1 values(10,null);
select t2.b, ifnull(t2.b,"this is null") from t1 as t2 left join t1 as t3 on
t2.b=t3.a order by 1;
b	ifnull(t2.b,"this is null")
NULL	this is null
NULL	this is null
drop table t1;
create table t1 (a varchar(10) not null) engine=myisam;
create table t2 (b varchar(10) not null unique) engine=innodb;
select t1.a from t1,t2 where t1.a=t2.b;
a
drop table t1,t2;
create table t1 (a int not null, b int, primary key (a)) engine = innodb;
create table t2 (a int not null, b int, primary key (a)) engine = innodb;
insert into t1 values (10, 20);
insert into t2 values (10, 20);
update t1, t2 set t1.b = 150, t2.b = t1.b where t2.a = t1.a and t1.a = 10;
drop table t1,t2;
CREATE TABLE t1 (id INT NOT NULL, PRIMARY KEY (id)) ENGINE=INNODB;
CREATE TABLE t2 (id INT PRIMARY KEY, t1_id INT, INDEX par_ind (t1_id), FOREIGN KEY (t1_id) REFERENCES t1(id)  ON DELETE CASCADE ) ENGINE=INNODB;
insert into t1 set id=1;
insert into t2 set id=1, t1_id=1;
delete t1,t2 from t1,t2 where t1.id=t2.t1_id;
select * from t1;
id
select * from t2;
id	t1_id
drop table t2,t1;
CREATE TABLE t1(id INT NOT NULL,  PRIMARY KEY (id)) ENGINE=INNODB;
CREATE TABLE t2(id  INT PRIMARY KEY, t1_id INT, INDEX par_ind (t1_id)  ) ENGINE=INNODB;
INSERT INTO t1 VALUES(1);
INSERT INTO t2 VALUES(1, 1);
SELECT * from t1;
id
1
UPDATE t1,t2 SET t1.id=t1.id+1, t2.t1_id=t1.id+1;
SELECT * from t1;
id
2
UPDATE t1,t2 SET t1.id=t1.id+1 where t1.id!=t2.id;
SELECT * from t1;
id
3
DROP TABLE t1,t2;
set autocommit=0;
CREATE TABLE t1 (id CHAR(15) NOT NULL, value CHAR(40) NOT NULL, PRIMARY KEY(id)) ENGINE=InnoDB;
CREATE TABLE t2 (id CHAR(15) NOT NULL, value CHAR(40) NOT NULL, PRIMARY KEY(id)) ENGINE=InnoDB;
CREATE TABLE t3 (id1 CHAR(15) NOT NULL, id2 CHAR(15) NOT NULL, PRIMARY KEY(id1, id2)) ENGINE=InnoDB;
INSERT INTO t3 VALUES("my-test-1", "my-test-2");
COMMIT;
INSERT INTO t1 VALUES("this-key", "will disappear");
INSERT INTO t2 VALUES("this-key", "will also disappear");
DELETE FROM t3 WHERE id1="my-test-1";
SELECT * FROM t1;
id	value
this-key	will disappear
SELECT * FROM t2;
id	value
this-key	will also disappear
SELECT * FROM t3;
id1	id2
ROLLBACK;
SELECT * FROM t1;
id	value
SELECT * FROM t2;
id	value
SELECT * FROM t3;
id1	id2
my-test-1	my-test-2
SELECT * FROM t3 WHERE id1="my-test-1" LOCK IN SHARE MODE;
id1	id2
my-test-1	my-test-2
COMMIT;
set autocommit=1;
DROP TABLE t1,t2,t3;
CREATE TABLE t1 (a int not null primary key, b int not null, unique (b)) engine=innodb;
INSERT INTO t1 values (1,1),(2,2),(3,3),(4,4),(5,5),(6,6),(7,7),(8,8),(9,9);
UPDATE t1 set a=a+100 where b between 2 and 3 and a < 1000;
SELECT * from t1;
a	b
1	1
102	2
103	3
4	4
5	5
6	6
7	7
8	8
9	9
drop table t1;
CREATE TABLE t1 (a int not null primary key, b int not null, key (b)) engine=innodb;
CREATE TABLE t2 (a int not null primary key, b int not null, key (b)) engine=innodb;
INSERT INTO t1 values (1,1),(2,2),(3,3),(4,4),(5,5),(6,6),(7,7),(8,8),(9,9),(10,10),(11,11),(12,12);
INSERT INTO t2 values (1,1),(2,2),(3,3),(4,4),(5,5),(6,6),(7,7),(8,8),(9,9);
update t1,t2 set t1.a=t1.a+100;
select * from t1;
a	b
101	1
102	2
103	3
104	4
105	5
106	6
107	7
108	8
109	9
110	10
111	11
112	12
update t1,t2 set t1.a=t1.a+100 where t1.a=101;
select * from t1;
a	b
201	1
102	2
103	3
104	4
105	5
106	6
107	7
108	8
109	9
110	10
111	11
112	12
update t1,t2 set t1.b=t1.b+10 where t1.b=2;
select * from t1;
a	b
201	1
103	3
104	4
105	5
106	6
107	7
108	8
109	9
110	10
111	11
102	12
112	12
update t1,t2 set t1.b=t1.b+2,t2.b=t1.b+10 where t1.b between 3 and 5 and t1.a=t2.a+100;
select * from t1;
a	b
201	1
103	5
104	6
106	6
105	7
107	7
108	8
109	9
110	10
111	11
102	12
112	12
select * from t2;
a	b
1	1
2	2
6	6
7	7
8	8
9	9
3	13
4	14
5	15
drop table t1,t2;
CREATE TABLE t2 (   NEXT_T         BIGINT NOT NULL PRIMARY KEY) ENGINE=MyISAM;
CREATE TABLE t1 (  B_ID           INTEGER NOT NULL PRIMARY KEY) ENGINE=InnoDB;
SET AUTOCOMMIT=0;
INSERT INTO t1 ( B_ID ) VALUES ( 1 );
INSERT INTO t2 ( NEXT_T ) VALUES ( 1 );
ROLLBACK;
Warnings:
Warning	1196	Some non-transactional changed tables couldn't be rolled back
SELECT * FROM t1;
B_ID
drop table  t1,t2;
create table t1  ( pk         int primary key,    parent     int not null,    child      int not null,       index (parent)  ) engine = innodb;
insert into t1 values   (1,0,4),  (2,1,3),  (3,2,1),  (4,1,2);
select distinct  parent,child   from t1   order by parent;
parent	child
0	4
1	2
1	3
2	1
drop table t1;
create table t1 (a int not null auto_increment primary key, b int, c int, key(c)) engine=innodb;
create table t2 (a int not null auto_increment primary key, b int);
insert into t1 (b) values (null),(null),(null),(null),(null),(null),(null);
insert into t2 (a) select b from t1;
insert into t1 (b) select b from t2;
insert into t2 (a) select b from t1;
insert into t1 (a) select b from t2;
insert into t2 (a) select b from t1;
insert into t1 (a) select b from t2;
insert into t2 (a) select b from t1;
insert into t1 (a) select b from t2;
insert into t2 (a) select b from t1;
insert into t1 (a) select b from t2;
insert into t2 (a) select b from t1;
insert into t1 (a) select b from t2;
insert into t2 (a) select b from t1;
insert into t1 (a) select b from t2;
insert into t2 (a) select b from t1;
insert into t1 (a) select b from t2;
insert into t2 (a) select b from t1;
insert into t1 (a) select b from t2;
select count(*) from t1;
count(*)
29267
explain select * from t1 where c between 1 and 10000;
id	select_type	table	type	possible_keys	key	key_len	ref	rows	Extra
1	SIMPLE	t1	range	c	c	5	NULL	#	Using where
update t1 set c=a;
explain select * from t1 where c between 1 and 10000;
id	select_type	table	type	possible_keys	key	key_len	ref	rows	Extra
1	SIMPLE	t1	ALL	c	NULL	NULL	NULL	#	Using where
drop table t1,t2;
create table t1 (id int primary key auto_increment, fk int, index index_fk (fk)) engine=innodb;
insert into t1 (id) values (null),(null),(null),(null),(null);
update t1 set fk=69 where fk is null order by id limit 1;
SELECT * from t1;
id	fk
2	NULL
3	NULL
4	NULL
5	NULL
1	69
drop table t1;
create table t1 (a int not null, b int not null, key (a));
insert into t1 values (1,1),(1,2),(1,3),(3,1),(3,2),(3,3),(3,1),(3,2),(3,3),(2,1),(2,2),(2,3);
SET @tmp=0;
update t1 set b=(@tmp:=@tmp+1) order by a;
update t1 set b=99 where a=1 order by b asc limit 1;
update t1 set b=100 where a=1 order by b desc limit 2;
update t1 set a=a+10+b where a=1 order by b;
select * from t1 order by a,b;
a	b
2	4
2	5
2	6
3	7
3	8
3	9
3	10
3	11
3	12
13	2
111	100
111	100
drop table t1;
create table t1 ( c char(8) not null ) engine=innodb;
insert into t1 values ('0'),('1'),('2'),('3'),('4'),('5'),('6'),('7'),('8'),('9');
insert into t1 values ('A'),('B'),('C'),('D'),('E'),('F');
alter table t1 add b char(8) not null;
alter table t1 add a char(8) not null;
alter table t1 add primary key (a,b,c);
update t1 set a=c, b=c;
create table t2 (c char(8) not null, b char(8) not null, a char(8) not null, primary key(a,b,c)) engine=innodb;
insert into t2 select * from t1;
delete t1,t2 from t2,t1 where t1.a<'B' and t2.b=t1.b;
drop table t1,t2;
SET AUTOCOMMIT=1;
create table t1 (a integer auto_increment primary key) engine=innodb;
insert into t1 (a) values (NULL),(NULL);
truncate table t1;
insert into t1 (a) values (NULL),(NULL);
SELECT * from t1;
a
3
4
drop table t1;
CREATE TABLE t1 (`id 1` INT NOT NULL, PRIMARY KEY (`id 1`)) ENGINE=INNODB;
CREATE TABLE t2 (id INT PRIMARY KEY, t1_id INT, INDEX par_ind (t1_id), FOREIGN KEY (`t1_id`) REFERENCES `t1`(`id 1`)  ON DELETE CASCADE ) ENGINE=INNODB;
drop table t2,t1;
create table `t1` (`id` int( 11 ) not null  ,primary key ( `id` )) engine = innodb;
insert into `t1`values ( 1 ) ;
create table `t2` (`id` int( 11 ) not null default '0',unique key `id` ( `id` ) ,constraint `t1_id_fk` foreign key ( `id` ) references `t1` (`id` )) engine = innodb;
insert into `t2`values ( 1 ) ;
create table `t3` (`id` int( 11 ) not null default '0',key `id` ( `id` ) ,constraint `t2_id_fk` foreign key ( `id` ) references `t2` (`id` )) engine = innodb;
insert into `t3`values ( 1 ) ;
delete t3,t2,t1 from t1,t2,t3 where t1.id =1 and t2.id = t1.id and t3.id = t2.id;
ERROR 23000: Cannot delete or update a parent row: a foreign key constraint fails
update t1,t2,t3 set t3.id=5, t2.id=6, t1.id=7  where t1.id =1 and t2.id = t1.id and t3.id = t2.id;
ERROR 23000: Cannot delete or update a parent row: a foreign key constraint fails
update t3 set  t3.id=7  where t1.id =1 and t2.id = t1.id and t3.id = t2.id;
ERROR 42S02: Unknown table 't1' in where clause
drop table t3,t2,t1;
create table t1(
id int primary key,
pid int,
index(pid),
foreign key(pid) references t1(id) on delete cascade) engine=innodb;
insert into t1 values(0,0),(1,0),(2,1),(3,2),(4,3),(5,4),(6,5),(7,6),
(8,7),(9,8),(10,9),(11,10),(12,11),(13,12),(14,13),(15,14);
delete from t1 where id=0;
ERROR 23000: Cannot delete or update a parent row: a foreign key constraint fails
delete from t1 where id=15;
delete from t1 where id=0;
drop table t1;
CREATE TABLE t1 (col1 int(1))ENGINE=InnoDB;
CREATE TABLE t2 (col1 int(1),stamp TIMESTAMP,INDEX stamp_idx
(stamp))ENGINE=InnoDB;
insert into t1 values (1),(2),(3);
insert into t2 values (1, 20020204130000),(2, 20020204130000),(4,20020204310000 ),(5,20020204230000);
Warnings:
Warning	1265	Data truncated for column 'stamp' at row 3
SELECT col1 FROM t1 UNION SELECT col1 FROM t2 WHERE stamp <
'20020204120000' GROUP BY col1;
col1
1
2
3
4
drop table t1,t2;
CREATE TABLE t1 (
`id` int(10) unsigned NOT NULL auto_increment,
`id_object` int(10) unsigned default '0',
`id_version` int(10) unsigned NOT NULL default '1',
label varchar(100) NOT NULL default '',
`description` text,
PRIMARY KEY  (`id`),
KEY `id_object` (`id_object`),
KEY `id_version` (`id_version`)
) ENGINE=InnoDB;
INSERT INTO t1 VALUES("6", "3382", "9", "Test", NULL), ("7", "102", "5", "Le Pekin (Test)", NULL),("584", "1794", "4", "Test de resto", NULL),("837", "1822", "6", "Test 3", NULL),("1119", "3524", "1", "Societe Test", NULL),("1122", "3525", "1", "Fournisseur Test", NULL);
CREATE TABLE t2 (
`id` int(10) unsigned NOT NULL auto_increment,
`id_version` int(10) unsigned NOT NULL default '1',
PRIMARY KEY  (`id`),
KEY `id_version` (`id_version`)
) ENGINE=InnoDB;
INSERT INTO t2 VALUES("3524", "1"),("3525", "1"),("1794", "4"),("102", "5"),("1822", "6"),("3382", "9");
SELECT t2.id, t1.label FROM t2 INNER JOIN
(SELECT t1.id_object as id_object FROM t1 WHERE t1.label LIKE '%test%') AS lbl 
ON (t2.id = lbl.id_object) INNER JOIN t1 ON (t2.id = t1.id_object);
id	label
3382	Test
102	Le Pekin (Test)
1794	Test de resto
1822	Test 3
3524	Societe Test
3525	Fournisseur Test
drop table t1,t2;
create table t1 (c1 char(5) unique not null, c2 int, stamp timestamp) engine=innodb;
select * from t1;
c1	c2	stamp
replace delayed into t1 (c1, c2)  values ( "text1","11"),( "text2","12");
select * from t1;
c1	c2	stamp
text1	11	2004-12-01 13:23:14
text2	12	2004-12-01 13:23:14
replace delayed into t1 (c1, c2)  values ( "text1","12"),( "text2","13"),( "text3","14", "a" ),( "text4","15", "b" );
ERROR 21S01: Column count doesn't match value count at row 3
select * from t1;
c1	c2	stamp
text1	11	2004-12-01 13:23:14
text2	12	2004-12-01 13:23:14
drop table t1;
create table t1 (a int, b varchar(200), c text not null) checksum=1 engine=myisam;
create table t2 (a int, b varchar(200), c text not null) checksum=0 engine=innodb;
create table t3 (a int, b varchar(200), c text not null) checksum=1 engine=innodb;
insert t1 values (1, "aaa", "bbb"), (NULL, "", "ccccc"), (0, NULL, "");
insert t2 select * from t1;
insert t3 select * from t1;
checksum table t1, t2, t3, t4 quick;
Table	Checksum
test.t1	968604391
test.t2	NULL
test.t3	NULL
test.t4	NULL
checksum table t1, t2, t3, t4;
Table	Checksum
test.t1	968604391
test.t2	968604391
test.t3	968604391
test.t4	NULL
checksum table t1, t2, t3, t4 extended;
Table	Checksum
test.t1	968604391
test.t2	968604391
test.t3	968604391
test.t4	NULL
drop table t1,t2,t3;
create table t1 (id int,  name char(10) not null,  name2 char(10) not null) engine=innodb;
insert into t1 values(1,'first','fff'),(2,'second','sss'),(3,'third','ttt');
select name2 from t1  union all  select name from t1 union all select id from t1;
name2
fff
sss
ttt
first
second
third
1
2
3
drop table t1;
create table t1 (a int) engine=innodb;
create table t2 like t1;
drop table t1,t2;
create table t1 (id int(11) not null, id2 int(11) not null, unique (id,id2)) engine=innodb;
create table t2 (id int(11) not null, constraint t1_id_fk foreign key ( id ) references t1 (id)) engine = innodb;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int(11) NOT NULL default '0',
  `id2` int(11) NOT NULL default '0',
  UNIQUE KEY `id` (`id`,`id2`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
show create table t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `id` int(11) NOT NULL default '0',
  KEY `t1_id_fk` (`id`),
  CONSTRAINT `t1_id_fk` FOREIGN KEY (`id`) REFERENCES `t1` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
create index id on t2 (id);
show create table t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `id` int(11) NOT NULL default '0',
  KEY `id` (`id`),
  CONSTRAINT `t1_id_fk` FOREIGN KEY (`id`) REFERENCES `t1` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
create index id2 on t2 (id);
show create table t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `id` int(11) NOT NULL default '0',
  KEY `id` (`id`),
  KEY `id2` (`id`),
  CONSTRAINT `t1_id_fk` FOREIGN KEY (`id`) REFERENCES `t1` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
drop index id2 on t2;
drop index id on t2;
Got one of the listed errors
show create table t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `id` int(11) NOT NULL default '0',
  KEY `id` (`id`),
  CONSTRAINT `t1_id_fk` FOREIGN KEY (`id`) REFERENCES `t1` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
drop table t2;
create table t2 (id int(11) not null, id2 int(11) not null, constraint t1_id_fk foreign key (id,id2) references t1 (id,id2)) engine = innodb;
show create table t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `id` int(11) NOT NULL default '0',
  `id2` int(11) NOT NULL default '0',
  KEY `t1_id_fk` (`id`,`id2`),
  CONSTRAINT `t1_id_fk` FOREIGN KEY (`id`, `id2`) REFERENCES `t1` (`id`, `id2`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
create unique index id on t2 (id,id2);
show create table t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `id` int(11) NOT NULL default '0',
  `id2` int(11) NOT NULL default '0',
  UNIQUE KEY `id` (`id`,`id2`),
  CONSTRAINT `t1_id_fk` FOREIGN KEY (`id`, `id2`) REFERENCES `t1` (`id`, `id2`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
drop table t2;
create table t2 (id int(11) not null, id2 int(11) not null, unique (id,id2),constraint t1_id_fk foreign key (id2,id) references t1 (id,id2)) engine = innodb;
show create table t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `id` int(11) NOT NULL default '0',
  `id2` int(11) NOT NULL default '0',
  UNIQUE KEY `id` (`id`,`id2`),
  KEY `t1_id_fk` (`id2`,`id`),
  CONSTRAINT `t1_id_fk` FOREIGN KEY (`id2`, `id`) REFERENCES `t1` (`id`, `id2`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
drop table t2;
create table t2 (id int(11) not null, id2 int(11) not null, unique (id,id2), constraint t1_id_fk foreign key (id) references t1 (id)) engine = innodb;
show create table t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `id` int(11) NOT NULL default '0',
  `id2` int(11) NOT NULL default '0',
  UNIQUE KEY `id` (`id`,`id2`),
  CONSTRAINT `t1_id_fk` FOREIGN KEY (`id`) REFERENCES `t1` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
drop table t2;
create table t2 (id int(11) not null, id2 int(11) not null, unique (id,id2),constraint t1_id_fk foreign key (id2,id) references t1 (id,id2)) engine = innodb;
show create table t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `id` int(11) NOT NULL default '0',
  `id2` int(11) NOT NULL default '0',
  UNIQUE KEY `id` (`id`,`id2`),
  KEY `t1_id_fk` (`id2`,`id`),
  CONSTRAINT `t1_id_fk` FOREIGN KEY (`id2`, `id`) REFERENCES `t1` (`id`, `id2`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
drop table t2;
create table t2 (id int(11) not null auto_increment, id2 int(11) not null, constraint t1_id_fk foreign key (id) references t1 (id), primary key (id), index (id,id2)) engine = innodb;
show create table t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `id` int(11) NOT NULL auto_increment,
  `id2` int(11) NOT NULL default '0',
  PRIMARY KEY  (`id`),
  KEY `id` (`id`,`id2`),
  CONSTRAINT `t1_id_fk` FOREIGN KEY (`id`) REFERENCES `t1` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
drop table t2;
create table t2 (id int(11) not null auto_increment, id2 int(11) not null, constraint t1_id_fk foreign key (id) references t1 (id)) engine= innodb;
show create table t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `id` int(11) NOT NULL auto_increment,
  `id2` int(11) NOT NULL default '0',
  KEY `t1_id_fk` (`id`),
  CONSTRAINT `t1_id_fk` FOREIGN KEY (`id`) REFERENCES `t1` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
alter table t2 add index id_test (id), add index id_test2 (id,id2);
show create table t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `id` int(11) NOT NULL auto_increment,
  `id2` int(11) NOT NULL default '0',
  KEY `id_test` (`id`),
  KEY `id_test2` (`id`,`id2`),
  CONSTRAINT `t1_id_fk` FOREIGN KEY (`id`) REFERENCES `t1` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
drop table t2;
create table t2 (id int(11) not null, id2 int(11) not null, constraint t1_id_fk foreign key (id2,id) references t1 (id)) engine = innodb;
ERROR HY000: Can't create table '/home/hf/work/mysql-4.1.clean/mysql-test/var/master-data/test/t2.frm' (errno: 150)
create table t2 (a int auto_increment primary key, b int, index(b), foreign key (b) references t1(id), unique(b)) engine=innodb;
show create table t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `a` int(11) NOT NULL auto_increment,
  `b` int(11) default NULL,
  PRIMARY KEY  (`a`),
  UNIQUE KEY `b_2` (`b`),
  KEY `b` (`b`),
  CONSTRAINT `t2_ibfk_1` FOREIGN KEY (`b`) REFERENCES `t1` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
drop table t2;
create table t2 (a int auto_increment primary key, b int, foreign key (b) references t1(id), foreign key (b) references t1(id), unique(b)) engine=innodb;
show create table t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `a` int(11) NOT NULL auto_increment,
  `b` int(11) default NULL,
  PRIMARY KEY  (`a`),
  UNIQUE KEY `b` (`b`),
  CONSTRAINT `t2_ibfk_1` FOREIGN KEY (`b`) REFERENCES `t1` (`id`),
  CONSTRAINT `t2_ibfk_2` FOREIGN KEY (`b`) REFERENCES `t1` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
drop table t2, t1;
show status like "binlog_cache_use";
Variable_name	Value
Binlog_cache_use	24
show status like "binlog_cache_disk_use";
Variable_name	Value
Binlog_cache_disk_use	0
create table t1 (a int) engine=innodb;
show status like "binlog_cache_use";
Variable_name	Value
Binlog_cache_use	25
show status like "binlog_cache_disk_use";
Variable_name	Value
Binlog_cache_disk_use	1
begin;
delete from t1;
commit;
show status like "binlog_cache_use";
Variable_name	Value
Binlog_cache_use	26
show status like "binlog_cache_disk_use";
Variable_name	Value
Binlog_cache_disk_use	1
drop table t1;
create table t1 (c char(10), index (c,c)) engine=innodb;
ERROR 42S21: Duplicate column name 'c'
create table t1 (c1 char(10), c2 char(10), index (c1,c2,c1)) engine=innodb;
ERROR 42S21: Duplicate column name 'c1'
create table t1 (c1 char(10), c2 char(10), index (c1,c1,c2)) engine=innodb;
ERROR 42S21: Duplicate column name 'c1'
create table t1 (c1 char(10), c2 char(10), index (c2,c1,c1)) engine=innodb;
ERROR 42S21: Duplicate column name 'c1'
create table t1 (c1 char(10), c2 char(10)) engine=innodb;
alter table t1 add key (c1,c1);
ERROR 42S21: Duplicate column name 'c1'
alter table t1 add key (c2,c1,c1);
ERROR 42S21: Duplicate column name 'c1'
alter table t1 add key (c1,c2,c1);
ERROR 42S21: Duplicate column name 'c1'
alter table t1 add key (c1,c1,c2);
ERROR 42S21: Duplicate column name 'c1'
drop table t1;
