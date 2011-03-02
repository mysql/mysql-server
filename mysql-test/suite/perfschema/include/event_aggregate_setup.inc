# Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
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

# Tests for the performance schema

# =============
# DOCUMENTATION
# =============

# Verify how events are aggregated into various tables
#
# In the thread dimension:
# - events_waits_summary_by_thread_by_event_name
#
# Globally:
# - events_waits_summary_global_by_event_name
#
# The tests are written with the following helpers:
# - include/event_aggregate_setup.inc
# - include/event_aggregate_load.inc
# - include/event_aggregate_cleanup.inc
#
# Helpers are intended to be used as follows.
#
# A Typical test t/event_aggregate_xxx.test will consist of:
# --source ../include/event_aggregate_setup.inc
# --source ../include/event_aggregate_load.inc
# --source ../include/event_aggregate_cleanup.inc
# and a t/event_aggregate_xxx-master.opt file
#
# ========================================
# HELPER include/event_aggregate_setup.inc
# ========================================

--source include/not_embedded.inc
--source include/have_perfschema.inc

# The file with expected results fits only to a run without
# ps-protocol/sp-protocol/cursor-protocol/view-protocol.
if (`SELECT $PS_PROTOCOL + $SP_PROTOCOL + $CURSOR_PROTOCOL
            + $VIEW_PROTOCOL > 0`)
{
   --skip Test requires: ps-protocol/sp-protocol/cursor-protocol/view-protocol disabled
}

--disable_query_log

grant ALL on *.* to user1@localhost;
grant ALL on *.* to user2@localhost;
grant ALL on *.* to user3@localhost;
grant ALL on *.* to user4@localhost;

flush privileges;

# Save the setup

--disable_warnings
drop table if exists test.setup_actors;
drop table if exists test.t1;
--enable_warnings

create table test.t1(a varchar(64));

create table test.setup_actors as
  select * from performance_schema.setup_actors;

# Only instrument the user connections
truncate table performance_schema.setup_actors;
insert into performance_schema.setup_actors
  set host= 'localhost', user= 'user1', role= '%';
insert into performance_schema.setup_actors
  set host= 'localhost', user= 'user2', role= '%';
insert into performance_schema.setup_actors
  set host= 'localhost', user= 'user3', role= '%';
insert into performance_schema.setup_actors
  set host= 'localhost', user= 'user4', role= '%';

update performance_schema.threads set instrumented='NO';

# Only instrument a few events of each kind
update performance_schema.setup_instruments set enabled='NO', timed='NO';
update performance_schema.setup_instruments set enabled='YES', timed='YES'
  where name in ('wait/synch/mutex/sql/LOCK_connection_count',
                 'wait/synch/mutex/sql/LOCK_user_locks',
                 'wait/synch/rwlock/sql/LOCK_grant',
                 'wait/io/file/sql/query_log');

truncate performance_schema.events_waits_summary_by_thread_by_event_name;
truncate performance_schema.events_waits_summary_global_by_event_name;
truncate performance_schema.events_waits_history_long;

prepare dump_thread from
  "select event_name, count_star
  from performance_schema.events_waits_summary_by_thread_by_event_name
  where event_name in ('wait/synch/mutex/sql/LOCK_connection_count',
                       'wait/synch/mutex/sql/LOCK_user_locks',
                       'wait/synch/rwlock/sql/LOCK_grant',
                       'wait/io/file/sql/query_log')
  and thread_id in
  (select thread_id from performance_schema.threads
   where processlist_user like \'user%\')
  order by thread_id, event_name;";

prepare dump_global from
  "select event_name, count_star
  from performance_schema.events_waits_summary_global_by_event_name
  where event_name in ('wait/synch/mutex/sql/LOCK_connection_count',
                       'wait/synch/mutex/sql/LOCK_user_locks',
                       'wait/synch/rwlock/sql/LOCK_grant',
                       'wait/io/file/sql/query_log')
  order by event_name;";

prepare dump_history from
  "select event_name, count(event_name)
  from performance_schema.events_waits_history_long
  where event_name in ('wait/synch/mutex/sql/LOCK_connection_count',
                       'wait/synch/mutex/sql/LOCK_user_locks',
                       'wait/synch/rwlock/sql/LOCK_grant',
                       'wait/io/file/sql/query_log')
  group by event_name order by event_name;";

--enable_query_log
