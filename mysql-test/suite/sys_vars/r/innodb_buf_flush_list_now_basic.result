#
# Basic test for innodb_buf_flush_list_now.
#
SELECT @@global.innodb_buf_flush_list_now;
@@global.innodb_buf_flush_list_now
0
set global innodb_buf_flush_list_now = 1;
SELECT @@global.innodb_buf_flush_list_now;
@@global.innodb_buf_flush_list_now
0
set global innodb_buf_flush_list_now = 0;
set global innodb_buf_flush_list_now = dummy;
ERROR 42000: Variable 'innodb_buf_flush_list_now' can't be set to the value of 'dummy'
set innodb_buf_flush_list_now = ON;
ERROR HY000: Variable 'innodb_buf_flush_list_now' is a GLOBAL variable and should be set with SET GLOBAL
# Setting to ON is ok.
set global innodb_buf_flush_list_now = ON;
# Setting to OFF is ok.
set global innodb_buf_flush_list_now = OFF;
# Must always be 0.
SELECT @@global.innodb_buf_flush_list_now;
@@global.innodb_buf_flush_list_now
0
