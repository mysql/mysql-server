#
# WL#13469: secure storage for sensitive system variables
#
INSTALL COMPONENT 'file://component_test_sensitive_system_variables';
# ----------------------------------------------------------------------
# 1. If persist_sensitive_variables_in_plaintext is
#    set to FALSE/OFF and keyring component is not
#    available, trying to persist a SENSITIVE variable
#    should result into error.
# With --persist_sensitive_variables_in_plaintext set
# to FALSE and keyring component not configured,
# trying to persist SENSITIVE variables should fail.
SELECT @@global.persist_sensitive_variables_in_plaintext;
@@global.persist_sensitive_variables_in_plaintext
0
SET PERSIST test_component.sensitive_string_1 = 'haha';
ERROR HY000: Cannot persist SENSITIVE system variable 'test_component.sensitive_string_1' because keyring component support is unavailable.
SET PERSIST_ONLY test_component.sensitive_string_1 = 'haha';
ERROR HY000: Cannot persist SENSITIVE system variable 'test_component.sensitive_string_1' because keyring component support is unavailable.
SET PERSIST_ONLY test_component.sensitive_ro_string_1 = 'haha';
ERROR HY000: Cannot persist SENSITIVE system variable 'test_component.sensitive_ro_string_1' because keyring component support is unavailable.
# ----------------------------------------------------------------------
# 2. If persist_sensitive_variables_in_plaintext is set
#    to TRUE/ON and keyring component support is not
#    available, it should possible to persist
#    SENSITIVE variables' values.
# Stop the running server.
# Restart server without keyiring and with persist_sensitive_variables_in_plaintext=TRUE
# With --persist_sensitive_variables_in_plaintext set
# to TRUE, even if keyring component not configured,
# trying to persist SENSITIVE variable should succeed.
SELECT @@global.persist_sensitive_variables_in_plaintext;
@@global.persist_sensitive_variables_in_plaintext
1
SET PERSIST test_component.sensitive_string_1 = 'haha';
Warnings:
Warning	4098	SENSITIVE system variable 'test_component.sensitive_string_1' was persisted without encryption. Consider restarting server with keyring component support to persist SENSITIVE variables securely.
SET PERSIST_ONLY test_component.sensitive_string_1 = 'haha';
Warnings:
Warning	4098	SENSITIVE system variable 'test_component.sensitive_string_1' was persisted without encryption. Consider restarting server with keyring component support to persist SENSITIVE variables securely.
SET PERSIST_ONLY test_component.sensitive_ro_string_1 = 'haha';
Warnings:
Warning	4098	SENSITIVE system variable 'test_component.sensitive_ro_string_1' was persisted without encryption. Consider restarting server with keyring component support to persist SENSITIVE variables securely.
# Stop the running server.
# Restart server without keyiring and with persist_sensitive_variables_in_plaintext=TRUE
SELECT a.variable_name, b.variable_value, a.variable_source FROM performance_schema.variables_info AS a, performance_schema.global_variables AS b WHERE a.variable_name = b.variable_name AND a.variable_name LIKE 'test_component.sensitive%';
variable_name	variable_value	variable_source
test_component.sensitive_ro_string_1	haha	PERSISTED
test_component.sensitive_ro_string_2		COMPILED
test_component.sensitive_ro_string_3		COMPILED
test_component.sensitive_string_1	haha	PERSISTED
test_component.sensitive_string_2		COMPILED
test_component.sensitive_string_3		COMPILED
RESET PERSIST;
UNINSTALL COMPONENT 'file://component_test_sensitive_system_variables';
# ----------------------------------------------------------------------
