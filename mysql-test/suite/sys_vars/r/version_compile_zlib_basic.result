SELECT COUNT(@@GLOBAL.version_compile_zlib);
COUNT(@@GLOBAL.version_compile_zlib)
1
SET @@GLOBAL.version_compile_zlib=1;
ERROR HY000: Variable 'version_compile_zlib' is a read only variable
SELECT @@GLOBAL.version_compile_zlib = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='version_compile_zlib';
@@GLOBAL.version_compile_zlib = VARIABLE_VALUE
1
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='version_compile_zlib';
COUNT(VARIABLE_VALUE)
1
SELECT @@version_compile_zlib = @@GLOBAL.version_compile_zlib;
@@version_compile_zlib = @@GLOBAL.version_compile_zlib
1
SELECT COUNT(@@version_compile_zlib);
COUNT(@@version_compile_zlib)
1
SELECT COUNT(@@local.version_compile_zlib);
ERROR HY000: Variable 'version_compile_zlib' is a GLOBAL variable
SELECT COUNT(@@SESSION.version_compile_zlib);
ERROR HY000: Variable 'version_compile_zlib' is a GLOBAL variable
SELECT COUNT(@@GLOBAL.version_compile_zlib);
COUNT(@@GLOBAL.version_compile_zlib)
1
SELECT version_compile_zlib = @@SESSION.version_compile_zlib;
ERROR 42S22: Unknown column 'version_compile_zlib' in 'field list'
