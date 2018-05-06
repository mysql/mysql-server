RESET MASTER;
CREATE TABLE t1(c1 INT AUTO_INCREMENT PRIMARY KEY, c2 VARCHAR(200))
PARTITION BY HASH (c1) PARTITIONS 4;
INSERT INTO t1 VALUES(1, ""), (2, ""), (3, ""),(4, "");
#
# Verify INSERT after CREATE DATABASE logs correct last_committed
#
[connection admin_conn]
SET debug_sync = "waiting_in_the_middle_of_flush_stage SIGNAL admin_ready
                  WAIT_FOR admin_continue";
CREATE DATABASE d1 CHARACTER SET UTF8MB4;
[connection insert_conn]
SET debug_sync = "now WAIT_FOR admin_ready";
SET debug_sync = "bgc_after_enrolling_for_flush_stage SIGNAL insert_ready
                  WAIT_FOR insert_continue";
INSERT INTO t1(c2) VALUES("CREATE DATABASE d1 CHARACTER SET UTF8MB4");
[connection default]
SET debug_sync = "now WAIT_FOR insert_ready";
SET debug_sync = "now SIGNAL admin_continue,insert_continue";
[connection admin_conn]
[connection insert_conn]
SET debug_sync = "RESET";
include/include/assert_logical_timestamps.inc [3 4]
#
# Verify INSERT after ALTER DATABASE logs correct last_committed
#
[connection admin_conn]
SET debug_sync = "waiting_in_the_middle_of_flush_stage SIGNAL admin_ready
                  WAIT_FOR admin_continue";
ALTER DATABASE d1 DEFAULT CHARACTER SET = 'latin1';
[connection insert_conn]
SET debug_sync = "now WAIT_FOR admin_ready";
SET debug_sync = "bgc_after_enrolling_for_flush_stage SIGNAL insert_ready
                  WAIT_FOR insert_continue";
INSERT INTO t1(c2) VALUES("ALTER DATABASE d1 DEFAULT CHARACTER SET = 'latin1'");
[connection default]
SET debug_sync = "now WAIT_FOR insert_ready";
SET debug_sync = "now SIGNAL admin_continue,insert_continue";
[connection admin_conn]
[connection insert_conn]
SET debug_sync = "RESET";
include/include/assert_logical_timestamps.inc [5 6]
#
# Verify INSERT after DROP DATABASE logs correct last_committed
#
[connection admin_conn]
SET debug_sync = "waiting_in_the_middle_of_flush_stage SIGNAL admin_ready
                  WAIT_FOR admin_continue";
DROP DATABASE d1;
[connection insert_conn]
SET debug_sync = "now WAIT_FOR admin_ready";
SET debug_sync = "bgc_after_enrolling_for_flush_stage SIGNAL insert_ready
                  WAIT_FOR insert_continue";
INSERT INTO t1(c2) VALUES("DROP DATABASE d1");
[connection default]
SET debug_sync = "now WAIT_FOR insert_ready";
SET debug_sync = "now SIGNAL admin_continue,insert_continue";
[connection admin_conn]
[connection insert_conn]
SET debug_sync = "RESET";
include/include/assert_logical_timestamps.inc [7 8]
DROP TABLE t1;
