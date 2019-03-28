--source include/no_ps_protocol.inc
--source include/force_myisam_default.inc
--source include/have_myisam.inc
--source include/count_sessions.inc

# Description
# -----------
# Testing string functions

--disable_warnings
drop table if exists t1,t2;
--enable_warnings

set names latin1;

#t bug in concat_ws
#

CREATE TABLE t1 (
  id int(10) unsigned NOT NULL,
  title varchar(255) default NULL,
  prio int(10) unsigned default NULL,
  category int(10) unsigned default NULL,
  program int(10) unsigned default NULL,
  bugdesc text,
  created datetime default NULL,
  modified timestamp NOT NULL,
  bugstatus int(10) unsigned default NULL,
  submitter int(10) unsigned default NULL
) ENGINE=MyISAM;

INSERT INTO t1 VALUES (1,'Link',1,1,1,'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','2001-02-28 08:40:16',20010228084016,0,4);
SELECT CONCAT('"',CONCAT_WS('";"',title,prio,category,program,bugdesc,created,modified+0,bugstatus,submitter), '"') FROM t1;
SELECT CONCAT('"',CONCAT_WS('";"',title,prio,category,program,bugstatus,submitter), '"') FROM t1;
SELECT CONCAT_WS('";"',title,prio,category,program,bugdesc,created,modified+0,bugstatus,submitter) FROM t1;
SELECT bugdesc, REPLACE(bugdesc, 'xxxxxxxxxxxxxxxxxxxx', 'bbbbbbbbbbbbbbbbbbbb') from t1 group by bugdesc;
drop table t1;

#
# Test bug in AES_DECRYPT() when called with wrong argument
#

CREATE TABLE t1 (id int(11) NOT NULL auto_increment, tmp text NOT NULL, KEY id (id)) ENGINE=MyISAM;
INSERT INTO t1 VALUES (1, 'a545f661efdd1fb66fdee3aab79945bf');
SELECT 1 FROM t1 WHERE tmp=AES_DECRYPT(tmp,"password");
DROP TABLE t1;

CREATE TABLE t1 (
  wid int(10) unsigned NOT NULL auto_increment,
  data_podp date default NULL,
  status_wnio enum('nowy','podp','real','arch') NOT NULL default 'nowy',
  PRIMARY KEY(wid)
);

INSERT INTO t1 VALUES (8,NULL,'real');
INSERT INTO t1 VALUES (9,NULL,'nowy');
SELECT elt(status_wnio,data_podp) FROM t1 GROUP BY wid;
DROP TABLE t1;

#
# test for #739

CREATE TABLE t1 (title text) ENGINE=MyISAM;
INSERT INTO t1 VALUES ('Congress reconvenes in September to debate welfare and adult education');
INSERT INTO t1 VALUES ('House passes the CAREERS bill');
SELECT CONCAT("</a>",RPAD("",(55 - LENGTH(title)),".")) from t1;
DROP TABLE t1;

#
# Bug #7751 - conversion for a bigint unsigned constant 
#

CREATE TABLE t1 (
  id int(11) NOT NULL auto_increment,
  a bigint(20) unsigned default NULL,
  PRIMARY KEY  (id)
) ENGINE=MyISAM;

INSERT INTO t1 VALUES
('0','16307858876001849059');

SELECT CONV('e251273eb74a8ee3', 16, 10);

EXPLAIN
SELECT id
  FROM t1
  WHERE a = 16307858876001849059;

EXPLAIN
  SELECT id
  FROM t1
  WHERE a = CONV('e251273eb74a8ee3', 16, 10);

DROP TABLE t1;

#
# Bug #31758 inet_ntoa, oct, crashes server with null + filesort 
#
create table t1 (a bigint not null)engine=myisam;
insert into t1 set a = 1024*1024*1024*4;
SET @orig_sql_mode = @@SQL_MODE;
SET SQL_MODE='';
delete from t1 order by (inet_ntoa(a)) desc limit 10;
SET SQL_MODE=@orig_sql_mode;
drop table t1;
create table t1 (a char(36) not null)engine=myisam;
insert ignore into t1 set a = ' ';
insert ignore into t1 set a = ' ';
select * from t1 order by (oct(a));
drop table t1;

--echo End of 4.1 tests

