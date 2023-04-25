CREATE TABLE t1(a int) ENGINE=EXAMPLE;
ERROR 42000: Unknown storage engine 'EXAMPLE'
INSTALL PLUGIN example SONAME 'ha_example.so';
INSTALL PLUGIN EXAMPLE SONAME 'ha_example.so';
ERROR HY000: Function 'EXAMPLE' already exists
UNINSTALL PLUGIN example;
INSTALL PLUGIN example SONAME 'ha_example.so';
# Example engine does not support indexes
CREATE TABLE t1 (a int PRIMARY KEY) ENGINE=EXAMPLE;
ERROR 42000: Too many keys specified; max 0 keys allowed
CREATE TABLE t1 (a int, KEY (a)) ENGINE=EXAMPLE;
ERROR 42000: Too many keys specified; max 0 keys allowed
CREATE TABLE t1(a int) ENGINE=EXAMPLE;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL
) ENGINE=EXAMPLE DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
# Let's do some advanced ops with the example engine :)
INSERT INTO t1 VALUES (0);
# Only supports table scans (and does always return zero rows :)
SELECT * FROM t1;
a
SELECT * FROM t1 WHERE a = 0;
a
# Since there are no rows found, it will never do update_row.
UPDATE t1 SET a = 1 WHERE a = 0;
# Since there are no rows found, it will never do delete_row.
DELETE FROM t1 WHERE a = 0;
# No support for SQL HANDLER statement
HANDLER t1 OPEN;
ERROR HY000: Table storage engine for 't1' doesn't have this option
# Try to delete table when plugin is not loaded.
# Remove table from TDC to allow plugin unload.
FLUSH TABLE t1;
UNINSTALL PLUGIN example;
DROP TABLE t1;
ERROR 42000: Unknown storage engine 'EXAMPLE'
INSTALL PLUGIN example SONAME 'ha_example.so';
DROP TABLE t1;
set global example_ulong_var=500;
set global example_enum_var= e1;
select * from performance_schema.global_status where variable_name like 'example%' order by variable_name;
VARIABLE_NAME	VARIABLE_VALUE
example_func_example	enum_var is 0, ulong_var is 500, double_var is 8.500000, signed_int_var is -10, signed_long_var is -10, signed_longlong_var is -10
example_status_array_var1	100
example_status_array_var2	20.010000
example_status_var3	three hundred
example_status_var4	ON
example_status_var5	OFF
example_status_var6	8250
show variables like 'example%';
Variable_name	Value
example_create_count_thdvar	1
example_double_thdvar	8.500000
example_double_var	8.500000
example_enum_var	e1
example_last_create_thdvar	Last creation 't1'
example_signed_int_thdvar	-10
example_signed_int_var	-10
example_signed_long_thdvar	-10
example_signed_long_var	-10
example_signed_longlong_thdvar	-10
example_signed_longlong_var	-10
example_ulong_var	500
UNINSTALL PLUGIN example;
UNINSTALL PLUGIN EXAMPLE;
ERROR 42000: PLUGIN EXAMPLE does not exist
UNINSTALL PLUGIN non_exist;
ERROR 42000: PLUGIN non_exist does not exist
#
# Bug#32034: check_func_enum() does not check correct values but set it
#            to impossible int val
#
INSTALL PLUGIN example SONAME 'ha_example.so';
SET GLOBAL example_enum_var= e1;
SET GLOBAL example_enum_var= e2;
SET GLOBAL example_enum_var= impossible;
ERROR 42000: Variable 'example_enum_var' can't be set to the value of 'impossible'
UNINSTALL PLUGIN example;
INSTALL PLUGIN example SONAME 'ha_example.so';
select @@session.sql_mode into @old_sql_mode;
set session sql_mode='';
set global example_ulong_var=500;
select @@global.example_ulong_var;
@@global.example_ulong_var
500
set global example_ulong_var=1111;
Warnings:
Warning	1292	Truncated incorrect example_ulong_var value: '1111'
select @@global.example_ulong_var;
@@global.example_ulong_var
1000
set session sql_mode='STRICT_ALL_TABLES';
Warnings:
Warning	3135	'NO_ZERO_DATE', 'NO_ZERO_IN_DATE' and 'ERROR_FOR_DIVISION_BY_ZERO' sql modes should be used with strict mode. They will be merged with strict mode in a future release.
set global example_ulong_var=500;
select @@global.example_ulong_var;
@@global.example_ulong_var
500
set global example_ulong_var=1111;
ERROR 42000: Variable 'example_ulong_var' can't be set to the value of '1111'
select @@global.example_ulong_var;
@@global.example_ulong_var
500
set session sql_mode=@old_sql_mode;
set session old=bla;
ERROR HY000: Variable 'old' is a read only variable
UNINSTALL PLUGIN example;
#
# Bug #16194302  SUPPORT FOR FLOATING-POINT SYSTEM
#                VARIABLES USING THE PLUGIN INTERFACE.
#
INSTALL PLUGIN example SONAME 'ha_example.so';
SET GLOBAL example_double_var = -0.1;
Warnings:
Warning	1292	Truncated incorrect example_double_var value: '-0.1'
SELECT @@GLOBAL.example_double_var;
@@GLOBAL.example_double_var
0.500000
SET GLOBAL example_double_var = 0.000001;
Warnings:
Warning	1292	Truncated incorrect example_double_var value: '0.000001'
SELECT @@GLOBAL.example_double_var;
@@GLOBAL.example_double_var
0.500000
SET GLOBAL example_double_var = 0.4;
Warnings:
Warning	1292	Truncated incorrect example_double_var value: '0.4'
SELECT @@GLOBAL.example_double_var;
@@GLOBAL.example_double_var
0.500000
SET GLOBAL example_double_var = 123.456789;
SELECT @@GLOBAL.example_double_var;
@@GLOBAL.example_double_var
123.456789
SET GLOBAL example_double_var = 500;
SELECT @@GLOBAL.example_double_var;
@@GLOBAL.example_double_var
500.000000
SET GLOBAL example_double_var = 999.999999;
SELECT @@GLOBAL.example_double_var;
@@GLOBAL.example_double_var
999.999999
SET GLOBAL example_double_var = 1000.51;
Warnings:
Warning	1292	Truncated incorrect example_double_var value: '1000.51'
SELECT @@GLOBAL.example_double_var;
@@GLOBAL.example_double_var
1000.500000
SET SESSION example_double_thdvar = -0.1;
Warnings:
Warning	1292	Truncated incorrect example_double_thdvar value: '-0.1'
SELECT @@SESSION.example_double_thdvar;
@@SESSION.example_double_thdvar
0.500000
SET SESSION example_double_thdvar = 0.000001;
Warnings:
Warning	1292	Truncated incorrect example_double_thdvar value: '0.000001'
SELECT @@SESSION.example_double_thdvar;
@@SESSION.example_double_thdvar
0.500000
SET SESSION example_double_thdvar = 0.4;
Warnings:
Warning	1292	Truncated incorrect example_double_thdvar value: '0.4'
SELECT @@SESSION.example_double_thdvar;
@@SESSION.example_double_thdvar
0.500000
SET SESSION example_double_thdvar = 123.456789;
SELECT @@SESSION.example_double_thdvar;
@@SESSION.example_double_thdvar
123.456789
SET SESSION example_double_thdvar = 500;
SELECT @@SESSION.example_double_thdvar;
@@SESSION.example_double_thdvar
500.000000
SET SESSION example_double_thdvar = 999.999999;
SELECT @@SESSION.example_double_thdvar;
@@SESSION.example_double_thdvar
999.999999
SET SESSION example_double_thdvar = 1000.51;
Warnings:
Warning	1292	Truncated incorrect example_double_thdvar value: '1000.51'
SELECT @@SESSION.example_double_thdvar;
@@SESSION.example_double_thdvar
1000.500000
UNINSTALL PLUGIN example;
#
# Bug #11759453: UNINSTALL PLUGIN MISLEADING ERROR
#                MESSAGE FOR NON-DYNAMIC PLUGINS
#
UNINSTALL PLUGIN innodb;
ERROR HY000: Built-in plugins cannot be deleted
#
# Bug #19917521: MEMORY LEAK WITH STRING THREAD VARIABLE 
#                THAT SET MEMALLOC FLAG
#
INSTALL PLUGIN example SONAME 'ha_example.so';
SET SESSION example_create_count_thdvar = 0;
SET SESSION example_last_create_thdvar = '';
CREATE TABLE t10(a INT) ENGINE=EXAMPLE;
SELECT @@SESSION.example_create_count_thdvar;
@@SESSION.example_create_count_thdvar
1
SELECT @@SESSION.example_last_create_thdvar;
@@SESSION.example_last_create_thdvar
Last creation 't10'
CREATE TABLE t20(a INT) ENGINE=EXAMPLE;
SELECT @@SESSION.example_create_count_thdvar;
@@SESSION.example_create_count_thdvar
2
SELECT @@SESSION.example_last_create_thdvar;
@@SESSION.example_last_create_thdvar
Last creation 't20'
DROP TABLE t10, t20;
UNINSTALL PLUGIN example;
#
# BUG#24344026 - SIG 11 IN MY_VALID_MBCHARLEN_UTF8 AT STRINGS/CTYPE-UTF8.CC:9437
#
SET GLOBAL DEBUG='+d,set_uninstall_sync_point';
INSTALL PLUGIN example SONAME 'ha_example.so';
SET DEBUG_SYNC='before_store_plugin_name SIGNAL uninstall_plugin WAIT_FOR plugin_uninstalled';
SELECT * FROM INFORMATION_SCHEMA.PLUGINS WHERE PLUGIN_NAME='example';;
SET DEBUG_SYNC='now WAIT_FOR uninstall_plugin';
UNINSTALL PLUGIN example;
SET DEBUG_SYNC='now SIGNAL plugin_uninstalled';
PLUGIN_NAME	PLUGIN_VERSION	PLUGIN_STATUS	PLUGIN_TYPE	PLUGIN_TYPE_VERSION	PLUGIN_LIBRARY	PLUGIN_LIBRARY_VERSION	PLUGIN_AUTHOR	PLUGIN_DESCRIPTION	PLUGIN_LICENSE	LOAD_OPTION
SET DEBUG_SYNC='RESET';
SET GLOBAL DEBUG='-d,set_uninstall_sync_point';
#
# Bug#51770: UNINSTALL PLUGIN requires no privileges
#
CREATE USER bug51770@localhost;
GRANT INSERT ON mysql.plugin TO bug51770@localhost;
INSTALL PLUGIN example SONAME 'ha_example.so';
UNINSTALL PLUGIN example;
ERROR 42000: DELETE command denied to user 'bug51770'@'localhost' for table 'plugin'
GRANT DELETE ON mysql.plugin TO bug51770@localhost;
UNINSTALL PLUGIN example;
DROP USER bug51770@localhost;
INSTALL PLUGIN example SONAME '../ha_example.so';
ERROR HY000: No paths allowed for shared library
#
# Bug #28534414: NEGATIVE NUMBERS IN PLUGIN VARIABLES
#                NOT DISPLAYED CORRECTLY
#
INSTALL PLUGIN example SONAME 'ha_example.so';
SELECT * FROM performance_schema.global_status WHERE variable_name LIKE 'example_func_example' ORDER BY variable_name;
VARIABLE_NAME	VARIABLE_VALUE
example_func_example	enum_var is 0, ulong_var is 8, double_var is 8.500000, signed_int_var is -10, signed_long_var is -10, signed_longlong_var is -10
SHOW VARIABLES LIKE 'example_signed_int_var';
Variable_name	Value
example_signed_int_var	-10
SELECT @@GLOBAL.example_signed_int_var;
@@GLOBAL.example_signed_int_var
-10
SET GLOBAL example_signed_int_var = -2147483648;
SELECT @@GLOBAL.example_signed_int_var;
@@GLOBAL.example_signed_int_var
-2147483648
SET GLOBAL example_signed_int_var = -100;
SELECT @@GLOBAL.example_signed_int_var;
@@GLOBAL.example_signed_int_var
-100
SET GLOBAL example_signed_int_var = 0;
SELECT @@GLOBAL.example_signed_int_var;
@@GLOBAL.example_signed_int_var
0
SET GLOBAL example_signed_int_var = 100;
SELECT @@GLOBAL.example_signed_int_var;
@@GLOBAL.example_signed_int_var
100
SET GLOBAL example_signed_int_var = 2147483647;
SELECT @@GLOBAL.example_signed_int_var;
@@GLOBAL.example_signed_int_var
2147483647
SET GLOBAL example_signed_int_var = -2147483649;
Warnings:
Warning	1292	Truncated incorrect example_signed_int_var value: '-2147483649'
SELECT @@GLOBAL.example_signed_int_var;
@@GLOBAL.example_signed_int_var
-2147483648
SET GLOBAL example_signed_int_var = 2147483648;
Warnings:
Warning	1292	Truncated incorrect example_signed_int_var value: '2147483648'
SELECT @@GLOBAL.example_signed_int_var;
@@GLOBAL.example_signed_int_var
2147483647
SHOW SESSION VARIABLES LIKE 'example_signed_int_thdvar';
Variable_name	Value
example_signed_int_thdvar	-10
SELECT @@SESSION.example_signed_int_thdvar;
@@SESSION.example_signed_int_thdvar
-10
SET SESSION example_signed_int_thdvar = -2147483648;
SELECT @@SESSION.example_signed_int_thdvar;
@@SESSION.example_signed_int_thdvar
-2147483648
SET SESSION example_signed_int_thdvar = -100;
SELECT @@SESSION.example_signed_int_thdvar;
@@SESSION.example_signed_int_thdvar
-100
SET SESSION example_signed_int_thdvar = 0;
SELECT @@SESSION.example_signed_int_thdvar;
@@SESSION.example_signed_int_thdvar
0
SET SESSION example_signed_int_thdvar = 100;
SELECT @@SESSION.example_signed_int_thdvar;
@@SESSION.example_signed_int_thdvar
100
SET SESSION example_signed_int_thdvar = 2147483647;
SELECT @@SESSION.example_signed_int_thdvar;
@@SESSION.example_signed_int_thdvar
2147483647
SET SESSION example_signed_int_thdvar = -2147483649;
Warnings:
Warning	1292	Truncated incorrect example_signed_int_thdvar value: '-2147483649'
SELECT @@SESSION.example_signed_int_thdvar;
@@SESSION.example_signed_int_thdvar
-2147483648
SET SESSION example_signed_int_thdvar = 2147483648;
Warnings:
Warning	1292	Truncated incorrect example_signed_int_thdvar value: '2147483648'
SELECT @@SESSION.example_signed_int_thdvar;
@@SESSION.example_signed_int_thdvar
2147483647
SHOW VARIABLES LIKE 'example_signed_long_var';
Variable_name	Value
example_signed_long_var	-10
SELECT @@GLOBAL.example_signed_long_var;
@@GLOBAL.example_signed_long_var
-10
SET GLOBAL example_signed_long_var = -9223372036854775808;
SELECT @@GLOBAL.example_signed_long_var IN (-2147483648, -9223372036854775808);
@@GLOBAL.example_signed_long_var IN (-2147483648, -9223372036854775808)
1
SET GLOBAL example_signed_long_var = -100;
SELECT @@GLOBAL.example_signed_long_var;
@@GLOBAL.example_signed_long_var
-100
SET GLOBAL example_signed_long_var = 0;
SELECT @@GLOBAL.example_signed_long_var;
@@GLOBAL.example_signed_long_var
0
SET GLOBAL example_signed_long_var = 100;
SELECT @@GLOBAL.example_signed_long_var;
@@GLOBAL.example_signed_long_var
100
SET GLOBAL example_signed_long_var = 9223372036854775807;
SELECT @@GLOBAL.example_signed_long_var IN (2147483647, 9223372036854775807);
@@GLOBAL.example_signed_long_var IN (2147483647, 9223372036854775807)
1
SET GLOBAL example_signed_long_var = -9223372036854775809;
ERROR 42000: Incorrect argument type to variable 'example_signed_long_var'
SELECT @@GLOBAL.example_signed_long_var IN (2147483647, 9223372036854775807);
@@GLOBAL.example_signed_long_var IN (2147483647, 9223372036854775807)
1
SET GLOBAL example_signed_long_var = 9223372036854775808;
SELECT @@GLOBAL.example_signed_long_var IN (2147483647, 9223372036854775807);
@@GLOBAL.example_signed_long_var IN (2147483647, 9223372036854775807)
1
SHOW SESSION VARIABLES LIKE 'example_signed_long_thdvar';
Variable_name	Value
example_signed_long_thdvar	-10
SELECT @@SESSION.example_signed_long_thdvar;
@@SESSION.example_signed_long_thdvar
-10
SET SESSION example_signed_long_thdvar = -9223372036854775808;
SELECT @@SESSION.example_signed_long_thdvar IN (-2147483648, -9223372036854775808);
@@SESSION.example_signed_long_thdvar IN (-2147483648, -9223372036854775808)
1
SET SESSION example_signed_long_thdvar = -100;
SELECT @@SESSION.example_signed_long_thdvar;
@@SESSION.example_signed_long_thdvar
-100
SET SESSION example_signed_long_thdvar = 0;
SELECT @@SESSION.example_signed_long_thdvar;
@@SESSION.example_signed_long_thdvar
0
SET SESSION example_signed_long_thdvar = 100;
SELECT @@SESSION.example_signed_long_thdvar;
@@SESSION.example_signed_long_thdvar
100
SET SESSION example_signed_long_thdvar = 9223372036854775807;
SELECT @@SESSION.example_signed_long_thdvar IN (2147483647, 9223372036854775807);
@@SESSION.example_signed_long_thdvar IN (2147483647, 9223372036854775807)
1
SET SESSION example_signed_long_thdvar = -9223372036854775809;
ERROR 42000: Incorrect argument type to variable 'example_signed_long_thdvar'
SELECT @@SESSION.example_signed_long_thdvar IN (2147483647, 9223372036854775807);
@@SESSION.example_signed_long_thdvar IN (2147483647, 9223372036854775807)
1
SET SESSION example_signed_long_thdvar = 9223372036854775808;
SELECT @@SESSION.example_signed_long_thdvar IN (2147483647, 9223372036854775807);
@@SESSION.example_signed_long_thdvar IN (2147483647, 9223372036854775807)
1
SHOW VARIABLES LIKE 'example_signed_longlong_var';
Variable_name	Value
example_signed_longlong_var	-10
SELECT @@GLOBAL.example_signed_longlong_var;
@@GLOBAL.example_signed_longlong_var
-10
SET GLOBAL example_signed_longlong_var = -9223372036854775808;
SELECT @@GLOBAL.example_signed_longlong_var;
@@GLOBAL.example_signed_longlong_var
-9223372036854775808
SET GLOBAL example_signed_longlong_var = -100;
SELECT @@GLOBAL.example_signed_longlong_var;
@@GLOBAL.example_signed_longlong_var
-100
SET GLOBAL example_signed_longlong_var = 0;
SELECT @@GLOBAL.example_signed_longlong_var;
@@GLOBAL.example_signed_longlong_var
0
SET GLOBAL example_signed_longlong_var = 100;
SELECT @@GLOBAL.example_signed_longlong_var;
@@GLOBAL.example_signed_longlong_var
100
SET GLOBAL example_signed_longlong_var = 9223372036854775807;
SELECT @@GLOBAL.example_signed_longlong_var;
@@GLOBAL.example_signed_longlong_var
9223372036854775807
SET GLOBAL example_signed_longlong_var = -9223372036854775809;
ERROR 42000: Incorrect argument type to variable 'example_signed_longlong_var'
SELECT @@GLOBAL.example_signed_longlong_var;
@@GLOBAL.example_signed_longlong_var
9223372036854775807
SET GLOBAL example_signed_longlong_var = 9223372036854775808;
Warnings:
Warning	1292	Truncated incorrect example_signed_longlong_var value: '9223372036854775808'
SELECT @@GLOBAL.example_signed_longlong_var;
@@GLOBAL.example_signed_longlong_var
9223372036854775807
SHOW SESSION VARIABLES LIKE 'example_signed_longlong_thdvar';
Variable_name	Value
example_signed_longlong_thdvar	-10
SELECT @@SESSION.example_signed_longlong_thdvar;
@@SESSION.example_signed_longlong_thdvar
-10
SET SESSION example_signed_longlong_thdvar = -9223372036854775808;
SELECT @@SESSION.example_signed_longlong_thdvar;
@@SESSION.example_signed_longlong_thdvar
-9223372036854775808
SET SESSION example_signed_longlong_thdvar = -100;
SELECT @@SESSION.example_signed_longlong_thdvar;
@@SESSION.example_signed_longlong_thdvar
-100
SET SESSION example_signed_longlong_thdvar = 0;
SELECT @@SESSION.example_signed_longlong_thdvar;
@@SESSION.example_signed_longlong_thdvar
0
SET SESSION example_signed_longlong_thdvar = 100;
SELECT @@SESSION.example_signed_longlong_thdvar;
@@SESSION.example_signed_longlong_thdvar
100
SET SESSION example_signed_longlong_thdvar = 9223372036854775807;
SELECT @@SESSION.example_signed_longlong_thdvar;
@@SESSION.example_signed_longlong_thdvar
9223372036854775807
SET SESSION example_signed_longlong_thdvar = -9223372036854775809;
ERROR 42000: Incorrect argument type to variable 'example_signed_longlong_thdvar'
SELECT @@SESSION.example_signed_longlong_thdvar;
@@SESSION.example_signed_longlong_thdvar
9223372036854775807
SET SESSION example_signed_longlong_thdvar = 9223372036854775808;
Warnings:
Warning	1292	Truncated incorrect example_signed_longlong_thdvar value: '9223372036854775808'
SELECT @@SESSION.example_signed_longlong_thdvar;
@@SESSION.example_signed_longlong_thdvar
9223372036854775807
UNINSTALL PLUGIN example;
