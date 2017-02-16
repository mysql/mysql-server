# handler.inc
#
# See init.inc for setup of variables for this script
# 
# The variables
#     $engine_type       -- storage engine to be tested
#     $other_handler_engine_type -- storage engine <> $engine_type, if possible
#                           1. $other_handler_engine_type must support handler
#                           2. $other_handler_engine_type must point to an all
#                              time available storage engine
#                              2006-08 MySQL 5.1 MyISAM and MEMORY only
#
# test of HANDLER ...
#
# Last update:
# 2006-07-31 ML test refactored (MySQL 5.1)
#               code of t/handler.test and t/innodb_handler.test united
#               main testing code put into handler.inc
#

source include/have_csv.inc;

#
# Start testing the table created in init.inc
#
handler t1 open as t2;
handler t2 read b first;
handler t2 read b next;
handler t2 read b next;
handler t2 read b prev;
handler t2 read b last;
handler t2 read b prev;
handler t2 read b prev;

handler t2 read b first;
handler t2 read b prev;

handler t2 read b last;
handler t2 read b prev;
handler t2 read b next;
handler t2 read b next;

handler t2 read a=(15);
handler t2 read a=(21);

--error 1070
handler t2 read a=(19,"fff");

handler t2 read b=(19,"fff");
handler t2 read b=(19,"yyy");
handler t2 read b=(19);

--error 1109
handler t1 read a last;

handler t2 read a=(11);
handler t2 read a>=(11);

# Search on something we can find
handler t2 read b=(18);
handler t2 read b>=(18);
handler t2 read b>(18);
handler t2 read b<=(18);
handler t2 read b<(18);

# Search on something we can't find
--sorted_result
handler t2 read a=(15);
--sorted_result
handler t2 read a>=(15) limit 2;
--sorted_result
handler t2 read a>(15) limit 2;
handler t2 read a<=(15);
handler t2 read a<(15);

# Search from upper end
handler t2 read a=(54);
handler t2 read a>=(54);
handler t2 read a>(54);
handler t2 read a<=(54);
handler t2 read a<(54);

# Search from lower end
handler t2 read a=(1);
handler t2 read a>=(1);
handler t2 read a>(1);
handler t2 read a<=(1);
handler t2 read a<(1);

handler t2 read b first limit 5;
handler t2 read b next  limit 3;
handler t2 read b prev  limit 10;

handler t2 read b>=(16) limit 4;
handler t2 read b>=(16) limit 2,2;
select * from t1 where a>=16 order by a,b limit 2,2;
handler t2 read a last  limit 3;
handler t2 read b=(16) limit 1,3;
handler t2 read b=(19);
handler t2 read b=(19) where b="yyy";

handler t2 read first;
handler t2 read next;
handler t2 read next;
handler t2 close;

handler t1 open;
handler t1 read b next; # this used to crash as a bug#5373
handler t1 read b next;
handler t1 close;

handler t1 open;
handler t1 read a prev; # this used to crash as a bug#5373
handler t1 read a prev;
handler t1 close;

handler t1 open as t2;
handler t2 read first;
eval alter table t1 engine = $engine_type;
--error 1109
handler t2 read first;

handler t1 open;
handler t1 read a=(20) limit 1,3;
flush tables;
handler t1 read a=(20) limit 1,3;
handler t1 close;

#
# Search after end and before start of index
#

handler t1 open;
handler t1 read a=(25);
handler t1 read a next;
handler t1 read a next;
handler t1 read a next;
handler t1 read a prev;
handler t1 read a=(1000);
handler t1 read a next;
handler t1 read a prev;
handler t1 read a=(1000);
handler t1 read a prev;

handler t1 read a=(14);
handler t1 read a prev;
handler t1 read a prev;
handler t1 read a next;
handler t1 read a=(1);
handler t1 read a prev;
handler t1 read a next;
handler t1 read a=(1);
handler t1 read a next;

handler t1 close;

#
# Test with prepared statements
#

handler t1 open;
prepare stmt from 'handler t1 read a=(?) limit ?,?';
set @a=20,@b=1,@c=100;
execute stmt using @a,@b,@c;
set @a=20,@b=2,@c=1;
execute stmt using @a,@b,@c;
set @a=20,@b=0,@c=2;
execute stmt using @a,@b,@c;
deallocate prepare stmt;

prepare stmt from 'handler t1 read a next limit ?';
handler t1 read a>=(21);
set @a=3;
execute stmt using @a;
execute stmt using @a;
execute stmt using @a;
deallocate prepare stmt;

prepare stmt from 'handler t1 read b prev limit ?';
execute stmt using @a;
execute stmt using @a;
execute stmt using @a;
execute stmt using @a;
deallocate prepare stmt;

prepare stmt from 'handler t1 read b=(?,?)';
set @a=14, @b='aaa';
execute stmt using @a,@b;
set @a=14, @b='not found';
execute stmt using @a,@b;
deallocate prepare stmt;

prepare stmt from 'handler t1 read b=(1+?) limit 10';
set @a=15;
execute stmt using @a;
execute stmt using @a;
deallocate prepare stmt;

prepare stmt from 'handler t1 read b>=(?) where a < ? limit 5';
set @a=17, @b=24;
execute stmt using @a,@b;
execute stmt using @a,@b;
deallocate prepare stmt;

prepare stmt from 'handler t1 read a=(?)';
set @a=17;
execute stmt using @a;
alter table t1 add c int;
--error 1109
execute stmt using @a;
deallocate prepare stmt;
--error 1109
handler t1 close;

handler t1 open;
prepare stmt from 'handler t1 read a=(?)';
flush tables;
set @a=17;
execute stmt using @a;
deallocate prepare stmt;
handler t1 close;

#
# DROP TABLE / ALTER TABLE
#
handler t1 open as t2;
drop table t1;
create table t1 (a int not null);
insert into t1 values (17);
--error 1109
handler t2 read first;
handler t1 open as t2;
alter table t1 engine=csv;
--error 1109
handler t2 read first;
drop table t1;

#
# Test case for the bug #787
#
create table t1 (a int);
insert into t1 values (1),(2),(3),(4),(5),(6);
delete from t1 limit 2;
handler t1 open;
handler t1 read first;
handler t1 read first limit 1,1;
handler t1 read first limit 2,2;
delete from t1 limit 3;
handler t1 read first;
drop table t1;

#
# Test for #751
#
create table t1(a int, index using btree (a));
insert into t1 values (1), (2), (3);
handler t1 open;
--error 1054
handler t1 read a=(W);
--error 1210
handler t1 read a=(a);
drop table t1;
#
# BUG#2304
#
create table t1 (a char(5));
insert into t1 values ("Ok");
handler t1 open as t;
handler t read first;
use mysql;
handler t read first;
handler t close;
handler test.t1 open as t;
handler t read first;
handler t close;
use test;
drop table t1;

#
# BUG#3649
#
create table t1 ( a int, b int, INDEX a using btree (a) );
insert into t1 values (1,2), (2,1);
handler t1 open;
handler t1 read a=(1) where b=2;
handler t1 read a=(1) where b=3;
handler t1 read a=(1) where b=1;
handler t1 close;
drop table t1;

#
# Test if fix for BUG#4286 correctly closes handler tables.
#
create table t1 (c1 char(20));
insert into t1 values ("t1");
handler t1 open as h1;
handler h1 read first limit 9;
create table t2 (c1 char(20));
insert into t2 values ("t2");
handler t2 open as h2;
handler h2 read first limit 9;
create table t3 (c1 char(20));
insert into t3 values ("t3");
handler t3 open as h3;
handler h3 read first limit 9;
create table t4 (c1 char(20));
insert into t4 values ("t4");
handler t4 open as h4;
handler h4 read first limit 9;
create table t5 (c1 char(20));
insert into t5 values ("t5");
handler t5 open as h5;
handler h5 read first limit 9;
# close first
eval alter table t1 engine=$other_handler_engine_type;
--error 1109
handler h1 read first limit 9;
handler h2 read first limit 9;
handler h3 read first limit 9;
handler h4 read first limit 9;
handler h5 read first limit 9;
# close last
eval alter table t5 engine=$other_handler_engine_type;
--error 1109
handler h1 read first limit 9;
handler h2 read first limit 9;
handler h3 read first limit 9;
handler h4 read first limit 9;
--error 1109
handler h5 read first limit 9;
# close middle
eval alter table t3 engine=$other_handler_engine_type;
--error 1109
handler h1 read first limit 9;
handler h2 read first limit 9;
--error 1109
handler h3 read first limit 9;
handler h4 read first limit 9;
--error 1109
handler h5 read first limit 9;
handler h2 close;
handler h4 close;
# close all depending handler tables
handler t1 open as h1_1;
handler t1 open as h1_2;
handler t1 open as h1_3;
handler h1_1 read first limit 9;
handler h1_2 read first limit 9;
handler h1_3 read first limit 9;
eval alter table t1 engine=$engine_type;
--error 1109
handler h1_1 read first limit 9;
--error 1109
handler h1_2 read first limit 9;
--error 1109
handler h1_3 read first limit 9;
drop table t1;
drop table t2;
drop table t3;
drop table t4;
drop table t5;

#
# Bug#14397 - OPTIMIZE TABLE with an open HANDLER causes a crash
#
create table t1 (c1 int);
insert into t1 values (1);
# client 1
handler t1 open;
handler t1 read first;
# client 2
connect (con2,localhost,root,,);
connection con2;
--echo send the below to another connection, do not wait for the result
send optimize table t1;
--sleep 1
# client 1
--echo proceed with the normal connection
connection default;
handler t1 read next;
handler t1 close;
# client 2
--echo read the result from the other connection
connection con2;
reap;
# client 1
--echo proceed with the normal connection
connection default;
drop table t1;

CREATE TABLE t1 (  no1 smallint(5) NOT NULL default '0',  no2 int(10) NOT NULL default '0',  PRIMARY KEY using btree (no1,no2));
INSERT INTO t1 VALUES (1,274),(1,275),(2,6),(2,8),(4,1),(4,2);
HANDLER t1 OPEN;
HANDLER t1 READ `primary` = (1, 1000);
HANDLER t1 READ `primary` PREV;
HANDLER t1 READ `primary` = (1, 1000);
HANDLER t1 READ `primary` NEXT;
DROP TABLE t1;

# End of 4.1 tests

#
# Addendum to Bug#14397 - OPTIMIZE TABLE with an open HANDLER causes a crash
# Show that DROP TABLE can no longer deadlock against
# FLUSH TABLES WITH READ LOCK. This is a 5.0 issue.
#
create table t1 (c1 int);
insert into t1 values (14397);
flush tables with read lock;
# The thread with the global read lock cannot drop the table itself:
--error 1223
drop table t1;
#
# client 2
# We need a second connection to try the drop.
# The drop waits for the global read lock to go away.
# Without the addendum fix it locked LOCK_open before entering the wait loop.
connection con2;
--echo send the below to another connection, do not wait for the result
send drop table t1;
--sleep 1
#
# client 1
# Now we need something that wants LOCK_open. A simple table access which
# opens the table does the trick.
--echo proceed with the normal connection
connection default;
# This would hang on LOCK_open without the 5.0 addendum fix.
select * from t1;
# Release the read lock. This should make the DROP go through.
unlock tables;
#
# client 2
# Read the result of the drop command.
connection con2;
--echo read the result from the other connection
reap;
#
# client 1
# Now back to normal operation. The table should not exist any more.
--echo proceed with the normal connection
connection default;
--error 1146
select * from t1;
# Just to be sure and not confuse the next test case writer.
drop table if exists t1;

#
# Bug#25856 - HANDLER table OPEN in one connection lock DROP TABLE in another one
#
create table t1 (a int not null) ENGINE=csv;
--echo --> client 2
connection con2;
--error 1031
handler t1 open;
--echo --> client 1
connection default;
drop table t1;
disconnect con2;

#
# Bug#30632 HANDLER read failure causes hang
#
create table t1 (a int);
handler t1 open as t1_alias;
--error 1176
handler t1_alias read a next;
--error 1054
handler t1_alias READ a next where inexistent > 0;
--error 1176
handler t1_alias read a next;
--error 1054
handler t1_alias READ a next where inexistent > 0;
handler t1_alias close;
drop table t1;

#
# Bug#30882 Dropping a temporary table inside a stored function may cause a server crash
#
# Test HANDLER statements in conjunction with temporary tables. While the temporary table
# is open by a HANDLER, no other statement can access it.
#

create temporary table t1 (a int, b char(1), key a using btree (a), key b using btree (a,b));
insert into t1 values (0,"a"),(1,"b"),(2,"c"),(3,"d"),(4,"e"),
                      (5,"f"),(6,"g"),(7,"h"),(8,"i"),(9,"j"),(9,'k');
select a,b from t1;
handler t1 open as a1;
handler a1 read a=(1);
handler a1 read a next;
handler a1 read a next;
--error ER_CANT_REOPEN_TABLE
select a,b from t1;
handler a1 read a prev;
handler a1 read a prev;
handler a1 read a=(6) where b="g";
handler a1 close;
select a,b from t1;
handler t1 open as a2;
handler a2 read b=(9);
handler a2 read b next;
handler a2 read b prev limit 2;
--error 0,1031
handler a2 read b last;
handler a2 read b prev;
handler a2 close;
drop table t1;

# Test that temporary tables associated with handlers are properly dropped.

create table t1 (a int);
create temporary table t2 (a int, key using btree (a));
handler t1 open as a1;
handler t2 open as a2;
handler a2 read a first;
drop table t1, t2;
--error ER_UNKNOWN_TABLE
handler a2 read a next;
--error ER_UNKNOWN_TABLE
handler a1 close;

# Alter table drop handlers

create table t1 (a int, key using btree (a));
create table t2 like t1;
handler t1 open as a1;
handler t2 open as a2;
handler a1 read a first;
handler a2 read a first;
alter table t1 add b int;
--error ER_UNKNOWN_TABLE
handler a1 close;
handler a2 close;
drop table t1, t2;

# Rename table drop handlers

create table t1 (a int, key using btree (a));
handler t1 open as a1;
handler a1 read a first;
rename table t1 to t2;
--error ER_UNKNOWN_TABLE
handler a1 read a first;
drop table t2;

# Optimize table drop handlers

create table t1 (a int, key using btree (a));
create table t2 like t1;
handler t1 open as a1;
handler t2 open as a2;
handler a1 read a first;
handler a2 read a first;
optimize table t1;
--error ER_UNKNOWN_TABLE
handler a1 close;
handler a2 close;
drop table t1, t2;

--echo #
--echo # Add test coverage for HANDLER and LOCK TABLES, HANDLER and DDL.
--echo #
--disable_warnings
drop table if exists t1, t2, t3;
--enable_warnings
create table t1 (a int, key a (a));
insert into t1 (a) values (1), (2), (3), (4), (5);
create table t2 (a int, key a (a)) select * from t1; 
create temporary table t3 (a int, key a (a)) select * from t2;
handler t1 open;
handler t2 open;
handler t3 open;
--echo #
--echo # No HANDLER sql is allowed under LOCK TABLES.
--echo # But it does not implicitly closes all handlers.
--echo #
lock table t1 read;
--error ER_LOCK_OR_ACTIVE_TRANSACTION
handler t1 open;
--error ER_LOCK_OR_ACTIVE_TRANSACTION
handler t1 read next;
--error ER_LOCK_OR_ACTIVE_TRANSACTION
handler t2 close;
--error ER_LOCK_OR_ACTIVE_TRANSACTION
handler t3 open;
--echo # After UNLOCK TABLES handlers should be around and
--echo # we should be able to continue reading through them.
unlock tables;
handler t1 read next;
handler t1 close;
handler t2 read next;
handler t2 close;
handler t3 read next;
handler t3 close;
drop temporary table t3;
--echo #
--echo # Other operations that implicitly close handler:
--echo #
--echo # TRUNCATE
--echo #
handler t1 open;
truncate table t1;
--error ER_UNKNOWN_TABLE
handler t1 read next;
handler t1 open;
--echo #
--echo # CREATE TRIGGER
--echo #
create trigger t1_ai after insert on t1 for each row set @a=1;
--error ER_UNKNOWN_TABLE
handler t1 read next;
--echo #
--echo # DROP TRIGGER
--echo #
handler t1 open;
drop trigger t1_ai;
--error ER_UNKNOWN_TABLE
handler t1 read next;
--echo #
--echo # ALTER TABLE
--echo # 
handler t1 open;
alter table t1 add column b int;
--error ER_UNKNOWN_TABLE
handler t1 read next;
--echo #
--echo # ANALYZE TABLE
--echo # 
handler t1 open;
analyze table t1;
--error ER_UNKNOWN_TABLE
handler t1 read next;
--echo #
--echo # OPTIMIZE TABLE
--echo # 
handler t1 open;
optimize table t1;
--error ER_UNKNOWN_TABLE
handler t1 read next;
--echo #
--echo # REPAIR TABLE
--echo # 
handler t1 open;
repair table t1;
--error ER_UNKNOWN_TABLE
handler t1 read next;
--echo #
--echo # DROP TABLE, naturally.
--echo # 
handler t1 open;
drop table t1;
--error ER_UNKNOWN_TABLE
handler t1 read next;
create table t1 (a int, b int, key a using btree (a)) select a from t2;
--echo #
--echo # RENAME TABLE, naturally
--echo # 
handler t1 open;
rename table t1 to t3;
--error ER_UNKNOWN_TABLE
handler t1 read next;
--echo #
--echo # CREATE TABLE (even with IF NOT EXISTS clause,
--echo # and the table exists).
--echo # 
handler t2 open;
create table if not exists t2 (a int);
--error ER_UNKNOWN_TABLE
handler t2 read next;
rename table t3 to t1;
drop table t2;
--echo #
--echo # FLUSH TABLE doesn't close the table but loses the position
--echo # 
handler t1 open;
handler t1 read a prev;
flush table t1;
handler t1 read a prev;
handler t1 close;
--echo #
--echo # FLUSH TABLES WITH READ LOCK behaves like FLUSH TABLE.
--echo # 
handler t1 open;
handler t1 read a prev;
flush tables with read lock;
handler t1 read a prev;
handler t1 close;
unlock tables;
--echo #
--echo # Let us also check that these operations behave in similar
--echo # way under LOCK TABLES.
--echo #
--echo # TRUNCATE under LOCK TABLES.
--echo #
handler t1 open;
lock tables t1 write;
truncate table t1;
unlock tables;
--error ER_UNKNOWN_TABLE
handler t1 read next;
handler t1 open;
--echo #
--echo # CREATE TRIGGER under LOCK TABLES.
--echo #
lock tables t1 write;
create trigger t1_ai after insert on t1 for each row set @a=1;
unlock tables;
--error ER_UNKNOWN_TABLE
handler t1 read next;
--echo #
--echo # DROP TRIGGER under LOCK TABLES.
--echo #
handler t1 open;
lock tables t1 write;
drop trigger t1_ai;
unlock tables;
--error ER_UNKNOWN_TABLE
handler t1 read next;
--echo #
--echo # ALTER TABLE under LOCK TABLES.
--echo # 
handler t1 open;
lock tables t1 write;
alter table t1 drop column b;
unlock tables;
--error ER_UNKNOWN_TABLE
handler t1 read next;
--echo #
--echo # ANALYZE TABLE under LOCK TABLES.
--echo # 
handler t1 open;
lock tables t1 write;
analyze table t1;
unlock tables;
--error ER_UNKNOWN_TABLE
handler t1 read next;
--echo #
--echo # OPTIMIZE TABLE under LOCK TABLES.
--echo # 
handler t1 open;
lock tables t1 write;
optimize table t1;
unlock tables;
--error ER_UNKNOWN_TABLE
handler t1 read next;
--echo #
--echo # REPAIR TABLE under LOCK TABLES.
--echo # 
handler t1 open;
lock tables t1 write;
repair table t1;
unlock tables;
--error ER_UNKNOWN_TABLE
handler t1 read next;
--echo #
--echo # DROP TABLE under LOCK TABLES, naturally.
--echo # 
handler t1 open;
lock tables t1 write;
drop table t1;
unlock tables;
--error ER_UNKNOWN_TABLE
handler t1 read next;
create table t1 (a int, b int, key a using btree (a));
insert into t1 (a) values (1), (2), (3), (4), (5);
--echo #
--echo # FLUSH TABLE doesn't close the table but loses the position
--echo # 
handler t1 open;
handler t1 read a prev;
lock tables t1 write;
flush table t1;
unlock tables;
handler t1 read a prev;
handler t1 close;
--echo #
--echo # Explore the effect of HANDLER locks on concurrent DDL
--echo #
handler t1 open;
--echo # Establishing auxiliary connections con1, con2, con3
connect(con1, localhost, root,,);
connect(con2, localhost, root,,);
connect(con3, localhost, root,,);
--echo # --> connection con1;
connection con1;
--echo # Sending:
--send drop table t1 
--echo # We can't use connection 'default' as wait_condition will 
--echo # autoclose handlers.
--echo # --> connection con2 
connection con2;
--echo # Waitng for 'drop table t1' to get blocked...
let $wait_condition=select count(*)=1 from information_schema.processlist
                    where state='Waiting for table metadata lock' and
                          info='drop table t1';
--source include/wait_condition.inc
--echo # --> connection default
connection default;
handler t1 read a prev;
handler t1 read a prev;
handler t1 close;
--echo # --> connection con1
connection con1;
--echo # Reaping 'drop table t1'...
--reap
--echo # --> connection default
connection default;
--echo #
--echo # Explore the effect of HANDLER locks in parallel with SELECT 
--echo #
create table t1 (a int, key a using btree (a));
insert into t1 (a) values (1), (2), (3), (4), (5);
begin;
select * from t1;
handler t1 open;
handler t1 read a prev;
handler t1 read a prev;
handler t1 close;
--echo # --> connection con1;
connection con1;
--echo # Sending:
--send drop table t1 
--echo # --> connection con2 
connection con2;
--echo # Waiting for 'drop table t1' to get blocked...
let $wait_condition=select count(*)=1 from information_schema.processlist
                    where state='Waiting for table metadata lock' and
                          info='drop table t1';
--source include/wait_condition.inc
--echo # --> connection default
connection default;
--echo # We can still use the table, it's part of the transaction
select * from t1;
--echo # Such are the circumstances that t1 is a part of transaction,
--echo # thus we can reopen it in the handler
handler t1 open;
--echo # We can commit the transaction, it doesn't close the handler
--echo # and doesn't let DROP to proceed.
commit;
handler t1 read a prev;
handler t1 read a prev;
handler t1 read a prev;
handler t1 close;
--echo # --> connection con1
connection con1;
--echo # Now drop can proceed
--echo # Reaping 'drop table t1'...
--reap
--echo # --> connection default
connection default;
--echo #
--echo # Demonstrate that HANDLER locks and transaction locks
--echo # reside in the same context.
--echo #
create table t1 (a int, key a (a));
insert into t1 (a) values (1), (2), (3), (4), (5);
create table t0 (a int, key a using btree (a));
insert into t0 (a) values (1), (2), (3), (4), (5);
begin;
select * from t1;
--echo # --> connection con2 
connection con2;
--echo # Sending:
send rename table t0 to t3, t1 to t0, t3 to t1;
--echo # --> connection con1 
connection con1;
--echo # Waiting for 'rename table ...' to get blocked...
let $wait_condition=select count(*)=1 from information_schema.processlist
                    where state='Waiting for table metadata lock' and
                          info='rename table t0 to t3, t1 to t0, t3 to t1';
--source include/wait_condition.inc
--echo # --> connection default
connection default;
--echo # We back-off on hitting deadlock condition.
--error ER_LOCK_DEADLOCK
handler t0 open;
select * from t0;
handler t1 open;
commit;
handler t1 close;
--echo # --> connection con2
connection con2;
--echo # Reaping 'rename table ...'...
--reap
--echo # --> connection default
connection default;
handler t1 open;
handler t1 read a prev;
handler t1 close;
drop table t0;
--echo #
--echo # Originally there was a deadlock error in this test.
--echo # With implementation of deadlock detector
--echo # we no longer deadlock, but block and wait on a lock.
--echo # The HANDLER is auto-closed as soon as the connection
--echo # sees a pending conflicting lock against it.
--echo #
create table t2 (a int, key a (a));
handler t1 open;
--echo # --> connection con1
connection con1;
lock tables t2 read;
--echo # --> connection con2
connection con2;
--echo # Sending 'drop table t2'...
--send drop table t2
--echo # --> connection con1
connection con1;
--echo # Waiting for 'drop table t2' to get blocked...
let $wait_condition=select count(*)=1 from information_schema.processlist
                    where state='Waiting for table metadata lock' and
                          info='drop table t2';
--source include/wait_condition.inc
--echo # --> connection default
connection default;
--echo # Sending 'select * from t2'
send select * from t2;
--echo # --> connection con1
connection con1;
--echo # Waiting for 'select * from t2' to get blocked...
let $wait_condition=select count(*)=1 from information_schema.processlist
                    where state='Waiting for table metadata lock' and
                          info='select * from t2';
unlock tables;
--echo # --> connection con2
connection con2;
--echo # Reaping 'drop table t2'...
--reap
--echo # --> connection default
connection default;
--echo # Reaping 'select * from t2'
--error ER_NO_SUCH_TABLE
reap;
handler t1 close;

--echo #
--echo # ROLLBACK TO SAVEPOINT releases transactional locks,
--echo # but has no effect on open HANDLERs
--echo #
create table t2 like t1;
create table t3 like t1;
begin;
--echo # Have something before the savepoint
select * from t3;
savepoint sv;
handler t1 open;
handler t1 read a first;
handler t1 read a next;
select * from t2;
--echo # --> connection con1
connection con1;
--echo # Sending:
--send drop table t1
--echo # --> connection con2
connection con2;
--echo # Sending:
--send drop table t2
--echo # --> connection default
connection default;
--echo # Let DROP TABLE statements sync in. We must use
--echo # a separate connection for that, because otherwise SELECT
--echo # will auto-close the HANDLERs, becaues there are pending
--echo # exclusive locks against them.
--echo # --> connection con3
connection con3;
--echo # Waiting for 'drop table t1' to get blocked...
let $wait_condition=select count(*)=1 from information_schema.processlist
                    where state='Waiting for table metadata lock' and
                          info='drop table t1';
--source include/wait_condition.inc
--echo # Waiting for 'drop table t2' to get blocked...
let $wait_condition=select count(*)=1 from information_schema.processlist
                    where state='Waiting for table metadata lock' and
                          info='drop table t2';
--source include/wait_condition.inc
--echo # Demonstrate that t2 lock was released and t2 was dropped
--echo # after ROLLBACK TO SAVEPOINT
--echo # --> connection default
connection default;
rollback to savepoint sv;
--echo # --> connection con2
connection con2;
--echo # Reaping 'drop table t2'...
--reap
--echo # Demonstrate that ROLLBACK TO SAVEPOINT didn't release the handler
--echo # lock.
--echo # --> connection default
connection default;
handler t1 read a next;
handler t1 read a next;
--echo # Demonstrate that the drop will go through as soon as we close the 
--echo # HANDLER
handler t1 close;
--echo # connection con1
connection con1;
--echo # Reaping 'drop table t1'...
--reap
--echo # --> connection default
connection default;
commit;
drop table t3;
--echo #
--echo # A few special cases when using SAVEPOINT/ROLLBACK TO
--echo # SAVEPOINT and HANDLER.
--echo #
--echo # Show that rollback to the savepoint taken in the beginning
--echo # of the transaction doesn't release mdl lock on
--echo # the HANDLER that was opened later.
--echo #
create table t1 (a int, key using btree (a));
insert into t1 (a) values (1), (2), (3), (4), (5);
create table t2 like t1;
begin;
savepoint sv;
handler t1 open;
handler t1 read a first;
handler t1 read a next;
select * from t2;
--echo # --> connection con1
connection con1;
--echo # Sending:
--send drop table t1
--echo # --> connection con2
connection con2;
--echo # Sending:
--send drop table t2
--echo # --> connection default
connection default;
--echo # Let DROP TABLE statements sync in. We must use
--echo # a separate connection for that, because otherwise SELECT
--echo # will auto-close the HANDLERs, becaues there are pending
--echo # exclusive locks against them.
--echo # --> connection con3
connection con3;
--echo # Waiting for 'drop table t1' to get blocked...
let $wait_condition=select count(*)=1 from information_schema.processlist
                    where state='Waiting for table metadata lock' and
                          info='drop table t1';
--source include/wait_condition.inc
--echo # Waiting for 'drop table t2' to get blocked...
let $wait_condition=select count(*)=1 from information_schema.processlist
                    where state='Waiting for table metadata lock' and
                          info='drop table t2';
--source include/wait_condition.inc
--echo # Demonstrate that t2 lock was released and t2 was dropped
--echo # after ROLLBACK TO SAVEPOINT
--echo # --> connection default
connection default;
rollback to savepoint sv;
--echo # --> connection con2
connection con2;
--echo # Reaping 'drop table t2'...
--reap
--echo # Demonstrate that ROLLBACK TO SAVEPOINT didn't release the handler
--echo # lock.
--echo # --> connection default
connection default;
handler t1 read a next;
handler t1 read a next;
--echo # Demonstrate that the drop will go through as soon as we close the 
--echo # HANDLER
handler t1 close;
--echo # connection con1
connection con1;
--echo # Reaping 'drop table t1'...
--reap
--echo # --> connection default
connection default;
commit;
--echo #
--echo # Show that rollback to the savepoint taken in the beginning
--echo # of the transaction works properly (no valgrind warnins, etc),
--echo # even though it's done after the HANDLER mdl lock that was there
--echo # at the beginning is released and added again.
--echo #
create table t1 (a int, key using btree (a));
insert into t1 (a) values (1), (2), (3), (4), (5);
create table t2 like t1;
create table t3 like t1;
insert into t3 (a) select a from t1;
begin;
handler t1 open;
savepoint sv;
handler t1 read a first;
select * from t2;
handler t1 close;
handler t3 open;
handler t3 read a first;
rollback to savepoint sv;
--echo # --> connection con1
connection con1;
drop table t1, t2;
--echo # Sending:
--send drop table t3
--echo # Let DROP TABLE statement sync in.
--echo # --> connection con2
connection con2;
--echo # Waiting for 'drop table t3' to get blocked...
let $wait_condition=select count(*)=1 from information_schema.processlist
                    where state='Waiting for table metadata lock' and
                          info='drop table t3';
--source include/wait_condition.inc
--echo # Demonstrate that ROLLBACK TO SAVEPOINT didn't release the handler
--echo # lock.
--echo # --> connection default
connection default;
handler t3 read a next;
--echo # Demonstrate that the drop will go through as soon as we close the 
--echo # HANDLER
handler t3 close;
--echo # connection con1
connection con1;
--echo # Reaping 'drop table t3'...
--reap
--echo # --> connection default
connection default;
commit;

--echo # 
--echo # If we have to wait on an exclusive locks while having
--echo # an open HANDLER, ER_LOCK_DEADLOCK is reported.
--echo #
create table t1 (a int, key a(a));
create  table t2 like t1;
handler t1 open;
--echo # --> connection con1
connection con1;
lock table t1 write, t2 write;
--echo # --> connection default
connection default;
send drop table t2;
--echo # --> connection con2
connection con2;
--echo # Waiting for 'drop table t2' to get blocked...
let $wait_condition=select count(*)=1 from information_schema.processlist
                    where state='Waiting for table metadata lock' and
                          info='drop table t2';
--source include/wait_condition.inc
--echo # --> connection con1
connection con1;
--error ER_LOCK_DEADLOCK
drop table t1;
unlock tables;
--echo # --> connection default 
connection default;
reap;

--echo # Demonstrate that there is no deadlock with FLUSH TABLE,
--echo # even though it is waiting for the other table to go away
create table t2 like t1;
--echo # Sending:
--send flush table t2
--echo # --> connection con2
connection con2;
drop table t1;
--echo # --> connection con1
connection con1;
unlock tables;
--echo # --> connection default
connection default;
--echo # Reaping 'flush table t2'...
--reap
drop table t2;

--echo # 
--echo # Bug #46224 HANDLER statements within a transaction might 
--echo #            lead to deadlocks
--echo #
create table t1 (a int, key using btree (a));
insert into t1 values (1), (2);

--echo # --> connection default
connection default;
begin;
select * from t1;
handler t1 open;

--echo # --> connection con1
connection con1;
--echo # Sending:
--send lock tables t1 write

--echo # --> connection con2
connection con2;
--echo # Check that 'lock tables t1 write' waits until transaction which
--echo # has read from the table commits.
let $wait_condition=
  select count(*) = 1 from information_schema.processlist
  where state = "Waiting for table metadata lock" and
        info = "lock tables t1 write";
--source include/wait_condition.inc

--echo # --> connection default
connection default;
--echo # The below 'handler t1 read ...' should not be blocked as
--echo # 'lock tables t1 write' has not succeeded yet.
handler t1 read a next;

--echo # Unblock 'lock tables t1 write'.
commit;

--echo # --> connection con1
connection con1;
--echo # Reap 'lock tables t1 write'.
--reap

--echo # --> connection default
connection default;
--echo # Sending:
--send handler t1 read a next

--echo # --> connection con1
connection con1;
--echo # Waiting for 'handler t1 read a next' to get blocked...
let $wait_condition=
  select count(*) = 1 from information_schema.processlist
  where state = "Waiting for table level lock" and
        info = "handler t1 read a next";
--source include/wait_condition.inc

--echo # The below 'drop table t1' should be able to proceed without
--echo # waiting as it will force HANDLER to be closed.
drop table t1;
unlock tables;

--echo # --> connection default 
connection default;
--echo # Reaping 'handler t1 read a next'...
--error ER_NO_SUCH_TABLE
--reap
handler t1 close;

--echo # --> connection con1
connection con1;
disconnect con1;
--source include/wait_until_disconnected.inc
--echo # --> connection con2
connection con2;
disconnect con2;
--source include/wait_until_disconnected.inc
--echo # --> connection con3
connection con3;
disconnect con3;
--source include/wait_until_disconnected.inc
connection default;

--echo #
--echo # A temporary table test.
--echo # Check that we don't loose positions of HANDLER opened
--echo # against a temporary table.
--echo #
create table t1 (a int, b int, key using btree (a));
insert into t1 (a) values (1), (2), (3), (4), (5);
create temporary table t2 (a int, b int, key using btree (a));
insert into t2 (a) select a from t1;
handler t1 open;
handler t1 read a next;
handler t2 open;
handler t2 read a next;
flush table t1;
handler t2 read a next;
--echo # Sic: the position is lost
handler t1 read a next;
select * from t1;
--echo # Sic: the position is not lost
handler t2 read a next;
--error ER_CANT_REOPEN_TABLE
select * from t2;
handler t2 read a next;
drop table t1;
drop temporary table t2;

--echo #
--echo # A test for lock_table_names()/unlock_table_names() function.
--echo # It should work properly in presence of open HANDLER.
--echo #
create table t1 (a int, b int, key a (a));
create table t2 like t1;
create table t3 like t1;
create table t4 like t1;
handler t1 open;
handler t2 open;
rename table t4 to t5, t3 to t4, t5 to t3;
handler t1 read first;
handler t2 read first;
drop table t1, t2, t3, t4;

--echo #
--echo # A test for FLUSH TABLES WITH READ LOCK and HANDLER statements.
--echo #
set autocommit=0;
create table t1 (a int, b int, key a (a));
insert into t1 (a, b) values (1, 1), (2, 1), (3, 2), (4, 2), (5, 5);
create table t2 like t1;
insert into t2 (a, b) select a, b from t1;
create table t3 like t1;
insert into t3 (a, b) select a, b from t1;
commit;
flush tables with read lock;
handler t1 open;
lock table t1 read;
--error ER_LOCK_OR_ACTIVE_TRANSACTION
handler t1 read next;
--echo # This implicitly leaves LOCK TABLES but doesn't drop the GLR
--error ER_NO_SUCH_TABLE
lock table not_exists_write read;
--echo # We still have the read lock.
--error ER_CANT_UPDATE_WITH_READLOCK
drop table t1;
handler t1 open;
select a from t2;
handler t1 read next;
flush tables with read lock;
handler t2 open;
flush tables with read lock;
handler t1 read next;
select a from t3;
handler t2 read next;
handler t1 close;
rollback;
handler t2 close;
--error ER_CANT_UPDATE_WITH_READLOCK
drop table t1;
commit;
flush tables;
--error ER_CANT_UPDATE_WITH_READLOCK
drop table t1;
unlock tables;
drop table t1;
set autocommit=default;
drop table t2, t3;

--echo # 
--echo # HANDLER statement and operation-type aware metadata locks.
--echo # Check that when we clone a ticket for HANDLER we downrade
--echo # the lock.
--echo #
--echo # Establish an auxiliary connection con1.
connect (con1,localhost,root,,);
--echo # -> connection default
connection default;
create table t1 (a int, b int, key using btree (a));
insert into t1 (a, b) values (1, 1), (2, 1), (3, 2), (4, 2), (5, 5);
begin;
insert into t1 (a, b) values (6, 6);
handler t1 open;
handler t1 read a last;
insert into t1 (a, b) values (7, 7);
handler t1 read a last;
commit;
--echo # -> connection con1
connection con1;
--echo # Demonstrate that the HANDLER doesn't hold MDL_SHARED_WRITE.
lock table t1 write;
unlock tables;
--echo # -> connection default
connection default;
handler t1 read a prev;
handler t1 close;
--echo # Cleanup.
drop table t1;
--echo # -> connection con1
connection con1;
disconnect con1;
--source include/wait_until_disconnected.inc
--echo # -> connection default
connection default;

--echo #
--echo # A test for Bug#50555 "handler commands crash server in
--echo # my_hash_first()". 
--echo #
--error ER_UNKNOWN_TABLE
handler no_such_table read no_such_index first;
--error ER_UNKNOWN_TABLE
handler no_such_table close;


--echo #
--echo # Bug#50907 Assertion `hash_tables->table->next == __null' on 
--echo #           HANDLER OPEN
--echo #

--disable_warnings
DROP TABLE IF EXISTS t1, t2;
--enable_warnings

CREATE TEMPORARY TABLE t1 (i INT);
CREATE TEMPORARY TABLE t2 (i INT);

# This used to trigger the assert
HANDLER t2 OPEN;

# This also used to trigger the assert
HANDLER t2 READ FIRST;

HANDLER t2 CLOSE;
DROP TABLE t1, t2;


--echo #
--echo # Bug#50912 Assertion `ticket->m_type >= mdl_request->type'
--echo #           failed on HANDLER + I_S
--echo #

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

CREATE TABLE t1 (id INT);
HANDLER t1 OPEN;

# This used to trigger the assert.
SELECT table_name, table_comment FROM information_schema.tables
  WHERE table_schema= 'test' AND table_name= 't1';

HANDLER t1 CLOSE;
DROP TABLE t1;


--echo #
--echo # Test for bug #50908 "Assertion `handler_tables_hash.records == 0'
--echo #                      failed in enter_locked_tables_mode".
--echo #
--disable_warnings
drop tables if exists t1, t2;
drop function if exists f1;
--enable_warnings
create table t1 (i int);
insert into t1 values (1), (2);
create table t2 (j int);
insert into t2 values (1);
create function f1() returns int return (select count(*) from t2);
--echo # Check that open HANDLER survives statement executed in
--echo # prelocked mode.
handler t1 open;
handler t1 read next;
--echo # The below statement were aborted due to an assertion failure.
select f1() from t2;
handler t1 read next;
handler t1 close;
--echo # Check that the same happens under GLOBAL READ LOCK.
flush tables with read lock;
handler t1 open;
handler t1 read next;
select f1() from t2;
handler t1 read next;
unlock tables;
handler t1 close;
--echo # Now, check that the same happens if LOCK TABLES is executed.
handler t1 open;
handler t1 read next;
lock table t2 read;
select * from t2;
unlock tables;
handler t1 read next;
handler t1 close;
--echo # Finally, check scenario with GRL and LOCK TABLES.
flush tables with read lock;
handler t1 open;
handler t1 read next;
lock table t2 read;
select * from t2;
--echo # This unlocks both tables and GRL.
unlock tables;
handler t1 read next;
handler t1 close;
--echo # Clean-up.
drop function f1;
drop tables t1, t2;


--echo #
--echo # Test for bug #51136 "Crash in pthread_rwlock_rdlock on TEMPORARY +
--echo #                      HANDLER + LOCK + SP".
--echo # Also see additional coverage for this bug in flush.test.
--echo #
--disable_warnings
drop tables if exists t1, t2;
--enable_warnings
create table t1 (i int);
create temporary table t2 (j int);
handler t1 open;
lock table t2 read;
--echo # This commit should not release any MDL locks.
commit;
unlock tables;
--echo # The below statement crashed before the bug fix as it
--echo # has attempted to release metadata lock which was
--echo # already released by commit.
handler t1 close;
drop tables t1, t2;


--echo #
--echo # Bug#51355 handler stmt cause assertion in
--echo #           bool MDL_context::try_acquire_lock(MDL_request*)
--echo #

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

connect(con51355, localhost, root);

--echo # Connection default
connection default;
CREATE TABLE t1(id INT, KEY id(id));
HANDLER t1 OPEN;

--echo # Connection con51355
connection con51355;
--echo # Sending:
--send DROP TABLE t1

--echo # Connection default
connection default;
--echo # This I_S query will cause the handler table to be closed and
--echo # the metadata lock to be released. This will allow DROP TABLE
--echo # to proceed. Waiting for the table to be removed.
let $wait_condition=
  SELECT COUNT(*) = 0 FROM information_schema.tables WHERE table_name = "t1";
--source include/wait_condition.inc

--echo # Connection con51355
connection con51355;
--echo # Reaping: DROP TABLE t1
--reap

--echo # Connection default
connection default;
--error ER_NO_SUCH_TABLE
HANDLER t1 READ id NEXT;
# This caused an assertion
--error ER_NO_SUCH_TABLE
HANDLER t1 READ id NEXT;

HANDLER t1 CLOSE;
--echo # Connection con51355
connection con51355;
disconnect con51355;
--source include/wait_until_disconnected.inc
--echo # Connection default
connection default;


--echo #
--echo # Bug#54401 assert in Diagnostics_area::set_eof_status , HANDLER
--echo #

--disable_warnings
DROP TABLE IF EXISTS t1, t2;
DROP FUNCTION IF EXISTS f1;
--enable_warnings

delimiter |;
CREATE FUNCTION f1() RETURNS INTEGER
BEGIN
  SELECT 1 FROM t2 INTO @a;
  RETURN 1;
END|
delimiter ;|

# Get f1() parsed and cached
--error ER_NO_SUCH_TABLE
SELECT f1();

CREATE TABLE t1(a INT);
INSERT INTO t1 VALUES (1);
HANDLER t1 OPEN;
# This used to cause the assert
--error ER_NOT_SUPPORTED_YET
HANDLER t1 READ FIRST WHERE f1() = 1;
HANDLER t1 CLOSE;

DROP FUNCTION f1;
DROP TABLE t1;

--echo #
--echo # Bug#54920 Stored functions are allowed in HANDLER statements,
--echo #           but broken.
--echo #

--disable_warnings
DROP TABLE IF EXISTS t1;
DROP FUNCTION IF EXISTS f1;
--enable_warnings

CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1), (2);
CREATE FUNCTION f1() RETURNS INT RETURN 1;
HANDLER t1 OPEN;

--error ER_NOT_SUPPORTED_YET
HANDLER t1 READ FIRST WHERE f1() = 1;

HANDLER t1 CLOSE;
DROP FUNCTION f1;
DROP TABLE t1;

--echo #
--echo # BUG#51877 - HANDLER interface causes invalid memory read
--echo #
CREATE TABLE t1(a INT, KEY using btree (a));
HANDLER t1 OPEN;
HANDLER t1 READ a FIRST;
INSERT INTO t1 VALUES(1);
--error 0,ER_CHECKREAD
HANDLER t1 READ a NEXT;
HANDLER t1 CLOSE;
DROP TABLE t1;
