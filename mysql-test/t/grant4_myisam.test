--source include/force_myisam_default.inc
--source include/have_myisam.inc

--echo #
--echo # Additional coverage for refactoring which is made as part
--echo # of fix for bug #27480 "Extend CREATE TEMPORARY TABLES privilege
--echo # to allow temp table operations".
--echo # 
--echo # Check that for statements like CHECK/REPAIR and OPTIMIZE TABLE
--echo # privileges for all tables involved are checked before processing
--echo # any tables. Doing otherwise, i.e. checking privileges for table
--echo # right before processing it might result in lost results for tables
--echo # which were processed by the time when table for which privileges
--echo # are insufficient are discovered.
--echo #
call mtr.add_suppression("Got an error from thread_id=.*ha_myisam.cc:");
call mtr.add_suppression("MySQL thread id .*, query id .* localhost.*mysqltest_u1 Checking table");
--disable_warnings
drop database if exists mysqltest_db1;
--enable_warnings
let $MYSQLD_DATADIR = `SELECT @@datadir`;
create database mysqltest_db1;
--echo # Create tables which we are going to CHECK/REPAIR.
create table mysqltest_db1.t1 (a int, key(a)) engine=myisam;
create table mysqltest_db1.t2 (b int);
insert into mysqltest_db1.t1 values (1), (2);
insert into mysqltest_db1.t2 values (1);
--echo # Create user which will try to do this.
create user mysqltest_u1@localhost;
grant insert, select on mysqltest_db1.t1 to mysqltest_u1@localhost;
connect (con1,localhost,mysqltest_u1,,);
connection default;

--echo # Corrupt t1 by replacing t1.MYI with a corrupt + unclosed one created
--echo # by doing: 'create table t1 (a int key(a))'
--echo #           head -c1024 t1.MYI > corrupt_t1.MYI 
flush table mysqltest_db1.t1;
--remove_file $MYSQLD_DATADIR/mysqltest_db1/t1.MYI
--copy_file std_data/corrupt_t1.MYI $MYSQLD_DATADIR/mysqltest_db1/t1.MYI

--echo # Switching to connection 'con1'.
connection con1;
check table mysqltest_db1.t1;
--echo # The below statement should fail before repairing t1.
--echo # Otherwise info about such repair will be missing from its result-set.
--error ER_TABLEACCESS_DENIED_ERROR
repair table mysqltest_db1.t1, mysqltest_db1.t2;
--echo # The same is true for CHECK TABLE statement.
--error ER_TABLEACCESS_DENIED_ERROR
check table mysqltest_db1.t1, mysqltest_db1.t2;
check table mysqltest_db1.t1;
repair table mysqltest_db1.t1;

--echo # Clean-up.
disconnect con1;
--source include/wait_until_disconnected.inc
--echo # Switching to connection 'default'.
connection default;
drop database mysqltest_db1;
drop user mysqltest_u1@localhost;
