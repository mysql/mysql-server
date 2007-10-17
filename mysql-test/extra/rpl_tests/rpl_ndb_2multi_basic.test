#######################################
# Author: Rafal Somla                 #
# Date: 2006-08-20                    #
# Purpose: Test replication of basic  #
# table operations in various setups  #   
#                                     #
# Based on rpl_ndb_2multi_eng.test by #
# JBM                                 #
#######################################

--echo --- Doing pre test cleanup --- 

connection master;
--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_query_log

#################################################
--echo --- Create Table Section ---

CREATE TABLE t1 (id MEDIUMINT NOT NULL, 
                 b1 INT, 
                 vc VARCHAR(255), 
                 bc CHAR(255), 
                 d DECIMAL(10,4) DEFAULT 0, 
                 f FLOAT DEFAULT 0, 
                 total BIGINT UNSIGNED, 
                 y YEAR, 
                 t DATE, 
                 PRIMARY KEY(id));

--echo --- Show table on master ---

SHOW CREATE TABLE t1;

--echo --- Show table on slave ---

sync_slave_with_master;
SHOW CREATE TABLE t1;

--source include/rpl_multi_engine2.inc

#################################################
# Okay lets see how it holds up to table changes
--echo --- Check that simple Alter statements are replicated correctly --

ALTER TABLE t1 DROP PRIMARY KEY;
# note: table with no PK can't contain blobs if it is to be replicated.
ALTER TABLE t1 MODIFY vc char(32);  

--echo --- Show the new improved table on the master ---

SHOW CREATE TABLE t1;

--echo --- Make sure that our tables on slave are still same engine ---
--echo --- and that the alter statements replicated correctly ---

sync_slave_with_master;
SHOW CREATE TABLE t1;

--source include/rpl_multi_engine2.inc

#################################################
--echo --- Check that replication works when slave has more columns than master
connection master;
ALTER TABLE t1 ADD PRIMARY KEY(id,total);
ALTER TABLE t1 MODIFY vc TEXT;

INSERT INTO t1 VALUES(3,1,'Testing MySQL databases is a cool ',
                      'Must make it bug free for the customer',
                       654321.4321,15.21,0,1965,"1905-11-14");
INSERT INTO t1 VALUES(20,1,'Testing MySQL databases is a cool ',
                      'Must make it bug free for the customer',
                       654321.4321,15.21,0,1965,"1965-11-14");
INSERT INTO t1 VALUES(50,1,'Testing MySQL databases is a cool ',
                      'Must make it bug free for the customer',
                       654321.4321,15.21,0,1965,"1985-11-14");

--echo --- Add columns on slave ---
--sync_slave_with_master
ALTER TABLE t1 ADD (u int, v char(16) default 'default');
UPDATE t1 SET u=7 WHERE id < 50;
UPDATE t1 SET v='explicit' WHERE id >10;

--echo --- Show changed table on slave ---

SHOW CREATE TABLE t1;
SELECT * 
FROM t1
ORDER BY id;

--source include/rpl_multi_engine2.inc
TRUNCATE TABLE t1;

#################################################
--echo --- Check that replication works when master has more columns than slave
connection master;

--echo --- Remove columns on slave ---
--sync_slave_with_master
ALTER TABLE t1 DROP COLUMN v;
ALTER TABLE t1 DROP COLUMN u;
ALTER TABLE t1 DROP COLUMN t;
ALTER TABLE t1 DROP COLUMN y;

--echo --- Show changed table on slave ---

SHOW CREATE TABLE t1;

--source include/rpl_multi_engine2.inc
TRUNCATE TABLE t1;

#################################################
--echo --- Do Cleanup --
connection master;
DROP TABLE IF EXISTS t1;

sync_slave_with_master;
connection master;
