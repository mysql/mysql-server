include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
# Create table
# Create connections
# Insert rows
# Master results
SELECT COUNT(a) FROM t;
COUNT(a)
200
include/sync_slave_sql_with_master.inc
# Slave results
SELECT COUNT(a) FROM t;
COUNT(a)
200
# Clean up
DROP TABLE t;
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
include/rpl_end.inc
# End of tests
