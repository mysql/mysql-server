include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
Verify that example engine is not installed.
SELECT * FROM INFORMATION_SCHEMA.ENGINES WHERE ENGINE='EXAMPLE';
ENGINE	SUPPORT	COMMENT	TRANSACTIONS	XA	SAVEPOINTS
Get binlog position before install plugin.
Install example engine.
INSTALL PLUGIN example SONAME 'ha_example.so';
Get binlog position after install plugin.
Compute the difference of the  binlog positions.
Should be zero as install plugin should not be replicated.
Delta
0
Verify that example engine is installed.
SELECT * FROM INFORMATION_SCHEMA.ENGINES WHERE ENGINE='EXAMPLE';
ENGINE	SUPPORT	COMMENT	TRANSACTIONS	XA	SAVEPOINTS
EXAMPLE	YES	Example storage engine	NO	NO	NO
include/sync_slave_sql_with_master.inc
connection slave: Verify that example engine is not installed.
SELECT * FROM INFORMATION_SCHEMA.ENGINES WHERE ENGINE='EXAMPLE';
ENGINE	SUPPORT	COMMENT	TRANSACTIONS	XA	SAVEPOINTS
connection master: Uninstall example engine.
Get binlog position before uninstall plugin.
UNINSTALL PLUGIN example;
Get binlog position after uninstall plugin.
Compute the difference of the  binlog positions.
Should be zero as uninstall plugin should not be replicated.
Delta
0
Verify that example engine is not installed.
SELECT * FROM INFORMATION_SCHEMA.ENGINES WHERE ENGINE='EXAMPLE';
ENGINE	SUPPORT	COMMENT	TRANSACTIONS	XA	SAVEPOINTS
include/sync_slave_sql_with_master.inc
include/rpl_end.inc
End of test
