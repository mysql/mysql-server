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

# Verify how table io is aggregated into various tables
#
# In the instance dimension:
# - table_io_waits_summary_by_index_usage
# - table_io_waits_summary_by_table
# - table_lock_waits_summary_by_table
# - objects_summary_global_by_type
#
# In the thread dimension:
# - events_waits_summary_by_thread_by_event_name
#
# Globally:
# - events_waits_summary_global_by_event_name
#
# The tests are written with the following helpers:
# - include/table_aggregate_setup.inc
# - include/table_aggregate_load.inc
# - include/table_aggregate_cleanup.inc
#
# Helpers are intended to be used as follows.
#
# A Typical test t/table_aggregate_xxx.test will consist of:
# --source ../include/table_aggregate_setup.inc
# 
# ... statements to modify the default configuration ...
#
# --source ../include/table_aggregate_load.inc
# --source ../include/table_aggregate_cleanup.inc
#
# Naming conventions for t/table_aggregate_xxx.test are as follows:
# t/<instrument>aggregate_<mode><actors><objects>
#
# <instrument> corresponds to different instruments settings
# - table: both table io and table lock are instrumented
# - table_io: only table io is instrumented
# - table_lock: only table lock is instrumented
#
# <mode> corresponds to different consumers settings
# - off: global_instrumentation OFF
# - global: global_instrumentation ON, thread_instrumentation OFF
# - thread: global_instrumentation ON, thread_instrumentation ON,
#           events_* consumers OFF
# - history: global_instrumentation ON, thread_instrumentation ON,
#           events_* consumers ON
#
# <actors> corresponds to different setup_actors settings
# - 4u: every test user (user1, user2, user3, user4) is ON
# - 2u: (user1, user3) are ON, (user2, user4) are OFF
#
# <objects> corresponds to different setup_objects settings
# - 3t: tables t1, t2 and t3 are ON
# - 2t: tables t1 and t3 are ON, table t2 is OFF
#

# ========================================
# HELPER include/table_aggregate_setup.inc
# ========================================

--source include/not_embedded.inc
--source include/have_perfschema.inc

--disable_query_log
grant ALL on *.* to user1@localhost;
grant ALL on *.* to user2@localhost;
grant ALL on *.* to user3@localhost;
grant ALL on *.* to user4@localhost;

flush privileges;

# Save the setup

--disable_warnings
drop table if exists test.setup_actors;
drop table if exists test.setup_objects;
--enable_warnings

create table test.setup_actors as
  select * from performance_schema.setup_actors;

create table test.setup_objects as
  select * from performance_schema.setup_objects;

# Only instrument the user connections (by default)
truncate table performance_schema.setup_actors;
insert into performance_schema.setup_actors
  set host= 'localhost', user= 'user1', role= '%';
insert into performance_schema.setup_actors
  set host= 'localhost', user= 'user2', role= '%';
insert into performance_schema.setup_actors
  set host= 'localhost', user= 'user3', role= '%';
insert into performance_schema.setup_actors
  set host= 'localhost', user= 'user4', role= '%';

# Only instrument test.t% tables (by default)
truncate table performance_schema.setup_objects;
insert into performance_schema.setup_objects
  set object_type='TABLE', object_schema='test', object_name='t1', timed='YES';
insert into performance_schema.setup_objects
  set object_type='TABLE', object_schema='test', object_name='t2', timed='NO';
insert into performance_schema.setup_objects
  set object_type='TABLE', object_schema='test', object_name='t3', timed='NO';

update performance_schema.threads set instrumented='NO';

# Only instrument table io and lock (by default)
update performance_schema.setup_instruments set enabled='NO', timed='NO';
update performance_schema.setup_instruments set enabled='YES', timed='YES'
  where name in ('wait/io/table/sql/handler',
                 'wait/lock/table/sql/handler');

# Enable all consumers (by default)
update performance_schema.setup_consumers set enabled='YES';

create table test.t1(a int, b int, c int, d int default 0,
  primary key(a),
  index index_b(b),
  index index_cb(c, b));
create table test.t2 like test.t1;
create table test.t3 like test.t1;

prepare dump_thread from
  "select event_name, count_star
  from performance_schema.events_waits_summary_by_thread_by_event_name
  where event_name in
  (\'wait/io/table/sql/handler\',
   \'wait/lock/table/sql/handler\')
  and thread_id in
  (select thread_id from performance_schema.threads
   where processlist_user like \'user%\')
  order by thread_id, event_name;";

prepare dump_global from
  "select event_name, count_star
  from performance_schema.events_waits_summary_global_by_event_name
  where event_name in
  (\'wait/io/table/sql/handler\',
   \'wait/lock/table/sql/handler\')
  order by event_name;";

prepare dump_history from
  "select event_name, count(event_name), object_type, object_schema, object_name
  from performance_schema.events_waits_history_long
  where event_name in
  (\'wait/io/table/sql/handler\',
   \'wait/lock/table/sql/handler\')
  group by object_type, object_schema, object_name, event_name
  order by object_type, object_schema, object_name, event_name;";

prepare dump_index_io from
  "select object_type, object_schema, object_name, index_name,
  count_star, count_read, count_write,
  count_fetch, count_insert, count_update, count_delete
  from performance_schema.table_io_waits_summary_by_index_usage
  where object_type='TABLE' and object_schema='test' and object_name like 't%'
  order by object_type, object_schema, object_name, index_name;";

prepare dump_table_io from
  "select object_type, object_schema, object_name,
  count_star, count_read, count_write,
  count_fetch, count_insert, count_update, count_delete
  from performance_schema.table_io_waits_summary_by_table
  where object_type='TABLE' and object_schema='test' and object_name like 't%'
  order by object_type, object_schema, object_name";

prepare dump_table_lock from
  "select object_type, object_schema, object_name,
  count_star, count_read, count_write,
  count_read_normal, count_read_with_shared_locks,
  count_read_high_priority, count_read_no_insert,
  count_read_external,
  count_write_delayed, count_write_low_priority,
  count_write_external
  from performance_schema.table_lock_waits_summary_by_table
  where object_type='TABLE' and object_schema='test' and object_name like 't%'
  order by object_type, object_schema, object_name";

prepare dump_objects_summary from
  "select object_type, object_schema, object_name, count_star 
  from performance_schema.objects_summary_global_by_type
  where object_type='TABLE' and object_schema='test' and object_name like 't%'
  order by object_type, object_schema, object_name";

--enable_query_log
