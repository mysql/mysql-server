# Initial values
SELECT @@global.max_execution_time, @@session.max_execution_time;
@@global.max_execution_time	@@session.max_execution_time
0	0
# Save initial value
SET @start_global_value= @@global.max_execution_time;
SELECT @start_global_value;
@start_global_value
0
SET @start_session_value = @@session.max_execution_time;
SELECT @start_session_value;
@start_session_value
0
# Display the DEFAULT value of max_execution_time
SET @@session.max_execution_time = 20000;
SET @@session.max_execution_time = DEFAULT;
SELECT @@session.max_execution_time;
@@session.max_execution_time
0
SET @@global.max_execution_time = 20000;
SET @@global.max_execution_time = DEFAULT;
SELECT @@global.max_execution_time;
@@global.max_execution_time
0
# Check the DEFAULT value of max_execution_time
SET @@global.max_execution_time = DEFAULT;
SELECT @@global.max_execution_time = 28800;
@@global.max_execution_time = 28800
0
SET @@session.max_execution_time = DEFAULT;
SELECT @@session.max_execution_time = 28800;
@@session.max_execution_time = 28800
0
# Check Max value for max_execution_time.
"Max value for @@global.max_execution_time is set properly."
"Max value for @@session.max_execution_time is set properly."
SET @@global.max_execution_time = 0;
SELECT @@global.max_execution_time;
@@global.max_execution_time
0
SET @@session.max_execution_time = 0;
SELECT @@global.max_execution_time;
@@global.max_execution_time
0
# Change the value of max_execution_time to a valid value for SESSION scope
SET @@session.max_execution_time = 10000;
SELECT @@global.max_execution_time, @@session.max_execution_time;
@@global.max_execution_time	@@session.max_execution_time
0	10000
SET @@session.max_execution_time = 50050;
SELECT @@global.max_execution_time, @@session.max_execution_time;
@@global.max_execution_time	@@session.max_execution_time
0	50050
SET @@session.max_execution_time = 65535;
SELECT @@global.max_execution_time, @@session.max_execution_time;
@@global.max_execution_time	@@session.max_execution_time
0	65535
# Change the value of max_execution_time to an invalid value #
SET @@global.max_execution_time = -2;
Warnings:
Warning	1292	Truncated incorrect max_execution_time value: '-2'
SELECT @@global.max_execution_time;
@@global.max_execution_time
0
SET @@global.max_execution_time = 65530.34;
ERROR 42000: Incorrect argument type to variable 'max_execution_time'
SELECT @@global.max_execution_time;
@@global.max_execution_time
0
SET @@global.max_execution_time = test;
ERROR 42000: Incorrect argument type to variable 'max_execution_time'
SELECT @@global.max_execution_time;
@@global.max_execution_time
0
# Change the value of max_execution_time to a valid value for SESSION scope
SET @@session.max_execution_time = 10000;
SELECT @@global.max_execution_time, @@session.max_execution_time;
@@global.max_execution_time	@@session.max_execution_time
0	10000
SET @@session.max_execution_time = 50050;
SELECT @@global.max_execution_time, @@session.max_execution_time;
@@global.max_execution_time	@@session.max_execution_time
0	50050
SET @@session.max_execution_time = 65535;
SELECT @@global.max_execution_time, @@session.max_execution_time;
@@global.max_execution_time	@@session.max_execution_time
0	65535
# Change the value of max_execution_time to an invalid value #
SET @@global.max_execution_time = -2;
Warnings:
Warning	1292	Truncated incorrect max_execution_time value: '-2'
SELECT @@global.max_execution_time;
@@global.max_execution_time
0
SET @@global.max_execution_time = 65530.34;
ERROR 42000: Incorrect argument type to variable 'max_execution_time'
SELECT @@global.max_execution_time;
@@global.max_execution_time
0
SET @@global.max_execution_time = test;
ERROR 42000: Incorrect argument type to variable 'max_execution_time'
SELECT @@global.max_execution_time;
@@global.max_execution_time
0
SET @@global.max_execution_time = "";
ERROR 42000: Incorrect argument type to variable 'max_execution_time'
SELECT @@global.max_execution_time;
@@global.max_execution_time
0
SET @@global.max_execution_time = "*";
ERROR 42000: Incorrect argument type to variable 'max_execution_time'
SELECT @@global.max_execution_time;
@@global.max_execution_time
0
SET @@global.max_execution_time = NULL;
ERROR 42000: Incorrect argument type to variable 'max_execution_time'
SELECT @@global.max_execution_time;
@@global.max_execution_time
0
SET @@global.max_execution_time = "0";
ERROR 42000: Incorrect argument type to variable 'max_execution_time'
SELECT @@global.max_execution_time;
@@global.max_execution_time
0
SET @@global.max_execution_time = "*#$**$(@(@ ##@";
ERROR 42000: Incorrect argument type to variable 'max_execution_time'
SELECT @@global.max_execution_time;
@@global.max_execution_time
0
SET @@session.max_execution_time = -2;
Warnings:
Warning	1292	Truncated incorrect max_execution_time value: '-2'
SELECT @@session.max_execution_time;
@@session.max_execution_time
0
SET @@session.max_execution_time = 65530.34;
ERROR 42000: Incorrect argument type to variable 'max_execution_time'
SELECT @@session.max_execution_time;
@@session.max_execution_time
0
SET @@session.max_execution_time = test;
ERROR 42000: Incorrect argument type to variable 'max_execution_time'
SELECT @@session.max_execution_time;
@@session.max_execution_time
0
SET @@session.max_execution_time = "";
ERROR 42000: Incorrect argument type to variable 'max_execution_time'
SELECT @@session.max_execution_time;
@@session.max_execution_time
0
SET @@session.max_execution_time = "*";
ERROR 42000: Incorrect argument type to variable 'max_execution_time'
SELECT @@session.max_execution_time;
@@session.max_execution_time
0
SET @@session.max_execution_time = NULL;
ERROR 42000: Incorrect argument type to variable 'max_execution_time'
SELECT @@session.max_execution_time;
@@session.max_execution_time
0
SET @@session.max_execution_time = "0";
ERROR 42000: Incorrect argument type to variable 'max_execution_time'
SELECT @@session.max_execution_time;
@@session.max_execution_time
0
SET @@session.max_execution_time = "*#$**$(@(@ ##@";
ERROR 42000: Incorrect argument type to variable 'max_execution_time'
SELECT @@session.max_execution_time;
@@session.max_execution_time
0
# Check if the value in GLOBAL Table matches value in variable
SELECT @@global.max_execution_time = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='max_execution_time';
@@global.max_execution_time = VARIABLE_VALUE
1
# Check if the value in SESSION Table matches value in variable
SELECT @@session.max_execution_time = VARIABLE_VALUE
FROM performance_schema.session_variables
WHERE VARIABLE_NAME='max_execution_time';
@@session.max_execution_time = VARIABLE_VALUE
1
# Check if TRUE and FALSE values can be used on variable
SET @@global.max_execution_time = TRUE;
SELECT @@global.max_execution_time;
@@global.max_execution_time
1
SET @@global.max_execution_time = FALSE;
SELECT @@global.max_execution_time;
@@global.max_execution_time
0
SET @@session.max_execution_time = TRUE;
SELECT @@session.max_execution_time;
@@session.max_execution_time
1
SET @@session.max_execution_time = FALSE;
SELECT @@session.max_execution_time;
@@session.max_execution_time
0
# Check if accessing variable with SESSION,LOCAL and without SCOPE points to same session variable
SET @@max_execution_time = 10000;
SELECT @@max_execution_time = @@local.max_execution_time;
@@max_execution_time = @@local.max_execution_time
1
SELECT @@local.max_execution_time = @@session.max_execution_time;
@@local.max_execution_time = @@session.max_execution_time
1
# Check if max_execution_time can be accessed with and without @@ sign
SET max_execution_time = 10000;
SELECT @@max_execution_time;
@@max_execution_time
10000
SELECT local.max_execution_time;
ERROR 42S02: Unknown table 'local' in field list
SELECT session.max_execution_time;
ERROR 42S02: Unknown table 'session' in field list
# Restore initial value
SET @@global.max_execution_time = @start_global_value;
SELECT @@global.max_execution_time;
@@global.max_execution_time
0
SET @@session.max_execution_time = @start_session_value;
SELECT @@session.max_execution_time;
@@session.max_execution_time
0
