#
# WL#12002: SET system variable values as a component service
#
call mtr.add_suppression("Setting session values for system variables only makes sense in a user session");
INSTALL COMPONENT "file://component_test_mysql_system_variable_set";
# Ensure the value of super read only is off
SELECT @@global.super_read_only;
@@global.super_read_only
0
# Try setting wrong variable name in the same thread. Should fail
SELECT test_set_system_variable_string(0, "the_meaning_of_life", "42", "GLOBAL");
ERROR HY000: Unknown system variable 'the_meaning_of_life'
# Try setting wrong variable name in a new sub-thread. Should fail
SELECT test_set_system_variable_string(1, "the_meaning_of_life", "42", "GLOBAL");
ERROR HY000: Unknown system variable 'the_meaning_of_life'
# Try setting wrong value to super_read_only, Should fail
SELECT test_set_system_variable_string(0, "super_read_only", "gizmo", "GLOBAL");
ERROR 42000: Variable 'super_read_only' can't be set to the value of 'gizmo'
# Ensure the value of super read only is still off
SELECT @@global.super_read_only;
@@global.super_read_only
0
# Try setting super_read_only on using the same thread. Should succeed
SELECT test_set_system_variable_string(0, "super_read_only", "on", "GLOBAL");
test_set_system_variable_string(0, "super_read_only", "on", "GLOBAL")
0
# Ensure the value of super read only is on
SELECT @@global.super_read_only;
@@global.super_read_only
1
# Try setting super_read_only to off using a new sub-thread. Should succeed
SELECT test_set_system_variable_string(1, "super_read_only", "off", "GLOBAL");
test_set_system_variable_string(1, "super_read_only", "off", "GLOBAL")
0
# Ensure the value of super read only is off
SELECT @@global.super_read_only;
@@global.super_read_only
0
# Check that the variable is not yet persisted:
SELECT * FROM performance_schema.persisted_variables WHERE VARIABLE_NAME='super_read_only';
VARIABLE_NAME	VARIABLE_VALUE
# Try setting PERSIST super_read_only. Should succeed
SELECT test_set_system_variable_string(0, "super_read_only", "off", "PERSIST");
test_set_system_variable_string(0, "super_read_only", "off", "PERSIST")
0
# Check that the variable is now persisted:
SELECT VARIABLE_VALUE,@@global.super_read_only
FROM performance_schema.persisted_variables WHERE VARIABLE_NAME='super_read_only';
VARIABLE_VALUE	@@global.super_read_only
OFF	0
# reset persisted value
RESET PERSIST super_read_only;
# Try setting PERSIST_ONLY super_read_only. Should succeed
SELECT test_set_system_variable_string(0, "super_read_only", "on", "PERSIST_ONLY");
test_set_system_variable_string(0, "super_read_only", "on", "PERSIST_ONLY")
0
# Check that the variable is now persisted only but not changed:
SELECT VARIABLE_VALUE,@@global.super_read_only
FROM performance_schema.persisted_variables WHERE VARIABLE_NAME='super_read_only';
VARIABLE_VALUE	@@global.super_read_only
ON	0
# reset persisted value
RESET PERSIST super_read_only;
# Show initial value of super read only before next test
SELECT @@global.super_read_only;
@@global.super_read_only
0
# Bug#34072134: mysql_system_variable_update_string::set does not allow NULL for variable_type
# Try setting super_read_only on using NULL type (defaults to GLOBAL). Should succeed
SELECT test_set_system_variable_string(0, "super_read_only", "on", NULL);
test_set_system_variable_string(0, "super_read_only", "on", NULL)
0
# Ensure the value of super read only is on
SELECT @@global.super_read_only;
@@global.super_read_only
1
# Bug#34072155: Improve mysql_system_variable_update_string::set to add error information
# Try using SESSION type with sub-thread, Should fail with NULL
SELECT test_set_system_variable_string(1, "super_read_only", "off", "SESSION");
test_set_system_variable_string(1, "super_read_only", "off", "SESSION")
NULL
# Ensure the value of super read only is still on
SELECT @@global.super_read_only;
@@global.super_read_only
1
# Cleanup
SET GLOBAL read_only = DEFAULT;
RESET PERSIST;
UNINSTALL COMPONENT "file://component_test_mysql_system_variable_set";
# End of 8.0 tests
