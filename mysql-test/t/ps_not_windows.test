# Non-windows specific ps tests.
--source include/not_windows.inc
# requires dynamic loading
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip ps_not_windows requires dynamic loading
}

#
# Bug #20665: All commands supported in Stored Procedures should work in
# Prepared Statements
#

create procedure proc_1() install plugin my_plug soname '/root/some_plugin.so';
--error ER_UDF_NO_PATHS
call proc_1();
--error ER_UDF_NO_PATHS
call proc_1();
--error ER_UDF_NO_PATHS
call proc_1();
drop procedure proc_1;

prepare abc from "install plugin my_plug soname '/root/some_plugin.so'";
--error ER_UDF_NO_PATHS
execute abc;
--error ER_UDF_NO_PATHS
execute abc;
deallocate prepare abc;
