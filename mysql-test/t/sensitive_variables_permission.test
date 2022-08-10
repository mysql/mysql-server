--source include/have_debug.inc

--echo #
--echo # WL#13469: secure storage for sensitive system variables
--echo #

--echo # ----------------------------------------------------------------------
--echo # 1. User without SENSITIVE_VARIABLES_OBSERVER must
--echo #    not be able to access SENSITIVE variables

INSTALL COMPONENT 'file://component_test_sensitive_system_variables';
CREATE USER wl13469_no_privilege;

--connect(conn_no_priv, localhost, wl13469_no_privilege,,)

--echo # 1.1 Verify that a user without
--echo #     SENSITIVE_VARIABLES_OBSERVER privilege
--echo #     cannot view SENSITIVE variables' values

--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SELECT @@global.test_component.sensitive_string_1;

--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SELECT @@session.test_component.sensitive_string_1;

--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SELECT @@global.test_component.sensitive_ro_string_1;

--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SELECT @@session.test_component.sensitive_ro_string_1;

--echo # 1.2 Verify that PFS tables does not show
--echo #     SENSITIVE variables to a user with
--echo #     SENSITIVE_VARIABLES_OBSERVER privilege

--let $expected_entries = 0
--echo
--let $assert_text= PFS table global_variables should not show SENSITIVE variables for users without SENSITIVE_VARIABLES_OBSERVER privilege.
--let $assert_cond= [SELECT COUNT(*) AS entries FROM performance_schema.global_variables WHERE VARIABLE_NAME LIKE "test_component.sensitive%", entries, 1] = $expected_entries
--source include/assert.inc

--echo
--let $assert_text= PFS table session_variables should not show SENSITIVE variables for users without SENSITIVE_VARIABLES_OBSERVER privilege.
--let $assert_cond= [SELECT COUNT(*) AS entries FROM performance_schema.session_variables WHERE VARIABLE_NAME LIKE "test_component.sensitive%", entries, 1] = $expected_entries
--source include/assert.inc

--echo
--let $assert_text= PFS table variables_info should not show SENSITIVE variables for users without SENSITIVE_VARIABLES_OBSERVER privilege.
--let $assert_cond= [SELECT COUNT(*) AS entries FROM performance_schema.variables_info WHERE VARIABLE_NAME LIKE "test_component.sensitive%", entries, 1] = $expected_entries
--source include/assert.inc

--echo # 1.3 Verify that tracking state change
--echo #     of a SENSITIVE variable is not
--echo #     possible if user does not have
--echo #     SENSITIVE_VARIABLES_OBSERVER privilege

SELECT @@session.session_track_system_variables INTO @save_session_track_system_variables;
SELECT @@session.autocommit INTO @save_session_autocommit;
--enable_session_track_info
SET @@session.session_track_system_variables='autocommit, debug_sensitive_session_string';
SET @@session.autocommit= 1;
SET @@session.autocommit= 0;

--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET @@session.debug_sensitive_session_string= "haha";

--disable_session_track_info
SET @@session.autocommit= @save_session_autocommit;
SET @@session.session_track_system_variables= @save_session_track_system_variables;

--connection default
--disconnect conn_no_priv
DROP USER wl13469_no_privilege;

--echo # ----------------------------------------------------------------------
--echo # 2. User with SENSITIVE_VARIABLES_OBSERVER must
--echo #    be able to access SENSITIVE variables

CREATE USER wl13469_with_privilege;
GRANT SENSITIVE_VARIABLES_OBSERVER ON *.* TO wl13469_with_privilege;
--connect(conn_with_priv, localhost, wl13469_with_privilege,,)

--echo # 2.1 Verify that a user with
--echo #     SENSITIVE_VARIABLES_OBSERVER privilege
--echo #     can view SENSITIVE variables' values

SELECT @@global.test_component.sensitive_string_1;

--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SELECT @@session.test_component.sensitive_string_1;

SELECT @@global.test_component.sensitive_ro_string_1;

--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SELECT @@session.test_component.sensitive_ro_string_1;

--echo # 2.2 Verify that PFS tables show SENSITIVE
--echo #     variables to a user wit
--echo #     SENSITIVE_VARIABLES_OBSERVER privilege

--let $expected_entries = 6
--echo
--let $assert_text= PFS table global_variables should show SENSITIVE variables for users with SENSITIVE_VARIABLES_OBSERVER privilege.
--let $assert_cond= [SELECT COUNT(*) AS entries FROM performance_schema.global_variables WHERE VARIABLE_NAME LIKE "test_component.sensitive%", entries, 1] = $expected_entries
--source include/assert.inc

--echo
--let $assert_text= PFS table session_variables should show SENSITIVE variables for users with SENSITIVE_VARIABLES_OBSERVER privilege.
--let $assert_cond= [SELECT COUNT(*) AS entries FROM performance_schema.session_variables WHERE VARIABLE_NAME LIKE "test_component.sensitive%", entries, 1] = $expected_entries
--source include/assert.inc

--echo
--let $assert_text= PFS table variables_info should show SENSITIVE variables for users with SENSITIVE_VARIABLES_OBSERVER privilege.
--let $assert_cond= [SELECT COUNT(*) AS entries FROM performance_schema.variables_info WHERE VARIABLE_NAME LIKE "test_component.sensitive%", entries, 1] = $expected_entries
--source include/assert.inc

--echo # 2.3 User with SENSITIVE_VARIABLES_OBSERVER
--echo #     should not be able to set the value of
--echo #     SENSITIVE variables.

--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET GLOBAL test_component.sensitive_string_1 = 'haha';

--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET GLOBAL test_component.sensitive_string_2 = 'hoho';

--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET GLOBAL test_component.sensitive_string_3 = 'hehe';

--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET GLOBAL debug_sensitive_session_string = 'hehe';

--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET PERSIST test_component.sensitive_string_1 = 'haha';

--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET PERSIST test_component.sensitive_string_2 = 'hoho';

--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET PERSIST test_component.sensitive_string_3 = 'hehe';

--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET PERSIST debug_sensitive_session_string = 'hehe';

--error ER_PERSIST_ONLY_ACCESS_DENIED_ERROR
SET PERSIST_ONLY test_component.sensitive_string_1 = 'haha';

--error ER_PERSIST_ONLY_ACCESS_DENIED_ERROR
SET PERSIST_ONLY test_component.sensitive_string_2 = 'hoho';

--error ER_PERSIST_ONLY_ACCESS_DENIED_ERROR
SET PERSIST_ONLY test_component.sensitive_string_3 = 'hehe';

--error ER_PERSIST_ONLY_ACCESS_DENIED_ERROR
SET PERSIST_ONLY debug_sensitive_session_string = 'hehe';

--error ER_PERSIST_ONLY_ACCESS_DENIED_ERROR
SET PERSIST_ONLY test_component.sensitive_ro_string_1 = 'haha';

--error ER_PERSIST_ONLY_ACCESS_DENIED_ERROR
SET PERSIST_ONLY test_component.sensitive_ro_string_2 = 'hoho';

--error ER_PERSIST_ONLY_ACCESS_DENIED_ERROR
SET PERSIST_ONLY test_component.sensitive_ro_string_3 = 'hehe';

--echo # 2.4 Verify that tracking state change of a
--echo #     SENSITIVE variable is possible if user has
--echo #     SENSITIVE_VARIABLES_OBSERVER privilege

--echo # Session tracking
SELECT @@session.session_track_system_variables INTO @save_session_track_system_variables;
SELECT @@session.autocommit INTO @save_session_autocommit;
SELECT @@session.debug_sensitive_session_string INTO @save_debug_sensitive_session_string;
--enable_session_track_info
SET @@session.session_track_system_variables='autocommit, debug_sensitive_session_string';
SET @@session.autocommit= 1;
SET @@session.autocommit= 0;

SET @@session.debug_sensitive_session_string= "haha";
SET @@session.debug_sensitive_session_string= "hoho";

--disable_session_track_info
SET @@session.debug_sensitive_session_string = @save_debug_sensitive_session_string;
SET @@session.autocommit= @save_session_autocommit;
SET @@session.session_track_system_variables= @save_session_track_system_variables;
--connection default
--disconnect conn_with_priv
DROP USER wl13469_with_privilege;
UNINSTALL COMPONENT "file://component_test_sensitive_system_variables";
--echo # ----------------------------------------------------------------------
