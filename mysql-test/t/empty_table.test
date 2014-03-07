
#
# Some special cases with empty tables
#

--disable_warnings
drop table if exists t1;
--enable_warnings

create table t1 (nr int(5) not null auto_increment,b blob,str char(10), primary key (nr));
select count(*) from t1;
select * from t1;
select * from t1 limit 0;
drop table t1;

# End of 4.1 tests
