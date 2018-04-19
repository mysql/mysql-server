# Tests for the performance schema

# =============
# DOCUMENTATION
# =============

# Verify how memory stats are aggregated into various tables
#
# In the thread dimension:
# - memory_summary_by_thread_by_event_name
# - memory_summary_by_account_by_event_name
# - memory_summary_by_user_by_event_name
# - memory_summary_by_host_by_event_name
#
# Globally:
# - memory_summary_global_by_event_name
#
# The tests are written with the following helpers:
# - include/memory_aggregate_setup.inc
# - include/memory_aggregate_load.inc
# - include/memory_aggregate_cleanup.inc
#
# Helpers are intended to be used as follows.
#
# A Typical test t/memory_aggregate_xxx.test will consist of:
# --source ../include/memory_aggregate_setup.inc
# --source ../include/memory_aggregate_load.inc
# --source ../include/memory_aggregate_cleanup.inc
# and a t/memory_aggregate_xxx-master.opt file
#
# Naming conventions for t/memory_aggregate_xxx.test are as follows:
# t/memory_aggregate_<account><user><host>
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

# =========================================
# HELPER include/memory_aggregate_setup.inc
# =========================================

# Test requires: sp-protocol/ps-protocol/view-protocol/cursor-protocol disabled
--source include/no_protocol.inc
--source ../include/wait_for_pfs_thread_count.inc

--disable_query_log

create user user1@localhost, user2@localhost,
            user3@localhost, user4@localhost;
grant ALL on *.* to user1@localhost;
grant ALL on *.* to user2@localhost;
grant ALL on *.* to user3@localhost;
grant ALL on *.* to user4@localhost;

flush privileges;

# Purge old users, hosts, user/host from previous tests
truncate table performance_schema.accounts;
truncate table performance_schema.users;
truncate table performance_schema.hosts;

# Save the setup

--disable_warnings
drop table if exists test.setup_actors;
--enable_warnings

create table test.setup_actors as
  select * from performance_schema.setup_actors;

# Only instrument the user connections
truncate table performance_schema.setup_actors;
insert into performance_schema.setup_actors
  set host= 'localhost', user= 'user1', `role`= '%';
insert into performance_schema.setup_actors
  set host= 'localhost', user= 'user2', `role`= '%';
insert into performance_schema.setup_actors
  set host= 'localhost', user= 'user3', `role`= '%';
insert into performance_schema.setup_actors
  set host= 'localhost', user= 'user4', `role`= '%';

update performance_schema.threads set instrumented='NO';

# Only instrument a few events of each kind
update performance_schema.setup_instruments set enabled='NO', timed='NO';

update performance_schema.setup_instruments set enabled='YES', timed='YES'
  where name in ('memory/sql/user_var_entry::value',
                 'idle');

# Start from a known clean state, to avoid noise from previous tests
flush tables;
flush status;
truncate performance_schema.memory_summary_by_thread_by_event_name;
truncate performance_schema.memory_summary_by_account_by_event_name;
truncate performance_schema.memory_summary_by_user_by_event_name;
truncate performance_schema.memory_summary_by_host_by_event_name;
truncate performance_schema.memory_summary_global_by_event_name;

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
                        where processlist_user=username LIMIT 1);

  if (my_thread_id is not null) then
    select username,
      EVENT_NAME, COUNT_ALLOC, COUNT_FREE,
      SUM_NUMBER_OF_BYTES_ALLOC, SUM_NUMBER_OF_BYTES_FREE,
      LOW_COUNT_USED, CURRENT_COUNT_USED, HIGH_COUNT_USED,
      LOW_NUMBER_OF_BYTES_USED, CURRENT_NUMBER_OF_BYTES_USED, HIGH_NUMBER_OF_BYTES_USED
      from performance_schema.memory_summary_by_thread_by_event_name
      where event_name in ('memory/sql/user_var_entry::value')
      and thread_id = my_thread_id
      order by event_name;
  else
    select username, "not found" as status;
  end if;
end
$$

delimiter ;$$

prepare dump_memory_account from
  "select *
  from performance_schema.memory_summary_by_account_by_event_name
  where user like \'user%\'
  and event_name in ('memory/sql/user_var_entry::value')
  order by user, host, event_name;";

prepare dump_memory_user from
  "select *
  from performance_schema.memory_summary_by_user_by_event_name
  where user like \'user%\'
  and event_name in ('memory/sql/user_var_entry::value')
  order by user, event_name;";

prepare dump_memory_host from
  "select *
  from performance_schema.memory_summary_by_host_by_event_name
  where host=\'localhost\'
  and event_name in ('memory/sql/user_var_entry::value')
  order by host, event_name;";

prepare dump_memory_global from
  "select *
  from performance_schema.memory_summary_global_by_event_name
  where event_name in ('memory/sql/user_var_entry::value')
  order by event_name;";

prepare dump_users from
  "select * from performance_schema.users where user is not null order by user;";

prepare dump_hosts from
  "select * from performance_schema.hosts where host is not null order by host;";

prepare dump_accounts from
  "select * from performance_schema.accounts where (user is not null) and (host is not null) order by user, host;";

--enable_query_log

# Make sure all the instrumentation is present
show global status like "performance_schema_memory_classes_lost";


