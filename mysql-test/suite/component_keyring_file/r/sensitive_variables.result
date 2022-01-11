# ----------------------------------------------------------------------
# Setup
# Creating local configuration file for keyring component: component_keyring_file
# Creating manifest file for current MySQL server instance
# Re-starting mysql server with manifest file
# ----------------------------------------------------------------------
#
# WL#13469: secure storage for sensitive system variables
#
# ----------------------------------------------------------------------
# 1. Tests for persisted SENSITIVE variables
INSTALL COMPONENT "file://component_test_sensitive_system_variables";
CREATE USER wl13469_no_privilege;
SET PERSIST test_component.sensitive_string_1 = 'haha';
SET PERSIST_ONLY test_component.sensitive_ro_string_1 = 'haha';

include/assert.inc [PFS table persisted_variables should show SENSITIVE variables for users with SENSITIVE_VARIABLES_OBSERVER privilege]

include/assert.inc [PFS table persisted_variables should not show SENSITIVE variables for users without SENSITIVE_VARIABLES_OBSERVER privilege]
DROP USER wl13469_no_privilege;
# Stop the running server.
# Restart server with keyring component
SELECT a.variable_name, b.variable_value, a.variable_source FROM performance_schema.variables_info AS a, performance_schema.global_variables AS b WHERE a.variable_name = b.variable_name AND a.variable_name LIKE 'test_component.sensitive%';
variable_name	variable_value	variable_source
test_component.sensitive_ro_string_1	haha	PERSISTED
test_component.sensitive_ro_string_2		COMPILED
test_component.sensitive_ro_string_3		COMPILED
test_component.sensitive_string_1	haha	PERSISTED
test_component.sensitive_string_2		COMPILED
test_component.sensitive_string_3		COMPILED
SET PERSIST test_component.sensitive_string_2 = 'hoho';
SET PERSIST_ONLY test_component.sensitive_ro_string_2 = 'hoho';
SET PERSIST test_component.sensitive_string_3 = 'hoho';
SET PERSIST_ONLY test_component.sensitive_ro_string_3 = 'hoho';
# Stop the running server.
# Restart server with keyring component
SELECT a.variable_name, b.variable_value, a.variable_source FROM performance_schema.variables_info AS a, performance_schema.global_variables AS b WHERE a.variable_name = b.variable_name AND a.variable_name LIKE 'test_component.sensitive%';
variable_name	variable_value	variable_source
test_component.sensitive_ro_string_1	haha	PERSISTED
test_component.sensitive_ro_string_2	hoho	PERSISTED
test_component.sensitive_ro_string_3	hoho	PERSISTED
test_component.sensitive_string_1	haha	PERSISTED
test_component.sensitive_string_2	hoho	PERSISTED
test_component.sensitive_string_3	hoho	PERSISTED
RESET PERSIST;
UNINSTALL COMPONENT "file://component_test_sensitive_system_variables";
# ----------------------------------------------------------------------
# ----------------------------------------------------------------------
# Teardown
# Removing manifest file for current MySQL server instance
# Removing local keyring file for keyring component: component_keyring_file
# Removing local configuration file for keyring component: component_keyring_file
# Restarting server without the manifest file
# ----------------------------------------------------------------------
