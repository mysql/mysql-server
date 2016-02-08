SET @start_global_value = @@global.innodb_file_per_table;
SELECT @start_global_value;
@start_global_value
1
'#---------------------BS_STVARS_028_01----------------------#'
SELECT COUNT(@@GLOBAL.innodb_file_per_table);
COUNT(@@GLOBAL.innodb_file_per_table)
1
1 Expected
'#---------------------BS_STVARS_028_02----------------------#'
SET @@global.innodb_file_per_table = 0;
SELECT @@global.innodb_file_per_table;
@@global.innodb_file_per_table
0
SET @@global.innodb_file_per_table ='On' ;
SELECT @@global.innodb_file_per_table;
@@global.innodb_file_per_table
1
SET @@global.innodb_file_per_table ='Off' ;
SELECT @@global.innodb_file_per_table;
@@global.innodb_file_per_table
0
SET @@global.innodb_file_per_table = 1;
SELECT @@global.innodb_file_per_table;
@@global.innodb_file_per_table
1
'#---------------------BS_STVARS_028_03----------------------#'
SELECT IF(@@GLOBAL.innodb_file_per_table,'ON','OFF') = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_file_per_table';
IF(@@GLOBAL.innodb_file_per_table,'ON','OFF') = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.innodb_file_per_table);
COUNT(@@GLOBAL.innodb_file_per_table)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='innodb_file_per_table';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_028_04----------------------#'
SELECT @@innodb_file_per_table = @@GLOBAL.innodb_file_per_table;
@@innodb_file_per_table = @@GLOBAL.innodb_file_per_table
1
1 Expected
'#---------------------BS_STVARS_028_05----------------------#'
SELECT COUNT(@@innodb_file_per_table);
COUNT(@@innodb_file_per_table)
1
1 Expected
SELECT COUNT(@@local.innodb_file_per_table);
ERROR HY000: Variable 'innodb_file_per_table' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.innodb_file_per_table);
ERROR HY000: Variable 'innodb_file_per_table' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.innodb_file_per_table);
COUNT(@@GLOBAL.innodb_file_per_table)
1
1 Expected
SELECT innodb_file_per_table = @@SESSION.innodb_file_per_table;
ERROR 42S22: Unknown column 'innodb_file_per_table' in 'field list'
SET @@global.innodb_file_per_table = @start_global_value;
SELECT @@global.innodb_file_per_table;
@@global.innodb_file_per_table
1
