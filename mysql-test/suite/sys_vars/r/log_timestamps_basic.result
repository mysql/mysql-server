SET @old_log_timestamps     = @@global.log_timestamps;
SET GLOBAL  log_timestamps=UTC;
SELECT      @@global.log_timestamps;
@@global.log_timestamps
UTC
SET GLOBAL  log_timestamps=SYSTEM;
SELECT      @@global.log_timestamps;
@@global.log_timestamps
SYSTEM
SET SESSION log_timestamps=UTC;
ERROR HY000: Variable 'log_timestamps' is a GLOBAL variable and should be set with SET GLOBAL
SET GLOBAL  log_timestamps=ON;
ERROR 42000: Variable 'log_timestamps' can't be set to the value of 'ON'
SET GLOBAL  log_timestamps=2;
ERROR 42000: Variable 'log_timestamps' can't be set to the value of '2'
SET GLOBAL  log_timestamps=DEFAULT;
SELECT      @@global.log_timestamps;
@@global.log_timestamps
UTC
SET GLOBAL log_timestamps      = @old_log_timestamps;
