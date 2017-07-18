SELECT @@GLOBAL.binlog_format;
@@GLOBAL.binlog_format
#
'#---------------------BS_STVARS_002_01----------------------#'
SET @start_value= @@global.binlog_format;
SELECT COUNT(@@GLOBAL.binlog_format);
COUNT(@@GLOBAL.binlog_format)
1
1 Expected
SELECT COUNT(@@SESSION.binlog_format);
COUNT(@@SESSION.binlog_format)
1
1 Expected
'#---------------------BS_STVARS_002_02----------------------#'
SET @@GLOBAL.binlog_format=1;
SELECT @@GLOBAL.binlog_format;
@@GLOBAL.binlog_format
STATEMENT
SET @@SESSION.binlog_format=1;
SELECT @@SESSION.binlog_format;
@@SESSION.binlog_format
STATEMENT
'#---------------------BS_STVARS_002_03----------------------#'
SELECT @@GLOBAL.binlog_format = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='binlog_format';
@@GLOBAL.binlog_format = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.binlog_format);
COUNT(@@GLOBAL.binlog_format)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='binlog_format';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_002_04----------------------#'
SELECT @@SESSION.binlog_format = VARIABLE_VALUE
FROM performance_schema.session_variables
WHERE VARIABLE_NAME='binlog_format';
@@SESSION.binlog_format = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@SESSION.binlog_format);
COUNT(@@SESSION.binlog_format)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.session_variables
WHERE VARIABLE_NAME='binlog_format';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_002_05----------------------#'
SELECT COUNT(@@binlog_format);
COUNT(@@binlog_format)
1
1 Expected
SELECT COUNT(@@local.binlog_format);
COUNT(@@local.binlog_format)
1
1 Expected
SELECT COUNT(@@SESSION.binlog_format);
COUNT(@@SESSION.binlog_format)
1
1 Expected
SELECT COUNT(@@GLOBAL.binlog_format);
COUNT(@@GLOBAL.binlog_format)
1
1 Expected
#########################################################################
# Bug #26269280: SET SESSION BINLOG_FORMAT = ROW GIVES SYNTAX ERROR     #
#########################################################################
SET binlog_format = ROW;
SET SESSION binlog_format = ROW;
SET @@session.binlog_format= ROW;
#########################################################################
SET @@global.binlog_format= @start_value;
