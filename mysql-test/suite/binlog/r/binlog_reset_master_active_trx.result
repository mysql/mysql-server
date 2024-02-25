#
# Step-1) Initial Setup which is required for the test.
#
RESET MASTER;
#
# Step-1.1) Create four client connections.
#
#
# Step-1.2) Create a sample table.
#
CREATE TABLE t1 (i INT);
#
#  Step-2) Execute a simple insert transaction that will wait
#          for a signal at various stages of transaction commit.
#          Execute 'RESET MASTER' when it is waiting for the signal
#          and check binlog contents and GTID_EXECUTED (if GTID
#          enabled) that they are as expected.
#
[connection conn1]
BEGIN;
INSERT INTO t1 VALUES (1);
SET debug_sync="ha_commit_trans_before_acquire_commit_lock SIGNAL reached WAIT_FOR insert_continue";
COMMIT;
[connection conn2]
SET debug_sync="now WAIT_FOR reached";
RESET MASTER;
SET debug_sync="now SIGNAL insert_continue";
[connection conn1]
#
# Step-2.4) Reset the debug signals for the next iteration
#
[connection conn2]
SET debug_sync="RESET";
[connection conn1]
BEGIN;
INSERT INTO t1 VALUES (1);
SET debug_sync="bgc_before_flush_stage SIGNAL reached WAIT_FOR insert_continue";
COMMIT;
[connection conn2]
SET debug_sync="now WAIT_FOR reached";
RESET MASTER;
[connection conn3]
SET debug_sync="now SIGNAL insert_continue";
[connection conn2]
[connection conn1]
#
# Step-2.4) Reset the debug signals for the next iteration
#
[connection conn2]
SET debug_sync="RESET";
[connection conn1]
BEGIN;
INSERT INTO t1 VALUES (1);
SET debug_sync="bgc_after_flush_stage_before_sync_stage SIGNAL reached WAIT_FOR insert_continue";
COMMIT;
[connection conn2]
SET debug_sync="now WAIT_FOR reached";
RESET MASTER;
[connection conn3]
SET debug_sync="now SIGNAL insert_continue";
[connection conn2]
[connection conn1]
#
# Step-2.4) Reset the debug signals for the next iteration
#
[connection conn2]
SET debug_sync="RESET";
[connection conn1]
BEGIN;
INSERT INTO t1 VALUES (1);
SET debug_sync="bgc_after_sync_stage_before_commit_stage SIGNAL reached WAIT_FOR insert_continue";
COMMIT;
[connection conn2]
SET debug_sync="now WAIT_FOR reached";
RESET MASTER;
[connection conn3]
SET debug_sync="now SIGNAL insert_continue";
[connection conn2]
[connection conn1]
#
# Step-2.4) Reset the debug signals for the next iteration
#
[connection conn2]
SET debug_sync="RESET";
[connection conn1]
BEGIN;
INSERT INTO t1 VALUES (1);
SET debug_sync="bgc_after_commit_stage_before_rotation SIGNAL reached WAIT_FOR insert_continue";
COMMIT;
[connection conn2]
SET debug_sync="now WAIT_FOR reached";
RESET MASTER;
[connection conn3]
SET debug_sync="now SIGNAL insert_continue";
[connection conn2]
[connection conn1]
#
# Step-2.4) Reset the debug signals for the next iteration
#
[connection conn2]
SET debug_sync="RESET";
[connection conn1]
BEGIN;
INSERT INTO t1 VALUES (1);
SET debug_sync="ha_commit_trans_before_acquire_commit_lock SIGNAL reached WAIT_FOR insert_continue";
COMMIT;
[connection conn2]
SET debug_sync="now WAIT_FOR reached";
FLUSH TABLES WITH READ LOCK;
SET debug_sync="now SIGNAL insert_continue";
RESET MASTER;
UNLOCK TABLES;
[connection conn1]
#
# Step-2.4) Reset the debug signals for the next iteration
#
[connection conn2]
SET debug_sync="RESET";
[connection conn1]
BEGIN;
INSERT INTO t1 VALUES (1);
SET debug_sync="bgc_before_flush_stage SIGNAL reached WAIT_FOR insert_continue";
COMMIT;
[connection conn2]
SET debug_sync="now WAIT_FOR reached";
FLUSH TABLES WITH READ LOCK;
[connection conn3]
SET debug_sync="now SIGNAL insert_continue";
[connection conn2]
RESET MASTER;
UNLOCK TABLES;
[connection conn1]
#
# Step-2.4) Reset the debug signals for the next iteration
#
[connection conn2]
SET debug_sync="RESET";
[connection conn1]
BEGIN;
INSERT INTO t1 VALUES (1);
SET debug_sync="bgc_after_flush_stage_before_sync_stage SIGNAL reached WAIT_FOR insert_continue";
COMMIT;
[connection conn2]
SET debug_sync="now WAIT_FOR reached";
FLUSH TABLES WITH READ LOCK;
[connection conn3]
SET debug_sync="now SIGNAL insert_continue";
[connection conn2]
RESET MASTER;
UNLOCK TABLES;
[connection conn1]
#
# Step-2.4) Reset the debug signals for the next iteration
#
[connection conn2]
SET debug_sync="RESET";
[connection conn1]
BEGIN;
INSERT INTO t1 VALUES (1);
SET debug_sync="bgc_after_sync_stage_before_commit_stage SIGNAL reached WAIT_FOR insert_continue";
COMMIT;
[connection conn2]
SET debug_sync="now WAIT_FOR reached";
FLUSH TABLES WITH READ LOCK;
[connection conn3]
SET debug_sync="now SIGNAL insert_continue";
[connection conn2]
RESET MASTER;
UNLOCK TABLES;
[connection conn1]
#
# Step-2.4) Reset the debug signals for the next iteration
#
[connection conn2]
SET debug_sync="RESET";
[connection conn1]
BEGIN;
INSERT INTO t1 VALUES (1);
SET debug_sync="bgc_after_commit_stage_before_rotation SIGNAL reached WAIT_FOR insert_continue";
COMMIT;
[connection conn2]
SET debug_sync="now WAIT_FOR reached";
FLUSH TABLES WITH READ LOCK;
[connection conn3]
SET debug_sync="now SIGNAL insert_continue";
[connection conn2]
RESET MASTER;
UNLOCK TABLES;
[connection conn1]
#
# Step-2.4) Reset the debug signals for the next iteration
#
[connection conn2]
SET debug_sync="RESET";
[connection conn1]
BEGIN;
INSERT INTO t1 VALUES (1);
SET debug_sync="ha_commit_trans_before_acquire_commit_lock SIGNAL reached WAIT_FOR insert_continue";
COMMIT;
[connection conn2]
SET debug_sync="now WAIT_FOR reached";
FLUSH TABLES WITH READ LOCK;
SET debug_sync="now SIGNAL insert_continue";
[connection conn4]
RESET MASTER;
[connection conn2]
UNLOCK TABLES;
[connection conn1]
#
# Step-2.4) Reset the debug signals for the next iteration
#
[connection conn2]
SET debug_sync="RESET";
[connection conn1]
BEGIN;
INSERT INTO t1 VALUES (1);
SET debug_sync="bgc_before_flush_stage SIGNAL reached WAIT_FOR insert_continue";
COMMIT;
[connection conn2]
SET debug_sync="now WAIT_FOR reached";
FLUSH TABLES WITH READ LOCK;
[connection conn3]
SET debug_sync="now SIGNAL insert_continue";
[connection conn2]
[connection conn4]
RESET MASTER;
[connection conn2]
UNLOCK TABLES;
[connection conn1]
#
# Step-2.4) Reset the debug signals for the next iteration
#
[connection conn2]
SET debug_sync="RESET";
[connection conn1]
BEGIN;
INSERT INTO t1 VALUES (1);
SET debug_sync="bgc_after_flush_stage_before_sync_stage SIGNAL reached WAIT_FOR insert_continue";
COMMIT;
[connection conn2]
SET debug_sync="now WAIT_FOR reached";
FLUSH TABLES WITH READ LOCK;
[connection conn3]
SET debug_sync="now SIGNAL insert_continue";
[connection conn2]
[connection conn4]
RESET MASTER;
[connection conn2]
UNLOCK TABLES;
[connection conn1]
#
# Step-2.4) Reset the debug signals for the next iteration
#
[connection conn2]
SET debug_sync="RESET";
[connection conn1]
BEGIN;
INSERT INTO t1 VALUES (1);
SET debug_sync="bgc_after_sync_stage_before_commit_stage SIGNAL reached WAIT_FOR insert_continue";
COMMIT;
[connection conn2]
SET debug_sync="now WAIT_FOR reached";
FLUSH TABLES WITH READ LOCK;
[connection conn3]
SET debug_sync="now SIGNAL insert_continue";
[connection conn2]
[connection conn4]
RESET MASTER;
[connection conn2]
UNLOCK TABLES;
[connection conn1]
#
# Step-2.4) Reset the debug signals for the next iteration
#
[connection conn2]
SET debug_sync="RESET";
[connection conn1]
BEGIN;
INSERT INTO t1 VALUES (1);
SET debug_sync="bgc_after_commit_stage_before_rotation SIGNAL reached WAIT_FOR insert_continue";
COMMIT;
[connection conn2]
SET debug_sync="now WAIT_FOR reached";
FLUSH TABLES WITH READ LOCK;
[connection conn3]
SET debug_sync="now SIGNAL insert_continue";
[connection conn2]
[connection conn4]
RESET MASTER;
[connection conn2]
UNLOCK TABLES;
[connection conn1]
#
# Step-2.4) Reset the debug signals for the next iteration
#
[connection conn2]
SET debug_sync="RESET";
#
# Step-3) Check that 'RESET MASTER' fails if it is executed from a
#         session that has already acquired locks on a table.
#
LOCK TABLE t1 READ;
RESET MASTER;
ERROR HY000: Can't execute the given command because you have active locked tables or an active transaction
UNLOCK TABLES;
LOCK TABLE t1 WRITE;
RESET MASTER;
ERROR HY000: Can't execute the given command because you have active locked tables or an active transaction
UNLOCK TABLES;
# Step-4) Cleanup
#
DROP TABLE t1;
