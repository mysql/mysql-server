# Tests for the performance schema

# =====================================
# HELPER include/connection_cleanup.inc
# =====================================

--disable_query_log
revoke all privileges, grant option from user1@localhost;
revoke all privileges, grant option from user2@localhost;
revoke all privileges, grant option from user3@localhost;
revoke all privileges, grant option from user4@localhost;
revoke all privileges, grant option from user5@localhost;
drop user user1@localhost;
drop user user2@localhost;
drop user user3@localhost;
drop user user4@localhost;
drop user user5@localhost;
flush privileges;

drop procedure dump_all;

truncate table performance_schema.accounts;
truncate table performance_schema.users;
truncate table performance_schema.hosts;

--enable_query_log

