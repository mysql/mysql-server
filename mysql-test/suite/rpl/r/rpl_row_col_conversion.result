include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
# Bug#22110284: RBR CONVERSION FROM SMALLER MASTER TYPE TO BIGGER SLAVE
# TYPE IS BROKEN
# Verify that 100kb string is not truncated when replicated to LONGTEXT
#
[connection slave]
SET @saved_global_slave_type_conversions = @@global.SLAVE_TYPE_CONVERSIONS;
SET @@global.SLAVE_TYPE_CONVERSIONS='ALL_NON_LOSSY';
[connection master]
CREATE TABLE t1 (col MEDIUMTEXT);
include/sync_slave_sql_with_master.inc
[connection slave]
# Drop table on slave and recreate with LONGTEXT
DROP TABLE t1;
CREATE TABLE t1 (col LONGTEXT);
[connection master]
INSERT INTO t1(col) VALUES (REPEAT('a',65534)), (REPEAT('b',65535)), (REPEAT('c',65536)), (REPEAT('d',100000));
include/sync_slave_sql_with_master.inc
[connection slave]
include/assert.inc [Verify that the column has the same lengths on the slave as on the master]
SET @@global.SLAVE_TYPE_CONVERSIONS = @saved_global_slave_type_conversions;
[connection master]
DROP TABLE t1;
include/rpl_end.inc
