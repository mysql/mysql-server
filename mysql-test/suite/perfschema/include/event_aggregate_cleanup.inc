# Tests for the performance schema

# ==========================================
# HELPER include/event_aggregate_cleanup.inc
# ==========================================

--disable_query_log
revoke all privileges, grant option from user1@localhost;
revoke all privileges, grant option from user2@localhost;
revoke all privileges, grant option from user3@localhost;
revoke all privileges, grant option from user4@localhost;
drop user user1@localhost;
drop user user2@localhost;
drop user user3@localhost;
drop user user4@localhost;
flush privileges;

drop procedure dump_thread;
drop procedure dump_one_thread;

drop prepare dump_waits_account;
drop prepare dump_waits_user;
drop prepare dump_waits_host;
drop prepare dump_waits_history;
drop prepare dump_waits_global;

drop prepare dump_stages_account;
drop prepare dump_stages_user;
drop prepare dump_stages_host;
drop prepare dump_stages_history;
drop prepare dump_stages_global;

drop prepare dump_statements_account;
drop prepare dump_statements_user;
drop prepare dump_statements_host;
drop prepare dump_statements_history;
drop prepare dump_statements_global;

drop prepare dump_users;
drop prepare dump_hosts;
drop prepare dump_accounts;

truncate table performance_schema.accounts;
truncate table performance_schema.users;
truncate table performance_schema.hosts;

truncate table performance_schema.setup_actors;
insert into performance_schema.setup_actors
  select * from test.setup_actors;
drop table test.setup_actors;
drop table test.t1;

update performance_schema.threads set instrumented='YES';
update performance_schema.setup_instruments set enabled='YES', timed='YES';

--enable_query_log

