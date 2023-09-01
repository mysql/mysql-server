--source include/force_myisam_default.inc
--source include/have_myisam.inc

create table t1 (a set (' ','a','b ') not null default 'b ');
show create table t1;
drop table t1;
CREATE TABLE t1 (   user varchar(64) NOT NULL default '',   path varchar(255) NOT NULL default '',   privilege   set('select','RESERVED30','RESERVED29','RESERVED28','RESERVED27','RESERVED26',   'RESERVED25','RESERVED24','data.delete','RESERVED22','RESERVED21',   'RESERVED20','data.insert.none','data.insert.approve',   'data.insert.delete','data.insert.move','data.insert.propose',   'data.insert.reject','RESERVED13','RESERVED12','RESERVED11','RESERVED10',   'RESERVED09','data.update','RESERVED07','RESERVED06','RESERVED05',   'RESERVED04','metadata.delete','metadata.put','RESERVED01','RESERVED00')   NOT NULL default '',   KEY user (user)   ) ENGINE=MyISAM CHARSET=utf8mb3;
DROP TABLE t1;

