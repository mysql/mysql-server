include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
==== Initialize ====
SET GLOBAL SLAVE_TRANSACTION_RETRIES= 0;
SET GLOBAL SLAVE_PARALLEL_WORKERS= 1;
include/start_slave.inc
==== Test ====
include/assert.inc [The worker status should be set in P_S.threads]
==== Look for more mistakes in mysqld.cc ====
==== Clean up ====
include/stop_slave.inc
SET GLOBAL SLAVE_PARALLEL_WORKERS= OLD_VALUE;
SET GLOBAL SLAVE_TRANSACTION_RETRIES= OLD_VALUE;
include/rpl_end.inc
