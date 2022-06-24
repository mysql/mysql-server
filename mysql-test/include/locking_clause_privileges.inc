#
# locking_clause_privileges.inc
#
# Parameterized test that runs some queries as a test user with FOR UPDATE,
# FOR SHARE, and without locking clause on one of the tables.
#
# Arguments:
#
# - locking_clause_privileges
#       The privileges that the user will have on the test table
#
# - locking_clause_privileges_expected_error_no_clause
#       Expected error when there is no locking clause
#
# - locking_clause_privileges_expected_error_for_update
#       Expected error when the locking mode is FOR UPDATE
#
# - locking_clause_privileges_expected_error_for_share
#       Expected error when the locking mode is FOR SHARE

CREATE USER mysqltestuser1@localhost;

--eval GRANT $locking_clause_privileges ON db1.* TO mysqltestuser1@localhost

--connect(conn, localhost, mysqltestuser1,,db1)
--error $locking_clause_privileges_expected_error_no_clause
SELECT * FROM t1;
--error $locking_clause_privileges_expected_error_no_clause
SELECT HIGH_PRIORITY * FROM t1;

--let $locking_clause_subquery_privileges_expected_error=$locking_clause_privileges_expected_error_for_update

--let $locking_clause_subquery_privileges_query=SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ * FROM t1 FOR UPDATE
--source include/locking_clause_subquery_privileges.inc

--let $locking_clause_subquery_privileges_query=SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ 1 FROM t1, t2 FOR UPDATE OF t1
--source include/locking_clause_subquery_privileges.inc


--let $locking_clause_subquery_privileges_expected_error=$locking_clause_privileges_expected_error_for_share

--let $locking_clause_subquery_privileges_query=SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ * FROM t1 FOR SHARE
--source include/locking_clause_subquery_privileges.inc

--let $locking_clause_subquery_privileges_query=SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ 1 FROM t1, t2 FOR SHARE OF t1
--source include/locking_clause_subquery_privileges.inc

--disconnect conn
--connection default
DROP USER mysqltestuser1@localhost;
