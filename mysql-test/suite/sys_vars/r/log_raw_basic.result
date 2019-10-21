# Bug 16636373 SERVER DOES NOT EXPOSE --LOG-RAW IN SHOW VARIABLES
# Expected value ON as opt file contains --log-raw option
SHOW VARIABLES LIKE 'log_raw';
Variable_name	Value
log_raw	ON
SET GLOBAL log_raw=0;
SHOW VARIABLES LIKE 'log_raw';
Variable_name	Value
log_raw	OFF
SET GLOBAL log_raw=1;
SHOW VARIABLES LIKE 'log_raw';
Variable_name	Value
log_raw	ON
SET GLOBAL log_raw=DEFAULT;
SHOW VARIABLES LIKE 'log_raw';
Variable_name	Value
log_raw	OFF
SET GLOBAL log_raw=ON;
