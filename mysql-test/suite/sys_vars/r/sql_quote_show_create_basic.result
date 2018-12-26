SET @session_start_value = @@session.sql_quote_show_create;
SELECT @session_start_value;
@session_start_value
1
'#--------------------FN_DYNVARS_162_01------------------------#'
SET @@session.sql_quote_show_create = 0;
SET @@session.sql_quote_show_create = DEFAULT;
SELECT @@session.sql_quote_show_create;
@@session.sql_quote_show_create
1
SET @@session.sql_quote_show_create = 1;
SET @@session.sql_quote_show_create = DEFAULT;
SELECT @@session.sql_quote_show_create;
@@session.sql_quote_show_create
1
'#---------------------FN_DYNVARS_162_02-------------------------#'
SET sql_quote_show_create = 1;
SELECT @@sql_quote_show_create;
@@sql_quote_show_create
1
SELECT session.sql_quote_show_create;
ERROR 42S02: Unknown table 'session' in field list
SELECT local.sql_quote_show_create;
ERROR 42S02: Unknown table 'local' in field list
SET session sql_quote_show_create = 0;
SELECT @@session.sql_quote_show_create;
@@session.sql_quote_show_create
0
'#--------------------FN_DYNVARS_162_03------------------------#'
SET @@session.sql_quote_show_create = 0;
SELECT @@session.sql_quote_show_create;
@@session.sql_quote_show_create
0
SET @@session.sql_quote_show_create = 1;
SELECT @@session.sql_quote_show_create;
@@session.sql_quote_show_create
1
'#--------------------FN_DYNVARS_162_04-------------------------#'
SET @@session.sql_quote_show_create = -1;
ERROR 42000: Variable 'sql_quote_show_create' can't be set to the value of '-1'
SET @@session.sql_quote_show_create = 2;
ERROR 42000: Variable 'sql_quote_show_create' can't be set to the value of '2'
SET @@session.sql_quote_show_create = "T";
ERROR 42000: Variable 'sql_quote_show_create' can't be set to the value of 'T'
SET @@session.sql_quote_show_create = "Y";
ERROR 42000: Variable 'sql_quote_show_create' can't be set to the value of 'Y'
SET @@session.sql_quote_show_create = TRÜE;
ERROR 42000: Variable 'sql_quote_show_create' can't be set to the value of 'TRÜE'
SET @@session.sql_quote_show_create = ÕN;
ERROR 42000: Variable 'sql_quote_show_create' can't be set to the value of 'ÕN'
SET @@session.sql_quote_show_create = OF;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'OF' at line 1
SET @@session.sql_quote_show_create = ÓFF;
ERROR 42000: Variable 'sql_quote_show_create' can't be set to the value of 'ÓFF'
SET @@session.sql_quote_show_create = '¹';
ERROR 42000: Variable 'sql_quote_show_create' can't be set to the value of '¹'
SET @@session.sql_quote_show_create = NO;
ERROR 42000: Variable 'sql_quote_show_create' can't be set to the value of 'NO'
'#-------------------FN_DYNVARS_162_05----------------------------#'
SET @@global.sql_quote_show_create = 1-@@global.sql_quote_show_create;
SELECT @@global.sql_quote_show_create;
@@global.sql_quote_show_create
0
SET @@global.sql_quote_show_create = 1-@@global.sql_quote_show_create;
SELECT @@global.sql_quote_show_create;
@@global.sql_quote_show_create
1
'#----------------------FN_DYNVARS_162_06------------------------#'
SELECT count(VARIABLE_VALUE) 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='sql_quote_show_create';
count(VARIABLE_VALUE)
1
'#----------------------FN_DYNVARS_162_07------------------------#'
SELECT IF(@@session.sql_quote_show_create, "ON", "OFF") = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='sql_quote_show_create';
IF(@@session.sql_quote_show_create, "ON", "OFF") = VARIABLE_VALUE
1
SELECT @@session.sql_quote_show_create;
@@session.sql_quote_show_create
1
SELECT VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='sql_quote_show_create';
VARIABLE_VALUE
ON
'#---------------------FN_DYNVARS_162_08-------------------------#'
SET @@session.sql_quote_show_create = OFF;
SELECT @@session.sql_quote_show_create;
@@session.sql_quote_show_create
0
SET @@session.sql_quote_show_create = ON;
SELECT @@session.sql_quote_show_create;
@@session.sql_quote_show_create
1
'#---------------------FN_DYNVARS_162_09----------------------#'
SET @@session.sql_quote_show_create = TRUE;
SELECT @@session.sql_quote_show_create;
@@session.sql_quote_show_create
1
SET @@session.sql_quote_show_create = FALSE;
SELECT @@session.sql_quote_show_create;
@@session.sql_quote_show_create
0
SET @@session.sql_quote_show_create = @session_start_value;
SELECT @@session.sql_quote_show_create;
@@session.sql_quote_show_create
1
