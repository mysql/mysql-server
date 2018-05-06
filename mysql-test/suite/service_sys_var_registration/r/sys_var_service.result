call mtr.add_suppression("duplicate variable name");
# Simple load test
INSTALL COMPONENT "file://component_test_sys_var_service";
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
UNINSTALL COMPONENT "file://component_test_sys_var_service";
########## test_component_sys_var_service.log:
test_component_sys_var init:
int register_variable failed.
variable value : 8
variable value : 1024
variable value : 100
variable value : 8192
variable value : 8192
variable value : 8192
variable value : ON
variable value : MEDIUM
variable value : 
test_component_sys_var end of init:
test_component_sys_var deinit:
test_component_sys_var end of deinit:
