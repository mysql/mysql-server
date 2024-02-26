--echo # Replicating simple inserts
--echo [Connection Master]
--connection master

# Check if lock mode is changed on master
SELECT @@global.innodb_autoinc_lock_mode;

CREATE TABLE t1 (pkey INT NOT NULL AUTO_INCREMENT PRIMARY KEY, aa INT);
INSERT INTO t1 VALUES (NULL,10),
                      (NULL,20),
                      (NULL,30);

--source include/sync_slave_sql_with_master.inc
--echo [Connection Slave]

# Check if lock mode is changed on slave
SELECT @@global.innodb_autoinc_lock_mode;

SELECT * FROM t1;

--let $diff_tables = master:t1, slave:t1
--source include/diff_tables.inc

--echo [Connection Master]
--connection master

# Check with insert on duplicate key update

INSERT INTO t1 VALUES (3,35)
ON DUPLICATE KEY UPDATE aa = aa+1;

--source include/sync_slave_sql_with_master.inc
--echo [Connection Slave]

SELECT * FROM t1;

--let $diff_tables = master:t1, slave:t1
--source include/diff_tables.inc


--echo [Connection Master]
--connection master

# Change the auto_increment_offset and auto_increment_increment values
SET @default_auto_increment_offset = @@auto_increment_offset;
SET @default_auto_increment_increment = @@auto_increment_increment;
SET auto_increment_offset = 5;
SET auto_increment_increment = 15;

INSERT INTO t1 VALUES (NULL,40),
                      (NULL,50),
                      (NULL,60),
                      (NULL,70),
                      (NULL,80);

--source include/sync_slave_sql_with_master.inc
--echo [Connection Slave]

SELECT * FROM t1;

--let $diff_tables = master:t1, slave:t1
--source include/diff_tables.inc

--echo [Conenction Master]
--connection master

# Delete records

DELETE FROM t1 WHERE aa IN (40,60);

--source include/sync_slave_sql_with_master.inc
--echo [Connection Slave]

SELECT * FROM t1;

--let $diff_tables = master:t1, slave:t1
--source include/diff_tables.inc

--echo [Connection Master]
--connection master

SET @@auto_increment_offset = @default_auto_increment_offset;
SET @@auto_increment_increment= @default_auto_increment_increment;

DROP TABLE t1;

--source include/sync_slave_sql_with_master.inc
--echo [Connection Slave]

--error ER_NO_SUCH_TABLE
SELECT * FROM t1;

--echo # Replicating bulk inserts
--echo [Connection Master]
--connection master

CREATE TABLE t1(aa INT);
INSERT INTO t1 VALUES(5),(10),(15),(20),(25),(30),(35),(40);
CREATE TABLE t2(pkey INT NOT NULL AUTO_INCREMENT PRIMARY KEY, aa INT);
--disable_warnings
INSERT INTO t2(aa) SELECT * FROM t1;
INSERT INTO t2(aa) SELECT * FROM t1;
--enable_warnings

--source include/sync_slave_sql_with_master.inc
--echo [Connection Slave]

SELECT COUNT(*) FROM t2;

--let $diff_tables = master:t2, slave:t2
--source include/diff_tables.inc

--echo [Connection Master]
--connection master

# Bulk inserts with auto increment offset

SET auto_increment_offset = 4;
SET auto_increment_increment = 16;

--disable_warnings
INSERT INTO t2(aa) SELECT * FROM t1;
INSERT INTO t2(aa) SELECT * FROM t1;
--enable_warnings

--source include/sync_slave_sql_with_master.inc
--echo [Connection Slave]

SELECT COUNT(*) FROM t2;

--let $diff_tables = master:t2, slave:t2
--source include/diff_tables.inc

--echo [Connection Master]
--connection master

SET @@auto_increment_offset = @default_auto_increment_offset;
SET @@auto_increment_increment = @default_auto_increment_increment;

DROP TABLE t1;
DROP TABLE t2;

--source include/sync_slave_sql_with_master.inc
--echo [Connection Slave]

--error ER_NO_SUCH_TABLE
SELECT * FROM t2;


--echo # Replicating mixed mode inserts
--echo [Connection Master]
--connection master

CREATE TABLE t1(pkey INT NOT NULL AUTO_INCREMENT PRIMARY KEY, bb INT);
INSERT INTO t1 VALUES(NULL,20),
                       (20,40),
                     (NULL,60),
                     (NULL,80),
                      (30,100),
                    (NULL,120);

--source include/sync_slave_sql_with_master.inc
--echo [Connection Slave]

SELECT COUNT(*) FROM t1;

--let $diff_tables = master:t1, slave:t1
--source include/diff_tables.inc

--echo [Connection Master]
--connection master

DROP TABLE t1;

--source include/sync_slave_sql_with_master.inc
--echo [Connection Slave]

--error ER_NO_SUCH_TABLE
SELECT * FROM t1;
