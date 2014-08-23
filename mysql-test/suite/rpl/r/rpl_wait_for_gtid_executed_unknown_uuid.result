include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CREATE TABLE t1 (i int);
SELECT WAIT_FOR_EXECUTED_GTID_SET('UNKNOWN_UUID:42', 9999);
The connection master should be waiting for the WAIT_FOR_EXECUTED_GTID_SET here
SET GTID_NEXT = 'UNKNOWN_UUID:42';
INSERT INTO t1 VALUES (1);
COMMIT;
WAIT_FOR_EXECUTED_GTID_SET('UNKNOWN_UUID:42', 9999)
0
DROP TABLE t1;
include/rpl_end.inc
