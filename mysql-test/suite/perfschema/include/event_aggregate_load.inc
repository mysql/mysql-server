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

# =======================================
# HELPER include/event_aggregate_load.inc
# =======================================

echo "================== Step 1 ==================";
execute dump_thread;
execute dump_global;
execute dump_history;

# Notes about this test
#
# Each connect causes 2 wait/synch/mutex/sql/LOCK_connection_count events:
# - 1 in mysqld.cc, create_new_thread(), for the main thread
# - 1 in sql_connect.cc, check_user(), for the connected thread
# The main thread does not count for BY_USER_HOST / BY_HOST.
# The user thread does count for BY_USER_HOST, BY_HOST
#
# Each get_lock() causes 1 wait/synch/mutex/sql/LOCK_user_locks
#
# To avoid noise from main, the background threads are disabled.

connect (con1, localhost, user1, , );

echo "================== con1 connected ==================";

--connection default

echo "================== Step 2 ==================";
execute dump_thread;
execute dump_global;
execute dump_history;

--connection con1

select get_lock("marker", 10);
select release_lock("marker");
insert into test.t1 values ("marker");

echo "================== con1 marker ==================";

--connection default

echo "================== Step 3 ==================";
execute dump_thread;
execute dump_global;
execute dump_history;

# Debugging helpers
# select * from performance_schema.events_waits_history_long;
# select PROCESSLIST_USER, PROCESSLIST_HOST, INSTRUMENTED from performance_schema.threads;

connect (con2, localhost, user2, , );

echo "================== con2 connected ==================";

--connection default

echo "================== Step 4 ==================";
execute dump_thread;
execute dump_global;
execute dump_history;

--connection con2

select get_lock("marker", 10);
select release_lock("marker");
insert into test.t1 values ("marker");

echo "================== con2 marker ==================";

--connection default

echo "================== Step 5 ==================";
execute dump_thread;
execute dump_global;
execute dump_history;

connect (con3, localhost, user3, , );

echo "================== con3 connected ==================";

--connection default

echo "================== Step 6 ==================";
execute dump_thread;
execute dump_global;
execute dump_history;

--connection con3

select get_lock("marker", 10);
select release_lock("marker");
insert into test.t1 values ("marker");

echo "================== con3 marker ==================";

--connection default

echo "================== Step 7 ==================";
execute dump_thread;
execute dump_global;
execute dump_history;

connect (con4, localhost, user4, , );

echo "================== con4 connected ==================";

--connection default

echo "================== Step 8 ==================";
execute dump_thread;
execute dump_global;
execute dump_history;

--connection con4

select get_lock("marker", 10);
select release_lock("marker");
insert into test.t1 values ("marker");

echo "================== con4 marker ==================";

--connection default

echo "================== Step 9 ==================";
execute dump_thread;
execute dump_global;
execute dump_history;

--disconnect con1

# Wait for the disconnect to complete
let $wait_condition=
  select count(*) = 3 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER like 'user%';
--source include/wait_condition.inc

echo "================== con1 disconnected ==================";

echo "================== Step 10 ==================";
execute dump_thread;
execute dump_global;
execute dump_history;

--disconnect con2

# Wait for the disconnect to complete
let $wait_condition=
  select count(*) = 2 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER like 'user%';
--source include/wait_condition.inc

echo "================== con2 disconnected ==================";

echo "================== Step 11 ==================";
execute dump_thread;
execute dump_global;
execute dump_history;

--disconnect con3

# Wait for the disconnect to complete
let $wait_condition=
  select count(*) = 1 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER like 'user%';
--source include/wait_condition.inc

echo "================== con3 disconnected ==================";

echo "================== Step 12 ==================";
execute dump_thread;
execute dump_global;
execute dump_history;

--disconnect con4

# Wait for the disconnect to complete
let $wait_condition=
  select count(*) = 0 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER like 'user%';
--source include/wait_condition.inc

echo "================== con4 disconnected ==================";

echo "================== Step 13 ==================";
execute dump_thread;
execute dump_global;
execute dump_history;

--connection default

truncate performance_schema.events_waits_summary_by_thread_by_event_name;

echo "================== BY_THREAD truncated ==================";

echo "================== Step 14 ==================";
execute dump_thread;
execute dump_global;
execute dump_history;

truncate performance_schema.events_waits_summary_global_by_event_name;

echo "================== GLOBAL truncated ==================";

echo "================== Step 18 ==================";
execute dump_thread;
execute dump_global;
execute dump_history;

