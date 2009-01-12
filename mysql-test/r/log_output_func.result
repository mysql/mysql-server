SET @start_value= @@global.log_output;
SET @start_general_log= @@global.general_log;
SET @start_general_log_file= @@global.general_log_file;
'#--------------------FN_DYNVARS_065_01-------------------------#'
SET @@global.log_output = 'NONE';
'connect (con1,localhost,root,,,,)'
SET @@global.log_output = 'TABLE,FILE';
'connect (con2,localhost,root,,,,)'
SELECT @@global.log_output;
@@global.log_output
FILE,TABLE
'#--------------------FN_DYNVARS_065_02-------------------------#'
'connection con1'
'---Checking general_log when log_output is NONE---'
SET @@global.log_output = 'NONE';
TRUNCATE TABLE mysql.general_log;
DROP TABLE IF EXISTS t1;
CREATE TABLE t1(a INT);
INSERT INTO t1 VALUE(1);
SELECT 'abc';
abc
abc
SELECT count(*) FROM mysql.general_log;
count(*)
0
'---Checking general_log when log_output is TABLE---'
SET @@global.log_output = 'TABLE';
TRUNCATE TABLE mysql.general_log;
DROP TABLE IF EXISTS t1;
CREATE TABLE t1(a int);
INSERT INTO t1 VALUE(1);
SELECT 'abc';
abc
abc
SELECT count(*)>4 FROM mysql.general_log;
count(*)>4
1
SET @@global.general_log = 'OFF';
FLUSH LOGS;
SET @@global.general_log_file = 'MYSQLTEST_VARDIR/run/mytest.log';
SET @@global.general_log = 'ON';
SET @@global.log_output = 'FILE';
TRUNCATE TABLE mysql.general_log;
DROP TABLE IF EXISTS t1;
CREATE TABLE t1(a INT);
INSERT INTO t1 VALUE(1);
SELECT 'abc';
abc
abc
SELECT count(*) FROM mysql.general_log;
count(*)
0
DROP TABLE t1;
connection default;
SET @@global.general_log= 'OFF';
SET @@global.general_log_file= @start_general_log_file;
SET @@global.log_output= @start_value;
SET @@global.general_log= @start_general_log;
SET @@global.general_log= 'ON';
