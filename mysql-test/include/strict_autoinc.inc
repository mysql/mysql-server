#
# Test for strict-mode autoincrement
#

--disable_warnings
drop table if exists t1;
--enable_warnings

set @org_mode=@@sql_mode;
eval create table t1
(
  `a` tinyint(4) NOT NULL auto_increment,
  primary key (`a`)
) engine = $type ;
set @@sql_mode='strict_all_tables';
--error ER_WARN_DATA_OUT_OF_RANGE
insert into t1 values(1000);
select count(*) from t1;

set auto_increment_increment=1000;
set auto_increment_offset=700;
--error ER_WARN_DATA_OUT_OF_RANGE
insert into t1 values(null);
select count(*) from t1;

set @@sql_mode=@org_mode;
insert ignore into t1 values(null);
select * from t1;

drop table t1;

# End of test
