# Tests for PERFORMANCE_SCHEMA

--disable_warnings
drop table if exists test.t1;
--enable_warnings

## The result of show grants is not consistent across platforms ...
## show grants;

## Not enforced yet: deny CREATE_ACL and DROP_ACL
## Waiting to remove .FRM files first
##
## --error ER_DBACCESS_DENIED_ERROR
## create table performance_schema.t1(a int);
##
## --error ER_DBACCESS_DENIED_ERROR
## drop table performance_schema.t1;
##
## --error ER_DBACCESS_DENIED_ERROR
## create table performance_schema.setup_instruments(a int);
##
## --error ER_DBACCESS_DENIED_ERROR
## create table performance_schema.events_waits_current(a int);
##
## --error ER_DBACCESS_DENIED_ERROR
## create table performance_schema.file_instances(a int);
##
## --error ER_DBACCESS_DENIED_ERROR
## drop table performance_schema.setup_instruments;
##
## --error ER_DBACCESS_DENIED_ERROR
## drop table performance_schema.events_waits_current;
##
## --error ER_DBACCESS_DENIED_ERROR
## drop table performance_schema.file_instances;

--error ER_DBACCESS_DENIED_ERROR
rename table performance_schema.setup_instruments to test.t1;

--error ER_DBACCESS_DENIED_ERROR
rename table performance_schema.events_waits_current to test.t1;

--error ER_DBACCESS_DENIED_ERROR
rename table performance_schema.file_instances to test.t1;

--error ER_DBACCESS_DENIED_ERROR
rename table performance_schema.setup_instruments to performance_schema.t1;

--error ER_DBACCESS_DENIED_ERROR
rename table performance_schema.events_waits_current to performance_schema.t1;

--error ER_DBACCESS_DENIED_ERROR
rename table performance_schema.file_instances to performance_schema.t1;

--error ER_DBACCESS_DENIED_ERROR
rename table performance_schema.setup_instruments
  to performance_schema.events_waits_current;

--error ER_DBACCESS_DENIED_ERROR
rename table performance_schema.events_waits_current
  to performance_schema.setup_instruments;

--error ER_DBACCESS_DENIED_ERROR
create procedure performance_schema.my_proc() begin end;

--error ER_DBACCESS_DENIED_ERROR
create function performance_schema.my_func() returns int return 0;

--error ER_DBACCESS_DENIED_ERROR
create event performance_schema.my_event on schedule every 15 minute
do begin end;

--error ER_DBACCESS_DENIED_ERROR
create trigger performance_schema.bi_setup_instruments
  before insert on performance_schema.setup_instruments
  for each row begin end;

--error ER_DBACCESS_DENIED_ERROR
create trigger performance_schema.bi_events_waits_current
  before insert on performance_schema.events_waits_current
  for each row begin end;

--error ER_DBACCESS_DENIED_ERROR
create trigger performance_schema.bi_file_instances
  before insert on performance_schema.file_instances
  for each row begin end;

--error ER_CANT_CREATE_TABLE
create table test.t1(a int) engine=PERFORMANCE_SCHEMA;

--error ER_CANT_CREATE_TABLE
create table test.t1 like performance_schema.setup_instruments;

--error ER_CANT_CREATE_TABLE
create table test.t1 like performance_schema.events_waits_current;

--error ER_CANT_CREATE_TABLE
create table test.t1 like performance_schema.file_instances;

--error ER_TABLEACCESS_DENIED_ERROR
insert into performance_schema.setup_instruments
  set name="foo";

--error ER_TABLEACCESS_DENIED_ERROR
insert into performance_schema.events_waits_current
  set name="foo";

--error ER_TABLEACCESS_DENIED_ERROR
insert into performance_schema.file_instances
  set name="foo";

--error ER_TABLEACCESS_DENIED_ERROR
delete from performance_schema.setup_instruments;

--error ER_TABLEACCESS_DENIED_ERROR
delete from performance_schema.events_waits_current;

--error ER_TABLEACCESS_DENIED_ERROR
delete from performance_schema.file_instances;

lock table performance_schema.setup_instruments read;
unlock tables;

lock table performance_schema.setup_instruments write;
unlock tables;

--error ER_TABLEACCESS_DENIED_ERROR
lock table performance_schema.events_waits_current read;
unlock tables;

--error ER_TABLEACCESS_DENIED_ERROR
lock table performance_schema.events_waits_current write;
unlock tables;

--error ER_TABLEACCESS_DENIED_ERROR
lock table performance_schema.file_instances read;
unlock tables;

--error ER_TABLEACCESS_DENIED_ERROR
lock table performance_schema.file_instances write;
unlock tables;

--echo #
--echo # WL#4818, NFS2: Can use grants to give normal user access
--echo #                to view data from _current and _history tables
--echo #
--echo # Should work as pfs_user_1 and pfs_user_2, but not as pfs_user_3.
--echo # (Except for events_waits_current, which is granted.)

# Errors here will be caught by the diff afterwards
--disable_abort_on_error

SELECT "can select" FROM performance_schema.events_waits_history LIMIT 1;

SELECT "can select" FROM performance_schema.events_waits_history_long LIMIT 1;

SELECT "can select" FROM performance_schema.events_waits_current LIMIT 1;

SELECT "can select" FROM performance_schema.events_waits_summary_by_instance LIMIT 1;

SELECT "can select" FROM performance_schema.file_summary_by_instance LIMIT 1;

--enable_abort_on_error

