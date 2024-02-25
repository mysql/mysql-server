SELECT @@GLOBAL.binlog_row_metadata;
@@GLOBAL.binlog_row_metadata
MINIMAL
MINIMAL Expected
SELECT @@SESSION.binlog_row_metadata;
ERROR HY000: Variable 'binlog_row_metadata' is a GLOBAL variable
'#---------------------BS_STVARS_002_01----------------------#'
SET @start_value= @@global.binlog_row_metadata;
SELECT COUNT(@@GLOBAL.binlog_row_metadata);
COUNT(@@GLOBAL.binlog_row_metadata)
1
1 Expected
'#---------------------BS_STVARS_002_02----------------------#'
SET @@GLOBAL.binlog_row_metadata=0;
SELECT @@GLOBAL.binlog_row_metadata;
@@GLOBAL.binlog_row_metadata
MINIMAL
MINIMAL Expected
SET @@GLOBAL.binlog_row_metadata=1;
SELECT @@GLOBAL.binlog_row_metadata;
@@GLOBAL.binlog_row_metadata
FULL
FULL Expected
SET @@GLOBAL.binlog_row_metadata=MINIMAL;
SELECT @@GLOBAL.binlog_row_metadata;
@@GLOBAL.binlog_row_metadata
MINIMAL
MINIMAL Expected
SET @@GLOBAL.binlog_row_metadata=FULL;
SELECT @@GLOBAL.binlog_row_metadata;
@@GLOBAL.binlog_row_metadata
FULL
FULL Expected
SET @@GLOBAL.binlog_row_metadata='MINIMAL';
SELECT @@GLOBAL.binlog_row_metadata;
@@GLOBAL.binlog_row_metadata
MINIMAL
MINIMAL Expected
SET @@GLOBAL.binlog_row_metadata='FULL';
SELECT @@GLOBAL.binlog_row_metadata;
@@GLOBAL.binlog_row_metadata
FULL
FULL Expected
'#---------------------BS_STVARS_002_03----------------------#'
SELECT @@GLOBAL.binlog_row_metadata = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='binlog_row_metadata';
@@GLOBAL.binlog_row_metadata = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.binlog_row_metadata);
COUNT(@@GLOBAL.binlog_row_metadata)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='binlog_row_metadata';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_002_04----------------------#'
SELECT @@GLOBAL.binlog_row_metadata = VARIABLE_VALUE
FROM performance_schema.session_variables
WHERE VARIABLE_NAME LIKE 'binlog_row_metadata';
@@GLOBAL.binlog_row_metadata = VARIABLE_VALUE
1
'#---------------------BS_STVARS_002_05----------------------#'
SELECT COUNT(@@binlog_row_metadata);
COUNT(@@binlog_row_metadata)
1
1 Expected
SELECT COUNT(@@GLOBAL.binlog_row_metadata);
COUNT(@@GLOBAL.binlog_row_metadata)
1
1 Expected
'#---------------------BS_STVARS_002_06----------------------#'
SET GLOBAL binlog_row_metadata = full1;
ERROR 42000: Variable 'binlog_row_metadata' can't be set to the value of 'full1'
SET GLOBAL binlog_row_metadata = "full1";
ERROR 42000: Variable 'binlog_row_metadata' can't be set to the value of 'full1'
SET GLOBAL binlog_row_metadata = 2;
ERROR 42000: Variable 'binlog_row_metadata' can't be set to the value of '2'
SET GLOBAL binlog_row_metadata = -1;
ERROR 42000: Variable 'binlog_row_metadata' can't be set to the value of '-1'
SET @@global.binlog_row_metadata= @start_value;
