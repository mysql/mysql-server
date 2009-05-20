################### include/grant_cache.inc ####################
#
# Test grants with query cache
#
# Last update:
# 2007-05-03 ML - Move t/grant_cache.test to include/grant_cache.inc
#               - Remove the disabling of the ps-protocol
#               - minor improvements like error names instead of numbers
#               - Create two toplevel tests sourcing this routine
#
# Running this test with and without "--ps-protocol" produces different
# Qcache_not_cached results because of the following reason:
# In normal protocol, a SELECT failing due to insufficient privileges
# increments Qcache_not_cached, while in ps-protocol, no.
# In detail:
# - In normal protocol,
#   the "access denied" errors on SELECT are issued at (stack trace):
#   mysql_parse/mysql_execute_command/execute_sqlcom_select/handle_select/
#   mysql_select/JOIN::prepare/setup_wild/insert_fields/
#   check_grant_all_columns/my_error/my_message_sql, which then calls
#   push_warning/query_cache_abort: at this moment,
#   query_cache_store_query() has been called, so query exists in cache,
#   so thd->net.query_cache_query!=NULL, so query_cache_abort() removes
#   the query from cache, which causes a query_cache.refused++ (thus,
#   a Qcache_not_cached++).
# - In ps-protocol,
#   the error is issued at prepare time;
#   for this mysql_test_select() is called, not execute_sqlcom_select()
#   (and that also leads to JOIN::prepare/etc). Thus, as
#   query_cache_store_query() has not been called,
#   thd->net.query_cache_query==NULL, so query_cache_abort() does nothing:
#   Qcache_not_cached is not incremented.
#
# A run of this tests with sp/cursor/view protocol does not make sense
# because these protocols serve totally different purposes than this test.
#

--source include/add_anonymous_users.inc

#
--disable_warnings
drop table if exists test.t1,mysqltest.t1,mysqltest.t2;
drop database if exists mysqltest;
--enable_warnings

set GLOBAL query_cache_size=1355776;

reset query cache;
flush status;
--echo ----- establish connection root -----
connect (root,localhost,root,,test,$MASTER_MYPORT,$MASTER_MYSOCK);
connection root;
show grants for current_user;
show grants;
--disable_warnings
create database if not exists mysqltest;
--enable_warnings

create table mysqltest.t1 (a int,b int,c int);
create table mysqltest.t2 (a int,b int,c int);
insert into mysqltest.t1 values (1,1,1),(2,2,2);
insert into mysqltest.t2 values (3,3,3);
create table test.t1 (a char (10));
insert into test.t1 values ("test.t1");
select * from t1;
--echo ----- establish connection root2 -----
connect (root2,localhost,root,,mysqltest,$MASTER_MYPORT,$MASTER_MYSOCK);
connection root2;
# put queries in cache
select * from t1;
select a from t1;
select c from t1;
select * from t2;
select * from mysqltest.t1,test.t1;
show status like "Qcache_queries_in_cache";
show status like "Qcache_hits%";

# Create the test users
grant SELECT on mysqltest.* to mysqltest_1@localhost;
grant SELECT on mysqltest.t1 to mysqltest_2@localhost;
grant SELECT on test.t1 to mysqltest_2@localhost;
grant SELECT(a) on mysqltest.t1 to mysqltest_3@localhost;

# The following queries should be fetched from cache
--echo ----- establish connection user1 (user=mysqltest_1) -----
connect (user1,localhost,mysqltest_1,,mysqltest,$MASTER_MYPORT,$MASTER_MYSOCK);
connection user1;
show grants for current_user();
show status like "Qcache_queries_in_cache";
show status like "Qcache_hits";
show status like "Qcache_not_cached";
select "user1";
show status like "Qcache_queries_in_cache";
show status like "Qcache_hits";
show status like "Qcache_not_cached";
select * from t1;
show status like "Qcache_queries_in_cache";
show status like "Qcache_hits";
show status like "Qcache_not_cached";
# The pre and end space are intentional
 select a from t1 ;
show status like "Qcache_queries_in_cache";
show status like "Qcache_hits";
show status like "Qcache_not_cached";
select c from t1;
show status like "Qcache_queries_in_cache";
show status like "Qcache_hits";
show status like "Qcache_not_cached";


--echo ----- establish connection unkuser (user=unkuser) -----
# Don't use '' as user because it will pick Unix login
connect (unkuser,localhost,unkuser,,,$MASTER_MYPORT,$MASTER_MYSOCK);
connection unkuser;
show grants for current_user();

# The following queries should be fetched from cache
--echo ----- establish connection user2 (user=mysqltest_2) -----
connect (user2,localhost,mysqltest_2,,mysqltest,$MASTER_MYPORT,$MASTER_MYSOCK);
connection user2;
select "user2";
select * from t1;
select a from t1;
select c from t1;
select * from mysqltest.t1,test.t1;
--replace_result 127.0.0.1 localhost
--error ER_TABLEACCESS_DENIED_ERROR
select * from t2;
show status like "Qcache_queries_in_cache";
show status like "Qcache_hits";
show status like "Qcache_not_cached";

# The following queries should not be fetched from cache
--echo ----- establish connection user3 (user=mysqltest_3) -----
connect (user3,localhost,mysqltest_3,,mysqltest,$MASTER_MYPORT,$MASTER_MYSOCK);
connection user3;
select "user3";
--replace_result 127.0.0.1 localhost
--error ER_TABLEACCESS_DENIED_ERROR
select * from t1;
select a from t1;
--replace_result 127.0.0.1 localhost
--error ER_COLUMNACCESS_DENIED_ERROR
select c from t1;
--replace_result 127.0.0.1 localhost
--error ER_TABLEACCESS_DENIED_ERROR
select * from t2;
--replace_result 127.0.0.1 localhost
--error ER_COLUMNACCESS_DENIED_ERROR
select mysqltest.t1.c from test.t1,mysqltest.t1;
show status like "Qcache_queries_in_cache";
show status like "Qcache_hits";
show status like "Qcache_not_cached";

# Connect without a database
--echo ----- establish connection user4 (user=mysqltest_1) -----
connect (user4,localhost,mysqltest_1,,*NO-ONE*,$MASTER_MYPORT,$MASTER_MYSOCK);
connection user4;
select "user4";
show grants;
--error ER_NO_DB_ERROR
select a from t1;
# The following query is not cached before (different database)
select * from mysqltest.t1,test.t1;
# Cache a query with 'no database'
select a from mysqltest.t1;
select a from mysqltest.t1;
show status like "Qcache_queries_in_cache";
show status like "Qcache_hits";
show status like "Qcache_not_cached";

# Cleanup

--echo ----- close connections -----
connection root;
disconnect root;
--source include/wait_until_disconnected.inc
connection root2;
disconnect root2;
--source include/wait_until_disconnected.inc
connection user1;
disconnect user1;
--source include/wait_until_disconnected.inc
connection user2;
disconnect user2;
--source include/wait_until_disconnected.inc
connection user3;
disconnect user3;
--source include/wait_until_disconnected.inc
connection user4;
disconnect user4;
--source include/wait_until_disconnected.inc
connection unkuser;
disconnect unkuser;
--source include/wait_until_disconnected.inc
--echo ----- switch to connection default -----
connection default;

#
# A temporary 4.1 workaround to make this test pass if
# mysql was compiled with other than latin1 --with-charset=XXX.
# Without "set names binary" the below queries fail with
# "Illegal mix of collations" error.
# In 5.0 we will change grant tables to use NCHAR(N) instead
# of "CHAR(N) BINARY", and use cast-to-nchar:  N'mysqltest_1'.
#
set names binary;
delete from mysql.user where user in ("mysqltest_1","mysqltest_2","mysqltest_3");
delete from mysql.db where user in ("mysqltest_1","mysqltest_2","mysqltest_3");
delete from mysql.tables_priv where user in ("mysqltest_1","mysqltest_2","mysqltest_3");
delete from mysql.columns_priv where user in ("mysqltest_1","mysqltest_2","mysqltest_3");
flush privileges;
drop table test.t1,mysqltest.t1,mysqltest.t2;
drop database mysqltest;

set GLOBAL query_cache_size=default;

--source include/delete_anonymous_users.inc
