select @@global.lower_case_table_names=20;
@@global.lower_case_table_names=20
0
select @@session.lower_case_table_names;
ERROR HY000: Variable 'lower_case_table_names' is a GLOBAL variable
SELECT @@global.lower_case_table_names in (0,1,2);
@@global.lower_case_table_names in (0,1,2)
1
SELECT @@global.lower_case_table_names = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='lower_case_table_names';
@@global.lower_case_table_names = VARIABLE_VALUE
1
SELECT @@global.lower_case_table_names = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='lower_case_table_names';
@@global.lower_case_table_names = VARIABLE_VALUE
1
set global lower_case_table_names=1;
ERROR HY000: Variable 'lower_case_table_names' is a read only variable
set session lower_case_table_names=1;
ERROR HY000: Variable 'lower_case_table_names' is a read only variable
