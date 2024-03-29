use performance_schema;
set @start_read_only= @@global.read_only;
set @start_super_read_only= @@global.super_read_only;
create user pfsuser@localhost;
grant SELECT, UPDATE on performance_schema.* to pfsuser@localhost;
flush privileges;
connect  con1, localhost, pfsuser, , test;
connection default;
set global read_only=0;
connection con1;
select @@global.read_only;
@@global.read_only
0
show grants;
Grants for pfsuser@localhost
GRANT USAGE ON *.* TO `pfsuser`@`localhost`
GRANT SELECT, UPDATE ON `performance_schema`.* TO `pfsuser`@`localhost`
select * from performance_schema.setup_instruments;
update performance_schema.setup_instruments set enabled='NO';
update performance_schema.setup_instruments set enabled='YES';
connection default;
set global read_only=1;
connection con1;
select @@global.read_only;
@@global.read_only
1
show grants;
Grants for pfsuser@localhost
GRANT USAGE ON *.* TO `pfsuser`@`localhost`
GRANT SELECT, UPDATE ON `performance_schema`.* TO `pfsuser`@`localhost`
# Update on perf_schema is allowed in read_only mode.
select * from performance_schema.setup_instruments;
update performance_schema.setup_instruments set enabled='NO';
update performance_schema.setup_instruments set enabled='YES';
connection default;
grant super on *.* to pfsuser@localhost;
Warnings:
Warning	1287	The SUPER privilege identifier is deprecated
flush privileges;
disconnect con1;
connect  con1, localhost, pfsuser, , test;
select @@global.read_only;
@@global.read_only
1
show grants;
Grants for pfsuser@localhost
GRANT SUPER ON *.* TO `pfsuser`@`localhost`
GRANT SELECT, UPDATE ON `performance_schema`.* TO `pfsuser`@`localhost`
select * from performance_schema.setup_instruments;
update performance_schema.setup_instruments set enabled='NO';
update performance_schema.setup_instruments set enabled='YES';
connection default;
set global super_read_only=1;
connection con1;
select @@global.super_read_only;
@@global.super_read_only
1
show grants;
Grants for pfsuser@localhost
GRANT SUPER ON *.* TO `pfsuser`@`localhost`
GRANT SELECT, UPDATE ON `performance_schema`.* TO `pfsuser`@`localhost`
select * from performance_schema.setup_instruments;
# Update is allowed in super_read_only on perf schema for
# super user.
update performance_schema.setup_instruments set enabled='NO';
update performance_schema.setup_instruments set enabled='YES';
#
# Bug#31080309 - REGRESSION FOR BUG 81009 FIXED IN 5.7.17
#   Bug23103937(BUG81009) - PS_TRUNCATE_ALL_TABLES() DOES NOT WORK IN
#                           SUPER_READ_ONLY MODE.
#
connection default;
# Reset read_only and super_read_only mode.
set global read_only=0;
set global super_read_only=0;
# Grant truncate table and execute stored procedure privileges to pfsuser.
grant DROP on performance_schema.* to pfsuser@localhost;
grant EXECUTE on procedure sys.ps_truncate_all_tables to pfsuser@localhost;
#-----------------------------------------------------------------------
# Test case to verify truncate operation on performance schema table by
# super user with super_read_only=ON.
#-----------------------------------------------------------------------
# Set super_read_only mode.
set global super_read_only=1;
connection con1;
# truncate operation is allowed in super_read_only mode on performance schema
# tables for super user.
# Without fix following statements fail because of super read only mode.
truncate table performance_schema.events_statements_summary_by_digest;
call sys.ps_truncate_all_tables(0);
#-----------------------------------------------------------------------
# Test case to verify truncate operation on performance schema table by
# super user with read_only=ON.
#-----------------------------------------------------------------------
connection default;
# Reset super_read_only mode.
set global super_read_only=0;
# Set read_only mode.
set global read_only=1;
connection con1;
# truncate operation is allowed in read_only mode on performance schema
# tables for super user.
# Without fix following statements fail because of super read only mode.
truncate table performance_schema.events_statements_summary_by_digest;
call sys.ps_truncate_all_tables(0);
#-----------------------------------------------------------------------
# Test case to verify truncate operation on performance schema table by
# non-super user with super_read_only=ON.
#-----------------------------------------------------------------------
connection default;
set global super_read_only=0;
# Revoke SUPER privilege from pfsuser.
revoke SUPER on *.* from pfsuser@localhost;
Warnings:
Warning	1287	The SUPER privilege identifier is deprecated
# Reset super_read_only mode.
set global super_read_only=1;
connection con1;
# truncate operation is allowed in super_read_only mode on performance schema
# tables for non-super user.
truncate table performance_schema.events_statements_summary_by_digest;
call sys.ps_truncate_all_tables(0);
#-----------------------------------------------------------------------
# Test case to verify truncate operation on performance schema table by
# non-super user with read_only=ON.
#-----------------------------------------------------------------------
connection default;
# Reset super_read_only mode.
set global super_read_only=0;
# Set read_only mode.
set global read_only=1;
connection con1;
# truncate operation is allowed in read_only mode on performance schema
# tables for non-super user.
truncate table performance_schema.events_statements_summary_by_digest;
call sys.ps_truncate_all_tables(0);
########################################################################
disconnect con1;
connection default;
set global read_only= @start_read_only;
set global super_read_only= @start_super_read_only;
drop user pfsuser@localhost;
flush privileges;
