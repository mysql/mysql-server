--source include/force_myisam_default.inc
--source include/have_myisam.inc

# PS protocol gives slightly different metadata
--disable_ps_protocol

#
# Bug #20191: getTableName gives wrong or inconsistent result when using VIEWs
#
# wrong metadata result displayed when VIEWs created over innodb table.
# a Bug #27303036 opened to track this issue
# Remove ENGINE=MyISAM option when bug is fixed.
--enable_metadata
create table t1 (id int(10)) ENGINE=MyISAM;
insert into t1 values (1);
CREATE  VIEW v1 AS select t1.id as id from t1;
CREATE  VIEW v2 AS select t1.id as renamed from t1;
CREATE  VIEW v3 AS select t1.id + 12 as renamed from t1;
select * from v1 group by id limit 1;
select * from v1 group by id limit 0;
select * from v1 where id=1000 group by id;
select * from v1 where id=1 group by id;
select * from v2 where renamed=1 group by renamed;
select * from v3 where renamed=1 group by renamed;
drop table t1;
drop view v1,v2,v3;
--disable_metadata

