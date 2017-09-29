#
# Basic test for innodb_print_ddl_logs
#
# Save default value.
SET @default_var_value = @@GLOBAL.innodb_print_ddl_logs;
SELECT @@GLOBAL.innodb_print_ddl_logs;
@@GLOBAL.innodb_print_ddl_logs
0
# Try setting to 1.
SET @@GLOBAL.innodb_print_ddl_logs = 1;
SELECT @@GLOBAL.innodb_print_ddl_logs;
@@GLOBAL.innodb_print_ddl_logs
1
# Try setting to 0.
SET @@GLOBAL.innodb_print_ddl_logs = 0;
SELECT @@GLOBAL.innodb_print_ddl_logs;
@@GLOBAL.innodb_print_ddl_logs
0
# Try setting to 1 (again).
SET @@GLOBAL.innodb_print_ddl_logs = 1;
SELECT @@GLOBAL.innodb_print_ddl_logs;
@@GLOBAL.innodb_print_ddl_logs
1
# Try setting to 0 (again).
SET @@GLOBAL.innodb_print_ddl_logs = 0;
SELECT @@GLOBAL.innodb_print_ddl_logs;
@@GLOBAL.innodb_print_ddl_logs
0
# Check what happens if we set the same value as current.
SET @@GLOBAL.innodb_print_ddl_logs = 1;
SET @@GLOBAL.innodb_print_ddl_logs = 1;
SET @@GLOBAL.innodb_print_ddl_logs = 0;
SET @@GLOBAL.innodb_print_ddl_logs = 0;
# Try using variable as local.
SET innodb_print_ddl_logs = 1;
ERROR HY000: Variable 'innodb_print_ddl_logs' is a GLOBAL variable and should be set with SET GLOBAL
# Finished. Restore default value.
SET @@GLOBAL.innodb_print_ddl_logs = @default_var_value;
