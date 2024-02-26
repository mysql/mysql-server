#
# WL#9451 -- Backup Log
#
CREATE TABLE t1 (a INT);
CREATE USER 'testuser1'@'localhost';
GRANT all ON *.* TO 'testuser1'@'localhost';
connect  con1, localhost, testuser1,,;
SET lock_wait_timeout= 1;
SET autocommit= 0;
connection default;
LOCK INSTANCE FOR BACKUP;
connection con1;
CREATE TABLE testtable_2 (c1 int, c2 varchar(10)) ENGINE=MyISAM;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
connection default;
UNLOCK INSTANCE;
connection con1;
CREATE TABLE testtable_2 (c1 int, c2 varchar(10)) ENGINE=MyISAM;
SHOW CREATE TABLE testtable_2;
Table	Create Table
testtable_2	CREATE TABLE `testtable_2` (
  `c1` int DEFAULT NULL,
  `c2` varchar(10) DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
connection default;
LOCK INSTANCE FOR BACKUP;
LOCK INSTANCE FOR BACKUP;
connection con1;
INSERT INTO testtable_2 VALUES(4,'ddd'),(5,'eeeee'),(3,'fffffff');
ALTER TABLE testtable_2 ADD INDEX i2(c2);
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
connection default;
UNLOCK INSTANCE;
UNLOCK INSTANCE;
connection con1;
DROP TABLE testtable_2;
connection default;
disconnect con1;
LOCK INSTANCE FOR BACKUP;
# Restart server
# restart
CREATE TABLE t2 (c1 int);
DROP USER 'testuser1'@'localhost';
DROP TABLE t1, t2;
