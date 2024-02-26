INSTALL COMPONENT "file://component_test_status_var_service_reg_only";
SHOW STATUS WHERE VARIABLE_NAME LIKE 'int_var%';
Variable_name	Value
int_var_2	2048
SHOW STATUS WHERE VARIABLE_NAME LIKE 'test_int_component.int_variable';
Variable_name	Value
test_int_component.int_variable	4096
SHOW STATUS WHERE VARIABLE_NAME LIKE 'test_str_component.str_variable';
Variable_name	Value
test_str_component.str_variable	This is the initial text value.
INSTALL COMPONENT "file://component_test_status_var_service_unreg_only";
SELECT * FROM performance_schema.global_status WHERE VARIABLE_NAME LIKE '%component%';
VARIABLE_NAME	VARIABLE_VALUE
UNINSTALL COMPONENT "file://component_test_status_var_service_reg_only";
UNINSTALL COMPONENT "file://component_test_status_var_service_unreg_only";
########## test_component_status_var_service_reg_only.log:
test_component_status_var_reg init:
test_component_status_var_reg end of init:
########## test_component_status_var_service_unreg_only.log:
test_component_status_var_unreg init:
test_component_status_var_unreg end of init:
