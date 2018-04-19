RESET MASTER;
CREATE TABLE t1(c1 INT AUTO_INCREMENT PRIMARY KEY, c2 VARCHAR(200))
PARTITION BY HASH (c1) PARTITIONS 4;
INSERT INTO t1 VALUES(1, ""), (2, ""), (3, ""),(4, "");
#
# Verify INSERT after ANALYZE TABLE logs correct last_committed
#
[connection admin_conn]
SET debug_sync = "waiting_in_the_middle_of_flush_stage SIGNAL admin_ready
                  WAIT_FOR admin_continue";
ANALYZE TABLE t1;;
[connection insert_conn]
SET debug_sync = "now WAIT_FOR admin_ready";
SET debug_sync = "bgc_after_enrolling_for_flush_stage SIGNAL insert_ready
                  WAIT_FOR insert_continue";
INSERT INTO t1(c2) VALUES("ANALYZE TABLE t1;");
[connection default]
SET debug_sync = "now WAIT_FOR insert_ready";
SET debug_sync = "now SIGNAL admin_continue,insert_continue";
[connection admin_conn]
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
[connection insert_conn]
SET debug_sync = "RESET";
include/include/assert_logical_timestamps.inc [3 4]
#
# Verify INSERT after OPTIMIZE TABLE logs correct last_committed
#
[connection admin_conn]
SET debug_sync = "waiting_in_the_middle_of_flush_stage SIGNAL admin_ready
                  WAIT_FOR admin_continue";
OPTIMIZE TABLE t1;;
[connection insert_conn]
SET debug_sync = "now WAIT_FOR admin_ready";
SET debug_sync = "bgc_after_enrolling_for_flush_stage SIGNAL insert_ready
                  WAIT_FOR insert_continue";
INSERT INTO t1(c2) VALUES("OPTIMIZE TABLE t1;");
[connection default]
SET debug_sync = "now WAIT_FOR insert_ready";
SET debug_sync = "now SIGNAL admin_continue,insert_continue";
[connection admin_conn]
Table	Op	Msg_type	Msg_text
test.t1	optimize	note	Table does not support optimize, doing recreate + analyze instead
test.t1	optimize	status	OK
[connection insert_conn]
SET debug_sync = "RESET";
include/include/assert_logical_timestamps.inc [5 6]
#
# Verify INSERT after REPAIR TABLE logs correct last_committed
#
[connection admin_conn]
SET debug_sync = "waiting_in_the_middle_of_flush_stage SIGNAL admin_ready
                  WAIT_FOR admin_continue";
REPAIR TABLE t1;;
[connection insert_conn]
SET debug_sync = "now WAIT_FOR admin_ready";
SET debug_sync = "bgc_after_enrolling_for_flush_stage SIGNAL insert_ready
                  WAIT_FOR insert_continue";
INSERT INTO t1(c2) VALUES("REPAIR TABLE t1;");
[connection default]
SET debug_sync = "now WAIT_FOR insert_ready";
SET debug_sync = "now SIGNAL admin_continue,insert_continue";
[connection admin_conn]
Table	Op	Msg_type	Msg_text
test.t1	repair	status	OK
[connection insert_conn]
SET debug_sync = "RESET";
include/include/assert_logical_timestamps.inc [7 8]
#
# Verify INSERT after ALTER TABLE ... REPAIR TABLE logs correct
# last_committed
#
[connection admin_conn]
SET debug_sync = "waiting_in_the_middle_of_flush_stage SIGNAL admin_ready
                  WAIT_FOR admin_continue";
ALTER TABLE t1 REPAIR PARTITION p0;;
[connection insert_conn]
SET debug_sync = "now WAIT_FOR admin_ready";
SET debug_sync = "bgc_after_enrolling_for_flush_stage SIGNAL insert_ready
                  WAIT_FOR insert_continue";
INSERT INTO t1(c2) VALUES("ALTER TABLE t1 REPAIR PARTITION p0;");
[connection default]
SET debug_sync = "now WAIT_FOR insert_ready";
SET debug_sync = "now SIGNAL admin_continue,insert_continue";
[connection admin_conn]
Table	Op	Msg_type	Msg_text
test.t1	repair	status	OK
[connection insert_conn]
SET debug_sync = "RESET";
include/include/assert_logical_timestamps.inc [9 10]
#
# Verify INSERT after ALTER TABLE ... OPTIMIZE TABLE logs correct
# last_committed
#
[connection admin_conn]
SET debug_sync = "waiting_in_the_middle_of_flush_stage SIGNAL admin_ready
                  WAIT_FOR admin_continue";
ALTER TABLE t1 OPTIMIZE PARTITION p1;;
[connection insert_conn]
SET debug_sync = "now WAIT_FOR admin_ready";
SET debug_sync = "bgc_after_enrolling_for_flush_stage SIGNAL insert_ready
                  WAIT_FOR insert_continue";
INSERT INTO t1(c2) VALUES("ALTER TABLE t1 OPTIMIZE PARTITION p1;");
[connection default]
SET debug_sync = "now WAIT_FOR insert_ready";
SET debug_sync = "now SIGNAL admin_continue,insert_continue";
[connection admin_conn]
Table	Op	Msg_type	Msg_text
test.t1	optimize	note	Table does not support optimize on partitions. All partitions will be rebuilt and analyzed.
test.t1	optimize	status	OK
[connection insert_conn]
SET debug_sync = "RESET";
include/include/assert_logical_timestamps.inc [11 12]
#
# Verify INSERT after ALTER TABLE ... ANALYZE TABLE logs correct
# last_committed
#
[connection admin_conn]
SET debug_sync = "waiting_in_the_middle_of_flush_stage SIGNAL admin_ready
                  WAIT_FOR admin_continue";
ALTER TABLE t1 ANALYZE PARTITION p2;;
[connection insert_conn]
SET debug_sync = "now WAIT_FOR admin_ready";
SET debug_sync = "bgc_after_enrolling_for_flush_stage SIGNAL insert_ready
                  WAIT_FOR insert_continue";
INSERT INTO t1(c2) VALUES("ALTER TABLE t1 ANALYZE PARTITION p2;");
[connection default]
SET debug_sync = "now WAIT_FOR insert_ready";
SET debug_sync = "now SIGNAL admin_continue,insert_continue";
[connection admin_conn]
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
[connection insert_conn]
SET debug_sync = "RESET";
include/include/assert_logical_timestamps.inc [13 14]
#
# Verify the statements the following statement in different commit
# window logs correct last_committed
#
INSERT INTO t1(c2) VALUES("use max_comitted as last_committed");
include/include/assert_logical_timestamps.inc [2 3]
#
# Verify binlog rotation doesn't effect the correctness of last_committed
#
FLUSH BINARY LOGS;
[connection admin_conn]
SET debug_sync = "waiting_in_the_middle_of_flush_stage SIGNAL admin_ready
                  WAIT_FOR admin_continue";
REPAIR TABLE t1;;
[connection insert_conn]
SET debug_sync = "now WAIT_FOR admin_ready";
SET debug_sync = "bgc_after_enrolling_for_flush_stage SIGNAL insert_ready
                  WAIT_FOR insert_continue";
INSERT INTO t1(c2) VALUES("REPAIR TABLE t1;");
[connection default]
SET debug_sync = "now WAIT_FOR insert_ready";
SET debug_sync = "now SIGNAL admin_continue,insert_continue";
[connection admin_conn]
Table	Op	Msg_type	Msg_text
test.t1	repair	status	OK
[connection insert_conn]
SET debug_sync = "RESET";
include/include/assert_logical_timestamps.inc [0 2]
REPAIR TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	repair	status	OK
FLUSH BINARY LOGS;
INSERT INTO t1(c2) VALUES("use 0 as last_committed");
include/include/assert_logical_timestamps.inc [0 1]
#
# Verify ALTER TABLE without [OPTIMIZE|REPAIR|ANALYZE] PARTITION and
# INSERT on different table should be logged in same commit window
#
CREATE TABLE t2(c1 INT);
[connection admin_conn]
SET debug_sync = "waiting_in_the_middle_of_flush_stage SIGNAL admin_ready
                  WAIT_FOR admin_continue";
ALTER TABLE t2 ADD COLUMN c2 INT;;
[connection insert_conn]
SET debug_sync = "now WAIT_FOR admin_ready";
SET debug_sync = "bgc_after_enrolling_for_flush_stage SIGNAL insert_ready
                  WAIT_FOR insert_continue";
INSERT INTO t1(c2) VALUES("ALTER TABLE t2 ADD COLUMN c2 INT;");
[connection default]
SET debug_sync = "now WAIT_FOR insert_ready";
SET debug_sync = "now SIGNAL admin_continue,insert_continue";
[connection admin_conn]
[connection insert_conn]
SET debug_sync = "RESET";
include/include/assert_logical_timestamps.inc [2 4]
DROP TABLE t1, t2;
