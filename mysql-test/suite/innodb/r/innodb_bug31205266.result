set debug_sync='reset';
CREATE TABLE t1 (
id int(11) NOT NULL auto_increment,
ggid varchar(32) binary DEFAULT '' NOT NULL,
email varchar(64) DEFAULT '' NOT NULL,
passwd varchar(32) binary DEFAULT '' NOT NULL,
PRIMARY KEY (id),
UNIQUE ggid (ggid)
) ENGINE=innodb;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1287	'BINARY as attribute of a type' is deprecated and will be removed in a future release. Please use a CHARACTER SET clause with _bin collation instead
Warning	1287	'BINARY as attribute of a type' is deprecated and will be removed in a future release. Please use a CHARACTER SET clause with _bin collation instead
SET GLOBAL innodb_limit_optimistic_insert_debug = 2;
insert into t1 (ggid,passwd) values ('test1','xxx');
insert into t1 (ggid,passwd) values ('test2','yyy');
set debug_sync='row_search_before_mtr_restart_for_extra_clust SIGNAL reached WAIT_FOR continue';
select * from t1 where ggid='test2';
set debug_sync='now WAIT_FOR reached';
update t1 set ggid='test0' where id=2;
insert into t1 (ggid,passwd) values ('test2','www');
set debug_sync='now SIGNAL continue';
id	ggid	email	passwd
2	test2		yyy
set debug_sync='reset';
drop table t1;
