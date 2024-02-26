#
# WL#13469: secure storage for sensitive system variables
#
# ----------------------------------------------------------------------
# 1. User without SENSITIVE_VARIABLES_OBSERVER must
#    not be able to access SENSITIVE variables
INSTALL COMPONENT 'file://component_test_sensitive_system_variables';
CREATE USER wl13469_no_privilege;
# 1.1 Verify that a user without
#     SENSITIVE_VARIABLES_OBSERVER privilege
#     cannot view SENSITIVE variables' values
SELECT @@global.test_component.sensitive_string_1;
ERROR 42000: Access denied; you need (at least one of) the SENSITIVE_VARIABLES_OBSERVER privilege(s) for this operation
SELECT @@session.test_component.sensitive_string_1;
ERROR 42000: Access denied; you need (at least one of) the SENSITIVE_VARIABLES_OBSERVER privilege(s) for this operation
SELECT @@global.test_component.sensitive_ro_string_1;
ERROR 42000: Access denied; you need (at least one of) the SENSITIVE_VARIABLES_OBSERVER privilege(s) for this operation
SELECT @@session.test_component.sensitive_ro_string_1;
ERROR 42000: Access denied; you need (at least one of) the SENSITIVE_VARIABLES_OBSERVER privilege(s) for this operation
# 1.2 Verify that PFS tables does not show
#     SENSITIVE variables to a user with
#     SENSITIVE_VARIABLES_OBSERVER privilege

include/assert.inc [PFS table global_variables should not show SENSITIVE variables for users without SENSITIVE_VARIABLES_OBSERVER privilege.]

include/assert.inc [PFS table session_variables should not show SENSITIVE variables for users without SENSITIVE_VARIABLES_OBSERVER privilege.]

include/assert.inc [PFS table variables_info should not show SENSITIVE variables for users without SENSITIVE_VARIABLES_OBSERVER privilege.]
# 1.3 Verify that tracking state change
#     of a SENSITIVE variable is not
#     possible if user does not have
#     SENSITIVE_VARIABLES_OBSERVER privilege
SELECT @@session.session_track_system_variables INTO @save_session_track_system_variables;
SELECT @@session.autocommit INTO @save_session_autocommit;
SET @@session.session_track_system_variables='autocommit, debug_sensitive_session_string';
Warnings:
Warning	1231	 debug_sensitive_session_string is not a valid system variable and will be ignored.
SET @@session.autocommit= 1;
-- Tracker : SESSION_TRACK_SYSTEM_VARIABLES
-- autocommit
-- ON

SET @@session.autocommit= 0;
-- Tracker : SESSION_TRACK_SYSTEM_VARIABLES
-- autocommit
-- OFF

SET @@session.debug_sensitive_session_string= "haha";
ERROR 42000: Access denied; you need (at least one of) the SENSITIVE_VARIABLES_OBSERVER privilege(s) for this operation
SET @@session.autocommit= @save_session_autocommit;
SET @@session.session_track_system_variables= @save_session_track_system_variables;
DROP USER wl13469_no_privilege;
# ----------------------------------------------------------------------
# 2. User with SENSITIVE_VARIABLES_OBSERVER must
#    be able to access SENSITIVE variables
CREATE USER wl13469_with_privilege;
GRANT SENSITIVE_VARIABLES_OBSERVER ON *.* TO wl13469_with_privilege;
# 2.1 Verify that a user with
#     SENSITIVE_VARIABLES_OBSERVER privilege
#     can view SENSITIVE variables' values
SELECT @@global.test_component.sensitive_string_1;
@@global.test_component.sensitive_string_1
NULL
SELECT @@session.test_component.sensitive_string_1;
ERROR HY000: Variable 'test_component.sensitive_string_1' is a GLOBAL variable
SELECT @@global.test_component.sensitive_ro_string_1;
@@global.test_component.sensitive_ro_string_1
NULL
SELECT @@session.test_component.sensitive_ro_string_1;
ERROR HY000: Variable 'test_component.sensitive_ro_string_1' is a GLOBAL variable
# 2.2 Verify that PFS tables show SENSITIVE
#     variables to a user wit
#     SENSITIVE_VARIABLES_OBSERVER privilege

include/assert.inc [PFS table global_variables should show SENSITIVE variables for users with SENSITIVE_VARIABLES_OBSERVER privilege.]

include/assert.inc [PFS table session_variables should show SENSITIVE variables for users with SENSITIVE_VARIABLES_OBSERVER privilege.]

include/assert.inc [PFS table variables_info should show SENSITIVE variables for users with SENSITIVE_VARIABLES_OBSERVER privilege.]
# 2.3 User with SENSITIVE_VARIABLES_OBSERVER
#     should not be able to set the value of
#     SENSITIVE variables.
SET GLOBAL test_component.sensitive_string_1 = 'haha';
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
SET GLOBAL test_component.sensitive_string_2 = 'hoho';
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
SET GLOBAL test_component.sensitive_string_3 = 'hehe';
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
SET GLOBAL debug_sensitive_session_string = 'hehe';
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
SET PERSIST test_component.sensitive_string_1 = 'haha';
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
SET PERSIST test_component.sensitive_string_2 = 'hoho';
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
SET PERSIST test_component.sensitive_string_3 = 'hehe';
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
SET PERSIST debug_sensitive_session_string = 'hehe';
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
SET PERSIST_ONLY test_component.sensitive_string_1 = 'haha';
ERROR 42000: Access denied; you need SYSTEM_VARIABLES_ADMIN and PERSIST_RO_VARIABLES_ADMIN privileges for this operation
SET PERSIST_ONLY test_component.sensitive_string_2 = 'hoho';
ERROR 42000: Access denied; you need SYSTEM_VARIABLES_ADMIN and PERSIST_RO_VARIABLES_ADMIN privileges for this operation
SET PERSIST_ONLY test_component.sensitive_string_3 = 'hehe';
ERROR 42000: Access denied; you need SYSTEM_VARIABLES_ADMIN and PERSIST_RO_VARIABLES_ADMIN privileges for this operation
SET PERSIST_ONLY debug_sensitive_session_string = 'hehe';
ERROR 42000: Access denied; you need SYSTEM_VARIABLES_ADMIN and PERSIST_RO_VARIABLES_ADMIN privileges for this operation
SET PERSIST_ONLY test_component.sensitive_ro_string_1 = 'haha';
ERROR 42000: Access denied; you need SYSTEM_VARIABLES_ADMIN and PERSIST_RO_VARIABLES_ADMIN privileges for this operation
SET PERSIST_ONLY test_component.sensitive_ro_string_2 = 'hoho';
ERROR 42000: Access denied; you need SYSTEM_VARIABLES_ADMIN and PERSIST_RO_VARIABLES_ADMIN privileges for this operation
SET PERSIST_ONLY test_component.sensitive_ro_string_3 = 'hehe';
ERROR 42000: Access denied; you need SYSTEM_VARIABLES_ADMIN and PERSIST_RO_VARIABLES_ADMIN privileges for this operation
# 2.4 Verify that tracking state change of a
#     SENSITIVE variable is possible if user has
#     SENSITIVE_VARIABLES_OBSERVER privilege
# Session tracking
SELECT @@session.session_track_system_variables INTO @save_session_track_system_variables;
SELECT @@session.autocommit INTO @save_session_autocommit;
SELECT @@session.debug_sensitive_session_string INTO @save_debug_sensitive_session_string;
SET @@session.session_track_system_variables='autocommit, debug_sensitive_session_string';
SET @@session.autocommit= 1;
-- Tracker : SESSION_TRACK_SYSTEM_VARIABLES
-- autocommit
-- ON

SET @@session.autocommit= 0;
-- Tracker : SESSION_TRACK_SYSTEM_VARIABLES
-- autocommit
-- OFF

SET @@session.debug_sensitive_session_string= "haha";
-- Tracker : SESSION_TRACK_SYSTEM_VARIABLES
-- debug_sensitive_session_string
-- haha

SET @@session.debug_sensitive_session_string= "hoho";
-- Tracker : SESSION_TRACK_SYSTEM_VARIABLES
-- debug_sensitive_session_string
-- hoho

SET @@session.debug_sensitive_session_string = @save_debug_sensitive_session_string;
SET @@session.autocommit= @save_session_autocommit;
SET @@session.session_track_system_variables= @save_session_track_system_variables;
DROP USER wl13469_with_privilege;
UNINSTALL COMPONENT "file://component_test_sensitive_system_variables";
# ----------------------------------------------------------------------
