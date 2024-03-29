#
# Checking min, max and incorrect values of innodb_fsync_threshold status variable
#
SET @orig_fsync_threshold = @@global.innodb_fsync_threshold;
SELECT @orig_fsync_threshold;
@orig_fsync_threshold
0
SET GLOBAL innodb_fsync_threshold = 1024*1024*1024;
SELECT @@global.innodb_fsync_threshold;
@@global.innodb_fsync_threshold
1073741824
# min value
SET GLOBAL innodb_fsync_threshold = 0;
SELECT @@global.innodb_fsync_threshold;
@@global.innodb_fsync_threshold
0
# invalid value - too small
SET GLOBAL innodb_fsync_threshold = -1;
Warnings:
Warning	1292	Truncated incorrect innodb_fsync_threshold value: '-1'
SELECT @@global.innodb_fsync_threshold;
@@global.innodb_fsync_threshold
0
# invalid value - not bound to page size
SET GLOBAL innodb_fsync_threshold = 12345;
Warnings:
Warning	1292	Truncated incorrect innodb_fsync_threshold value: '12345'
SELECT @@global.innodb_fsync_threshold;
@@global.innodb_fsync_threshold
0
SET GLOBAL innodb_fsync_threshold = @orig_fsync_threshold;
