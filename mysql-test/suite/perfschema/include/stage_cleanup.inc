# Tests for the performance schema

# ================================
# HELPER include/stage_cleanup.inc
# ================================

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

truncate table performance_schema.setup_actors;
insert into performance_schema.setup_actors
  select * from test.setup_actors;
drop table test.setup_actors;
drop table test.t1;

update performance_schema.threads set instrumented='YES';
update performance_schema.setup_instruments set enabled='YES', timed='YES';

--enable_query_log

