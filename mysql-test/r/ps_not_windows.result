create procedure proc_1() install plugin my_plug soname '/root/some_plugin.so';
call proc_1();
ERROR HY000: No paths allowed for shared library
call proc_1();
ERROR HY000: No paths allowed for shared library
call proc_1();
ERROR HY000: No paths allowed for shared library
drop procedure proc_1;
prepare abc from "install plugin my_plug soname '/root/some_plugin.so'";
execute abc;
ERROR HY000: No paths allowed for shared library
execute abc;
ERROR HY000: No paths allowed for shared library
deallocate prepare abc;
