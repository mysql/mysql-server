#
# Basic test for innodb_ddl_log_crash_reset_debug
#
# Save default value.
SET @default_var_value = @@GLOBAL.innodb_ddl_log_crash_reset_debug;
SELECT @@GLOBAL.innodb_ddl_log_crash_reset_debug;
@@GLOBAL.innodb_ddl_log_crash_reset_debug
0
# Try setting to 1.
SET @@GLOBAL.innodb_ddl_log_crash_reset_debug = 1;
SELECT @@GLOBAL.innodb_ddl_log_crash_reset_debug;
@@GLOBAL.innodb_ddl_log_crash_reset_debug
1
# Try setting to 0.
SET @@GLOBAL.innodb_ddl_log_crash_reset_debug = 0;
SELECT @@GLOBAL.innodb_ddl_log_crash_reset_debug;
@@GLOBAL.innodb_ddl_log_crash_reset_debug
0
# Try setting to 1 (again).
SET @@GLOBAL.innodb_ddl_log_crash_reset_debug = 1;
SELECT @@GLOBAL.innodb_ddl_log_crash_reset_debug;
@@GLOBAL.innodb_ddl_log_crash_reset_debug
1
# Try setting to 0 (again).
SET @@GLOBAL.innodb_ddl_log_crash_reset_debug = 0;
SELECT @@GLOBAL.innodb_ddl_log_crash_reset_debug;
@@GLOBAL.innodb_ddl_log_crash_reset_debug
0
# Check what happens if we set the same value as current.
SET @@GLOBAL.innodb_ddl_log_crash_reset_debug = 1;
SET @@GLOBAL.innodb_ddl_log_crash_reset_debug = 1;
SET @@GLOBAL.innodb_ddl_log_crash_reset_debug = 0;
SET @@GLOBAL.innodb_ddl_log_crash_reset_debug = 0;
# Try using variable as local.
SET innodb_ddl_log_crash_reset_debug = 1;
ERROR HY000: Variable 'innodb_ddl_log_crash_reset_debug' is a GLOBAL variable and should be set with SET GLOBAL
# Finished. Restore default value.
SET @@GLOBAL.innodb_ddl_log_crash_reset_debug = @default_var_value;
