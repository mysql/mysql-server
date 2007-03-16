##################################################################
# Author: Giuseppe, Chuck Bell                                   #
# Date: 17-January-2007                                          #
# Purpose: To test that event effects are replicated             #
# in both row based and statement based format                   #
##################################################################

--disable_warnings
DROP EVENT IF EXISTS justonce;
drop table if exists t1,t2;
--enable_warnings

# first, we need a table to record something from an event

eval CREATE TABLE `t1` (
  `id` INT(10) UNSIGNED NOT NULL AUTO_INCREMENT,
  `c` VARCHAR(50) NOT NULL,
  `ts` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE
CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`)
) ENGINE=$engine_type DEFAULT CHARSET=utf8;

INSERT INTO t1 (c) VALUES ('manually');

# then, we create the event
CREATE EVENT justonce ON SCHEDULE AT NOW() + INTERVAL 2 SECOND DO INSERT INTo t1
(c) VALUES ('from justonce');

# wait 3 seconds, so the event can trigger
--real_sleep 3

# check that table t1 contains something
--echo "in the master"
--enable_info
--replace_column 3 TIMESTAMP
SELECT * FROM t1;
--disable_info

sync_slave_with_master;
connection slave;

--echo "in the slave"
--enable_info
--replace_column 3 TIMESTAMP
SELECT * FROM t1;
--disable_info

# Create an event on the slave and check to see what the originator is.
--disable_warnings
DROP EVENT IF EXISTS test.slave_once;
--enable_warnings

CREATE EVENT test.slave_once ON SCHEDULE EVERY 5 MINUTE DO 
INSERT INTO t1(c) VALUES ('from slave_once');
SELECT db, name, status, originator FROM mysql.event;

--disable_warnings
DROP EVENT IF EXISTS test.slave_once;
--enable_warnings

connection master;

# BUG#20384 - disable events on slave
DROP EVENT IF EXISTS test.justonce;
CREATE EVENT test.er ON SCHEDULE EVERY 3 SECOND DO 
INSERT INTO t1(c) VALUES ('from er');
SELECT db, name, status, originator FROM mysql.event WHERE db = 'test' AND name = 'er';

sync_slave_with_master;
connection slave;
--echo "in the slave"
SELECT db, name, status, originator FROM mysql.event WHERE db = 'test' AND name = 'er';

connection master;
--echo "in the master"
ALTER EVENT test.er ON SCHEDULE EVERY 5 SECOND DO INSERT into t1(c) VALUES ('from alter er');
SELECT db, name, status, originator FROM mysql.event WHERE db = 'test' AND name = 'er';

sync_slave_with_master;
connection slave;
--echo "in the slave"
SELECT db, name, status, originator FROM mysql.event WHERE db = 'test' AND name = 'er';

connection master;
--echo "in the master"
DROP EVENT test.er;
--replace column 8 DATETIME
SELECT db, name, status, originator FROM mysql.event WHERE db = 'test' AND name = 'er';

--disable_info

sync_slave_with_master;
connection slave;
--echo "in the slave"
--replace column 8 DATETIME
SELECT db, name, status, originator FROM mysql.event WHERE db = 'test' AND name = 'er';

--echo "in the master"
connection master;
DROP TABLE t1;

