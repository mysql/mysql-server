SELECT @@GLOBAL.binlog_row_image;
@@GLOBAL.binlog_row_image
FULL
SELECT @@SESSION.binlog_row_image;
@@SESSION.binlog_row_image
FULL
FULL Expected
'#---------------------BS_STVARS_002_01----------------------#'
SET @start_value= @@global.binlog_row_image;
SELECT COUNT(@@GLOBAL.binlog_row_image);
COUNT(@@GLOBAL.binlog_row_image)
1
1 Expected
SELECT COUNT(@@SESSION.binlog_row_image);
COUNT(@@SESSION.binlog_row_image)
1
1 Expected
'#---------------------BS_STVARS_002_02----------------------#'
SET @@GLOBAL.binlog_row_image=0;
SELECT @@GLOBAL.binlog_row_image;
@@GLOBAL.binlog_row_image
MINIMAL
MINIMAL Expected
SET @@SESSION.binlog_row_image=0;
SELECT @@SESSION.binlog_row_image;
@@SESSION.binlog_row_image
MINIMAL
MINIMAL Expected
SET @@GLOBAL.binlog_row_image=1;
SELECT @@GLOBAL.binlog_row_image;
@@GLOBAL.binlog_row_image
NOBLOB
NOBLOB Expected
SET @@SESSION.binlog_row_image=1;
SELECT @@SESSION.binlog_row_image;
@@SESSION.binlog_row_image
NOBLOB
NOBLOB Expected
SET @@GLOBAL.binlog_row_image=2;
SELECT @@GLOBAL.binlog_row_image;
@@GLOBAL.binlog_row_image
FULL
FULL Expected
SET @@SESSION.binlog_row_image=2;
SELECT @@SESSION.binlog_row_image;
@@SESSION.binlog_row_image
FULL
FULL Expected
'#---------------------BS_STVARS_002_03----------------------#'
SELECT @@GLOBAL.binlog_row_image = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='binlog_row_image';
@@GLOBAL.binlog_row_image = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.binlog_row_image);
COUNT(@@GLOBAL.binlog_row_image)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='binlog_row_image';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_002_04----------------------#'
SELECT @@SESSION.binlog_row_image = VARIABLE_VALUE
FROM performance_schema.session_variables
WHERE VARIABLE_NAME='binlog_row_image';
@@SESSION.binlog_row_image = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@SESSION.binlog_row_image);
COUNT(@@SESSION.binlog_row_image)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.session_variables
WHERE VARIABLE_NAME='binlog_row_image';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_002_05----------------------#'
SELECT COUNT(@@binlog_row_image);
COUNT(@@binlog_row_image)
1
1 Expected
SELECT COUNT(@@local.binlog_row_image);
COUNT(@@local.binlog_row_image)
1
1 Expected
SELECT COUNT(@@SESSION.binlog_row_image);
COUNT(@@SESSION.binlog_row_image)
1
1 Expected
SELECT COUNT(@@GLOBAL.binlog_row_image);
COUNT(@@GLOBAL.binlog_row_image)
1
1 Expected
SET @@global.binlog_row_image= @start_value;
