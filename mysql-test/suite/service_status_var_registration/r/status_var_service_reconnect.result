call mtr.add_suppression("duplicate variable name");
INSTALL COMPONENT "file://component_test_status_var_service";
SHOW STATUS WHERE VARIABLE_NAME LIKE '%component%';
Variable_name	Value
Com_install_component	1
Com_uninstall_component	0
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
# Kill and restart
SHOW STATUS WHERE VARIABLE_NAME LIKE '%component%';
Variable_name	Value
Com_install_component	0
Com_uninstall_component	0
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
