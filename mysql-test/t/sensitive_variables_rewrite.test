--source include/have_debug.inc
--source include/no_ps_protocol.inc

--echo #
--echo # WL#13469: secure storage for sensitive system variables
--echo #

--echo # Test for sensitive variables value rewrite functionality

--disable_query_log
CALL mtr.add_suppression('Persisting SENSITIVE variables in encrypted form requires keyring component loaded through manifest file.');
--enable_query_log

--echo #
--echo # Setup
--echo #
INSTALL COMPONENT 'file://component_test_sensitive_system_variables';

# make sure we start with a clean slate. log_tables.test says this is OK.
TRUNCATE TABLE mysql.general_log;

LET old_log_output=          `select @@global.log_output`;
LET old_general_log=         `select @@global.general_log`;
LET old_general_log_file=    `select @@global.general_log_file`;

SELECT @@session.autocommit INTO @save_session_autocommit;
SELECT @@session.debug_sensitive_session_string INTO @save_debug_sensitive_session_string;

--replace_result $MYSQLTEST_VARDIR ...
eval SET GLOBAL general_log_file = '$MYSQLTEST_VARDIR/log/rewrite_general.log';
SET GLOBAL log_output =       'FILE,TABLE';
SET GLOBAL general_log=       'ON';

--echo #
--echo # Set sensitive system variables
--echo #

SET @@session.debug_sensitive_session_string= "haha";
SET @@session.autocommit = 0, @@session.debug_sensitive_session_string= "haha";
SET GLOBAL test_component.sensitive_string_1 = "haha";
SET PERSIST test_component.sensitive_string_2 = "haha";
SET PERSIST_ONLY test_component.sensitive_ro_string_1 = 'haha';
SET @@session.debug_sensitive_session_string = @save_debug_sensitive_session_string;
SET @@session.autocommit= @save_session_autocommit;
RESET PERSIST;

--echo #
--echo # Check error log data
--echo #

--echo # Must be 1 (Single SELECT statement)
SELECT COUNT(*) FROM mysql.general_log WHERE argument LIKE ('%haha%');

--echo # Must be 7 (6 SET statement + 1 SELECT statement)
SELECT COUNT(*) FROM mysql.general_log WHERE argument LIKE ('%REDACTED%');

--echo # List all SET statements
SELECT argument FROM mysql.general_log WHERE argument LIKE ('SET%');

--echo #
--echo # cleanup
--echo #

--remove_file $MYSQLTEST_VARDIR/log/rewrite_general.log
--replace_result $MYSQLTEST_VARDIR ...
eval SET GLOBAL general_log_file =  '$old_general_log_file';
eval SET GLOBAL log_output=        '$old_log_output';
eval SET GLOBAL general_log=       $old_general_log;
TRUNCATE TABLE mysql.general_log;
UNINSTALL COMPONENT "file://component_test_sensitive_system_variables";
let $MYSQLD_DATADIR= `select @@datadir`;
--remove_file $MYSQLD_DATADIR/mysqld-auto.cnf
--source include/force_restart.inc
