include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
# Create tables on master
CREATE TABLE t1 (z VARCHAR(36));
ALTER TABLE t1 ADD COLUMN a char(36) DEFAULT (PI());
ALTER TABLE t1 ADD COLUMN b char(36) DEFAULT (RAND());
ERROR HY000: Statement is unsafe because it uses a system function that may return a different value on the slave.
DROP TABLE t1;
include/rpl_end.inc
