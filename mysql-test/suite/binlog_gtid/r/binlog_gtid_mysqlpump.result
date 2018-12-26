==== Initialize ====
CREATE TABLE t1 (a INT);
FLUSH LOGS;
PURGE BINARY LOGS TO '<CURRENT_BINLOG>';
INSERT INTO t1 VALUES (1);
FLUSH LOGS;
INSERT INTO t1 VALUES (2);
==== pump and restore ====
# pump
DROP TABLE t1;
RESET MASTER;
# restore
==== Verify after restoring from pump ====
include/assert.inc [GTID_EXECUTED should equal master GTID_EXECUTED after pump restored]
include/assert.inc [GTID_PURGED should equal master GTID_EXECUTED after pump restored]
# restart
include/assert.inc [GTID_EXECUTED should equal master GTID_EXECUTED after server restarted]
include/assert.inc [GTID_PURGED should equal master GTID_EXECUTED after server restarted]
==== Clean up ====
DROP TABLE t1;
