CREATE USER mysqltestuser1@localhost;

--eval GRANT $locking_clause_privileges ON db1.* TO mysqltestuser1@localhost;

--connect(conn, localhost, mysqltestuser1,,db1)
SELECT * FROM t1;
SELECT HIGH_PRIORITY * FROM t1;

--let $locking_clause_subquery_privileges_expected_error=$locking_clause_privileges_expected_error

--let $locking_clause_subquery_privileges_query=SELECT * FROM t1 FOR UPDATE
--source include/locking_clause_subquery_privileges.inc

--let $locking_clause_subquery_privileges_query=SELECT 1 FROM t1, t2 FOR UPDATE OF t1
--source include/locking_clause_subquery_privileges.inc

--let $locking_clause_subquery_privileges_query=SELECT * FROM t1 FOR SHARE
--source include/locking_clause_subquery_privileges.inc

--let $locking_clause_subquery_privileges_query=SELECT 1 FROM t1, t2 FOR SHARE OF t1
--source include/locking_clause_subquery_privileges.inc

--disconnect conn
--connection default
DROP USER mysqltestuser1@localhost;
