# Copyright (C) 2009 Sun Microsystems, Inc
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

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
## create table performance_schema.SETUP_INSTRUMENTS(a int);
##
## --error ER_DBACCESS_DENIED_ERROR
## create table performance_schema.EVENTS_WAITS_CURRENT(a int);
##
## --error ER_DBACCESS_DENIED_ERROR
## create table performance_schema.FILE_INSTANCES(a int);
##
## --error ER_DBACCESS_DENIED_ERROR
## drop table performance_schema.SETUP_INSTRUMENTS;
##
## --error ER_DBACCESS_DENIED_ERROR
## drop table performance_schema.EVENTS_WAITS_CURRENT;
##
## --error ER_DBACCESS_DENIED_ERROR
## drop table performance_schema.FILE_INSTANCES;

--error ER_DBACCESS_DENIED_ERROR
rename table performance_schema.SETUP_INSTRUMENTS to test.t1;

--error ER_DBACCESS_DENIED_ERROR
rename table performance_schema.EVENTS_WAITS_CURRENT to test.t1;

--error ER_DBACCESS_DENIED_ERROR
rename table performance_schema.FILE_INSTANCES to test.t1;

--error ER_DBACCESS_DENIED_ERROR
rename table performance_schema.SETUP_INSTRUMENTS to performance_schema.t1;

--error ER_DBACCESS_DENIED_ERROR
rename table performance_schema.EVENTS_WAITS_CURRENT to performance_schema.t1;

--error ER_DBACCESS_DENIED_ERROR
rename table performance_schema.FILE_INSTANCES to performance_schema.t1;

--error ER_DBACCESS_DENIED_ERROR
rename table performance_schema.SETUP_INSTRUMENTS
  to performance_schema.EVENTS_WAITS_CURRENT;

--error ER_DBACCESS_DENIED_ERROR
rename table performance_schema.EVENTS_WAITS_CURRENT
  to performance_schema.SETUP_INSTRUMENTS;

--error ER_DBACCESS_DENIED_ERROR
create procedure performance_schema.my_proc() begin end;

--error ER_DBACCESS_DENIED_ERROR
create function performance_schema.my_func() returns int return 0;

--error ER_DBACCESS_DENIED_ERROR
create event performance_schema.my_event on schedule every 15 minute
do begin end;

--error ER_DBACCESS_DENIED_ERROR
create trigger performance_schema.bi_setup_instruments
  before insert on performance_schema.SETUP_INSTRUMENTS
  for each row begin end;

--error ER_DBACCESS_DENIED_ERROR
create trigger performance_schema.bi_events_waits_current
  before insert on performance_schema.EVENTS_WAITS_CURRENT
  for each row begin end;

--error ER_DBACCESS_DENIED_ERROR
create trigger performance_schema.bi_file_instances
  before insert on performance_schema.FILE_INSTANCES
  for each row begin end;

--error ER_WRONG_PERFSCHEMA_USAGE
create table test.t1(a int) engine=PERFORMANCE_SCHEMA;

--error ER_WRONG_PERFSCHEMA_USAGE
create table test.t1 like performance_schema.SETUP_INSTRUMENTS;

--error ER_WRONG_PERFSCHEMA_USAGE
create table test.t1 like performance_schema.EVENTS_WAITS_CURRENT;

--error ER_WRONG_PERFSCHEMA_USAGE
create table test.t1 like performance_schema.FILE_INSTANCES;

--replace_result '\'setup_instruments' '\'SETUP_INSTRUMENTS'
--error ER_TABLEACCESS_DENIED_ERROR
insert into performance_schema.SETUP_INSTRUMENTS
  set name="foo";

--replace_result '\'events_waits_current' '\'EVENTS_WAITS_CURRENT'
--error ER_TABLEACCESS_DENIED_ERROR
insert into performance_schema.EVENTS_WAITS_CURRENT
  set name="foo";

--replace_result '\'file_instances' '\'FILE_INSTANCES'
--error ER_TABLEACCESS_DENIED_ERROR
insert into performance_schema.FILE_INSTANCES
  set name="foo";

--replace_result '\'setup_instruments' '\'SETUP_INSTRUMENTS'
--error ER_TABLEACCESS_DENIED_ERROR
delete from performance_schema.SETUP_INSTRUMENTS;

--replace_result '\'events_waits_current' '\'EVENTS_WAITS_CURRENT'
--error ER_TABLEACCESS_DENIED_ERROR
delete from performance_schema.EVENTS_WAITS_CURRENT;

--replace_result '\'file_instances' '\'FILE_INSTANCES'
--error ER_TABLEACCESS_DENIED_ERROR
delete from performance_schema.FILE_INSTANCES;

lock table performance_schema.SETUP_INSTRUMENTS read;
unlock tables;

lock table performance_schema.SETUP_INSTRUMENTS write;
unlock tables;

--replace_result '\'events_waits_current' '\'EVENTS_WAITS_CURRENT'
--error ER_TABLEACCESS_DENIED_ERROR
lock table performance_schema.EVENTS_WAITS_CURRENT read;
unlock tables;

--replace_result '\'events_waits_current' '\'EVENTS_WAITS_CURRENT'
--error ER_TABLEACCESS_DENIED_ERROR
lock table performance_schema.EVENTS_WAITS_CURRENT write;
unlock tables;

--replace_result '\'file_instances' '\'FILE_INSTANCES'
--error ER_TABLEACCESS_DENIED_ERROR
lock table performance_schema.FILE_INSTANCES read;
unlock tables;

--replace_result '\'file_instances' '\'FILE_INSTANCES'
--error ER_TABLEACCESS_DENIED_ERROR
lock table performance_schema.FILE_INSTANCES write;
unlock tables;

--echo #
--echo # WL#4818, NFS2: Can use grants to give normal user access
--echo #                to view data from _CURRENT and _HISTORY tables
--echo #
--echo # Should work as pfs_user_1 and pfs_user_2, but not as pfs_user_3.
--echo # (Except for EVENTS_WAITS_CURRENT, which is granted.)

# Errors here will be caught by the diff afterwards
--disable_abort_on_error

--replace_result '\'events_waits_history' '\'EVENTS_WAITS_HISTORY'
SELECT "can select" FROM performance_schema.EVENTS_WAITS_HISTORY LIMIT 1;

--replace_result '\'events_waits_history_long' '\'EVENTS_WAITS_HISTORY_LONG'
SELECT "can select" FROM performance_schema.EVENTS_WAITS_HISTORY_LONG LIMIT 1;

--replace_result '\'events_waits_current' '\'EVENTS_WAITS_CURRENT'
SELECT "can select" FROM performance_schema.EVENTS_WAITS_CURRENT LIMIT 1;

--replace_result '\'events_waits_summary_by_instance' '\'EVENTS_WAITS_SUMMARY_BY_INSTANCE'
SELECT "can select" FROM performance_schema.EVENTS_WAITS_SUMMARY_BY_INSTANCE LIMIT 1;

--replace_result '\'file_summary_by_instance' '\'FILE_SUMMARY_BY_INSTANCE'
SELECT "can select" FROM performance_schema.FILE_SUMMARY_BY_INSTANCE LIMIT 1;

--enable_abort_on_error

