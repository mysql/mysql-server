# WL#12393: Logging: Add new command line option for richer slow query logging

# test global variable "log_slow_extra"

SELECT @@global.log_slow_extra INTO @old_lse;
SELECT @@global.log_output     INTO @old_lo;
SELECT @@global.slow_query_log INTO @old_sql;
# invalid values / types
SET GLOBAL log_slow_extra=symbol;
ERROR 42000: Variable 'log_slow_extra' can't be set to the value of 'symbol'
SET GLOBAL log_slow_extra="string";
ERROR 42000: Variable 'log_slow_extra' can't be set to the value of 'string'
SET GLOBAL log_slow_extra=99;
ERROR 42000: Variable 'log_slow_extra' can't be set to the value of '99'
SET GLOBAL log_slow_extra=0.5;
ERROR 42000: Incorrect argument type to variable 'log_slow_extra'

# only GLOBAL scope is valid
SET SESSION log_slow_extra=0;
ERROR HY000: Variable 'log_slow_extra' is a GLOBAL variable and should be set with SET GLOBAL

# valid values
SET GLOBAL slow_query_log=0;
SET GLOBAL log_slow_extra=0;
SELECT @@global.log_slow_extra;
@@global.log_slow_extra
0
SET GLOBAL log_slow_extra=1;
SELECT @@global.log_slow_extra;
@@global.log_slow_extra
1
SET GLOBAL log_slow_extra=DEFAULT;
SELECT @@global.log_slow_extra;
@@global.log_slow_extra
0

# warnings and errors
SET GLOBAL slow_query_log=1;

# Switching slow query log file format while target is not FILE is legal,
# but does nothing. Throw a warning!
SET GLOBAL log_output="TABLE";
SELECT @@global.log_slow_extra INTO @old;
SET GLOBAL log_slow_extra=1;
Warnings:
Warning	3795	slow query log file format changed as requested, but setting will have no effect when not actually logging to a file.
# Value must have changed:
SELECT @@global.log_slow_extra!=@old;
@@global.log_slow_extra!=@old
1
SET GLOBAL log_slow_extra=DEFAULT;
Warnings:
Warning	3795	slow query log file format changed as requested, but setting will have no effect when not actually logging to a file.

# Switching slow query log file format while target is not FILE is legal,
# but does nothing. Throw a warning!
SET GLOBAL log_output="NONE";
SELECT @@global.log_slow_extra INTO @old;
SET GLOBAL log_slow_extra=1;
Warnings:
Warning	3795	slow query log file format changed as requested, but setting will have no effect when not actually logging to a file.
# Value must have changed:
SELECT @@global.log_slow_extra!=@old;
@@global.log_slow_extra!=@old
1
SET GLOBAL log_slow_extra=DEFAULT;
Warnings:
Warning	3795	slow query log file format changed as requested, but setting will have no effect when not actually logging to a file.

# clean up
SET GLOBAL log_slow_extra=@old_lse;
Warnings:
Warning	3795	slow query log file format changed as requested, but setting will have no effect when not actually logging to a file.
SET GLOBAL log_output=@old_lo;
SET GLOBAL slow_query_log=@old_sql;
TRUNCATE TABLE mysql.general_log;
TRUNCATE TABLE mysql.slow_log;

# READY
