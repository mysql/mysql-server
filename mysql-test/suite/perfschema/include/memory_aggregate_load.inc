# Tests for the performance schema

# ========================================
# HELPER include/memory_aggregate_load.inc
# ========================================

echo "================== Step 1 ==================";
--vertical_results
call dump_thread();
execute dump_memory_account;
execute dump_memory_user;
execute dump_memory_host;
execute dump_memory_global;
--horizontal_results
execute dump_accounts;
execute dump_users;
execute dump_hosts;

# Notes about this test
#
# get_lock("marker") causes the following memory allocation:
# memory/sql/User_level_lock, 1 malloc, size 16 (size 8 for 32-bit systems).
# release_lock("marker") causes the following memory free:
# memory/sql/User_level_lock, 1 free, size 16 (size 8 for 32-bit systems).
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
--vertical_results
call dump_thread();
execute dump_memory_account;
execute dump_memory_user;
execute dump_memory_host;
execute dump_memory_global;
--horizontal_results
execute dump_accounts;
execute dump_users;
execute dump_hosts;

--connection con1

select get_lock("marker_1", 10);
select release_lock("marker_1");
set @v1 = repeat("a", 1000);
set @v2 = repeat("b", 2000);
set @v3 = repeat("c", 4000);

echo "================== con1 marker ==================";

--connection default

# Wait for the payload to complete
let $wait_condition=
  select count(*) = 1 from performance_schema.events_waits_current
  where EVENT_NAME= 'idle';
--source include/wait_condition.inc

echo "================== Step 3 ==================";
--vertical_results
call dump_thread();
execute dump_memory_account;
execute dump_memory_user;
execute dump_memory_host;
execute dump_memory_global;
--horizontal_results
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
--vertical_results
call dump_thread();
execute dump_memory_account;
execute dump_memory_user;
execute dump_memory_host;
execute dump_memory_global;
--horizontal_results
execute dump_accounts;
execute dump_users;
execute dump_hosts;

--connection con2

select get_lock("marker_2", 10);
select release_lock("marker_2");
set @v1 = repeat("a", 1000);
set @v2 = repeat("b", 2000);
set @v3 = repeat("c", 4000);

echo "================== con2 marker ==================";

--connection default

# Wait for the payload to complete
let $wait_condition=
  select count(*) = 2 from performance_schema.events_waits_current
  where EVENT_NAME= 'idle';
--source include/wait_condition.inc

echo "================== Step 5 ==================";
--vertical_results
call dump_thread();
execute dump_memory_account;
execute dump_memory_user;
execute dump_memory_host;
execute dump_memory_global;
--horizontal_results
execute dump_accounts;
execute dump_users;
execute dump_hosts;

truncate table performance_schema.memory_summary_by_thread_by_event_name;

echo "================== MEMORY_BY_THREAD truncated ==================";

echo "================== Step 5b ==================";
--vertical_results
call dump_thread();
execute dump_memory_account;
execute dump_memory_user;
execute dump_memory_host;
execute dump_memory_global;
--horizontal_results
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
--vertical_results
call dump_thread();
execute dump_memory_account;
execute dump_memory_user;
execute dump_memory_host;
execute dump_memory_global;
--horizontal_results
execute dump_accounts;
execute dump_users;
execute dump_hosts;

--connection con3

select get_lock("marker_3", 10);
select release_lock("marker_3");
set @v1 = repeat("a", 1000);
set @v2 = repeat("b", 2000);
set @v3 = repeat("c", 4000);

echo "================== con3 marker ==================";

--connection default

# Wait for the payload to complete
let $wait_condition=
  select count(*) = 3 from performance_schema.events_waits_current
  where EVENT_NAME= 'idle';
--source include/wait_condition.inc

echo "================== Step 7 ==================";
--vertical_results
call dump_thread();
execute dump_memory_account;
execute dump_memory_user;
execute dump_memory_host;
execute dump_memory_global;
--horizontal_results
execute dump_accounts;
execute dump_users;
execute dump_hosts;

connect (con4, localhost, user4, , );
connect (con5, localhost, user4, , );

echo "================== con4/con5 (both user4) connected ==================";

--connection default

# Wait for the connects to complete
let $wait_condition=
  select count(*) = 2 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER= 'user4';
--source include/wait_condition.inc

echo "================== Step 8 ==================";
--vertical_results
call dump_thread();
execute dump_memory_account;
execute dump_memory_user;
execute dump_memory_host;
execute dump_memory_global;
--horizontal_results
execute dump_accounts;
execute dump_users;
execute dump_hosts;

--connection con4

select get_lock("marker_4", 10);
select release_lock("marker_4");
set @v1 = repeat("a", 1000);
set @v2 = repeat("b", 2000);
set @v3 = repeat("c", 4000);

--connection con5
select get_lock("marker_5", 10);
select release_lock("marker_5");
set @v1 = repeat("a", 1000);
set @v2 = repeat("b", 2000);
set @v3 = repeat("c", 4000);

echo "================== con4/con5 marker ==================";

--connection default

# Wait for the payload to complete
let $wait_condition=
  select count(*) = 5 from performance_schema.events_waits_current
  where EVENT_NAME= 'idle';
--source include/wait_condition.inc

echo "================== Step 9 ==================";
--vertical_results
call dump_thread();
execute dump_memory_account;
execute dump_memory_user;
execute dump_memory_host;
execute dump_memory_global;
--horizontal_results
execute dump_accounts;
execute dump_users;
execute dump_hosts;

--disconnect con1
--disconnect con5

--connection default

# Wait for the disconnects to complete
let $wait_condition=
  select count(*) = 0 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER= 'user1';
--source include/wait_condition.inc
let $wait_condition=
  select count(*) = 1 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER= 'user4';
--source include/wait_condition.inc

echo "================== con1/con5 disconnected ==================";

echo "================== Step 10 ==================";
--vertical_results
call dump_thread();
execute dump_memory_account;
execute dump_memory_user;
execute dump_memory_host;
execute dump_memory_global;
--horizontal_results
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
--vertical_results
call dump_thread();
execute dump_memory_account;
execute dump_memory_user;
execute dump_memory_host;
execute dump_memory_global;
--horizontal_results
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
--vertical_results
call dump_thread();
execute dump_memory_account;
execute dump_memory_user;
execute dump_memory_host;
execute dump_memory_global;
--horizontal_results
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
--vertical_results
call dump_thread();
execute dump_memory_account;
execute dump_memory_user;
execute dump_memory_host;
execute dump_memory_global;
--horizontal_results
execute dump_accounts;
execute dump_users;
execute dump_hosts;

--connection default

truncate performance_schema.memory_summary_by_thread_by_event_name;

echo "================== MEMORY_BY_THREAD truncated ==================";

echo "================== Step 14 ==================";
--vertical_results
call dump_thread();
execute dump_memory_account;
execute dump_memory_user;
execute dump_memory_host;
execute dump_memory_global;
--horizontal_results
execute dump_accounts;
execute dump_users;
execute dump_hosts;

truncate performance_schema.memory_summary_by_account_by_event_name;

echo "================== MEMORY_BY_ACCOUNT truncated ==================";

echo "================== Step 15 ==================";
--vertical_results
call dump_thread();
execute dump_memory_account;
execute dump_memory_user;
execute dump_memory_host;
execute dump_memory_global;
--horizontal_results
execute dump_accounts;
execute dump_users;
execute dump_hosts;

truncate performance_schema.memory_summary_by_user_by_event_name;

echo "================== MEMORY_BY_USER truncated ==================";

echo "================== Step 16 ==================";
--vertical_results
call dump_thread();
execute dump_memory_account;
execute dump_memory_user;
execute dump_memory_host;
execute dump_memory_global;
--horizontal_results
execute dump_accounts;
execute dump_users;
execute dump_hosts;

truncate performance_schema.memory_summary_by_host_by_event_name;

echo "================== MEMORY_BY_HOST truncated ==================";

echo "================== Step 17 ==================";
--vertical_results
call dump_thread();
execute dump_memory_account;
execute dump_memory_user;
execute dump_memory_host;
execute dump_memory_global;
--horizontal_results
execute dump_accounts;
execute dump_users;
execute dump_hosts;

truncate performance_schema.memory_summary_global_by_event_name;

echo "================== MEMORY_GLOBAL truncated ==================";

echo "================== Step 18 ==================";
--vertical_results
call dump_thread();
execute dump_memory_account;
execute dump_memory_user;
execute dump_memory_host;
execute dump_memory_global;
--horizontal_results
execute dump_accounts;
execute dump_users;
execute dump_hosts;

truncate performance_schema.accounts;

echo "================== ACCOUNTS truncated ==================";

echo "================== Step 19 ==================";
--vertical_results
call dump_thread();
execute dump_memory_account;
execute dump_memory_user;
execute dump_memory_host;
execute dump_memory_global;
--horizontal_results
execute dump_accounts;
execute dump_users;
execute dump_hosts;

truncate performance_schema.users;

echo "================== USERS truncated ==================";

echo "================== Step 20 ==================";
--vertical_results
call dump_thread();
execute dump_memory_account;
execute dump_memory_user;
execute dump_memory_host;
execute dump_memory_global;
--horizontal_results
execute dump_accounts;
execute dump_users;
execute dump_hosts;

truncate performance_schema.hosts;

echo "================== HOSTS truncated ==================";

echo "================== Step 21 ==================";
--vertical_results
call dump_thread();
execute dump_memory_account;
execute dump_memory_user;
execute dump_memory_host;
execute dump_memory_global;
--horizontal_results
execute dump_accounts;
execute dump_users;
execute dump_hosts;

