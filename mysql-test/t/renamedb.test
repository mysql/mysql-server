

# TODO: enable these tests when RENAME DATABASE is implemented.
#
# --disable_warnings
# drop database if exists testdb1;
# --enable_warnings
# 
# create database testdb1 default character set latin2;
# use testdb1;
# create table t1 (a int);
# insert into t1 values (1),(2),(3);
# show create database testdb1;
# show tables;
# rename database testdb1 to testdb2;
# --error 1049
# show create database testdb1;
# show create database testdb2;
# select database();
# show tables;
# select a from t1 order by a;
# drop database testdb2;
# 
#
# Bug#19392 Rename Database: Crash if case change
#
# create database testdb1;
# --error 1007
# rename database testdb1 to testdb1;
# drop database testdb1;

#
# WL#4030 (Deprecate RENAME DATABASE: replace with ALTER DATABASE <name> UPGRADE)
#

--error ER_PARSE_ERROR
rename database testdb1 to testdb2;

# ... UPGRADE DATA DIRECTORY NAME syntax has been removed.
