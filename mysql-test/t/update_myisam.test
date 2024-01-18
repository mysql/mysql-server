# Skipping it for binlog_format=STATEMENT due to Unsafe statements:
# unsafe auto-increment column, LIMIT clause.
--source include/force_myisam_default.inc
--source include/have_myisam.inc
--source include/rpl/deprecated/not_binlog_format_statement.inc

# Test bug with update reported by Jan Legenhausen
# 

	
CREATE TABLE t1 (
  lfdnr int(10) unsigned NOT NULL default '0',
  ticket int(10) unsigned NOT NULL default '0',
  client varchar(255) NOT NULL default '',
  replyto varchar(255) NOT NULL default '',
  subject varchar(100) NOT NULL default '',
  timestamp int(10) unsigned NOT NULL default '0',
  tstamp timestamp NOT NULL,
  status int(3) NOT NULL default '0',
  type varchar(15) NOT NULL default '',
  assignment int(10) unsigned NOT NULL default '0',
  fupcount int(4) unsigned NOT NULL default '0',
  parent int(10) unsigned NOT NULL default '0',
  activity int(10) unsigned NOT NULL default '0',
  priority tinyint(1) unsigned NOT NULL default '1',
  cc varchar(255) NOT NULL default '',
  bcc varchar(255) NOT NULL default '',
  body text NOT NULL,
  comment text,
  header text,
  PRIMARY KEY  (lfdnr),
  KEY k1 (timestamp),
  KEY k2 (type),
  KEY k3 (parent),
  KEY k4 (assignment),
  KEY ticket (ticket)
) ENGINE=MyISAM;
INSERT INTO t1 VALUES (773,773,'','','',980257344,20010318180652,0,'Open',10,0,0,0,1,'','','','','');
alter table t1 change lfdnr lfdnr int(10) unsigned not null auto_increment;
update t1 set status=1 where type='Open';
select status from t1;
drop table t1;

	
#
#
# Test with limit (Bug #393)
#

CREATE TABLE t1 (
   `id_param` smallint(3) unsigned NOT NULL default '0',
   `nom_option` char(40) NOT NULL default '',
   `valid` tinyint(1) NOT NULL default '0',
   KEY `id_param` (`id_param`,`nom_option`)
 ) ENGINE=MyISAM;
INSERT INTO t1 (id_param,nom_option,valid) VALUES (185,'600x1200',1);
UPDATE t1 SET nom_option='test' WHERE id_param=185 AND nom_option='600x1200' AND valid=1 LIMIT 1;
select * from t1;
drop table t1;


# BUG#9103 "Erroneous data truncation warnings on multi-table updates"
create table t1 (a int, b varchar(10), key b(b(5))) engine=myisam;
create table t2 (a int, b varchar(10)) engine=myisam;
insert into t1 values ( 1, 'abcd1e');
insert into t1 values ( 2, 'abcd2e');
insert into t2 values ( 1, 'abcd1e');
insert into t2 values ( 2, 'abcd2e');
analyze table t1,t2;
update t1, t2 set t1.a = t2.a where t2.b = t1.b;
show warnings;
drop table t1, t2;

#
#
# Bug#14186 select datefield is null not updated
#
create table t1 (f1 date not null) engine=myisam;
insert into t1 values('2000-01-01'),('0000-00-00');
update t1 set f1='2002-02-02' where f1 is null;
select * from t1;
drop table t1;

--echo # Bug#18439019 Assert in mysql_multi_update with ordered view
CREATE TABLE t1 (
a int unsigned not null auto_increment primary key,
b int unsigned
) ENGINE=MyISAM;
CREATE TABLE t2 (
a int unsigned not null auto_increment primary key,
b int unsigned
) ENGINE=MyISAM;
INSERT INTO t1 VALUES (NULL, 0);
INSERT INTO t1 SELECT NULL, 0 FROM t1;
INSERT INTO t2 VALUES (NULL, 0), (NULL,1);
CREATE VIEW v1 AS SELECT a FROM t1 ORDER BY a;
ANALYZE TABLE t1,t2;
let $query=
UPDATE t2, v1 AS t SET t2.b = t.a+5 ;
eval explain $query;
eval $query;
DROP VIEW v1;
DROP TABLE t1, t2;


