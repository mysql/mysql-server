--source include/force_myisam_default.inc
--source include/have_myisam.inc

--echo #
--echo # WL#9451 -- Backup Log
--echo #

CREATE TABLE t1 (a INT);

--enable_connect_log

CREATE USER 'testuser1'@'localhost';
GRANT all ON *.* TO 'testuser1'@'localhost';

--connect (con1, localhost, testuser1,,)
SET lock_wait_timeout= 1;
SET autocommit= 0;

--connection default
LOCK INSTANCE FOR BACKUP;

--connection con1
--error ER_LOCK_WAIT_TIMEOUT
CREATE TABLE testtable_2 (c1 int, c2 varchar(10)) ENGINE=MyISAM;

--connection default
UNLOCK INSTANCE;

--connection con1
CREATE TABLE testtable_2 (c1 int, c2 varchar(10)) ENGINE=MyISAM;
SHOW CREATE TABLE testtable_2;

--connection default
LOCK INSTANCE FOR BACKUP;
LOCK INSTANCE FOR BACKUP;

--connection con1
INSERT INTO testtable_2 VALUES(4,'ddd'),(5,'eeeee'),(3,'fffffff');

--error ER_LOCK_WAIT_TIMEOUT
ALTER TABLE testtable_2 ADD INDEX i2(c2);

--connection default
UNLOCK INSTANCE;
UNLOCK INSTANCE;

--connection con1
DROP TABLE testtable_2;

--connection default
--disconnect con1

LOCK INSTANCE FOR BACKUP;

--echo # Restart server
--source include/restart_mysqld.inc

CREATE TABLE t2 (c1 int);

DROP USER 'testuser1'@'localhost';
DROP TABLE t1, t2;
