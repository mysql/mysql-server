SET @old_log_error_verbosity= @@global.log_error_verbosity;
SET GLOBAL log_error_verbosity=DEFAULT;
SELECT @@global.log_error_verbosity;
@@global.log_error_verbosity
2
SET GLOBAL log_error_verbosity=0;
Warnings:
Warning	1292	Truncated incorrect log_error_verbosity value: '0'
SELECT @@global.log_error_verbosity;
@@global.log_error_verbosity
1
SET GLOBAL log_error_verbosity=4;
Warnings:
Warning	1292	Truncated incorrect log_error_verbosity value: '4'
SELECT @@global.log_error_verbosity;
@@global.log_error_verbosity
3
SET SESSION log_error_verbosity=DEFAULT;
ERROR HY000: Variable 'log_error_verbosity' is a GLOBAL variable and should be set with SET GLOBAL
SET GLOBAL log_error_verbosity = @old_log_error_verbosity;
