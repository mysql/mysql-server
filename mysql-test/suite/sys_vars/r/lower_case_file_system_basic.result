select @@global.lower_case_file_system=2;
@@global.lower_case_file_system=2
0
select @@session.lower_case_file_system;
ERROR HY000: Variable 'lower_case_file_system' is a GLOBAL variable
SELECT @@global.lower_case_file_system in (0,1);
@@global.lower_case_file_system in (0,1)
1
SELECT IF(@@global.lower_case_file_system, "ON", "OFF") = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='lower_case_file_system';
IF(@@global.lower_case_file_system, "ON", "OFF") = VARIABLE_VALUE
1
SELECT IF(@@global.lower_case_file_system, "ON", "OFF") = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='lower_case_file_system';
IF(@@global.lower_case_file_system, "ON", "OFF") = VARIABLE_VALUE
1
set global lower_case_file_system=1;
ERROR HY000: Variable 'lower_case_file_system' is a read only variable
set session lower_case_file_system=1;
ERROR HY000: Variable 'lower_case_file_system' is a read only variable
