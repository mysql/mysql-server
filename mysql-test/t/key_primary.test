#
# test of primary key conversions
#

--disable_warnings
drop table if exists t1;
--enable_warnings

create table t1 (t1 char(3) primary key) charset utf8mb4;
insert into t1 values("ABC");
insert into t1 values("ABA");
insert into t1 values("AB%");
select * from t1 where t1="ABC";
select * from t1 where t1="ABCD";
select * from t1 where t1 like "a_\%";
describe select * from t1 where t1="ABC";
describe select * from t1 where t1="ABCD";
drop table t1;

# End of 4.1 tests
