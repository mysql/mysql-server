SET @orig = @@global.innodb_log_checksums;
SELECT @orig;
@orig
1
SET GLOBAL innodb_log_checksums = 'crc32';
ERROR 42000: Variable 'innodb_log_checksums' can't be set to the value of 'crc32'
SELECT @@global.innodb_log_checksums;
@@global.innodb_log_checksums
1
SET GLOBAL innodb_log_checksums = 2;
ERROR 42000: Variable 'innodb_log_checksums' can't be set to the value of '2'
SELECT @@global.innodb_log_checksums;
@@global.innodb_log_checksums
1
SET GLOBAL innodb_log_checksums = 1e2;
ERROR 42000: Incorrect argument type to variable 'innodb_log_checksums'
SELECT @@global.innodb_log_checksums;
@@global.innodb_log_checksums
1
SET GLOBAL innodb_log_checksums = 1.0;
ERROR 42000: Incorrect argument type to variable 'innodb_log_checksums'
SELECT @@global.innodb_log_checksums;
@@global.innodb_log_checksums
1
SET innodb_log_checksums = OFF;
ERROR HY000: Variable 'innodb_log_checksums' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@global.innodb_log_checksums;
@@global.innodb_log_checksums
1
SET GLOBAL innodb_log_checksums = OFF;
SELECT @@global.innodb_log_checksums;
@@global.innodb_log_checksums
0
SET GLOBAL innodb_log_checksums = default;
SET GLOBAL innodb_log_checksums = ON;
SELECT @@global.innodb_log_checksums;
@@global.innodb_log_checksums
1
SET GLOBAL innodb_log_checksums = @orig;
SELECT @@global.innodb_log_checksums;
@@global.innodb_log_checksums
1
