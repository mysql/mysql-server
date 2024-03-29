call mtr.add_suppression("duplicate variable name");
# Bug #28341329: COMPONENT OPTIONS WITH --LOOSE PREFIX AREN'T CONSIDERED
AFTER INSTALLATION
INSTALL COMPONENT "file://component_test_sys_var_service";
# test: must correspond to the cmd line value ("abcdefgsri")
SELECT @@test_component.str_sys_var;
@@test_component.str_sys_var
abcdefgsri
# Cleanup
UNINSTALL COMPONENT "file://component_test_sys_var_service";
