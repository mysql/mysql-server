--source include/force_myisam_default.inc
--source include/have_myisam.inc

# Test of datetime optimization
#
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
CREATE TABLE `t1` (
  `date` datetime NOT NULL default '0000-00-00 00:00:00',
  `numfacture` int(6) unsigned NOT NULL default '0',
  `expedition` datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (`numfacture`),
  KEY `date` (`date`),
  KEY `expedition` (`expedition`)
) ENGINE=MyISAM;
INSERT INTO t1 (expedition) VALUES ('0001-00-00 00:00:00');
SELECT * FROM t1 WHERE expedition='0001-00-00 00:00:00';
INSERT INTO t1 (numfacture,expedition) VALUES ('1212','0001-00-00 00:00:00');
SELECT * FROM t1 WHERE expedition='0001-00-00 00:00:00';
EXPLAIN SELECT * FROM t1 WHERE expedition='0001-00-00 00:00:00';
drop table t1;
create table t1 (a datetime not null, b datetime not null);
insert into t1 values (now(), now());
insert into t1 values (now(), now());
select * from t1 where a is null or b is null;
drop table t1;
