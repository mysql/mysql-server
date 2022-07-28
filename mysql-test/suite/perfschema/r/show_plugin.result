
================================================================================
SETUP
================================================================================
# Save the initial number of concurrent sessions

# Verify EXAMPLE plugin is not loaded
SELECT COUNT(*) = 0 AS "Expect 1" FROM information_schema.plugins WHERE plugin_name = "EXAMPLE";
Expect 1
1

# Create one session to force local and global system variables;
connect con0, localhost, root,,;
connection default;

================================================================================
TEST 1- NO PLUGIN VARS
================================================================================

SHOW GLOBAL STATUS LIKE "example_%";
Variable_name	Value

SHOW SESSION STATUS LIKE "example_%";
Variable_name	Value

SHOW GLOBAL VARIABLES LIKE "example_%";
Variable_name	Value

SHOW SESSION VARIABLES LIKE "example_%";
Variable_name	Value

SELECT variable_name, variable_value FROM performance_schema.global_status WHERE variable_name LIKE "example_%";
variable_name	variable_value

SELECT variable_name, variable_value FROM performance_schema.session_status WHERE variable_name LIKE "example_%";
variable_name	variable_value

SELECT variable_name, variable_value FROM performance_schema.global_variables WHERE variable_name LIKE "example_%";
variable_name	variable_value

SELECT variable_name, variable_value FROM performance_schema.session_variables WHERE variable_name LIKE "example_%";
variable_name	variable_value

================================================================================
TEST 2 - PLUGIN LOAD, UNLOAD, RELOAD
================================================================================
================================================================================
2.1 - INSTALL PLUGIN
================================================================================
INSTALL PLUGIN example SONAME 'ha_example.so';

================================================================================
2.1a - FORCE SYNC OF LOCAL AND GLOBAL SYSTEM VARS
================================================================================

SHOW GLOBAL STATUS LIKE "example_%";
Variable_name	Value
example_func_example	enum_var is 0, ulong_var is 8, double_var is 8.500000, signed_int_var is -10, signed_long_var is -10, signed_longlong_var is -10
example_status_array_var1	100
example_status_array_var2	20.010000
example_status_var3	three hundred
example_status_var4	ON
example_status_var5	OFF
example_status_var6	8250

SHOW SESSION STATUS LIKE "example_%";
Variable_name	Value
example_func_example	enum_var is 0, ulong_var is 8, double_var is 8.500000, signed_int_var is -10, signed_long_var is -10, signed_longlong_var is -10
example_status_array_var1	100
example_status_array_var2	20.010000
example_status_var3	three hundred
example_status_var4	ON
example_status_var5	OFF
example_status_var6	8250

SHOW GLOBAL VARIABLES LIKE "example_%";
Variable_name	Value
example_create_count_thdvar	0
example_double_thdvar	8.500000
example_double_var	8.500000
example_enum_var	e1
example_last_create_thdvar	
example_signed_int_thdvar	-10
example_signed_int_var	-10
example_signed_long_thdvar	-10
example_signed_long_var	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_var	-10
example_ulong_var	8

SHOW SESSION VARIABLES LIKE "example_%";
Variable_name	Value
example_create_count_thdvar	0
example_double_thdvar	8.500000
example_double_var	8.500000
example_enum_var	e1
example_last_create_thdvar	
example_signed_int_thdvar	-10
example_signed_int_var	-10
example_signed_long_thdvar	-10
example_signed_long_var	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_var	-10
example_ulong_var	8

SELECT variable_name, variable_value FROM performance_schema.global_status WHERE variable_name LIKE "example_%";
variable_name	variable_value
example_func_example	enum_var is 0, ulong_var is 8, double_var is 8.500000, signed_int_var is -10, signed_long_var is -10, signed_longlong_var is -10
example_status_array_var1	100
example_status_array_var2	20.010000
example_status_var3	three hundred
example_status_var4	ON
example_status_var5	OFF
example_status_var6	8250

SELECT variable_name, variable_value FROM performance_schema.session_status WHERE variable_name LIKE "example_%";
variable_name	variable_value
example_func_example	enum_var is 0, ulong_var is 8, double_var is 8.500000, signed_int_var is -10, signed_long_var is -10, signed_longlong_var is -10
example_status_array_var1	100
example_status_array_var2	20.010000
example_status_var3	three hundred
example_status_var4	ON
example_status_var5	OFF
example_status_var6	8250

SELECT variable_name, variable_value FROM performance_schema.global_variables WHERE variable_name LIKE "example_%";
variable_name	variable_value
example_create_count_thdvar	0
example_double_thdvar	8.500000
example_double_var	8.500000
example_enum_var	e1
example_last_create_thdvar	
example_signed_int_thdvar	-10
example_signed_int_var	-10
example_signed_long_thdvar	-10
example_signed_long_var	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_var	-10
example_ulong_var	8

SELECT variable_name, variable_value FROM performance_schema.session_variables WHERE variable_name LIKE "example_%";
variable_name	variable_value
example_create_count_thdvar	0
example_double_thdvar	8.500000
example_double_var	8.500000
example_enum_var	e1
example_last_create_thdvar	
example_signed_int_thdvar	-10
example_signed_int_var	-10
example_signed_long_thdvar	-10
example_signed_long_var	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_var	-10
example_ulong_var	8
disconnect con0;

================================================================================
2.2 - SET PLUGIN VARS
================================================================================
# GLOBAL
SET GLOBAL example_ulong_var = 100;
SET GLOBAL example_enum_var = e1;
SET GLOBAL example_double_var = 100.9990;
SET GLOBAL example_double_thdvar = 101.9991;
# SESSION
SET SESSION example_double_thdvar = 102.9992;

================================================================================
2.3 - VERIFY UPDATED PLUGIN VARS
================================================================================

SHOW GLOBAL STATUS LIKE "example_%";
Variable_name	Value
example_func_example	enum_var is 0, ulong_var is 100, double_var is 100.999000, signed_int_var is -10, signed_long_var is -10, signed_longlong_var is -10
example_status_array_var1	100
example_status_array_var2	20.010000
example_status_var3	three hundred
example_status_var4	ON
example_status_var5	OFF
example_status_var6	8250

SHOW SESSION STATUS LIKE "example_%";
Variable_name	Value
example_func_example	enum_var is 0, ulong_var is 100, double_var is 100.999000, signed_int_var is -10, signed_long_var is -10, signed_longlong_var is -10
example_status_array_var1	100
example_status_array_var2	20.010000
example_status_var3	three hundred
example_status_var4	ON
example_status_var5	OFF
example_status_var6	8250

SHOW GLOBAL VARIABLES LIKE "example_%";
Variable_name	Value
example_create_count_thdvar	0
example_double_thdvar	101.999100
example_double_var	100.999000
example_enum_var	e1
example_last_create_thdvar	
example_signed_int_thdvar	-10
example_signed_int_var	-10
example_signed_long_thdvar	-10
example_signed_long_var	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_var	-10
example_ulong_var	100

SHOW SESSION VARIABLES LIKE "example_%";
Variable_name	Value
example_create_count_thdvar	0
example_double_thdvar	102.999200
example_double_var	100.999000
example_enum_var	e1
example_last_create_thdvar	
example_signed_int_thdvar	-10
example_signed_int_var	-10
example_signed_long_thdvar	-10
example_signed_long_var	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_var	-10
example_ulong_var	100

SELECT variable_name, variable_value FROM performance_schema.global_status WHERE variable_name LIKE "example_%";
variable_name	variable_value
example_func_example	enum_var is 0, ulong_var is 100, double_var is 100.999000, signed_int_var is -10, signed_long_var is -10, signed_longlong_var is -10
example_status_array_var1	100
example_status_array_var2	20.010000
example_status_var3	three hundred
example_status_var4	ON
example_status_var5	OFF
example_status_var6	8250

SELECT variable_name, variable_value FROM performance_schema.session_status WHERE variable_name LIKE "example_%";
variable_name	variable_value
example_func_example	enum_var is 0, ulong_var is 100, double_var is 100.999000, signed_int_var is -10, signed_long_var is -10, signed_longlong_var is -10
example_status_array_var1	100
example_status_array_var2	20.010000
example_status_var3	three hundred
example_status_var4	ON
example_status_var5	OFF
example_status_var6	8250

SELECT variable_name, variable_value FROM performance_schema.global_variables WHERE variable_name LIKE "example_%";
variable_name	variable_value
example_create_count_thdvar	0
example_double_thdvar	101.999100
example_double_var	100.999000
example_enum_var	e1
example_last_create_thdvar	
example_signed_int_thdvar	-10
example_signed_int_var	-10
example_signed_long_thdvar	-10
example_signed_long_var	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_var	-10
example_ulong_var	100

SELECT variable_name, variable_value FROM performance_schema.session_variables WHERE variable_name LIKE "example_%";
variable_name	variable_value
example_create_count_thdvar	0
example_double_thdvar	102.999200
example_double_var	100.999000
example_enum_var	e1
example_last_create_thdvar	
example_signed_int_thdvar	-10
example_signed_int_var	-10
example_signed_long_thdvar	-10
example_signed_long_var	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_var	-10
example_ulong_var	100

================================================================================
2.4 - UNINSTALL PLUGIN
================================================================================
UNINSTALL PLUGIN example;

================================================================================
2.5 - VERIFY NO PLUGIN VARS
================================================================================

SHOW GLOBAL STATUS LIKE "example_%";
Variable_name	Value

SHOW SESSION STATUS LIKE "example_%";
Variable_name	Value

SHOW GLOBAL VARIABLES LIKE "example_%";
Variable_name	Value

SHOW SESSION VARIABLES LIKE "example_%";
Variable_name	Value

SELECT variable_name, variable_value FROM performance_schema.global_status WHERE variable_name LIKE "example_%";
variable_name	variable_value

SELECT variable_name, variable_value FROM performance_schema.session_status WHERE variable_name LIKE "example_%";
variable_name	variable_value

SELECT variable_name, variable_value FROM performance_schema.global_variables WHERE variable_name LIKE "example_%";
variable_name	variable_value

SELECT variable_name, variable_value FROM performance_schema.session_variables WHERE variable_name LIKE "example_%";
variable_name	variable_value

================================================================================
2.6 - REINSTALL PLUGIN
================================================================================
# Reinstall EXAMPLE plugin
INSTALL PLUGIN example SONAME 'ha_example.so';

================================================================================
2.7 - SET PLUGIN VARS AGAIN
================================================================================
# GLOBAL
SET GLOBAL example_ulong_var = 200;
SET GLOBAL example_enum_var = e2;
SET GLOBAL example_double_var = 200.8880;
SET GLOBAL example_double_thdvar = 201.8881;
# SESSION
SET SESSION example_double_thdvar = 202.8882;

================================================================================
2.8 - VERIFY PLUGIN VARS
================================================================================

SHOW GLOBAL STATUS LIKE "example_%";
Variable_name	Value
example_func_example	enum_var is 1, ulong_var is 200, double_var is 200.888000, signed_int_var is -10, signed_long_var is -10, signed_longlong_var is -10
example_status_array_var1	100
example_status_array_var2	20.010000
example_status_var3	three hundred
example_status_var4	ON
example_status_var5	OFF
example_status_var6	8250

SHOW SESSION STATUS LIKE "example_%";
Variable_name	Value
example_func_example	enum_var is 1, ulong_var is 200, double_var is 200.888000, signed_int_var is -10, signed_long_var is -10, signed_longlong_var is -10
example_status_array_var1	100
example_status_array_var2	20.010000
example_status_var3	three hundred
example_status_var4	ON
example_status_var5	OFF
example_status_var6	8250

SHOW GLOBAL VARIABLES LIKE "example_%";
Variable_name	Value
example_create_count_thdvar	0
example_double_thdvar	201.888100
example_double_var	200.888000
example_enum_var	e2
example_last_create_thdvar	
example_signed_int_thdvar	-10
example_signed_int_var	-10
example_signed_long_thdvar	-10
example_signed_long_var	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_var	-10
example_ulong_var	200

SHOW SESSION VARIABLES LIKE "example_%";
Variable_name	Value
example_create_count_thdvar	0
example_double_thdvar	202.888200
example_double_var	200.888000
example_enum_var	e2
example_last_create_thdvar	
example_signed_int_thdvar	-10
example_signed_int_var	-10
example_signed_long_thdvar	-10
example_signed_long_var	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_var	-10
example_ulong_var	200

SELECT variable_name, variable_value FROM performance_schema.global_status WHERE variable_name LIKE "example_%";
variable_name	variable_value
example_func_example	enum_var is 1, ulong_var is 200, double_var is 200.888000, signed_int_var is -10, signed_long_var is -10, signed_longlong_var is -10
example_status_array_var1	100
example_status_array_var2	20.010000
example_status_var3	three hundred
example_status_var4	ON
example_status_var5	OFF
example_status_var6	8250

SELECT variable_name, variable_value FROM performance_schema.session_status WHERE variable_name LIKE "example_%";
variable_name	variable_value
example_func_example	enum_var is 1, ulong_var is 200, double_var is 200.888000, signed_int_var is -10, signed_long_var is -10, signed_longlong_var is -10
example_status_array_var1	100
example_status_array_var2	20.010000
example_status_var3	three hundred
example_status_var4	ON
example_status_var5	OFF
example_status_var6	8250

SELECT variable_name, variable_value FROM performance_schema.global_variables WHERE variable_name LIKE "example_%";
variable_name	variable_value
example_create_count_thdvar	0
example_double_thdvar	201.888100
example_double_var	200.888000
example_enum_var	e2
example_last_create_thdvar	
example_signed_int_thdvar	-10
example_signed_int_var	-10
example_signed_long_thdvar	-10
example_signed_long_var	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_var	-10
example_ulong_var	200

SELECT variable_name, variable_value FROM performance_schema.session_variables WHERE variable_name LIKE "example_%";
variable_name	variable_value
example_create_count_thdvar	0
example_double_thdvar	202.888200
example_double_var	200.888000
example_enum_var	e2
example_last_create_thdvar	
example_signed_int_thdvar	-10
example_signed_int_var	-10
example_signed_long_thdvar	-10
example_signed_long_var	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_var	-10
example_ulong_var	200

================================================================================
2.9 - UNINSTALL PLUGIN
================================================================================
UNINSTALL PLUGIN example;

================================================================================
2.10 - VERIFY NO PLUGIN VARS
================================================================================

SHOW GLOBAL STATUS LIKE "example_%";
Variable_name	Value

SHOW SESSION STATUS LIKE "example_%";
Variable_name	Value

SHOW GLOBAL VARIABLES LIKE "example_%";
Variable_name	Value

SHOW SESSION VARIABLES LIKE "example_%";
Variable_name	Value

SELECT variable_name, variable_value FROM performance_schema.global_status WHERE variable_name LIKE "example_%";
variable_name	variable_value

SELECT variable_name, variable_value FROM performance_schema.session_status WHERE variable_name LIKE "example_%";
variable_name	variable_value

SELECT variable_name, variable_value FROM performance_schema.global_variables WHERE variable_name LIKE "example_%";
variable_name	variable_value

SELECT variable_name, variable_value FROM performance_schema.session_variables WHERE variable_name LIKE "example_%";
variable_name	variable_value

================================================================================
TEST 3 - SESSION PLUGIN VARS ON MULTIPLE CONNECTIONS
================================================================================

================================================================================
3.1 - INSTALL PLUGIN
================================================================================
INSTALL PLUGIN example SONAME 'ha_example.so';

================================================================================
3.2 - SET GLOBAL AND DEFAULT CONNECTION VARS
================================================================================
connection default;
SET GLOBAL example_ulong_var = 300;
SET GLOBAL example_enum_var = e1;
SET GLOBAL example_double_var = 301.0000;
SET GLOBAL example_double_thdvar = 302.0000;
SET SESSION example_double_thdvar = 300.0000;

================================================================================
3.3 - CONNECT 3 CLIENTS, SET LOCAL PLUGIN VARS
================================================================================
connect con1, localhost, root,,;
SET SESSION example_double_thdvar = 300.1111;

connect con2, localhost, root,,;
SET SESSION example_double_thdvar = 300.2222;

connect con3, localhost, root,,;
SET SESSION example_double_thdvar = 300.3333;

connection default;

================================================================================
3.4 - VERIFY GLOBAL AND SESSION PLUGIN VARS
================================================================================

SHOW GLOBAL STATUS LIKE "example_%";
Variable_name	Value
example_func_example	enum_var is 0, ulong_var is 300, double_var is 301.000000, signed_int_var is -10, signed_long_var is -10, signed_longlong_var is -10
example_status_array_var1	100
example_status_array_var2	20.010000
example_status_var3	three hundred
example_status_var4	ON
example_status_var5	OFF
example_status_var6	8250

SHOW SESSION STATUS LIKE "example_%";
Variable_name	Value
example_func_example	enum_var is 0, ulong_var is 300, double_var is 301.000000, signed_int_var is -10, signed_long_var is -10, signed_longlong_var is -10
example_status_array_var1	100
example_status_array_var2	20.010000
example_status_var3	three hundred
example_status_var4	ON
example_status_var5	OFF
example_status_var6	8250

SHOW GLOBAL VARIABLES LIKE "example_%";
Variable_name	Value
example_create_count_thdvar	0
example_double_thdvar	302.000000
example_double_var	301.000000
example_enum_var	e1
example_last_create_thdvar	
example_signed_int_thdvar	-10
example_signed_int_var	-10
example_signed_long_thdvar	-10
example_signed_long_var	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_var	-10
example_ulong_var	300

SHOW SESSION VARIABLES LIKE "example_%";
Variable_name	Value
example_create_count_thdvar	0
example_double_thdvar	300.000000
example_double_var	301.000000
example_enum_var	e1
example_last_create_thdvar	
example_signed_int_thdvar	-10
example_signed_int_var	-10
example_signed_long_thdvar	-10
example_signed_long_var	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_var	-10
example_ulong_var	300

SELECT variable_name, variable_value FROM performance_schema.global_status WHERE variable_name LIKE "example_%";
variable_name	variable_value
example_func_example	enum_var is 0, ulong_var is 300, double_var is 301.000000, signed_int_var is -10, signed_long_var is -10, signed_longlong_var is -10
example_status_array_var1	100
example_status_array_var2	20.010000
example_status_var3	three hundred
example_status_var4	ON
example_status_var5	OFF
example_status_var6	8250

SELECT variable_name, variable_value FROM performance_schema.session_status WHERE variable_name LIKE "example_%";
variable_name	variable_value
example_func_example	enum_var is 0, ulong_var is 300, double_var is 301.000000, signed_int_var is -10, signed_long_var is -10, signed_longlong_var is -10
example_status_array_var1	100
example_status_array_var2	20.010000
example_status_var3	three hundred
example_status_var4	ON
example_status_var5	OFF
example_status_var6	8250

SELECT variable_name, variable_value FROM performance_schema.global_variables WHERE variable_name LIKE "example_%";
variable_name	variable_value
example_create_count_thdvar	0
example_double_thdvar	302.000000
example_double_var	301.000000
example_enum_var	e1
example_last_create_thdvar	
example_signed_int_thdvar	-10
example_signed_int_var	-10
example_signed_long_thdvar	-10
example_signed_long_var	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_var	-10
example_ulong_var	300

SELECT variable_name, variable_value FROM performance_schema.session_variables WHERE variable_name LIKE "example_%";
variable_name	variable_value
example_create_count_thdvar	0
example_double_thdvar	300.000000
example_double_var	301.000000
example_enum_var	e1
example_last_create_thdvar	
example_signed_int_thdvar	-10
example_signed_int_var	-10
example_signed_long_thdvar	-10
example_signed_long_var	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_var	-10
example_ulong_var	300

# Variables by thread
SELECT variable_name, variable_value FROM performance_schema.variables_by_thread
WHERE variable_name LIKE "example_%" ORDER BY variable_name, variable_value;
variable_name	variable_value
example_create_count_thdvar	0
example_create_count_thdvar	0
example_create_count_thdvar	0
example_create_count_thdvar	0
example_double_thdvar	300.000000
example_double_thdvar	300.111100
example_double_thdvar	300.222200
example_double_thdvar	300.333300
example_last_create_thdvar	
example_last_create_thdvar	
example_last_create_thdvar	
example_last_create_thdvar	
example_signed_int_thdvar	-10
example_signed_int_thdvar	-10
example_signed_int_thdvar	-10
example_signed_int_thdvar	-10
example_signed_long_thdvar	-10
example_signed_long_thdvar	-10
example_signed_long_thdvar	-10
example_signed_long_thdvar	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_thdvar	-10

================================================================================
3.5 - DISCONNECT CLIENTS
================================================================================
connection con1;
disconnect con1;
connection con2;
disconnect con2;
connection con3;
disconnect con3;
connection default;

================================================================================
3.6 - VERIFY SESSION VARS ARE REMOVED
================================================================================

SHOW GLOBAL STATUS LIKE "example_%";
Variable_name	Value
example_func_example	enum_var is 0, ulong_var is 300, double_var is 301.000000, signed_int_var is -10, signed_long_var is -10, signed_longlong_var is -10
example_status_array_var1	100
example_status_array_var2	20.010000
example_status_var3	three hundred
example_status_var4	ON
example_status_var5	OFF
example_status_var6	8250

SHOW SESSION STATUS LIKE "example_%";
Variable_name	Value
example_func_example	enum_var is 0, ulong_var is 300, double_var is 301.000000, signed_int_var is -10, signed_long_var is -10, signed_longlong_var is -10
example_status_array_var1	100
example_status_array_var2	20.010000
example_status_var3	three hundred
example_status_var4	ON
example_status_var5	OFF
example_status_var6	8250

SHOW GLOBAL VARIABLES LIKE "example_%";
Variable_name	Value
example_create_count_thdvar	0
example_double_thdvar	302.000000
example_double_var	301.000000
example_enum_var	e1
example_last_create_thdvar	
example_signed_int_thdvar	-10
example_signed_int_var	-10
example_signed_long_thdvar	-10
example_signed_long_var	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_var	-10
example_ulong_var	300

SHOW SESSION VARIABLES LIKE "example_%";
Variable_name	Value
example_create_count_thdvar	0
example_double_thdvar	300.000000
example_double_var	301.000000
example_enum_var	e1
example_last_create_thdvar	
example_signed_int_thdvar	-10
example_signed_int_var	-10
example_signed_long_thdvar	-10
example_signed_long_var	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_var	-10
example_ulong_var	300

SELECT variable_name, variable_value FROM performance_schema.global_status WHERE variable_name LIKE "example_%";
variable_name	variable_value
example_func_example	enum_var is 0, ulong_var is 300, double_var is 301.000000, signed_int_var is -10, signed_long_var is -10, signed_longlong_var is -10
example_status_array_var1	100
example_status_array_var2	20.010000
example_status_var3	three hundred
example_status_var4	ON
example_status_var5	OFF
example_status_var6	8250

SELECT variable_name, variable_value FROM performance_schema.session_status WHERE variable_name LIKE "example_%";
variable_name	variable_value
example_func_example	enum_var is 0, ulong_var is 300, double_var is 301.000000, signed_int_var is -10, signed_long_var is -10, signed_longlong_var is -10
example_status_array_var1	100
example_status_array_var2	20.010000
example_status_var3	three hundred
example_status_var4	ON
example_status_var5	OFF
example_status_var6	8250

SELECT variable_name, variable_value FROM performance_schema.global_variables WHERE variable_name LIKE "example_%";
variable_name	variable_value
example_create_count_thdvar	0
example_double_thdvar	302.000000
example_double_var	301.000000
example_enum_var	e1
example_last_create_thdvar	
example_signed_int_thdvar	-10
example_signed_int_var	-10
example_signed_long_thdvar	-10
example_signed_long_var	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_var	-10
example_ulong_var	300

SELECT variable_name, variable_value FROM performance_schema.session_variables WHERE variable_name LIKE "example_%";
variable_name	variable_value
example_create_count_thdvar	0
example_double_thdvar	300.000000
example_double_var	301.000000
example_enum_var	e1
example_last_create_thdvar	
example_signed_int_thdvar	-10
example_signed_int_var	-10
example_signed_long_thdvar	-10
example_signed_long_var	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_var	-10
example_ulong_var	300

# Variables by thread
SELECT variable_name, variable_value FROM performance_schema.variables_by_thread
WHERE variable_name LIKE "example_%" ORDER BY variable_name, variable_value;
variable_name	variable_value
example_create_count_thdvar	0
example_double_thdvar	300.000000
example_last_create_thdvar	
example_signed_int_thdvar	-10
example_signed_long_thdvar	-10
example_signed_longlong_thdvar	-10

================================================================================
3.7 - RECONNECT 3 CLIENTS, SET SESSION VARS FOR EACH
================================================================================
connect con1, localhost, root,,;
SET SESSION example_double_thdvar = 311.1111;

connect con2, localhost, root,,;
SET SESSION example_double_thdvar = 322.2222;

connect con3, localhost, root,,;
SET SESSION example_double_thdvar = 333.3333;

connection default;

================================================================================
3.8 - VERIFY GLOBAL AND SESSION VARS
================================================================================

SHOW GLOBAL STATUS LIKE "example_%";
Variable_name	Value
example_func_example	enum_var is 0, ulong_var is 300, double_var is 301.000000, signed_int_var is -10, signed_long_var is -10, signed_longlong_var is -10
example_status_array_var1	100
example_status_array_var2	20.010000
example_status_var3	three hundred
example_status_var4	ON
example_status_var5	OFF
example_status_var6	8250

SHOW SESSION STATUS LIKE "example_%";
Variable_name	Value
example_func_example	enum_var is 0, ulong_var is 300, double_var is 301.000000, signed_int_var is -10, signed_long_var is -10, signed_longlong_var is -10
example_status_array_var1	100
example_status_array_var2	20.010000
example_status_var3	three hundred
example_status_var4	ON
example_status_var5	OFF
example_status_var6	8250

SHOW GLOBAL VARIABLES LIKE "example_%";
Variable_name	Value
example_create_count_thdvar	0
example_double_thdvar	302.000000
example_double_var	301.000000
example_enum_var	e1
example_last_create_thdvar	
example_signed_int_thdvar	-10
example_signed_int_var	-10
example_signed_long_thdvar	-10
example_signed_long_var	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_var	-10
example_ulong_var	300

SHOW SESSION VARIABLES LIKE "example_%";
Variable_name	Value
example_create_count_thdvar	0
example_double_thdvar	300.000000
example_double_var	301.000000
example_enum_var	e1
example_last_create_thdvar	
example_signed_int_thdvar	-10
example_signed_int_var	-10
example_signed_long_thdvar	-10
example_signed_long_var	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_var	-10
example_ulong_var	300

SELECT variable_name, variable_value FROM performance_schema.global_status WHERE variable_name LIKE "example_%";
variable_name	variable_value
example_func_example	enum_var is 0, ulong_var is 300, double_var is 301.000000, signed_int_var is -10, signed_long_var is -10, signed_longlong_var is -10
example_status_array_var1	100
example_status_array_var2	20.010000
example_status_var3	three hundred
example_status_var4	ON
example_status_var5	OFF
example_status_var6	8250

SELECT variable_name, variable_value FROM performance_schema.session_status WHERE variable_name LIKE "example_%";
variable_name	variable_value
example_func_example	enum_var is 0, ulong_var is 300, double_var is 301.000000, signed_int_var is -10, signed_long_var is -10, signed_longlong_var is -10
example_status_array_var1	100
example_status_array_var2	20.010000
example_status_var3	three hundred
example_status_var4	ON
example_status_var5	OFF
example_status_var6	8250

SELECT variable_name, variable_value FROM performance_schema.global_variables WHERE variable_name LIKE "example_%";
variable_name	variable_value
example_create_count_thdvar	0
example_double_thdvar	302.000000
example_double_var	301.000000
example_enum_var	e1
example_last_create_thdvar	
example_signed_int_thdvar	-10
example_signed_int_var	-10
example_signed_long_thdvar	-10
example_signed_long_var	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_var	-10
example_ulong_var	300

SELECT variable_name, variable_value FROM performance_schema.session_variables WHERE variable_name LIKE "example_%";
variable_name	variable_value
example_create_count_thdvar	0
example_double_thdvar	300.000000
example_double_var	301.000000
example_enum_var	e1
example_last_create_thdvar	
example_signed_int_thdvar	-10
example_signed_int_var	-10
example_signed_long_thdvar	-10
example_signed_long_var	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_var	-10
example_ulong_var	300

# Variables by thread
SELECT variable_name, variable_value FROM performance_schema.variables_by_thread
WHERE variable_name LIKE "example_%" ORDER BY variable_name, variable_value;
variable_name	variable_value
example_create_count_thdvar	0
example_create_count_thdvar	0
example_create_count_thdvar	0
example_create_count_thdvar	0
example_double_thdvar	300.000000
example_double_thdvar	311.111100
example_double_thdvar	322.222200
example_double_thdvar	333.333300
example_last_create_thdvar	
example_last_create_thdvar	
example_last_create_thdvar	
example_last_create_thdvar	
example_signed_int_thdvar	-10
example_signed_int_thdvar	-10
example_signed_int_thdvar	-10
example_signed_int_thdvar	-10
example_signed_long_thdvar	-10
example_signed_long_thdvar	-10
example_signed_long_thdvar	-10
example_signed_long_thdvar	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_thdvar	-10

================================================================================
3.9 - UNINSTALL PLUGIN, LEAVE CLIENTS CONNECTED
================================================================================
UNINSTALL PLUGIN example;

================================================================================
3.10 - VERIFY SESSION VARS ARE REMOVED
================================================================================

SHOW GLOBAL STATUS LIKE "example_%";
Variable_name	Value

SHOW SESSION STATUS LIKE "example_%";
Variable_name	Value

SHOW GLOBAL VARIABLES LIKE "example_%";
Variable_name	Value

SHOW SESSION VARIABLES LIKE "example_%";
Variable_name	Value

SELECT variable_name, variable_value FROM performance_schema.global_status WHERE variable_name LIKE "example_%";
variable_name	variable_value

SELECT variable_name, variable_value FROM performance_schema.session_status WHERE variable_name LIKE "example_%";
variable_name	variable_value

SELECT variable_name, variable_value FROM performance_schema.global_variables WHERE variable_name LIKE "example_%";
variable_name	variable_value

SELECT variable_name, variable_value FROM performance_schema.session_variables WHERE variable_name LIKE "example_%";
variable_name	variable_value

# VARIABLES BY THREAD
SELECT variable_name, variable_value FROM performance_schema.variables_by_thread
WHERE variable_name LIKE "example_%" ORDER BY variable_name, variable_value;
variable_name	variable_value

================================================================================
3.11 - DISCONNECT CLIENTS
================================================================================
connection con1;
disconnect con1;
connection con2;
disconnect con2;
connection con3;
disconnect con3;
connection default;

================================================================================
3.12 - VERIFY CLIENTS ARE REMOVED
================================================================================

SHOW GLOBAL STATUS LIKE "example_%";
Variable_name	Value

SHOW SESSION STATUS LIKE "example_%";
Variable_name	Value

SHOW GLOBAL VARIABLES LIKE "example_%";
Variable_name	Value

SHOW SESSION VARIABLES LIKE "example_%";
Variable_name	Value

SELECT variable_name, variable_value FROM performance_schema.global_status WHERE variable_name LIKE "example_%";
variable_name	variable_value

SELECT variable_name, variable_value FROM performance_schema.session_status WHERE variable_name LIKE "example_%";
variable_name	variable_value

SELECT variable_name, variable_value FROM performance_schema.global_variables WHERE variable_name LIKE "example_%";
variable_name	variable_value

SELECT variable_name, variable_value FROM performance_schema.session_variables WHERE variable_name LIKE "example_%";
variable_name	variable_value

# VARIABLES BY THREAD
SELECT variable_name, variable_value FROM performance_schema.variables_by_thread
WHERE variable_name LIKE "example_%" ORDER BY variable_name, variable_value;
variable_name	variable_value

================================================================================
TEST 5 - BUG#22225549 MYSQL_CHANGE_USER/MYSQL_RESET_CONNECTION + SET INNODB...
Update to plugin-defined session variable triggers resync with global
variables and deadlocks on THD::LOCK_thd_sysvar.
================================================================================
select @@session.innodb_strict_mode;
@@session.innodb_strict_mode
1
select user(), current_user();
user()	current_user()
root@localhost	root@localhost
change_user root
#
# Trigger a resync of session variables with global variables.
#
set @@session.innodb_strict_mode=off;

# Restore

set @@session.innodb_strict_mode=1;
select @@session.innodb_strict_mode;
@@session.innodb_strict_mode
1

================================================================================
CLEAN UP
================================================================================
connection default;



# Wait till we reached the initial number of concurrent sessions
