# Restart the server in force recovery mode (2)
# restart: --innodb-force-recovery=2
SELECT @@innodb_force_recovery;
@@innodb_force_recovery
2
# Restart server in normal mode
# restart
SELECT @@innodb_force_recovery;
@@innodb_force_recovery
0
CREATE TABLE t1(c1 INT PRIMARY KEY) ENGINE=InnoDB STATS_PERSISTENT=0;
BEGIN;
INSERT INTO t1 VALUES(1), (2), (3), (4);
CREATE TABLE t2(c2 INT PRIMARY KEY) ENGINE=InnoDB STATS_PERSISTENT=0;
DROP TABLE t2;
# Restart the server in force recovery mode (3)
# Kill and restart: --innodb-force-recovery=3
SELECT @@innodb_force_recovery;
@@innodb_force_recovery
3
SELECT COUNT(*) IN (0,4) yes FROM t1;
yes
1
# Restart the server in force recovery mode (5)
# restart: --innodb-force-recovery=5
SELECT @@innodb_force_recovery;
@@innodb_force_recovery
5
SELECT COUNT(*) IN (0,4) yes FROM t1;
yes
1
# Restart server in normal mode
# restart
SELECT @@innodb_force_recovery;
@@innodb_force_recovery
0
SELECT COUNT(*) IN (0,4) yes FROM t1;
yes
1
DROP TABLE t1;
