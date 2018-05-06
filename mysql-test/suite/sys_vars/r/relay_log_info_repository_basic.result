CALL mtr.add_suppression("The repository is not configured properly. Please, fix this before using replication.");
'#---------------------BS_STVARS_002_01----------------------#'
SELECT @@GLOBAL.relay_log_info_repository;
@@GLOBAL.relay_log_info_repository
TABLE
SELECT COUNT(@@GLOBAL.relay_log_info_repository);
COUNT(@@GLOBAL.relay_log_info_repository)
1
1 Expected
SELECT COUNT(@@SESSION.relay_log_info_repository);
ERROR HY000: Variable 'relay_log_info_repository' is a GLOBAL variable
'#---------------------BS_STVARS_002_02----------------------#'
SET @@GLOBAL.relay_log_info_repository= "FILE";
Warnings:
Warning	1287	'FILE' is deprecated and will be removed in a future release. Please use 'TABLE' instead
SELECT @@GLOBAL.relay_log_info_repository;
@@GLOBAL.relay_log_info_repository
FILE
'#---------------------BS_STVARS_002_03----------------------#'
SELECT @@GLOBAL.relay_log_info_repository = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='relay_log_info_repository';
@@GLOBAL.relay_log_info_repository = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.relay_log_info_repository);
COUNT(@@GLOBAL.relay_log_info_repository)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='relay_log_info_repository';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_002_05----------------------#'
SELECT COUNT(@@relay_log_info_repository);
COUNT(@@relay_log_info_repository)
1
1 Expected
SELECT COUNT(@@local.relay_log_info_repository);
ERROR HY000: Variable 'relay_log_info_repository' is a GLOBAL variable
SELECT COUNT(@@GLOBAL.relay_log_info_repository);
COUNT(@@GLOBAL.relay_log_info_repository)
1
1 Expected
'#---------------------BS_STVARS_002_06----------------------#'
SET @@GLOBAL.relay_log_info_repository= "TABLE";
