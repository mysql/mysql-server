--source include/force_myisam_default.inc
--source include/have_myisam.inc

#
# Test problem with bulk insert and auto_increment on second part keys
#

create table t1 (sid char(20), id int(2) NOT NULL auto_increment, key(sid, id)) engine=myisam;
insert into t1 values ('skr',NULL),('skr',NULL),('test',NULL);
--sorted_result
select * from t1;
insert into t1 values ('rts',NULL),('rts',NULL),('test',NULL);
--sorted_result
select * from t1;
drop table t1;

# Test of INSERT IGNORE and re-using auto_increment values
create table t1 (id int primary key auto_increment, data int, unique(data)) engine=myisam;
insert ignore into t1 values(NULL,100),(NULL,110),(NULL,120);
insert ignore into t1 values(NULL,10),(NULL,20),(NULL,110),(NULL,120),(NULL,100),(NULL,90);
insert ignore into t1 values(NULL,130),(NULL,140),(500,110),(550,120),(450,100),(NULL,150);
select * from t1 order by id;
drop table t1;

--echo #
--echo # Bug#22635253: ASSERTION `THD->IS_ERROR() BOOL
--echo #               SQL_CMD_INSERT::MYSQL_INSERT(THD*, TABLE_LIST*)
--echo #

CREATE TABLE t1(a INT PRIMARY KEY, b INT) ENGINE=MyISAM;
CREATE TABLE t2(a INT) ENGINE=MyISAM;
CREATE TRIGGER trig1 BEFORE INSERT ON t1 FOR EACH ROW INSERT INTO t2 VALUES (1);
# This statement used to give an assert on debug builds
INSERT INTO t1(b) VALUES (1);
DROP TABLE t2, t1;

