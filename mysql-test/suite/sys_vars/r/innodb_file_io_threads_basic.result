'#---------------------BS_STVARS_027_01----------------------#'
SELECT COUNT(@@GLOBAL.innodb_read_io_threads);
COUNT(@@GLOBAL.innodb_read_io_threads)
1
1 Expected
SELECT COUNT(@@GLOBAL.innodb_write_io_threads);
COUNT(@@GLOBAL.innodb_write_io_threads)
1
1 Expected
'#---------------------BS_STVARS_027_02----------------------#'
SET @@GLOBAL.innodb_read_io_threads=1;
ERROR HY000: Variable 'innodb_read_io_threads' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.innodb_read_io_threads);
COUNT(@@GLOBAL.innodb_read_io_threads)
1
1 Expected
SET @@GLOBAL.innodb_write_io_threads=1;
ERROR HY000: Variable 'innodb_write_io_threads' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.innodb_write_io_threads);
COUNT(@@GLOBAL.innodb_write_io_threads)
1
1 Expected
'#---------------------BS_STVARS_027_03----------------------#'
SELECT @@GLOBAL.innodb_read_io_threads = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_read_io_threads';
@@GLOBAL.innodb_read_io_threads = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.innodb_read_io_threads);
COUNT(@@GLOBAL.innodb_read_io_threads)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='innodb_read_io_threads';
COUNT(VARIABLE_VALUE)
1
1 Expected
SELECT @@GLOBAL.innodb_write_io_threads = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_write_io_threads';
@@GLOBAL.innodb_write_io_threads = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.innodb_write_io_threads);
COUNT(@@GLOBAL.innodb_write_io_threads)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='innodb_write_io_threads';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_027_04----------------------#'
SELECT @@innodb_read_io_threads = @@GLOBAL.innodb_read_io_threads;
@@innodb_read_io_threads = @@GLOBAL.innodb_read_io_threads
1
1 Expected
SELECT @@innodb_write_io_threads = @@GLOBAL.innodb_write_io_threads;
@@innodb_write_io_threads = @@GLOBAL.innodb_write_io_threads
1
1 Expected
'#---------------------BS_STVARS_027_05----------------------#'
SELECT COUNT(@@innodb_read_io_threads);
COUNT(@@innodb_read_io_threads)
1
1 Expected
SELECT COUNT(@@local.innodb_read_io_threads);
ERROR HY000: Variable 'innodb_read_io_threads' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.innodb_read_io_threads);
ERROR HY000: Variable 'innodb_read_io_threads' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.innodb_read_io_threads);
COUNT(@@GLOBAL.innodb_read_io_threads)
1
1 Expected
SELECT innodb_read_io_threads;
ERROR 42S22: Unknown column 'innodb_read_io_threads' in 'field list'
Expected error 'Unknown column'
SELECT COUNT(@@innodb_write_io_threads);
COUNT(@@innodb_write_io_threads)
1
1 Expected
SELECT COUNT(@@local.innodb_write_io_threads);
ERROR HY000: Variable 'innodb_write_io_threads' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.innodb_write_io_threads);
ERROR HY000: Variable 'innodb_write_io_threads' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.innodb_write_io_threads);
COUNT(@@GLOBAL.innodb_write_io_threads)
1
1 Expected
SELECT innodb_write_io_threads;
ERROR 42S22: Unknown column 'innodb_write_io_threads' in 'field list'
Expected error 'Unknown column'
