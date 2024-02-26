RESET MASTER;
CREATE TABLE t1 (
c1 INT NOT NULL PRIMARY KEY
);
# Generate binlog.000002
FLUSH LOGS;
INSERT INTO t1 VALUES (1);
# Generate binlog.000003
FLUSH LOGS;
INSERT INTO t1 VALUES (2);
# Generate binlog.000004
FLUSH LOGS;
INSERT INTO t1 VALUES (3);
# Move binlog.000002 to binlog.000002.bkp and
# binlog.000003 to binlog.000003.bkp
#
# Only binlog.000001 and binlog.000004 remain, others are moved
# , restart the server with enabled binlog-gtid-simple-recovery
# and gtid_mode on. If the server scans more than one binary log in
# every iteration, it will cause read error on the 2nd and 3rd files.
#
# restart:--gtid-mode=on --enforce-gtid-consistency --log-replica-updates --binlog-gtid-simple-recovery=ON
#
# Verify that GLOBAL.GTID_EXECUTED and GLOBAL.GTID_PURGED are empty
# after server restarts.
#
include/assert.inc [GLOBAL.GTID_EXECUTED must be empty.]
include/assert.inc [GLOBAL.GTID_PURGED must be empty.]
#
# Write one gtid event into binlog.000004
#
DROP TABLE t1;
# Move binlog.000004 to binlog.000004.bkp
#
# Only binlog.000001 and binlog.000005 remain, others are moved
# , restart the server with enabled binlog-gtid-simple-recovery
# and gtid_mode on again. If the server scans more than one binary
# log in every iteration, it will cause read error on the 2nd and
# 4th files.
#
# restart:--gtid-mode=on --enforce-gtid-consistency --log-replica-updates --binlog-gtid-simple-recovery=ON
#
# Verify that GLOBAL.GTID_EXECUTED contains committed gtid MASTER_UUID:1
# and GLOBAL.GTID_PURGED is empty after server restarts again.
#
include/assert.inc [committed gtid MASTER_UUID:1]
include/assert.inc [GLOBAL.GTID_PURGED is empty]
#
# Move binary logs back.
#
#
# PURGE BINARY LOGS TO binlog.000006
#
PURGE BINARY LOGS TO 'binlog.000006';
include/assert.inc [purged gtids MASTER_UUID:1]
