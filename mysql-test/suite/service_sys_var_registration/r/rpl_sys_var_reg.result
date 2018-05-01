include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
call mtr.add_suppression("duplicate variable name");
# Simple load test
INSTALL COMPONENT "file://component_test_sys_var_service";
SELECT VARIABLE_NAME, MIN_VALUE, MAX_VALUE FROM performance_schema.variables_info
WHERE VARIABLE_NAME LIKE '%component%'
       ORDER BY VARIABLE_NAME;
VARIABLE_NAME	MIN_VALUE	MAX_VALUE
test_component.bool_sys_var	0	0
test_component.enum_sys_var	0	0
test_component.int_sys_var	0	1024
test_component.long_sys_var	10	100
test_component.longlong_sys_var	1000	8192819281928192
test_component.str_sys_var	0	0
test_component.uint_sys_var	10	10241024
test_component.ulong_sys_var	1000	81928192
test_component.ulonglong_sys_var	1000	8192819281928192
# Testing integer and bool type variable
SELECT @@test_component.int_sys_var;
@@test_component.int_sys_var
8
SELECT @@global.test_component.int_sys_var;
@@global.test_component.int_sys_var
8
SELECT @@global.test_component.bool_sys_var;
@@global.test_component.bool_sys_var
1
SELECT @@test_component.int_sys_var, @@test_component.bool_sys_var;
@@test_component.int_sys_var	@@test_component.bool_sys_var
8	1
SET test_component.bool_sys_var=1;
ERROR HY000: Variable 'test_component.bool_sys_var' is a GLOBAL variable and should be set with SET GLOBAL
SET GLOBAL test_component.int_sys_var=7;
SET GLOBAL test_component.int_sys_var=1234567;
Warnings:
Warning	1292	Truncated incorrect test_component.int_sys_var value: '1234567'
SELECT @@test_component.int_sys_var;
@@test_component.int_sys_var
1024
SET GLOBAL test_component.int_sys_var=-1;
Warnings:
Warning	1292	Truncated incorrect test_component.int_sys_var value: '-1'
SELECT @@test_component.int_sys_var;
@@test_component.int_sys_var
0
SET GLOBAL test_component.int_sys_var=1.25;
ERROR 42000: Incorrect argument type to variable 'test_component.int_sys_var'
SET GLOBAL test_component.int_sys_var="hello";
ERROR 42000: Incorrect argument type to variable 'test_component.int_sys_var'
SET GLOBAL test_component.int_sys_var=123, test_component.bool_sys_var=0;
SELECT @@test_component.int_sys_var, @@test_component.bool_sys_var;
@@test_component.int_sys_var	@@test_component.bool_sys_var
123	0
SET SESSION test_component.int_sys_var=123;
ERROR HY000: Variable 'test_component.int_sys_var' is a GLOBAL variable and should be set with SET GLOBAL
# Testing enum type variable
SELECT @@test_component.enum_sys_var;
@@test_component.enum_sys_var
MEDIUM
SET GLOBAL test_component.enum_sys_var="LOW";
SELECT @@test_component.enum_sys_var;
@@test_component.enum_sys_var
LOW
# Testing string type variable
SELECT @@test_component.str_sys_var;
@@test_component.str_sys_var
NULL
SET GLOBAL test_component.str_sys_var="dictionary.txt";
SELECT @@test_component.str_sys_var;
@@test_component.str_sys_var
dictionary.txt
SET GLOBAL test_component.str_sys_var=default;
SELECT @@test_component.str_sys_var;
@@test_component.str_sys_var
NULL
# Testing unsigned integer type variable
SELECT @@test_component.uint_sys_var;
@@test_component.uint_sys_var
1024
SET GLOBAL test_component.uint_sys_var=12345678;
Warnings:
Warning	1292	Truncated incorrect test_component.uint_sys_var value: '12345678'
SELECT @@test_component.uint_sys_var;
@@test_component.uint_sys_var
10241024
SET GLOBAL test_component.uint_sys_var=default;
SELECT @@test_component.uint_sys_var;
@@test_component.uint_sys_var
1024
# Testing long type variable
SELECT @@test_component.long_sys_var;
@@test_component.long_sys_var
100
SET GLOBAL test_component.long_sys_var=1234567890;
Warnings:
Warning	1292	Truncated incorrect test_component.long_sys_var value: '1234567890'
SELECT @@test_component.long_sys_var;
@@test_component.long_sys_var
100
SET GLOBAL test_component.long_sys_var=default;
SELECT @@test_component.long_sys_var;
@@test_component.long_sys_var
100
# Testing unsigned long type variable
SELECT @@test_component.ulong_sys_var;
@@test_component.ulong_sys_var
8192
SET GLOBAL test_component.ulong_sys_var=1234567890;
Warnings:
Warning	1292	Truncated incorrect test_component.ulong_sys_var value: '1234567890'
SELECT @@test_component.ulong_sys_var;
@@test_component.ulong_sys_var
81928192
SET GLOBAL test_component.ulong_sys_var=default;
SELECT @@test_component.ulong_sys_var;
@@test_component.ulong_sys_var
8192
# Testing long long type variable
SELECT @@test_component.longlong_sys_var;
@@test_component.longlong_sys_var
8192
SET GLOBAL test_component.longlong_sys_var=1234567890;
SELECT @@test_component.longlong_sys_var;
@@test_component.longlong_sys_var
1234567890
SET GLOBAL test_component.longlong_sys_var=default;
SELECT @@test_component.longlong_sys_var;
@@test_component.longlong_sys_var
8192
# Testing unsigned long long type variable
SELECT @@test_component.ulonglong_sys_var;
@@test_component.ulonglong_sys_var
8192
SET GLOBAL test_component.ulonglong_sys_var=1234567890;
SELECT @@test_component.ulonglong_sys_var;
@@test_component.ulonglong_sys_var
1234567890
SET GLOBAL test_component.ulonglong_sys_var=default;
SELECT @@test_component.ulonglong_sys_var;
@@test_component.ulonglong_sys_var
8192
# Testing PERSIST for component system variables
SET PERSIST test_component.int_sys_var=7;
SELECT @@test_component.int_sys_var;
@@test_component.int_sys_var
7
SELECT * FROM performance_schema.persisted_variables;
VARIABLE_NAME	VARIABLE_VALUE
test_component.int_sys_var	7
RESET PERSIST `test_component.int_sys_var`;
SELECT * FROM performance_schema.persisted_variables;
VARIABLE_NAME	VARIABLE_VALUE
sync_slave_with_master;
connection slave;
INSTALL COMPONENT "file://component_test_sys_var_service";
SELECT VARIABLE_NAME, MIN_VALUE, MAX_VALUE FROM performance_schema.variables_info
WHERE VARIABLE_NAME LIKE '%component%'
       ORDER BY VARIABLE_NAME;
VARIABLE_NAME	MIN_VALUE	MAX_VALUE
test_component.bool_sys_var	0	0
test_component.enum_sys_var	0	0
test_component.int_sys_var	0	1024
test_component.long_sys_var	10	100
test_component.longlong_sys_var	1000	8192819281928192
test_component.str_sys_var	0	0
test_component.uint_sys_var	10	10241024
test_component.ulong_sys_var	1000	81928192
test_component.ulonglong_sys_var	1000	8192819281928192
SET GLOBAL test_component.int_sys_var=7;
SET GLOBAL test_component.str_sys_var="salve.txt";
SELECT @@test_component.int_sys_var;
@@test_component.int_sys_var
7
SELECT @@test_component.long_sys_var;
@@test_component.long_sys_var
100
SELECT @@test_component.ulong_sys_var;
@@test_component.ulong_sys_var
8192
SELECT @@test_component.longlong_sys_var;
@@test_component.longlong_sys_var
8192
SELECT @@test_component.ulonglong_sys_var;
@@test_component.ulonglong_sys_var
8192
SELECT @@test_component.str_sys_var;
@@test_component.str_sys_var
salve.txt
UNINSTALL COMPONENT "file://component_test_sys_var_service";
connection master;
UNINSTALL COMPONENT "file://component_test_sys_var_service";
include/rpl_end.inc
