# Tests for the performance schema

# =======================================
# HELPER include/event_aggregate_load.inc
# =======================================

echo "================== Step 1 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

# Notes about this test
#
# Each connect causes 2 wait/synch/mutex/sql/LOCK_connection_count events:
# - 1 in mysqld.cc, create_new_thread(), for the main thread
# - 1 in sql_connect.cc, check_user(), for the connected thread
# The main thread does not count for BY_ACCOUNT / BY_HOST.
# The user thread does count for BY_ACCOUNT, BY_HOST
#
# Each get_lock() causes 1 wait/synch/mutex/sql/LOCK_user_locks
#
# To avoid noise from main, the background threads are disabled.

connect (con1, localhost, user1, , );

echo "================== con1 connected ==================";

--connection default

# Wait for the connect to complete
let $wait_condition=
  select count(*) = 1 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER= 'user1';
--source include/wait_condition.inc

echo "================== Step 2 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

--connection con1

select get_lock("marker", 10);
select release_lock("marker");
insert into test.t1 values ("marker");

echo "================== con1 marker ==================";

--connection default

# Wait for the payload to complete
let $wait_condition=
  select count(*) = 1 from performance_schema.events_waits_current
  where EVENT_NAME= 'idle';
--source include/wait_condition.inc

echo "================== Step 3 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

# Debugging helpers
# select * from performance_schema.events_waits_history_long;
# select PROCESSLIST_USER, PROCESSLIST_HOST, INSTRUMENTED from performance_schema.threads;

connect (con2, localhost, user2, , );

echo "================== con2 connected ==================";

--connection default

# Wait for the connect to complete
let $wait_condition=
  select count(*) = 1 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER= 'user2';
--source include/wait_condition.inc

echo "================== Step 4 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

--connection con2

select get_lock("marker", 10);
select release_lock("marker");
insert into test.t1 values ("marker");

echo "================== con2 marker ==================";

--connection default

# Wait for the payload to complete
let $wait_condition=
  select count(*) = 2 from performance_schema.events_waits_current
  where EVENT_NAME= 'idle';
--source include/wait_condition.inc

echo "================== Step 5 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

connect (con3, localhost, user3, , );

echo "================== con3 connected ==================";

--connection default

# Wait for the connect to complete
let $wait_condition=
  select count(*) = 1 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER= 'user3';
--source include/wait_condition.inc

echo "================== Step 6 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

--connection con3

select get_lock("marker", 10);
select release_lock("marker");
insert into test.t1 values ("marker");

echo "================== con3 marker ==================";

--connection default

# Wait for the payload to complete
let $wait_condition=
  select count(*) = 3 from performance_schema.events_waits_current
  where EVENT_NAME= 'idle';
--source include/wait_condition.inc

echo "================== Step 7 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

connect (con4, localhost, user4, , );

echo "================== con4 connected ==================";

--connection default

# Wait for the connect to complete
let $wait_condition=
  select count(*) = 1 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER= 'user4';
--source include/wait_condition.inc

echo "================== Step 8 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

--connection con4

select get_lock("marker", 10);
select release_lock("marker");
insert into test.t1 values ("marker");

echo "================== con4 marker ==================";

--connection default

# Wait for the payload to complete
let $wait_condition=
  select count(*) = 4 from performance_schema.events_waits_current
  where EVENT_NAME= 'idle';
--source include/wait_condition.inc

echo "================== Step 9 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

--disconnect con1

--connection default

# Wait for the disconnect to complete
let $wait_condition=
  select count(*) = 0 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER= 'user1';
--source include/wait_condition.inc

echo "================== con1 disconnected ==================";

echo "================== Step 10 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

--disconnect con2

--connection default

# Wait for the disconnect to complete
let $wait_condition=
  select count(*) = 0 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER= 'user2';
--source include/wait_condition.inc

echo "================== con2 disconnected ==================";

echo "================== Step 11 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

--disconnect con3

--connection default

# Wait for the disconnect to complete
let $wait_condition=
  select count(*) = 0 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER= 'user3';
--source include/wait_condition.inc

echo "================== con3 disconnected ==================";

echo "================== Step 12 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

--disconnect con4

--connection default

# Wait for the disconnect to complete
let $wait_condition=
  select count(*) = 0 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER= 'user4';
--source include/wait_condition.inc

echo "================== con4 disconnected ==================";

echo "================== Step 13 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

--connection default

truncate performance_schema.events_waits_summary_by_thread_by_event_name;

echo "================== WAITS_BY_THREAD truncated ==================";

echo "================== Step 14 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

truncate performance_schema.events_waits_summary_by_account_by_event_name;

echo "================== WAITS_BY_ACCOUNT truncated ==================";

echo "================== Step 15 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

truncate performance_schema.events_waits_summary_by_user_by_event_name;

echo "================== WAITS_BY_USER truncated ==================";

echo "================== Step 16 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

truncate performance_schema.events_waits_summary_by_host_by_event_name;

echo "================== WAITS_BY_HOST truncated ==================";

echo "================== Step 17 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

truncate performance_schema.events_waits_summary_global_by_event_name;

echo "================== WAITS_GLOBAL truncated ==================";

echo "================== Step 18 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

truncate performance_schema.events_stages_summary_by_thread_by_event_name;

echo "================== STAGES_BY_THREAD truncated ==================";

echo "================== Step 19 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

truncate performance_schema.events_stages_summary_by_account_by_event_name;

echo "================== STAGES_BY_ACCOUNT truncated ==================";

echo "================== Step 20 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

truncate performance_schema.events_stages_summary_by_user_by_event_name;

echo "================== STAGES_BY_USER truncated ==================";

echo "================== Step 21 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

truncate performance_schema.events_stages_summary_by_host_by_event_name;

echo "================== STAGES_BY_HOST truncated ==================";

echo "================== Step 22 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

truncate performance_schema.events_stages_summary_global_by_event_name;

echo "================== STAGES_GLOBAL truncated ==================";

echo "================== Step 23 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

truncate performance_schema.events_statements_summary_by_thread_by_event_name;

echo "================== STATEMENTS_BY_THREAD truncated ==================";

echo "================== Step 24 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

truncate performance_schema.events_statements_summary_by_account_by_event_name;

echo "================== STATEMENTS_BY_ACCOUNT truncated ==================";

echo "================== Step 25 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

truncate performance_schema.events_statements_summary_by_user_by_event_name;

echo "================== STATEMENTS_BY_USER truncated ==================";

echo "================== Step 26 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

truncate performance_schema.events_statements_summary_by_host_by_event_name;

echo "================== STATEMENTS_BY_HOST truncated ==================";

echo "================== Step 27 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

truncate performance_schema.events_statements_summary_global_by_event_name;

echo "================== STATEMENTS_GLOBAL truncated ==================";

echo "================== Step 28 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

truncate performance_schema.accounts;

echo "================== ACCOUNTS truncated ==================";

echo "================== Step 29 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

truncate performance_schema.users;

echo "================== USERS truncated ==================";

echo "================== Step 30 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

truncate performance_schema.hosts;

echo "================== HOSTS truncated ==================";

echo "================== Step 31 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_stages_account;
execute dump_stages_user;
execute dump_stages_host;
execute dump_stages_global;
execute dump_stages_history;
execute dump_statements_account;
execute dump_statements_user;
execute dump_statements_host;
execute dump_statements_global;
execute dump_statements_history;
execute dump_accounts;
execute dump_users;
execute dump_hosts;

