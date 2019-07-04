include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
SET SQL_LOG_BIN=0;
CREATE DATABASE B37656;
SET SQL_LOG_BIN=1;
CREATE DATABASE BUG37656;
### action: show that database on slave is created in lowercase
SHOW DATABASES LIKE '%37656';
Database (%37656)
bug37656
USE B37656;
CREATE TABLE T1 (a int);
INSERT INTO T1 VALUES (1);
### assertion: master contains capitalized case table
SHOW TABLES;
Tables_in_B37656
T1
include/sync_slave_sql_with_master.inc
use bug37656;
### assertion: slave contains lowered case table
SHOW TABLES;
Tables_in_bug37656
t1
### assertion: master and slave tables do not differ
include/diff_tables.inc [master:B37656.T1, slave:bug37656.t1]
SET SQL_LOG_BIN=0;
DROP DATABASE B37656;
SET SQL_LOG_BIN=1;
SHOW DATABASES LIKE '%37656';
Database (%37656)
CALL mtr.add_suppression("The master\'s UUID has changed.*");
DROP DATABASE BUG37656;
SHOW DATABASES LIKE '%37656';
Database (%37656)
include/rpl_end.inc
