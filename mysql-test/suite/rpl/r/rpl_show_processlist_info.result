include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]

==== Test ====
CREATE USER IF NOT EXISTS 'user1'@'%' IDENTIFIED WITH 'mysql_native_password';
include/sync_slave_sql_with_master.inc

==== Clean up ====
[connection master]
DROP USER IF EXISTS 'user1'@'%';
include/rpl_end.inc
