# Save the initial number of concurrent sessions
--source include/count_sessions.inc

set @orig_sql_mode= @@sql_mode;

--disable_warnings
drop tables if exists t1, t2;
drop view if exists v1;
--enable_warnings

#
# Test for Bug#6116 SET time_zone := ... requires access to mysql.time_zone tables
# We should allow implicit access to time zone description tables even for
# unprivileged users.
#

# Let us prepare playground
delete from mysql.user where user like 'mysqltest\_%';
delete from mysql.db where user like 'mysqltest\_%';
delete from mysql.tables_priv where user like 'mysqltest\_%';
delete from mysql.columns_priv where user like 'mysqltest\_%';
flush privileges;
create table t1 (a int, b datetime);
create table t2 (c int, d datetime);
create user mysqltest_1@localhost;
grant all privileges on test.* to mysqltest_1@localhost;
connect (tzuser, localhost, mysqltest_1,,);
connection tzuser;
show grants for current_user();
set time_zone= '+00:00';
set time_zone= 'Europe/Moscow';
select convert_tz('2004-10-21 19:00:00', 'Europe/Moscow', 'UTC');
select convert_tz(b, 'Europe/Moscow', 'UTC') from t1;
# Let us also check whenever multi-update works ok
update t1, t2 set t1.b = convert_tz('2004-10-21 19:00:00', 'Europe/Moscow', 'UTC')
              where t1.a = t2.c and t2.d = (select max(d) from t2);
# But still these two statements should not work:
--error ER_TABLEACCESS_DENIED_ERROR
select * from mysql.time_zone_name;
--error ER_TABLEACCESS_DENIED_ERROR
select Name, convert_tz('2004-10-21 19:00:00', Name, 'UTC') from mysql.time_zone_name;

connection default;
disconnect tzuser;

#
# Bug#6765 Implicit access to time zone description tables requires privileges
#          for them if some table or column level grants present
#
connection default;
# Let use some table-level grants instead of db-level
# to make life more interesting
delete from mysql.db where user like 'mysqltest\_%';
flush privileges;
grant all privileges on test.t1 to mysqltest_1@localhost;
grant all privileges on test.t2 to mysqltest_1@localhost;
# The test itself is almost the same as previous one
connect (tzuser2, localhost, mysqltest_1,,);
connection tzuser2;
show grants for current_user();
set time_zone= '+00:00';
set time_zone= 'Europe/Moscow';
select convert_tz('2004-11-31 12:00:00', 'Europe/Moscow', 'UTC');
select convert_tz(b, 'Europe/Moscow', 'UTC') from t1;
update t1, t2 set t1.b = convert_tz('2004-11-30 12:00:00', 'Europe/Moscow', 'UTC')
              where t1.a = t2.c and t2.d = (select max(d) from t2);
# Again these two statements should not work (but with different errors):
--error ER_TABLEACCESS_DENIED_ERROR
select * from mysql.time_zone_name;
--error ER_TABLEACCESS_DENIED_ERROR
select Name, convert_tz('2004-11-30 12:00:00', Name, 'UTC') from mysql.time_zone_name;

#
# Bug#9979 Use of CONVERT_TZ in multiple-table UPDATE causes bogus
#          privilege error
#
drop table t1, t2;
create table t1 (a int, b datetime);
create table t2 (a int, b varchar(40));
update t1 set b = '2005-01-01 10:00';
update t1 set b = convert_tz(b, 'UTC', 'UTC');
update t1 join t2 on (t1.a = t2.a) set t1.b = '2005-01-01 10:00' where t2.b = 'foo';
update t1 join t2 on (t1.a = t2.a) set t1.b = convert_tz('2005-01-01 10:00','UTC','UTC') where t2.b = 'foo';

# Clean-up
connection default;
disconnect tzuser2;
delete from mysql.user where user like 'mysqltest\_%';
delete from mysql.db where user like 'mysqltest\_%';
delete from mysql.tables_priv where user like 'mysqltest\_%';
flush privileges;
drop table t1, t2;

# End of 4.1 tests

#
# Additional test for Bug#15153 CONVERT_TZ() is not allowed in all places in views.
#
# Let us check that usage of CONVERT_TZ() function in view does not
# require additional privileges.

# Let us rely on that previous tests done proper cleanups
create table t1 (a int, b datetime);
insert into t1 values (1, 20010101000000), (2, 20020101000000);
create user mysqltest_1@localhost;
grant all privileges on test.* to mysqltest_1@localhost;
connect (tzuser3, localhost, mysqltest_1,,);
create view v1 as select a, convert_tz(b, 'UTC', 'Europe/Moscow') as lb from t1;
select * from v1;
# Of course we should not be able select from mysql.time_zone tables
--error ER_TABLEACCESS_DENIED_ERROR
select * from v1, mysql.time_zone;
drop view v1;
--error ER_TABLEACCESS_DENIED_ERROR
create view v1 as select a, convert_tz(b, 'UTC', 'Europe/Moscow') as lb from t1, mysql.time_zone;
connection default;
disconnect tzuser3;
drop table t1;
drop user mysqltest_1@localhost;

set sql_mode= @orig_sql_mode;

# End of 5.0 tests

# Wait till we reached the initial number of concurrent sessions
--source include/wait_until_count_sessions.inc
