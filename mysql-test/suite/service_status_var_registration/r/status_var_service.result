call mtr.add_suppression("duplicate variable name");
call mtr.add_suppression("variable name 'test_component.int_var' not found");
# Simple load test
INSTALL COMPONENT "file://component_test_status_var_service";
# Testing integer and bool type variable
SHOW SESSION STATUS WHERE VARIABLE_NAME LIKE 'test_component.int_var';
Variable_name	Value
test_component.int_var	7
SHOW SESSION STATUS WHERE VARIABLE_NAME LIKE 'test_component.bool_var';
Variable_name	Value
test_component.bool_var	ON
SHOW SESSION STATUS WHERE VARIABLE_NAME LIKE 'test_component.long_var';
Variable_name	Value
test_component.long_var	1234567
SHOW SESSION STATUS WHERE VARIABLE_NAME LIKE 'test_component.longlong_var';
Variable_name	Value
test_component.longlong_var	123456789
SHOW SESSION STATUS WHERE VARIABLE_NAME LIKE 'test_component.func_var';
Variable_name	Value
test_component.func_var	Testing SHOW_FUNC type for component status variables
SHOW SESSION STATUS WHERE VARIABLE_NAME LIKE 'test_component.array';
Variable_name	Value
SHOW SESSION STATUS WHERE VARIABLE_NAME LIKE 'test_component.char_var';
Variable_name	Value
test_component.char_var	Testing CHAR status variable
SHOW SESSION STATUS WHERE VARIABLE_NAME LIKE 'test_component.char_ptr_var';
Variable_name	Value
test_component.char_ptr_var	Testing CHAR_PTR for status vars
SHOW SESSION STATUS WHERE VARIABLE_NAME LIKE 'test_component.double';
Variable_name	Value
SELECT * FROM performance_schema.session_status WHERE VARIABLE_NAME LIKE 'test_component%';
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
SELECT * FROM performance_schema.global_status WHERE VARIABLE_NAME LIKE 'test_component%';
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
UNINSTALL COMPONENT "file://component_test_status_var_service";
########## test_component_sys_var_service.log:
test_component_status_var init:
test_component_status_var end of init:
test_component_status_var deinit:
test_component_status_var_service end of deinit:
