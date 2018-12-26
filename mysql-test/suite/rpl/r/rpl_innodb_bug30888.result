include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
use test;
CREATE TABLE test.regular_tbl(id MEDIUMINT NOT NULL AUTO_INCREMENT,
dt TIMESTAMP, user CHAR(255), uuidf LONGBLOB,
fkid MEDIUMINT, filler VARCHAR(255),
PRIMARY KEY(id)) ENGINE='innodb';
CREATE PROCEDURE test.proc_norm()
BEGIN
DECLARE ins_count INT DEFAULT 1000; 
DECLARE del_count INT;
DECLARE cur_user VARCHAR(255);
DECLARE local_uuid VARCHAR(255);
DECLARE local_time TIMESTAMP;
SET local_time= NOW();
SET cur_user= CURRENT_USER();
SET local_uuid= UUID();
WHILE ins_count > 0 DO
INSERT INTO test.regular_tbl VALUES (NULL, NOW(), USER() , UUID(),
ins_count,'Going to test MBR for MySQL');
SET ins_count = ins_count - 1;
END WHILE;
SELECT MAX(id) FROM test.regular_tbl INTO del_count;
WHILE del_count > 0 DO
DELETE FROM test.regular_tbl WHERE id = del_count;
SET del_count = del_count - 2;
END WHILE;
END|
CALL test.proc_norm();
include/sync_slave_sql_with_master.inc
DROP PROCEDURE test.proc_norm;
DROP TABLE test.regular_tbl;
include/rpl_end.inc
