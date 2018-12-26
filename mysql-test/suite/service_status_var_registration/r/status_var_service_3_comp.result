INSTALL COMPONENT "file://component_test_status_var_service_int";
SHOW STATUS WHERE VARIABLE_NAME LIKE 'int_var%';
Variable_name	Value
int_var_2	2048
SHOW STATUS WHERE VARIABLE_NAME LIKE 'test_int_component.int_variable';
Variable_name	Value
test_int_component.int_variable	4096
SHOW STATUS WHERE VARIABLE_NAME LIKE 'test_int_component.int_variable_value';
Variable_name	Value
INSTALL COMPONENT "file://component_test_status_var_service_str";
SHOW SESSION STATUS WHERE VARIABLE_NAME LIKE 'test_component.int_variable_value';
Variable_name	Value
SHOW STATUS WHERE VARIABLE_NAME LIKE 'test_str_component.str_variable';
Variable_name	Value
INSTALL COMPONENT "file://component_test_status_var_service";
SHOW STATUS WHERE VARIABLE_NAME LIKE 'test_component.int_variable';
Variable_name	Value
SELECT * FROM performance_schema.global_status WHERE VARIABLE_NAME LIKE '%component%';
VARIABLE_NAME	VARIABLE_VALUE
test_component.array_var1	MySQL
test_component.array_var2	8.000000
test_component.bool_var	ON
test_component.char_ptr_var	Testing CHAR_PTR for status vars
test_component.char_var	Testing CHAR status variable
test_component.double_var	8.000000
test_component.func_var	Testing SHOW_FUNC type for component status variables
test_component.int_var	7
test_component.long_var	1234567
test_component.longlong_var	123456789
test_int_component.int_variable	4096
test_int_component.uint_variable	0
test_str_component.char_variable	This is the initial value if the char_variable.
UNINSTALL COMPONENT "file://component_test_status_var_service";
SELECT * FROM performance_schema.global_status WHERE VARIABLE_NAME LIKE '%component%';
VARIABLE_NAME	VARIABLE_VALUE
test_int_component.int_variable	4096
test_int_component.uint_variable	0
test_str_component.char_variable	This is the initial value if the char_variable.
UNINSTALL COMPONENT "file://component_test_status_var_service_int";
SELECT * FROM performance_schema.global_status WHERE VARIABLE_NAME LIKE '%component%';
VARIABLE_NAME	VARIABLE_VALUE
test_str_component.char_variable	This is the initial value if the char_variable.
UNINSTALL COMPONENT "file://component_test_status_var_service_str";
SELECT * FROM performance_schema.global_status WHERE VARIABLE_NAME LIKE '%component%';
VARIABLE_NAME	VARIABLE_VALUE
########## test_component_status_var_service.log:
test_component_status_var init:
test_component_status_var end of init:
test_component_status_var deinit:
test_component_status_var_service end of deinit:
########## test_component_status_var_service_int.log:
test_component_status_var_int init:
test_component_status_var_int end of init:
test_component_status_var_int deinit:
test_component_status_var_int end of deinit:
########## test_component_status_var_service_str.log:
test_component_status_var_str init:
test_component_status_var_str end of init:
test_component_status_var_str deinit:
test_component_status_var_str end of deinit:
