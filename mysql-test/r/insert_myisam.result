create table t1 (sid char(20), id int(2) NOT NULL auto_increment, key(sid, id)) engine=myisam;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
insert into t1 values ('skr',NULL),('skr',NULL),('test',NULL);
select * from t1;
sid	id
skr	1
skr	2
test	1
insert into t1 values ('rts',NULL),('rts',NULL),('test',NULL);
select * from t1;
sid	id
rts	1
rts	2
skr	1
skr	2
test	1
test	2
drop table t1;
create table t1 (id int primary key auto_increment, data int, unique(data)) engine=myisam;
insert ignore into t1 values(NULL,100),(NULL,110),(NULL,120);
insert ignore into t1 values(NULL,10),(NULL,20),(NULL,110),(NULL,120),(NULL,100),(NULL,90);
Warnings:
Warning	1062	Duplicate entry '110' for key 't1.data'
Warning	1062	Duplicate entry '120' for key 't1.data'
Warning	1062	Duplicate entry '100' for key 't1.data'
insert ignore into t1 values(NULL,130),(NULL,140),(500,110),(550,120),(450,100),(NULL,150);
Warnings:
Warning	1062	Duplicate entry '110' for key 't1.data'
Warning	1062	Duplicate entry '120' for key 't1.data'
Warning	1062	Duplicate entry '100' for key 't1.data'
select * from t1 order by id;
id	data
1	100
2	110
3	120
4	10
5	20
6	90
7	130
8	140
9	150
drop table t1;
#
# Bug#22635253: ASSERTION `THD->IS_ERROR() BOOL
#               SQL_CMD_INSERT::MYSQL_INSERT(THD*, TABLE_LIST*)
#
CREATE TABLE t1(a INT PRIMARY KEY, b INT) ENGINE=MyISAM;
CREATE TABLE t2(a INT) ENGINE=MyISAM;
CREATE TRIGGER trig1 BEFORE INSERT ON t1 FOR EACH ROW INSERT INTO t2 VALUES (1);
INSERT INTO t1(b) VALUES (1);
Warnings:
Warning	1364	Field 'a' doesn't have a default value
DROP TABLE t2, t1;
