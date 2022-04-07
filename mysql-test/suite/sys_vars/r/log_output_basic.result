SET @start_value = @@global.log_output;
SELECT @start_value;
@start_value
FILE
'#--------------------FN_DYNVARS_065_01------------------------#'
SET @@global.log_output = FILE;
SET @@global.log_output = DEFAULT;
SELECT @@global.log_output;
@@global.log_output
FILE
'#---------------------FN_DYNVARS_065_02-------------------------#'
SET @@global.log_output = NULL;
ERROR 42000: Variable 'log_output' can't be set to the value of 'NULL'
SET @@global.log_output = "";
ERROR 42000: Variable 'log_output' can't be set to the value of ''
'#--------------------FN_DYNVARS_065_03------------------------#'
SET @@global.log_output = 'TABLE';
SELECT @@global.log_output;
@@global.log_output
TABLE
SET @@global.log_output = 'FILE';
SELECT @@global.log_output;
@@global.log_output
FILE
SET @@global.log_output = 'NONE';
SELECT @@global.log_output;
@@global.log_output
NONE
SET @@global.log_output = 'FILE,TABLE';
SELECT @@global.log_output;
@@global.log_output
FILE,TABLE
SET @@global.log_output = 'TABLE,FILE';
SELECT @@global.log_output;
@@global.log_output
FILE,TABLE
SET @@global.log_output = 'NONE,TABLE';
SELECT @@global.log_output;
@@global.log_output
NONE,TABLE
SET @@global.log_output = 'NONE,FILE';
SELECT @@global.log_output;
@@global.log_output
NONE,FILE
SET @@global.log_output = 'FILE,TABLE,NONE';
SELECT @@global.log_output;
@@global.log_output
NONE,FILE,TABLE
SET @@global.log_output = 'TABLE,NONE';
SELECT @@global.log_output;
@@global.log_output
NONE,TABLE
SET @@global.log_output = 'TABLE,TABLE';
SELECT @@global.log_output;
@@global.log_output
TABLE
SET @@global.log_output = ',TABLE';
SELECT @@global.log_output;
@@global.log_output
TABLE
SET @@global.log_output = 'TABLE,,FILE,,,';
SELECT @@global.log_output;
@@global.log_output
FILE,TABLE
'#--------------------FN_DYNVARS_065_04-------------------------#'
SET @@global.log_output = -1;
ERROR 42000: Variable 'log_output' can't be set to the value of '-1'
SET @@global.log_output = TAB;
ERROR 42000: Variable 'log_output' can't be set to the value of 'TAB'
SET @@global.log_output = NIL;
ERROR 42000: Variable 'log_output' can't be set to the value of 'NIL'
SET @@global.log_output = 'FIL';
ERROR 42000: Variable 'log_output' can't be set to the value of 'FIL'
SET @@global.log_output = 'T';
ERROR 42000: Variable 'log_output' can't be set to the value of 'T'
SET @@global.log_output = 'TABLE, FILE';
ERROR 42000: Variable 'log_output' can't be set to the value of ' FILE'
SET @@global.log_output = ' TABLE,FILE';
ERROR 42000: Variable 'log_output' can't be set to the value of ' TABLE'
'#-------------------FN_DYNVARS_065_05----------------------------#'
SET @@log_output = 0;
ERROR HY000: Variable 'log_output' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@session.log_output;
ERROR HY000: Variable 'log_output' is a GLOBAL variable
'#----------------------FN_DYNVARS_065_06------------------------#'
SELECT @@log_output = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='log_output';
@@log_output = VARIABLE_VALUE
1
'#---------------------FN_DYNVARS_065_07-------------------------#'
SET @@global.log_output = 0;
ERROR 42000: Variable 'log_output' can't be set to the value of '0'
SELECT @@global.log_output;
@@global.log_output
FILE,TABLE
SET @@global.log_output = 1;
SELECT @@global.log_output;
@@global.log_output
NONE
SET @@global.log_output = 2;
SELECT @@global.log_output;
@@global.log_output
FILE
SET @@global.log_output = 3;
SELECT @@global.log_output;
@@global.log_output
NONE,FILE
SET @@global.log_output = 4;
SELECT @@global.log_output;
@@global.log_output
TABLE
SET @@global.log_output = 5;
SELECT @@global.log_output;
@@global.log_output
NONE,TABLE
SET @@global.log_output = 6;
SELECT @@global.log_output;
@@global.log_output
FILE,TABLE
SET @@global.log_output = 7;
SELECT @@global.log_output;
@@global.log_output
NONE,FILE,TABLE
SET @@global.log_output = 8;
ERROR 42000: Variable 'log_output' can't be set to the value of '8'
'#---------------------FN_DYNVARS_065_08----------------------#'
SET @@global.log_output = TRUE;
SELECT @@global.log_output;
@@global.log_output
NONE
SET @@global.log_output = FALSE;
ERROR 42000: Variable 'log_output' can't be set to the value of '0'
SELECT @@global.log_output;
@@global.log_output
NONE
'#---------------------FN_DYNVARS_065_09----------------------#'
SET log_output = ON;
ERROR HY000: Variable 'log_output' is a GLOBAL variable and should be set with SET GLOBAL
SET global log_output = 'TABLE';
SELECT @@global.log_output;
@@global.log_output
TABLE
SET @@global.log_output = @start_value;
SELECT @@global.log_output;
@@global.log_output
FILE
