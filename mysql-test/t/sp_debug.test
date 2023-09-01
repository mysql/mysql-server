#
--source include/have_debug.inc

--echo #
--echo # WL#7897 -- Use DD API for Stored Routines.
--echo #

--echo # Test case to verify stored routine load failure.
CREATE FUNCTION f1() RETURNS INT return 1;
CREATE PROCEDURE p1() SELECT 1 AS my_column;

SET DEBUG='+d,fail_stored_routine_load';
--error ER_SP_LOAD_FAILED
SELECT f1();
--error ER_SP_LOAD_FAILED
CALL p1();
SET DEBUG='-d,fail_stored_routine_load';

SELECT f1();
CALL p1();

DROP FUNCTION f1;
DROP PROCEDURE p1;

--echo # Test case to verify stored routine body length error.
SET DEBUG='+d,simulate_routine_length_error';
--error ER_TOO_LONG_BODY
CREATE PROCEDURE p1() SELECT "simulate_routine_length_error";
SET DEBUG='-d,simulate_routine_length_error';

--echo # Test case to verify the schema state after failure to drop routine.
CREATE SCHEMA new_db;
CREATE PROCEDURE new_db.proc() SELECT 1 AS my_column;

SET DEBUG='+d,fail_drop_db_routines';
--error ER_SP_DROP_FAILED
DROP SCHEMA IF EXISTS new_db;
SET DEBUG='-d,fail_drop_db_routines';

# Failure to drop routines in previous statement should not leave Schema in
# inconsistent state. Following DROP SCHEMA should work fine.
DROP SCHEMA IF EXISTS new_db;

# Creating schema with same name again should work fine.
CREATE SCHEMA new_db;

# Cleanup
DROP SCHEMA new_db;

--echo #
--echo # Bug#26040870 - ASSERT ON KILL'ING A STORED ROUTINE INVOCATION.
--echo #

CREATE TABLE t1 (a INT);
DELIMITER |;
CREATE FUNCTION f1() RETURNS INT
BEGIN
  INSERT INTO t1 VALUES (1);
  RETURN 1;
END|
DELIMITER ;|

--connect(con1,localhost,root)
--let $sp_con_id= `SELECT CONNECTION_ID()`
SET DEBUG_SYNC= "sp_lex_instr_before_exec_core SIGNAL sp_ready WAIT_FOR sp_finish";
send SELECT f1();

--connection default
SET DEBUG_SYNC="now WAIT_FOR sp_ready";
--replace_result $sp_con_id sp_con_id
--eval KILL QUERY $sp_con_id
SET DEBUG_SYNC="now SIGNAL sp_finish";

--connection con1
--echo # Diagnostics area is not set if routine statement execution is
--echo # interrupted by the KILL operation. Accessing diagnostics area in such
--echo # case results in the issue reported.
--echo # Patch for the bug25586773, checks if diagnostics area is set before
--echo # accessing it.
--error ER_QUERY_INTERRUPTED
reap;

--connection default
SET DEBUG_SYNC='RESET';
DROP TABLE t1;
DROP FUNCTION f1;
disconnect con1;

--echo #
--echo # Bug#28864244 : DATA DICTIONARY ASSERT IN MEB.UNICODE.
--echo #
SET NAMES utf8mb3;
SET DEBUG='+d,simulate_lctn_two_case_for_schema_case_compare';
CREATE DATABASE `tèst-db`;
--echo #Without fix, assert condition to check schema name case fails for following
--echo #statement.
CREATE PROCEDURE `tèst-db`.test() SELECT 1;
DROP DATABASE `tèst-db`;
SET DEBUG='-d,simulate_lctn_two_case_for_schema_case_compare';
SET NAMES default;

--echo
--echo #############################################################
--echo #  WL#9049 Add a dynamic privilege for stored routine backup.
--echo #
--echo #    Test Cases to verify that user with SHOW_ROUTINE can
--echo #    view all the SP/SF code and maintain behaviour of
--echo #    statements for user with global SELECT privilege
--echo #    and for user who is the routine DEFINER.
--echo #
--echo #############################################################
--echo

# Save the initial number of concurrent sessions
--source include/count_sessions.inc

CREATE SCHEMA testdb;

--echo
--echo # Create users.
--echo
CREATE USER usr_no_priv@localhost, usr_show_routine@localhost, usr_global_select@localhost, usr_definer@localhost, usr_role@localhost, usr_create_routine@localhost, usr_alter_routine@localhost, usr_execute@localhost;

--echo
--echo # Create and grant role.
--echo
CREATE ROLE role_show_routine;
GRANT role_show_routine to usr_role@localhost;

--echo
--echo # Grant privileges.
--echo
GRANT SHOW_ROUTINE ON *.* TO usr_show_routine@localhost, role_show_routine;
GRANT SELECT ON *.* TO usr_global_select@localhost;
GRANT EXECUTE ON *.* TO usr_execute@localhost;
GRANT CREATE ROUTINE ON *.* TO usr_create_routine@localhost;
GRANT ALTER ROUTINE ON *.* TO usr_alter_routine@localhost;

--echo
--echo # Create routines whose definer is root@localhost.
--echo
CREATE PROCEDURE testdb.proc_root() SELECT "ProcRoot";
CREATE FUNCTION testdb.func_root() RETURNS VARCHAR(8) DETERMINISTIC RETURN "FuncRoot";

--echo
--echo # Create routines whose definer is usr_definer@localhost.
--echo
CREATE DEFINER = `usr_definer`@`localhost` PROCEDURE testdb.proc_definer() SELECT "ProcDefiner";
CREATE DEFINER = `usr_definer`@`localhost` FUNCTION testdb.func_definer() RETURNS VARCHAR(11) DETERMINISTIC RETURN "FuncDefiner";

--echo
--echo # Connect as user with no privileges.
--echo
--connect (conn1, localhost, usr_no_priv,,)

--echo
--echo # Should pretend that routine does not exist.
--echo
--error ER_SP_DOES_NOT_EXIST
SHOW PROCEDURE CODE testdb.proc_root;
--error ER_SP_DOES_NOT_EXIST
SHOW PROCEDURE CODE testdb.proc_definer;
--error ER_SP_DOES_NOT_EXIST
SHOW FUNCTION CODE testdb.func_root;
--error ER_SP_DOES_NOT_EXIST
SHOW FUNCTION CODE testdb.func_definer;

--disconnect conn1

--echo
--echo # Connect as user with SHOW_ROUTINE privilege.
--echo
--connect (conn1, localhost, usr_show_routine,,)

--echo
--echo # Should show the Instructions of all 4 routines.
--echo
SHOW PROCEDURE CODE testdb.proc_root;
SHOW PROCEDURE CODE testdb.proc_definer;
SHOW FUNCTION CODE testdb.func_root;
SHOW FUNCTION CODE testdb.func_definer;

--disconnect conn1

--echo
--echo # Connect as user with role assigned.
--echo
--connect (conn1, localhost, usr_role,,)

--echo
--echo # Without activating role.
--echo

--echo
--echo # Should pretend that routine does not exist.
--echo
--error ER_SP_DOES_NOT_EXIST
SHOW PROCEDURE CODE testdb.proc_root;
--error ER_SP_DOES_NOT_EXIST
SHOW PROCEDURE CODE testdb.proc_definer;
--error ER_SP_DOES_NOT_EXIST
SHOW FUNCTION CODE testdb.func_root;
--error ER_SP_DOES_NOT_EXIST
SHOW FUNCTION CODE testdb.func_definer;

--echo
--echo # Activate role.
--echo
SET ROLE role_show_routine;

--echo
--echo # Should show the Instructions of all 4 routines.
--echo
SHOW PROCEDURE CODE testdb.proc_root;
SHOW PROCEDURE CODE testdb.proc_definer;
SHOW FUNCTION CODE testdb.func_root;
SHOW FUNCTION CODE testdb.func_definer;

--disconnect conn1

--echo
--echo # Connect as user with global SELECT privilege.
--echo
--connect (conn1, localhost, usr_global_select,,)

--echo
--echo # Should show the Instructions of all 4 routines.
--echo
SHOW PROCEDURE CODE testdb.proc_root;
SHOW PROCEDURE CODE testdb.proc_definer;
SHOW FUNCTION CODE testdb.func_root;
SHOW FUNCTION CODE testdb.func_definer;

--disconnect conn1

--echo
--echo # Connect as user who is DEFINER of routines proc_definer and func_definer.
--echo
--connect (conn1, localhost, usr_definer,,)

--echo
--echo # Should pretend that proc_root and func_root do not exist.
--echo
--error ER_SP_DOES_NOT_EXIST
SHOW PROCEDURE CODE testdb.proc_root;
--error ER_SP_DOES_NOT_EXIST
SHOW FUNCTION CODE testdb.func_root;

--echo
--echo # Should show the Instructions of proc_definer and func_definer.
--echo
SHOW PROCEDURE CODE testdb.proc_definer;
SHOW FUNCTION CODE testdb.func_definer;

--disconnect conn1

--echo
--echo # Connect as user who has EXECUTE privilege.
--echo
--connect (conn1, localhost, usr_execute,,)

--echo
--echo # Should pretend that routine does not exist.
--echo
--error ER_SP_DOES_NOT_EXIST
SHOW PROCEDURE CODE testdb.proc_root;
--error ER_SP_DOES_NOT_EXIST
SHOW PROCEDURE CODE testdb.proc_definer;
--error ER_SP_DOES_NOT_EXIST
SHOW FUNCTION CODE testdb.func_root;
--error ER_SP_DOES_NOT_EXIST
SHOW FUNCTION CODE testdb.func_definer;

--disconnect conn1

--echo
--echo # Connect as user who has CREATE ROUTINE privilege.
--echo
--connect (conn1, localhost, usr_create_routine,,)

--echo
--echo # Should pretend that routine does not exist.
--echo
--error ER_SP_DOES_NOT_EXIST
SHOW PROCEDURE CODE testdb.proc_root;
--error ER_SP_DOES_NOT_EXIST
SHOW PROCEDURE CODE testdb.proc_definer;
--error ER_SP_DOES_NOT_EXIST
SHOW FUNCTION CODE testdb.func_root;
--error ER_SP_DOES_NOT_EXIST
SHOW FUNCTION CODE testdb.func_definer;

--disconnect conn1

--echo
--echo # Connect as user who has ALTER ROUTINE privilege.
--echo
--connect (conn1, localhost, usr_alter_routine,,)

--echo
--echo # Should pretend that routine does not exist.
--echo
--error ER_SP_DOES_NOT_EXIST
SHOW PROCEDURE CODE testdb.proc_root;
--error ER_SP_DOES_NOT_EXIST
SHOW PROCEDURE CODE testdb.proc_definer;
--error ER_SP_DOES_NOT_EXIST
SHOW FUNCTION CODE testdb.func_root;
--error ER_SP_DOES_NOT_EXIST
SHOW FUNCTION CODE testdb.func_definer;

--disconnect conn1

--echo
--echo # Test showing code after partial revoke of SELECT privilege.
--echo
--connection default

--echo
--echo # Set partial revokes to ON and save the current value.
--echo
SET @start_partial_revokes = @@global.partial_revokes;
SET @@global.partial_revokes=ON;

REVOKE SELECT ON testdb.* FROM usr_global_select@localhost;
--connect (conn1, localhost, usr_global_select,,)

--echo
--echo # Should pretend that routine does not exist.
--echo
--error ER_SP_DOES_NOT_EXIST
SHOW PROCEDURE CODE testdb.proc_root;
--error ER_SP_DOES_NOT_EXIST
SHOW PROCEDURE CODE testdb.proc_definer;
--error ER_SP_DOES_NOT_EXIST
SHOW FUNCTION CODE testdb.func_root;
--error ER_SP_DOES_NOT_EXIST
SHOW FUNCTION CODE testdb.func_definer;

--disconnect conn1

--echo
--echo # Restore partial_revokes to its previous value.
--echo
--connection default
DROP USER usr_global_select@localhost;
SET @@global.partial_revokes = @start_partial_revokes;

--echo
--echo # Cleanup.
--echo
DROP USER usr_no_priv@localhost, usr_show_routine@localhost, usr_definer@localhost, usr_role@localhost, usr_create_routine@localhost, usr_alter_routine@localhost, usr_execute@localhost;
DROP ROLE role_show_routine;
DROP SCHEMA testdb;

--echo #
--echo # Bug#35067523: heap-use-after-free in sp_head::is_sql when calling stored function
--echo #
--echo #

CREATE FUNCTION f1() RETURNS INT return 1;
CREATE PROCEDURE p1() SELECT 1;

SET SESSION DEBUG='+d,simulate_dd_elements_cache_full';
--echo # DD object for routines f1 and p1 are not cached in the DD caches.
--echo # sp_head object is created from the DD object, and it is cached.
SELECT f1();
CALL p1();

--echo # Subsequent calls to routines uses sp_head cached.
--echo # Without fix, member of sp_head referring to language in DD object,
--echo # instead of creating copy of it, results in the issue reported.
SELECT f1();
CALL p1();
SET SESSION DEBUG='-d,simulate_dd_elements_cache_full';

DROP PROCEDURE p1;
DROP FUNCTION f1;

# Wait till all disconnects are completed.
--source include/wait_until_count_sessions.inc
