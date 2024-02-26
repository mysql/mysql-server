call mtr.add_suppression("duplicate variable name");
call mtr.add_suppression("variable name.*not found");
INSTALL COMPONENT "file://component_test_sys_var_service";
INSTALL COMPONENT "file://component_test_sys_var_service_int";
INSTALL COMPONENT "file://component_test_sys_var_service_str";
# file://component_test_sys_var_service:
SELECT @@test_component.enum_sys_var;
@@test_component.enum_sys_var
MEDIUM
SET GLOBAL test_component.enum_sys_var="LOW";
SELECT @@test_component.enum_sys_var;
@@test_component.enum_sys_var
LOW
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
# file://component_test_sys_var_service_str:
SELECT @@test_component_str.str_sys_var;
@@test_component_str.str_sys_var
NULL
SET GLOBAL test_component_str.str_sys_var="dictionary.txt";
SELECT @@test_component_str.str_sys_var;
@@test_component_str.str_sys_var
dictionary.txt
SET GLOBAL test_component_str.str_sys_var=default;
SELECT @@test_component_str.str_sys_var;
@@test_component_str.str_sys_var
NULL
# file://component_test_sys_var_service:
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
# file://component_test_sys_var_service_int:
SELECT @@test_component_int.uint_sys_var;
@@test_component_int.uint_sys_var
1024
SET GLOBAL test_component_int.uint_sys_var=12345678;
Warnings:
Warning	1292	Truncated incorrect test_component_int.uint_sys_var value: '12345678'
SELECT @@test_component_int.uint_sys_var;
@@test_component_int.uint_sys_var
10241024
SET GLOBAL test_component_int.uint_sys_var=default;
SELECT @@test_component_int.uint_sys_var;
@@test_component_int.uint_sys_var
1024
SELECT @@test_component_int.int_sys_var;
@@test_component_int.int_sys_var
8
SET GLOBAL test_component_int.int_sys_var=12345678;
Warnings:
Warning	1292	Truncated incorrect test_component_int.int_sys_var value: '12345678'
SELECT @@test_component_int.int_sys_var;
@@test_component_int.int_sys_var
1024
SET GLOBAL test_component_int.int_sys_var=default;
SELECT @@test_component_int.int_sys_var;
@@test_component_int.int_sys_var
8
# file://component_test_sys_var_service_str:
SELECT @@test_component_str.str_sys_var;
@@test_component_str.str_sys_var
NULL
SET GLOBAL test_component_str.str_sys_var="dictionary.txt";
SELECT @@test_component_str.str_sys_var;
@@test_component_str.str_sys_var
dictionary.txt
UNINSTALL COMPONENT "file://component_test_sys_var_service";
UNINSTALL COMPONENT "file://component_test_sys_var_service_int";
UNINSTALL COMPONENT "file://component_test_sys_var_service_str";
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
test_component_sys_var_int init:
test_component_sys_var_int end of init:
test_component_sys_var_int deinit:
variable value : 8
variable value : 1024
variable value : 1024
get_variable failed.
test_component_sys_var_int end of deinit:
test_component_sys_var_str init:
character_set_server=[utf8mb4]
get_variable mysql_server.character_set_server failed. The variable requires buffer 8 bytes long.
character_set_server=[utf8mb4]
get_variable mysql_server.datadir success.
test_component_sys_var_str end of init:
test_component_sys_var_str deinit:
variable value : dictionary.txt
test_component_sys_var_str end of deinit:
