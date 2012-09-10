# Tests for the performance schema
#

# See comments in include/table_aggregate_setup.inc

# Display the current setup used

select * from performance_schema.setup_actors
  order by USER, HOST, ROLE;

select * from performance_schema.setup_objects
  order by object_type, object_schema, object_name;

select * from performance_schema.setup_consumers;

# General cleanup

flush tables;

truncate performance_schema.objects_summary_global_by_type;
truncate performance_schema.table_io_waits_summary_by_index_usage;
truncate performance_schema.table_io_waits_summary_by_table;
truncate performance_schema.table_lock_waits_summary_by_table;
truncate performance_schema.events_waits_summary_by_thread_by_event_name;
truncate performance_schema.events_waits_summary_by_account_by_event_name;
truncate performance_schema.events_waits_summary_by_user_by_event_name;
truncate performance_schema.events_waits_summary_by_host_by_event_name;
truncate performance_schema.events_waits_summary_global_by_event_name;
truncate performance_schema.events_waits_history_long;

# Check the configuration is ok
show variables like "performance_schema%";
show status like "performance_schema%";

echo "================== Step 1 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_waits_index_io;
execute dump_waits_table_io;
execute dump_waits_table_lock;
execute dump_objects_summary;

# Notes about this test
#

connect (con1, localhost, user1, , );
select concat(current_user(), " is connected") as status;

--connection default

echo "================== Step 2 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_waits_index_io;
execute dump_waits_table_io;
execute dump_waits_table_lock;
execute dump_objects_summary;

--connection con1

insert into test.t1 set a=101, b=1, c=1;
insert into test.t2 set a=102, b=2, c=2;
insert into test.t2 set a=103, b=3, c=3;
insert into test.t3 set a=104, b=4, c=4;
insert into test.t3 set a=105, b=5, c=5;
insert into test.t3 set a=106, b=6, c=6;
select * from test.t1;
select * from test.t2;
select * from test.t3;
# Full table scan
update test.t1 set d=d+1;
update test.t2 set d=d+1;
update test.t3 set d=d+1;
# Update with PK
update test.t1 set d=d+1 where a=101;
update test.t2 set d=d+1 where a=101;
update test.t3 set d=d+1 where a=101;
# select with index
select * from test.t1 where b=5;
select * from test.t2 where b=5;
select * from test.t3 where b=5;

echo "================== con1 marker ==================";

--connection default

echo "================== Step 3 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_waits_index_io;
execute dump_waits_table_io;
execute dump_waits_table_lock;
execute dump_objects_summary;

# Debugging helpers
# select * from performance_schema.events_waits_history_long;
# select PROCESSLIST_USER, PROCESSLIST_HOST, INSTRUMENTED from performance_schema.threads;

connect (con2, localhost, user2, , );
select concat(current_user(), " is connected") as status;

--connection default

echo "================== Step 4 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_waits_index_io;
execute dump_waits_table_io;
execute dump_waits_table_lock;
execute dump_objects_summary;

--connection con2

insert into test.t1 set a=201, b=1, c=1;
insert into test.t2 set a=202, b=2, c=2;
insert into test.t2 set a=203, b=3, c=3;
insert into test.t3 set a=204, b=4, c=4;
insert into test.t3 set a=205, b=5, c=5;
insert into test.t3 set a=206, b=6, c=6;
select * from test.t1;
select * from test.t2;
select * from test.t3;
# Full table scan
update test.t1 set d=d+1;
update test.t2 set d=d+1;
update test.t3 set d=d+1;
# Update with PK
update test.t1 set d=d+1 where a=201;
update test.t2 set d=d+1 where a=201;
update test.t3 set d=d+1 where a=201;
# select with index
select * from test.t1 where b=5;
select * from test.t2 where b=5;
select * from test.t3 where b=5;

echo "================== con2 marker ==================";

--connection default

echo "================== Step 5 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_waits_index_io;
execute dump_waits_table_io;
execute dump_waits_table_lock;
execute dump_objects_summary;

connect (con3, localhost, user3, , );
select concat(current_user(), " is connected") as status;

--connection default

echo "================== Step 6 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_waits_index_io;
execute dump_waits_table_io;
execute dump_waits_table_lock;
execute dump_objects_summary;

--connection con3

insert into test.t1 set a=301, b=1, c=1;
insert into test.t2 set a=302, b=2, c=2;
insert into test.t2 set a=303, b=3, c=3;
insert into test.t3 set a=304, b=4, c=4;
insert into test.t3 set a=305, b=5, c=5;
insert into test.t3 set a=306, b=6, c=6;
select * from test.t1;
select * from test.t2;
select * from test.t3;
# Full table scan
update test.t1 set d=d+1;
update test.t2 set d=d+1;
update test.t3 set d=d+1;
# Update with PK
update test.t1 set d=d+1 where a=301;
update test.t2 set d=d+1 where a=301;
update test.t3 set d=d+1 where a=301;
# select with index
select * from test.t1 where b=5;
select * from test.t2 where b=5;
select * from test.t3 where b=5;

echo "================== con3 marker ==================";

--connection default

echo "================== Step 7 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_waits_index_io;
execute dump_waits_table_io;
execute dump_waits_table_lock;
execute dump_objects_summary;

connect (con4, localhost, user4, , );
select concat(current_user(), " is connected") as status;

--connection default

echo "================== Step 8 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_waits_index_io;
execute dump_waits_table_io;
execute dump_waits_table_lock;
execute dump_objects_summary;

--connection con4

insert into test.t1 set a=401, b=1, c=1;
insert into test.t2 set a=402, b=2, c=2;
insert into test.t2 set a=403, b=3, c=3;
insert into test.t3 set a=404, b=4, c=4;
insert into test.t3 set a=405, b=5, c=5;
insert into test.t3 set a=406, b=6, c=6;
select * from test.t1;
select * from test.t2;
select * from test.t3;
# Full table scan
update test.t1 set d=d+1;
update test.t2 set d=d+1;
update test.t3 set d=d+1;
# Update with PK
update test.t1 set d=d+1 where a=401;
update test.t2 set d=d+1 where a=401;
update test.t3 set d=d+1 where a=401;
# select with index
select * from test.t1 where b=5;
select * from test.t2 where b=5;
select * from test.t3 where b=5;

echo "================== con4 marker ==================";

--connection default

echo "================== Step 9 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_waits_index_io;
execute dump_waits_table_io;
execute dump_waits_table_lock;
execute dump_objects_summary;

--connection con1

lock tables test.t1 read, test.t2 read, test.t3 read;
unlock tables;
lock tables test.t1 write, test.t2 write, test.t3 write;
unlock tables;

echo "================== con1 marker ==================";

--connection default

echo "================== Step 10 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_waits_index_io;
execute dump_waits_table_io;
execute dump_waits_table_lock;
execute dump_objects_summary;

--connection default

flush tables;

echo "================== flush marker ==================";

echo "================== Step 11 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_waits_index_io;
execute dump_waits_table_io;
execute dump_waits_table_lock;
execute dump_objects_summary;

set global read_only=1;
set global read_only=0;

echo "================== global read_only marker ==================";

echo "================== Step 12 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_waits_index_io;
execute dump_waits_table_io;
execute dump_waits_table_lock;
execute dump_objects_summary;

--disconnect con1

# Wait for the disconnect to complete
let $wait_condition=
  select count(*) = 0 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER= 'user1';
--source include/wait_condition.inc

echo "================== con1 disconnected ==================";

echo "================== Step 13 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_waits_index_io;
execute dump_waits_table_io;
execute dump_waits_table_lock;
execute dump_objects_summary;

--disconnect con2

# Wait for the disconnect to complete
let $wait_condition=
  select count(*) = 0 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER= 'user2';
--source include/wait_condition.inc

echo "================== con2 disconnected ==================";

echo "================== Step 14 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_waits_index_io;
execute dump_waits_table_io;
execute dump_waits_table_lock;
execute dump_objects_summary;

--disconnect con3

# Wait for the disconnect to complete
let $wait_condition=
  select count(*) = 0 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER= 'user3';
--source include/wait_condition.inc

echo "================== con3 disconnected ==================";

echo "================== Step 15 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_waits_index_io;
execute dump_waits_table_io;
execute dump_waits_table_lock;
execute dump_objects_summary;

--disconnect con4

# Wait for the disconnect to complete
let $wait_condition=
  select count(*) = 0 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER= 'user4';
--source include/wait_condition.inc

echo "================== con4 disconnected ==================";

echo "================== Step 16 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_waits_index_io;
execute dump_waits_table_io;
execute dump_waits_table_lock;
execute dump_objects_summary;

--connection default

truncate performance_schema.events_waits_summary_by_thread_by_event_name;

echo "================== BY_THREAD truncated ==================";

echo "================== Step 17 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_waits_index_io;
execute dump_waits_table_io;
execute dump_waits_table_lock;
execute dump_objects_summary;

truncate performance_schema.events_waits_summary_by_account_by_event_name;

echo "================== BY_ACCOUNT truncated ==================";

echo "================== Step 18 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_waits_index_io;
execute dump_waits_table_io;
execute dump_waits_table_lock;
execute dump_objects_summary;

truncate performance_schema.events_waits_summary_by_user_by_event_name;

echo "================== BY_USER truncated ==================";

echo "================== Step 19 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_waits_index_io;
execute dump_waits_table_io;
execute dump_waits_table_lock;
execute dump_objects_summary;

truncate performance_schema.events_waits_summary_by_host_by_event_name;

echo "================== BY_HOST truncated ==================";

echo "================== Step 21 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_waits_index_io;
execute dump_waits_table_io;
execute dump_waits_table_lock;
execute dump_objects_summary;

truncate performance_schema.events_waits_summary_global_by_event_name;

echo "================== GLOBAL truncated ==================";

echo "================== Step 21 ==================";
call dump_thread();
execute dump_waits_account;
execute dump_waits_user;
execute dump_waits_host;
execute dump_waits_global;
execute dump_waits_history;
execute dump_waits_index_io;
execute dump_waits_table_io;
execute dump_waits_table_lock;
execute dump_objects_summary;

# On test failures, may help to track the root cause
show status like "performance_schema%";

