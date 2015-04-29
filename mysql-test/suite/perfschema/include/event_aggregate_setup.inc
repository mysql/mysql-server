# Tests for the performance schema

# =============
# DOCUMENTATION
# =============

# Verify how events are aggregated into various tables
#
# In the thread dimension:
# - events_waits_summary_by_thread_by_event_name
# - events_waits_summary_by_account_by_event_name
# - events_waits_summary_by_user_by_event_name
# - events_waits_summary_by_host_by_event_name
# - events_stages_summary_by_thread_by_event_name
# - events_stages_summary_by_account_by_event_name
# - events_stages_summary_by_user_by_event_name
# - events_stages_summary_by_host_by_event_name
# - events_statements_summary_by_thread_by_event_name
# - events_statements_summary_by_account_by_event_name
# - events_statements_summary_by_user_by_event_name
# - events_statements_summary_by_host_by_event_name
# - events_transactions_summary_by_thread_by_event_name
# - events_transactions_summary_by_account_by_event_name
# - events_transactions_summary_by_user_by_event_name
# - events_transactions_summary_by_host_by_event_name
#
# Globally:
# - events_waits_summary_global_by_event_name
# - events_stages_summary_global_by_event_name
# - events_statements_summary_global_by_event_name
# - events_transactions_summary_global_by_event_name
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
# Naming conventions for t/event_aggregate_xxx.test are as follows:
# t/event_aggregate_<account><user><host>
#
# <account> corresponds to different sizing settings for 
# the variable performance-schema-accounts-size
# - (blank): accounts-size sufficient to represent all records
# - no_a: accounts-size set to 0
#
# <user> corresponds to different sizing settings for 
# the variable performance-schema-users-size
# - (blank): users-size sufficient to represent all records
# - no_u: users-size set to 0
#
# <host> corresponds to different sizing settings for 
# the variable performance-schema-hosts-size
# - (blank): hosts-size sufficient to represent all records
# - no_h: hosts-size set to 0

# ========================================
# HELPER include/event_aggregate_setup.inc
# ========================================

--source include/not_embedded.inc
--source include/have_perfschema.inc
--source include/no_protocol.inc
--source ../include/wait_for_pfs_thread_count.inc

--disable_query_log

set @orig_sql_mode= @@sql_mode;
set sql_mode= (select replace(@@sql_mode,'NO_AUTO_CREATE_USER',''));
grant ALL on *.* to user1@localhost;
grant ALL on *.* to user2@localhost;
grant ALL on *.* to user3@localhost;
grant ALL on *.* to user4@localhost;
set sql_mode= @orig_sql_mode;

flush privileges;

# Purge old users, hosts, user/host from previous tests
truncate table performance_schema.accounts;
truncate table performance_schema.users;
truncate table performance_schema.hosts;

# Save the setup

--disable_warnings
drop table if exists test.setup_actors;
drop table if exists test.t1;
--enable_warnings

create table test.t1(a varchar(64));
create function test.f(a int, b int) returns int
return a+b;

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
                 'wait/synch/mutex/sql/LOCK_uuid_generator',
                 'wait/synch/rwlock/sql/LOCK_grant',
                 'wait/io/file/sql/query_log',
                 'idle');

update performance_schema.setup_instruments set enabled='YES', timed='YES'
  where name in ('stage/sql/starting',
                 'stage/sql/init',
                 'stage/sql/checking permissions',
                 'stage/sql/Opening tables',
                 'stage/sql/closing tables');

update performance_schema.setup_instruments set enabled='YES', timed='YES'
  where name in ('statement/sql/select',
                 'statement/sql/insert',
                 'statement/abstract/new_packet',
                 'statement/abstract/Query',
                 'statement/com/Quit',
                 'statement/com/error',
                 'statement/sp/freturn');

update performance_schema.setup_instruments set enabled='YES', timed='YES'
  where name in ('transaction');

# Start from a known clean state, to avoid noise from previous tests
flush tables;
flush status;
truncate performance_schema.events_waits_summary_by_thread_by_event_name;
truncate performance_schema.events_waits_summary_by_account_by_event_name;
truncate performance_schema.events_waits_summary_by_user_by_event_name;
truncate performance_schema.events_waits_summary_by_host_by_event_name;
truncate performance_schema.events_waits_summary_global_by_event_name;
truncate performance_schema.events_waits_history_long;

truncate performance_schema.events_stages_summary_by_thread_by_event_name;
truncate performance_schema.events_stages_summary_by_account_by_event_name;
truncate performance_schema.events_stages_summary_by_user_by_event_name;
truncate performance_schema.events_stages_summary_by_host_by_event_name;
truncate performance_schema.events_stages_summary_global_by_event_name;
truncate performance_schema.events_stages_history_long;

truncate performance_schema.events_statements_summary_by_thread_by_event_name;
truncate performance_schema.events_statements_summary_by_account_by_event_name;
truncate performance_schema.events_statements_summary_by_user_by_event_name;
truncate performance_schema.events_statements_summary_by_host_by_event_name;
truncate performance_schema.events_statements_summary_global_by_event_name;
truncate performance_schema.events_statements_history_long;

truncate performance_schema.events_transactions_summary_by_thread_by_event_name;
truncate performance_schema.events_transactions_summary_by_account_by_event_name;
truncate performance_schema.events_transactions_summary_by_user_by_event_name;
truncate performance_schema.events_transactions_summary_by_host_by_event_name;
truncate performance_schema.events_transactions_summary_global_by_event_name;
truncate performance_schema.events_transactions_history_long;

--disable_warnings
drop procedure if exists dump_thread;
drop procedure if exists dump_one_thread;
--enable_warnings

delimiter $$;

create procedure dump_thread()
begin
  call dump_one_thread('user1');
  call dump_one_thread('user2');
  call dump_one_thread('user3');
  call dump_one_thread('user4');
end
$$

create procedure dump_one_thread(in username varchar(64))
begin
  declare my_thread_id int;

  set my_thread_id = (select thread_id from performance_schema.threads
                        where processlist_user=username);

  if (my_thread_id is not null) then
    select username, event_name, count_star
      from performance_schema.events_waits_summary_by_thread_by_event_name
      where event_name in ('wait/synch/mutex/sql/LOCK_connection_count',
                           'wait/synch/mutex/sql/LOCK_uuid_generator',
                           'wait/synch/rwlock/sql/LOCK_grant',
                           'wait/io/file/sql/query_log')
      and thread_id = my_thread_id
      order by event_name;
  else
    select username, "not found" as status;
  end if;
end
$$

delimiter ;$$

prepare dump_waits_account from
  "select user, host, event_name, count_star
  from performance_schema.events_waits_summary_by_account_by_event_name
  where user like \'user%\'
  and event_name in ('wait/synch/mutex/sql/LOCK_connection_count',
                     'wait/synch/mutex/sql/LOCK_uuid_generator',
                     'wait/synch/rwlock/sql/LOCK_grant',
                     'wait/io/file/sql/query_log')
  order by user, host, event_name;";

prepare dump_waits_user from
  "select user, event_name, count_star
  from performance_schema.events_waits_summary_by_user_by_event_name
  where user like \'user%\'
  and event_name in ('wait/synch/mutex/sql/LOCK_connection_count',
                     'wait/synch/mutex/sql/LOCK_uuid_generator',
                     'wait/synch/rwlock/sql/LOCK_grant',
                     'wait/io/file/sql/query_log')
  order by user, event_name;";

prepare dump_waits_host from
  "select host, event_name, count_star
  from performance_schema.events_waits_summary_by_host_by_event_name
  where host=\'localhost\'
  and event_name in ('wait/synch/mutex/sql/LOCK_connection_count',
                     'wait/synch/mutex/sql/LOCK_uuid_generator',
                     'wait/synch/rwlock/sql/LOCK_grant',
                     'wait/io/file/sql/query_log')
  order by host, event_name;";

prepare dump_waits_global from
  "select event_name, count_star
  from performance_schema.events_waits_summary_global_by_event_name
  where event_name in ('wait/synch/mutex/sql/LOCK_connection_count',
                       'wait/synch/mutex/sql/LOCK_uuid_generator',
                       'wait/synch/rwlock/sql/LOCK_grant',
                       'wait/io/file/sql/query_log')
  order by event_name;";

prepare dump_waits_history from
  "select event_name, count(event_name)
  from performance_schema.events_waits_history_long
  where event_name in ('wait/synch/mutex/sql/LOCK_connection_count',
                       'wait/synch/mutex/sql/LOCK_uuid_generator',
                       'wait/synch/rwlock/sql/LOCK_grant',
                       'wait/io/file/sql/query_log')
  group by event_name order by event_name;";

prepare dump_stages_account from
  "select user, host, event_name, count_star
  from performance_schema.events_stages_summary_by_account_by_event_name
  where user like \'user%\'
  and event_name in ('stage/sql/starting',
                     'stage/sql/init',
                     'stage/sql/checking permissions',
                     'stage/sql/Opening tables',
                     'stage/sql/closing tables')
  order by user, host, event_name;";

prepare dump_stages_user from
  "select user, event_name, count_star
  from performance_schema.events_stages_summary_by_user_by_event_name
  where user like \'user%\'
  and event_name in ('stage/sql/starting',
                     'stage/sql/init',
                     'stage/sql/checking permissions',
                     'stage/sql/Opening tables',
                     'stage/sql/closing tables')
  order by user, event_name;";

prepare dump_stages_host from
  "select host, event_name, count_star
  from performance_schema.events_stages_summary_by_host_by_event_name
  where host=\'localhost\'
  and event_name in ('stage/sql/starting',
                     'stage/sql/init',
                     'stage/sql/checking permissions',
                     'stage/sql/Opening tables',
                     'stage/sql/closing tables')
  order by host, event_name;";

prepare dump_stages_global from
  "select event_name, count_star
  from performance_schema.events_stages_summary_global_by_event_name
  where event_name in ('stage/sql/starting',
                       'stage/sql/init',
                       'stage/sql/checking permissions',
                       'stage/sql/Opening tables',
                       'stage/sql/closing tables')
  order by event_name;";

prepare dump_stages_history from
  "select event_name, count(event_name)
  from performance_schema.events_stages_history_long
  where event_name in ('stage/sql/starting',
                       'stage/sql/init',
                       'stage/sql/checking permissions',
                       'stage/sql/Opening tables',
                       'stage/sql/closing tables')
  group by event_name order by event_name;";

prepare dump_statements_account from
  "select user, host, event_name, count_star
  from performance_schema.events_statements_summary_by_account_by_event_name
  where user like \'user%\'
  and event_name in ('statement/sql/select',
                     'statement/sql/insert',
                     'statement/com/Quit',
                     'statement/com/error',
                     'statement/sp/freturn')
  order by user, host, event_name;";

prepare dump_statements_user from
  "select user, event_name, count_star
  from performance_schema.events_statements_summary_by_user_by_event_name
  where user like \'user%\'
  and event_name in ('statement/sql/select',
                     'statement/sql/insert',
                     'statement/com/Quit',
                     'statement/com/error',
                     'statement/sp/freturn')
  order by user, event_name;";

prepare dump_statements_host from
  "select host, event_name, count_star
  from performance_schema.events_statements_summary_by_host_by_event_name
  where host=\'localhost\'
  and event_name in ('statement/sql/select',
                     'statement/sql/insert',
                     'statement/com/Quit',
                     'statement/com/error',
                     'statement/sp/freturn')
  order by host, event_name;";

prepare dump_statements_global from
  "select event_name, count_star
  from performance_schema.events_statements_summary_global_by_event_name
  where event_name in ('statement/sql/select',
                       'statement/sql/insert',
                       'statement/com/Quit',
                       'statement/com/error',
                       'statement/sp/freturn')
  order by event_name;";

prepare dump_statements_history from
  "select event_name, count(event_name)
  from performance_schema.events_statements_history_long
  where event_name in ('statement/sql/select',
                       'statement/sql/insert',
                       'statement/com/Quit',
                       'statement/com/error',
                       'statement/sp/freturn')
  group by event_name order by event_name;";

prepare dump_transactions_account from
  "select user, host, event_name, count_star
  from performance_schema.events_transactions_summary_by_account_by_event_name
  where user like \'user%\'
  and event_name in ('transaction')
  order by user, host, event_name;";

prepare dump_transactions_user from
  "select user, event_name, count_star
  from performance_schema.events_transactions_summary_by_user_by_event_name
  where user like \'user%\'
  and event_name in ('transaction')
  order by user, event_name;";

prepare dump_transactions_host from
  "select host, event_name, count_star
  from performance_schema.events_transactions_summary_by_host_by_event_name
  where host=\'localhost\'
  and event_name in ('transaction')
  order by host, event_name;";

prepare dump_transactions_global from
  "select event_name, count_star
  from performance_schema.events_transactions_summary_global_by_event_name
  where event_name in ('transaction')
  order by event_name;";

prepare dump_transactions_history from
  "select event_name, count(event_name)
  from performance_schema.events_transactions_history_long
  where event_name in ('transaction')
  group by event_name order by event_name;";

prepare dump_users from
  "select * from performance_schema.users where user is not null order by user;";

prepare dump_hosts from
  "select * from performance_schema.hosts where host is not null order by host;";

prepare dump_accounts from
  "select * from performance_schema.accounts where (user is not null) and (host is not null) order by user, host;";

--enable_query_log
