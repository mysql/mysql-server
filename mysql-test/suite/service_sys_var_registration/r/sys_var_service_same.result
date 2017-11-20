call mtr.add_suppression("duplicate variable name");
call mtr.add_suppression("variable name.*not found");
# Simple load test
INSTALL COMPONENT "file://component_test_sys_var_service";
INSTALL COMPONENT "file://component_test_sys_var_service_same";
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
UNINSTALL COMPONENT "file://component_test_sys_var_service";
UNINSTALL COMPONENT "file://component_test_sys_var_service_same";
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
########## test_component_sys_var_service_same.log:
test_component_sys_var_same init:
int register_variable failed.
int register_variable failed.
uint register_variable failed.
long register_variable failed.
ulong register_variable failed.
longlong register_variable failed.
unsigned longlong register_variable failed.
register_variable failed.
register_variable failed.
register_variable failed.
variable value : 8
End of init
test_component_sys_var_same deinit:
int unregister_variable failed.
unsigned int unregister_variable failed.
long unregister_variable failed.
unsigned long unregister_variable failed.
longlong unregister_variable failed.
unsigned longlong unregister_variable failed.
unregister_variable failed.
unregister_variable failed.
unregister_variable failed.
unregister_variable failed.
