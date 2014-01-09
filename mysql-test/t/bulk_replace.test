#
# this is a test of bulk-insert code
#

--disable_warnings
drop table if exists t1;
--enable_warnings
CREATE TABLE t1 (a int, unique (a), b int not null, unique(b), c int not null, index(c));
replace into t1 values (1,1,1),(2,2,2),(3,1,3);
select * from t1;
check table t1;
drop table t1;

# End of 4.1 tests
