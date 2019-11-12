#
# Additional coverage for refactoring which is made as part
# of fix for bug #27480 "Extend CREATE TEMPORARY TABLES privilege
# to allow temp table operations".
# 
# Check that for statements like CHECK/REPAIR and OPTIMIZE TABLE
# privileges for all tables involved are checked before processing
# any tables. Doing otherwise, i.e. checking privileges for table
# right before processing it might result in lost results for tables
# which were processed by the time when table for which privileges
# are insufficient are discovered.
#
call mtr.add_suppression("Got an error from thread_id=.*ha_myisam.cc:");
call mtr.add_suppression("MySQL thread id .*, query id .* localhost.*mysqltest_u1 Checking table");
drop database if exists mysqltest_db1;
create database mysqltest_db1;
# Create tables which we are going to CHECK/REPAIR.
create table mysqltest_db1.t1 (a int, key(a)) engine=myisam;
create table mysqltest_db1.t2 (b int);
insert into mysqltest_db1.t1 values (1), (2);
insert into mysqltest_db1.t2 values (1);
# Create user which will try to do this.
create user mysqltest_u1@localhost;
grant insert, select on mysqltest_db1.t1 to mysqltest_u1@localhost;
# Corrupt t1 by replacing t1.MYI with a corrupt + unclosed one created
# by doing: 'create table t1 (a int key(a))'
#           head -c1024 t1.MYI > corrupt_t1.MYI 
flush table mysqltest_db1.t1;
# Switching to connection 'con1'.
check table mysqltest_db1.t1;
Table	Op	Msg_type	Msg_text
mysqltest_db1.t1	check	warning	1 client is using or hasn't closed the table properly
mysqltest_db1.t1	check	error	Size of indexfile is: 1024        Should be: 2048
mysqltest_db1.t1	check	warning	Size of datafile is: 14       Should be: 7
mysqltest_db1.t1	check	error	Corrupt
# The below statement should fail before repairing t1.
# Otherwise info about such repair will be missing from its result-set.
repair table mysqltest_db1.t1, mysqltest_db1.t2;
ERROR 42000: SELECT, INSERT command denied to user 'mysqltest_u1'@'localhost' for table 't2'
# The same is true for CHECK TABLE statement.
check table mysqltest_db1.t1, mysqltest_db1.t2;
ERROR 42000: SELECT command denied to user 'mysqltest_u1'@'localhost' for table 't2'
check table mysqltest_db1.t1;
Table	Op	Msg_type	Msg_text
mysqltest_db1.t1	check	warning	Table is marked as crashed
mysqltest_db1.t1	check	warning	1 client is using or hasn't closed the table properly
mysqltest_db1.t1	check	error	Size of indexfile is: 1024        Should be: 2048
mysqltest_db1.t1	check	warning	Size of datafile is: 14       Should be: 7
mysqltest_db1.t1	check	error	Corrupt
repair table mysqltest_db1.t1;
Table	Op	Msg_type	Msg_text
mysqltest_db1.t1	repair	warning	Number of rows changed from 1 to 2
mysqltest_db1.t1	repair	status	OK
# Clean-up.
# Switching to connection 'default'.
drop database mysqltest_db1;
drop user mysqltest_u1@localhost;
