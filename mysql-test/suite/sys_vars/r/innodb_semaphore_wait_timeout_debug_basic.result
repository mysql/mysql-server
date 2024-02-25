#
# Basic test for innodb_semaphore_wait_timeout_debug
#
SELECT @@global.innodb_semaphore_wait_timeout_debug;
@@global.innodb_semaphore_wait_timeout_debug
600
SET GLOBAL innodb_semaphore_wait_timeout_debug = 10;
Warnings:
Warning	1292	Truncated incorrect innodb_semaphore_wait_timeout_debug value: '10'
SELECT @@global.innodb_semaphore_wait_timeout_debug;
@@global.innodb_semaphore_wait_timeout_debug
25
SET GLOBAL innodb_semaphore_wait_timeout_debug = 200;
SELECT @@global.innodb_semaphore_wait_timeout_debug;
@@global.innodb_semaphore_wait_timeout_debug
200
SET GLOBAL innodb_semaphore_wait_timeout_debug = dummy;
ERROR 42000: Incorrect argument type to variable 'innodb_semaphore_wait_timeout_debug'
SET innodb_semaphore_wait_timeout_debug = 100;
ERROR HY000: Variable 'innodb_semaphore_wait_timeout_debug' is a GLOBAL variable and should be set with SET GLOBAL
SET GLOBAL innodb_semaphore_wait_timeout_debug = 6000;
Warnings:
Warning	1292	Truncated incorrect innodb_semaphore_wait_timeout_debug value: '6000'
SELECT @@global.innodb_semaphore_wait_timeout_debug;
@@global.innodb_semaphore_wait_timeout_debug
600
SET GLOBAL innodb_semaphore_wait_timeout_debug = -1;
Warnings:
Warning	1292	Truncated incorrect innodb_semaphore_wait_timeout_debug value: '-1'
SELECT @@global.innodb_semaphore_wait_timeout_debug;
@@global.innodb_semaphore_wait_timeout_debug
25
SET GLOBAL innodb_semaphore_wait_timeout_debug = default;
SELECT @@global.innodb_semaphore_wait_timeout_debug;
@@global.innodb_semaphore_wait_timeout_debug
600
