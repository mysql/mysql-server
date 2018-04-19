#
# Basic test for innodb_checkpoint_disabled
#
# Save default value.
SET @default_var_value = @@GLOBAL.innodb_checkpoint_disabled;
SELECT @@GLOBAL.innodb_checkpoint_disabled;
@@GLOBAL.innodb_checkpoint_disabled
0
# Try setting to 1.
SET @@GLOBAL.innodb_checkpoint_disabled = 1;
SELECT @@GLOBAL.innodb_checkpoint_disabled;
@@GLOBAL.innodb_checkpoint_disabled
1
# Try setting to 0.
SET @@GLOBAL.innodb_checkpoint_disabled = 0;
SELECT @@GLOBAL.innodb_checkpoint_disabled;
@@GLOBAL.innodb_checkpoint_disabled
0
# Try setting to 1 (again).
SET @@GLOBAL.innodb_checkpoint_disabled = 1;
SELECT @@GLOBAL.innodb_checkpoint_disabled;
@@GLOBAL.innodb_checkpoint_disabled
1
# Try setting to 0 (again).
SET @@GLOBAL.innodb_checkpoint_disabled = 0;
SELECT @@GLOBAL.innodb_checkpoint_disabled;
@@GLOBAL.innodb_checkpoint_disabled
0
# Check what happens if we set the same value as current.
SET @@GLOBAL.innodb_checkpoint_disabled = 1;
SET @@GLOBAL.innodb_checkpoint_disabled = 1;
SET @@GLOBAL.innodb_checkpoint_disabled = 0;
SET @@GLOBAL.innodb_checkpoint_disabled = 0;
# Try using variable as local.
SET innodb_checkpoint_disabled = 1;
ERROR HY000: Variable 'innodb_checkpoint_disabled' is a GLOBAL variable and should be set with SET GLOBAL
# Finished. Restore default value.
SET @@GLOBAL.innodb_checkpoint_disabled = @default_var_value;
