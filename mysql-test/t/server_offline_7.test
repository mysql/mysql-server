# Created: Atanu Ghosh 2014-05-07
# WL#3836: Method to bring servers off line
 
--source include/not_threadpool.inc
--source include/force_myisam_default.inc
--source include/have_myisam.inc
	  
--echo #
--echo # WL#3836:  Method to bring servers off line
--echo #

--disable_query_log
CALL mtr.add_suppression("Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT");
--enable_query_log

# Save the global value to be used to restore the original value.
SET @global_saved_tmp =  @@global.offline_mode;

# Create a database to be used by all the connections.
# This is to ensure that all sessions connected to this DB is counted in this test case.

CREATE DATABASE wl3836;
USE wl3836;

--echo # Should report 1
SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST WHERE DB LIKE 'wl3836';

# Create 2 non-super users

CREATE USER 'user1'@'localhost';
CREATE USER 'user2'@'localhost';

GRANT ALL ON wl3836.* TO 'user1'@'localhost', 'user2'@'localhost';
--connect(conu1,localhost,user1,'',wl3836)

# Non-super user cannot set this value
--error 1227
SET GLOBAL offline_mode=ON;

CREATE TABLE t1 (id INT PRIMARY KEY AUTO_INCREMENT) ENGINE=MYISAM;
CREATE TABLE t2 (id INT UNSIGNED NOT NULL);

INSERT INTO t1 VALUES
(0),(0),(0),(0),(0),(0),(0),(0), (0),(0),(0),(0),(0),(0),(0),(0),
(0),(0),(0),(0),(0),(0),(0),(0), (0),(0),(0),(0),(0),(0),(0),(0),
(0),(0),(0),(0),(0),(0),(0),(0), (0),(0),(0),(0),(0),(0),(0),(0),
(0),(0),(0),(0),(0),(0),(0),(0), (0),(0),(0),(0),(0),(0),(0),(0);
--disable_warnings
INSERT t1 SELECT 0 FROM t1 AS a1, t1 AS a2 LIMIT 4032;
--enable_warnings

INSERT INTO t2 SELECT id FROM t1;

--source include/turn_off_only_full_group_by.inc

# Intentionally create a long running query
send SELECT id FROM t1 WHERE id IN
     (SELECT DISTINCT a.id FROM t2 a, t2 b, t2 c, t2 d
     GROUP BY ACOS(1/a.id), b.id, c.id, d.id
     HAVING a.id BETWEEN 10 AND 20);

--connect(conu2,localhost,user2,'',wl3836)

--connection default

--echo # Should report 3
SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST WHERE DB LIKE 'wl3836';

SET GLOBAL offline_mode = ON;

# Wait until all non super user have been disconnected (for slow machines)
let $wait_condition=SELECT COUNT(USER) = 1 FROM INFORMATION_SCHEMA.PROCESSLIST WHERE DB LIKE 'wl3836';
--source include/wait_condition.inc

--echo # Should report 1
SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST WHERE DB LIKE 'wl3836';

# No other non-super user should be allowed to connect
--error ER_SERVER_OFFLINE_MODE
--connection conu1

--echo # Root user should be allowed to connect
--connect(conu3,localhost,root,'',wl3836)

--connection default

--echo # Should report 2
SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST WHERE DB LIKE 'wl3836';

SET GLOBAL offline_mode=OFF;

DROP DATABASE wl3836;

DROP USER 'user1'@'localhost';
DROP USER 'user2'@'localhost';

--echo # Test for replication slave threads

--source include/rpl/init_source_replica.inc
connection slave;
connection master;
--echo # Should report 7 root processes
SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST;
SET GLOBAL offline_mode=ON;
# Connection count should remain same as Binlog dump thread should not be killed
--echo # Should report 7 root processes
SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST;
--connection slave
SET @slave_saved_tmp = @@global.offline_mode;
--echo # Should report 6 root processes
SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST;
SET GLOBAL offline_mode=ON;
# Connection count should remain same as slave threads should not be killed
--echo # Should report 6 root processes
SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST;

#Restore back original value
SET GLOBAL offline_mode = @slave_saved_tmp;
--source include/rpl/deinit.inc

--connection default
# Wait until all user have been disconnected (for slow machines)
#let $wait_condition=SELECT COUNT(USER) = 1 FROM INFORMATION_SCHEMA.PROCESSLIST;
#--source include/wait_condition.inc

--echo # Restoring the original values.
SET @@global.offline_mode = @global_saved_tmp;
