#############################################################
# Author: Rafal
# Date: 2007-08-20
# based on rpl_multi_engine3.inc
#############################################################

connection slave;
STOP SLAVE;
RESET SLAVE;

connection master;
RESET MASTER;

connection slave;
START SLAVE;

--echo --- Populate t1 with data ---
connection master;
--disable_query_log
INSERT INTO t1 VALUES(42,1,'Testing MySQL databases is a cool ',
                      'Must make it bug free for the customer',
                       654321.4321,15.21,0,1965,"1905-11-14");
INSERT INTO t1 VALUES(2,1,'Testing MySQL databases is a cool ',
                      'Must make it bug free for the customer',
                       654321.4321,15.21,0,1965,"1965-11-14");
INSERT INTO t1 VALUES(4,1,'Testing MySQL databases is a cool ',
                      'Must make it bug free for the customer',
                       654321.4321,15.21,0,1965,"1985-11-14");
INSERT INTO t1 VALUES(142,1,'Testing MySQL databases is a cool ',
                      'Must make it bug free for the customer',
                       654321.4321,15.21,0,1965,"1995-11-14");
INSERT INTO t1 VALUES(412,1,'Testing MySQL databases is a cool ',
                      'Must make it bug free for the customer',
                       654321.4321,15.21,0,1965,"2005-11-14");
--enable_query_log

--echo --- Select from t1 on master --- 
select *
from t1 
order by id;

sync_slave_with_master;
--echo --- Select from t1 on slave ---
select *
from t1 
order by id;

--echo --- Perform basic operation on master ---
--echo --- and ensure replicated correctly ---
connection master;

--echo --- Update t1 on master --
UPDATE t1 SET b1 = 0, bc='updated', t="2006-02-22" 
WHERE id < 100 
ORDER BY id;

--echo --- Check the update on master --- 
SELECT *
FROM t1 
WHERE id < 100
ORDER BY id;

# Must give injector thread a little time to get update
# into the binlog other wise we will miss the update.

sync_slave_with_master;
--echo --- Check Update on slave ---
SELECT *
FROM t1 
WHERE id < 100
ORDER BY id;

connection master;
--echo --- Remove a record from t1 on master ---
# Note: there is an error in replication of Delete_row
# from NDB to MyISAM (BUG#28538). However, if there is
# only one row in Delete_row event then it works fine,
# as this test demonstrates.
DELETE FROM t1 WHERE id = 412;

--echo --- Show current count on master for t1 ---
SELECT COUNT(*) FROM t1;

sync_slave_with_master;
--echo --- Show current count on slave for t1 --- 
SELECT COUNT(*) FROM t1;

connection master;
TRUNCATE TABLE t1;
sync_slave_with_master;
connection master;
