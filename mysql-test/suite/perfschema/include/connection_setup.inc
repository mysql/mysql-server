# Tests for the performance schema

# =============
# DOCUMENTATION
# =============

# Verify how connections are counted into various tables:
# - accounts
# - users
# - hosts
#
# The tests are written with the following helpers:
# - include/connection_setup.inc
# - include/connection_load.inc
# - include/connection_cleanup.inc
#
# Helpers are intended to be used as follows.
#
# A Typical test t/connection_xxx.test will consist of:
# --source ../include/connection_setup.inc
# --source ../include/connection_load.inc
# --source ../include/connection_cleanup.inc
# and a t/connection_xxx-master.opt file
#
# Naming conventions for t/connection_xxx.test are as follows:
# t/connection_<account><user><host>
#
# <account> corresponds to different sizing settings for 
# the variable performance-schema-accounts-size
# - (blank): accounts-size sufficient to represent all records
# - 3a: accounts-size set to 3
# - no_a: accounts-size set to 0
#
# <user> corresponds to different sizing settings for 
# the variable performance-schema-users-size
# - (blank): users-size sufficient to represent all records
# - 3u: users-size set to 3
# - no_u: users-size set to 0
#
# <host> corresponds to different sizing settings for 
# the variable performance-schema-hosts-size
# - (blank): hosts-size sufficient to represent all records
# - no_h: hosts-size set to 0

# ========================================
# HELPER include/event_aggregate_setup.inc
# ========================================

# Test requires: sp-protocol/ps-protocol/view-protocol/cursor-protocol disabled
--source include/no_protocol.inc
--source ../include/wait_for_pfs_thread_count.inc

--disable_query_log

create user user1@localhost, user2@localhost,
            user3@localhost, user4@localhost,
            user5@localhost;
grant ALL on *.* to user1@localhost;
grant ALL on *.* to user2@localhost;
grant ALL on *.* to user3@localhost;
grant ALL on *.* to user4@localhost;
grant ALL on *.* to user5@localhost;

flush privileges;

# Purge old users, hosts, user/host from previous tests
truncate table performance_schema.accounts;
truncate table performance_schema.users;
truncate table performance_schema.hosts;

# Save the setup

# Start from a known clean state, to avoid noise from previous tests
flush tables;
flush status;

--disable_warnings
drop procedure if exists dump_all;
--enable_warnings

delimiter $$;

create procedure dump_all()
begin
  select processlist_user, processlist_host
    from performance_schema.threads
    where (processlist_user is not null) and (processlist_host is not null)
    order by processlist_user;

  select * from performance_schema.accounts
    where (user is not null) and (host is not null)
    order by user, host;

  select * from performance_schema.users
    where user is not null
    order by user;

  select * from performance_schema.hosts
    where host is not null
    order by host;

  select variable_name, variable_value from performance_schema.global_status
    where variable_name in ('PERFORMANCE_SCHEMA_ACCOUNTS_LOST',
                            'PERFORMANCE_SCHEMA_USERS_LOST',
                            'PERFORMANCE_SCHEMA_HOSTS_LOST');
end
$$

delimiter ;$$

--enable_query_log
