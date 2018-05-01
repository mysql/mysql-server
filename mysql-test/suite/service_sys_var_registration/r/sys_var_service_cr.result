call mtr.add_suppression("duplicate variable name");
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
SET GLOBAL test_component.str_sys_var="Before crash";
SET GLOBAL test_component.int_sys_var=123;
SELECT @@test_component.str_sys_var;
@@test_component.str_sys_var
Before crash
SELECT @@test_component.int_sys_var;
@@test_component.int_sys_var
123
# Kill and restart
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
SELECT @@test_component.str_sys_var;
@@test_component.str_sys_var
NULL
SELECT @@test_component.int_sys_var;
@@test_component.int_sys_var
8
SET PERSIST test_component.int_sys_var=7;
SELECT @@test_component.int_sys_var;
@@test_component.int_sys_var
7
# Kill and restart
SELECT @@test_component.int_sys_var;
@@test_component.int_sys_var
7
SELECT @@test_component.str_sys_var;
@@test_component.str_sys_var
NULL
RESET PERSIST;
UNINSTALL COMPONENT "file://component_test_sys_var_service";
