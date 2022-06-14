--echo #
--echo # Test of locking clause privileges.
--echo #


--echo #
--echo #  Test of the parser tokens for use as role names.
--echo #

--error ER_PARSE_ERROR
CREATE ROLE of;
CREATE ROLE skip;
CREATE ROLE locked;
CREATE ROLE nowait;

DROP ROLE skip, locked, nowait;

CREATE DATABASE db1;
CREATE TABLE db1.t1( a INT );
CREATE TABLE db1.t2( a INT );

--let $locking_clause_privileges=SELECT
--let $locking_clause_privileges_expected_error_no_clause=0
--let $locking_clause_privileges_expected_error_for_update=ER_TABLEACCESS_DENIED_ERROR
--let $locking_clause_privileges_expected_error_for_share=0
--source include/locking_clause_privileges.inc

--let $locking_clause_privileges=INSERT
--let $locking_clause_privileges_expected_error_no_clause=ER_TABLEACCESS_DENIED_ERROR
--let $locking_clause_privileges_expected_error_for_share=ER_TABLEACCESS_DENIED_ERROR
--let $locking_clause_privileges_expected_error_for_update=ER_TABLEACCESS_DENIED_ERROR
--source include/locking_clause_privileges.inc

--let $locking_clause_privileges=UPDATE
--let $locking_clause_privileges_expected_error_no_clause=ER_TABLEACCESS_DENIED_ERROR
--let $locking_clause_privileges_expected_error_for_share=ER_TABLEACCESS_DENIED_ERROR
--let $locking_clause_privileges_expected_error_for_update=ER_TABLEACCESS_DENIED_ERROR
--source include/locking_clause_privileges.inc

--let $locking_clause_privileges=DELETE
--let $locking_clause_privileges_expected_error_no_clause=ER_TABLEACCESS_DENIED_ERROR
--let $locking_clause_privileges_expected_error_for_update=ER_TABLEACCESS_DENIED_ERROR
--let $locking_clause_privileges_expected_error_for_share=ER_TABLEACCESS_DENIED_ERROR
--source include/locking_clause_privileges.inc

--let $locking_clause_privileges=LOCK TABLES
--let $locking_clause_privileges_expected_error_no_clause=ER_TABLEACCESS_DENIED_ERROR
--let $locking_clause_privileges_expected_error_for_update=ER_TABLEACCESS_DENIED_ERROR
--let $locking_clause_privileges_expected_error_for_share=ER_TABLEACCESS_DENIED_ERROR
--source include/locking_clause_privileges.inc

--let $locking_clause_privileges=SELECT, UPDATE
--let $locking_clause_privileges_expected_error_no_clause=0
--let $locking_clause_privileges_expected_error_for_update=0
--let $locking_clause_privileges_expected_error_for_share=0
--source include/locking_clause_privileges.inc

--let $locking_clause_privileges=SELECT, UPDATE, DELETE
--let $locking_clause_privileges_expected_error_no_clause=0
--let $locking_clause_privileges_expected_error_for_update=0
--let $locking_clause_privileges_expected_error_for_share=0
--source include/locking_clause_privileges.inc

--let $locking_clause_privileges=SELECT, DELETE
--let $locking_clause_privileges_expected_error_no_clause=0
--let $locking_clause_privileges_expected_error_for_update=0
--let $locking_clause_privileges_expected_error_for_share=0
--source include/locking_clause_privileges.inc

--let $locking_clause_privileges=SELECT, LOCK TABLES
--let $locking_clause_privileges_expected_error_no_clause=0
--let $locking_clause_privileges_expected_error_for_update=0
--let $locking_clause_privileges_expected_error_for_share=0
--source include/locking_clause_privileges.inc

DROP DATABASE db1;

--echo #
--echo # Bug#28581664: SELECT FOR UPDATE PRODUCES ERROR 1143
--echo # (42000) SINCE MYSQL 8
--echo #

CREATE DATABASE db1;

CREATE TABLE db1.t1 ( a INT );

INSERT INTO db1.t1 VALUES ( 1 );

CREATE USER mysqluser1;
GRANT ALL ON db1.t1 TO mysqluser1;

--connect(conn1, localhost, mysqluser1, , db1)
--connection conn1

SELECT * FROM t1 WHERE a = 1 FOR UPDATE;

--disconnect conn1
--connection default

DROP DATABASE db1;
DROP USER mysqluser1;
