# include/query_cache.inc
#
# The variables
#     $engine_type       -- storage engine to be tested
#     $test_foreign_keys -- 0, skip foreign key tests
#                        -- 1, do not skip foreign key tests
# have to be set before sourcing this script.
#
# Last update:
# 2006-08-02 ML test refactored
#               old name was innodb_cache.test
#               main code went into include/query_cache.inc
#

eval SET SESSION STORAGE_ENGINE = $engine_type;

# Initialise
--disable_warnings
drop table if exists t1,t2,t3;
--enable_warnings

#
# Without auto_commit.
#
flush status;
set autocommit=0;
create table t1 (a int not null);
insert into t1 values (1),(2),(3);
select * from t1;
show status like "Qcache_queries_in_cache";
drop table t1;
commit;
set autocommit=1;
begin;
create table t1 (a int not null);
insert into t1 values (1),(2),(3);
select * from t1;
show status like "Qcache_queries_in_cache";
drop table t1;
commit;
create table t1 (a int not null);
create table t2 (a int not null);
create table t3 (a int not null);
insert into t1 values (1),(2);
insert into t2 values (1),(2);
insert into t3 values (1),(2);
select * from t1;
select * from t2;
select * from t3;
show status like "Qcache_queries_in_cache";
show status like "Qcache_hits";
begin;
select * from t1;
select * from t2;
select * from t3;
show status like "Qcache_queries_in_cache";
show status like "Qcache_hits";
insert into t1 values (3);
insert into t2 values (3);
insert into t1 values (4);
select * from t1;
select * from t2;
select * from t3;
show status like "Qcache_queries_in_cache";
show status like "Qcache_hits";
commit;
show status like "Qcache_queries_in_cache";
drop table t3,t2,t1;

CREATE TABLE t1 (id int(11) NOT NULL auto_increment, PRIMARY KEY  (id));
select count(*) from t1;
insert into t1 (id) values (0);
select count(*) from t1;
drop table t1;

if ($test_foreign_keys)
{
#
# one statement roll back inside transation
#
let $save_query_cache_size=`select @@global.query_cache_size`;
set GLOBAL query_cache_size=1355776;
CREATE TABLE t1 ( id int(10) NOT NULL auto_increment, a varchar(25) default NULL, PRIMARY KEY  (id), UNIQUE KEY a (a));
CREATE TABLE t2 ( id int(10) NOT NULL auto_increment, b varchar(25) default NULL, PRIMARY KEY  (id), UNIQUE KEY b (b));
CREATE TABLE t3 ( id int(10) NOT NULL auto_increment, t1_id int(10) NOT NULL default '0', t2_id int(10) NOT NULL default '0', state int(11) default NULL, PRIMARY KEY  (id), UNIQUE KEY t1_id (t1_id,t2_id), KEY t2_id (t2_id,t1_id), CONSTRAINT `t3_ibfk_1` FOREIGN KEY (`t1_id`) REFERENCES `t1` (`id`), CONSTRAINT `t3_ibfk_2` FOREIGN KEY (`t2_id`) REFERENCES `t2` (`id`));
INSERT INTO t1 VALUES (1,'me');
INSERT INTO t2 VALUES (1,'you');
INSERT INTO t3 VALUES (2,1,1,2);
delete from t3 where t1_id = 1 and t2_id = 1;
select t1.* from t1, t2, t3 where t3.state & 1 = 0 and t3.t1_id = t1.id and t3.t2_id = t2.id and t1.id = 1 order by t1.a asc;
begin;
insert into t3 VALUES ( NULL, 1, 1, 2 );
-- error 1062
insert into t3 VALUES ( NULL, 1, 1, 2 );
commit;
select t1.* from t1, t2, t3 where t3.state & 1 = 0 and t3.t1_id = t1.id and t3.t2_id = t2.id and t1.id = 1 order by t1.a asc;
drop table t3,t2,t1;
--disable_query_log
eval set GLOBAL query_cache_size=$save_query_cache_size;
--enable_query_log
}

# End of 4.1 tests
