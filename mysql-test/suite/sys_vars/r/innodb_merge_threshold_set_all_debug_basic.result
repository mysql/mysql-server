#
# Basic test for innodb_merge_threshold_set_all_debug
#
SELECT @@global.innodb_merge_threshold_set_all_debug;
@@global.innodb_merge_threshold_set_all_debug
50
set global innodb_merge_threshold_set_all_debug = 1;
SELECT @@global.innodb_merge_threshold_set_all_debug;
@@global.innodb_merge_threshold_set_all_debug
1
set global innodb_merge_threshold_set_all_debug = 51;
Warnings:
Warning	1292	Truncated incorrect innodb_merge_threshold_set_all_debug value: '51'
SELECT @@global.innodb_merge_threshold_set_all_debug;
@@global.innodb_merge_threshold_set_all_debug
50
set global innodb_merge_threshold_set_all_debug = 0;
Warnings:
Warning	1292	Truncated incorrect innodb_merge_threshold_set_all_debug value: '0'
SELECT @@global.innodb_merge_threshold_set_all_debug;
@@global.innodb_merge_threshold_set_all_debug
1
set innodb_merge_threshold_set_all_debug = 50;
ERROR HY000: Variable 'innodb_merge_threshold_set_all_debug' is a GLOBAL variable and should be set with SET GLOBAL
set global innodb_merge_threshold_set_all_debug = 50;
SELECT @@global.innodb_merge_threshold_set_all_debug;
@@global.innodb_merge_threshold_set_all_debug
50
