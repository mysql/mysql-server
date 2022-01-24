#
# Basic test for the innodb_use_fdatasync variable
#
SELECT @@global.innodb_use_fdatasync;
@@global.innodb_use_fdatasync
0
set global innodb_use_fdatasync = 1;
SELECT @@global.innodb_use_fdatasync;
@@global.innodb_use_fdatasync
1
set global innodb_use_fdatasync = 0;
