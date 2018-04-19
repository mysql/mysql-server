#
# This checks if a given global boolean variable exists and works.
#

--echo # Save default value.

eval SET @default_var_value = @@GLOBAL.$var_name;

eval SELECT @@GLOBAL.$var_name;

--echo # Try setting to 1.

eval SET @@GLOBAL.$var_name = 1;

eval SELECT @@GLOBAL.$var_name;

--echo # Try setting to 0.

eval SET @@GLOBAL.$var_name = 0;

eval SELECT @@GLOBAL.$var_name;

--echo # Try setting to 1 (again).

eval SET @@GLOBAL.$var_name = 1;

eval SELECT @@GLOBAL.$var_name;

--echo # Try setting to 0 (again).

eval SET @@GLOBAL.$var_name = 0;

eval SELECT @@GLOBAL.$var_name;

--echo # Check what happens if we set the same value as current.

eval SET @@GLOBAL.$var_name = 1;

eval SET @@GLOBAL.$var_name = 1;

eval SET @@GLOBAL.$var_name = 0;

eval SET @@GLOBAL.$var_name = 0;

--echo # Try using variable as local.

--error ER_GLOBAL_VARIABLE
eval SET $var_name = 1;

--echo # Finished. Restore default value.

eval SET @@GLOBAL.$var_name = @default_var_value;
