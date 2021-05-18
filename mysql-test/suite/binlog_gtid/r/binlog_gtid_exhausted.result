call mtr.add_suppression("An error occurred during flush stage of the commit");
call mtr.add_suppression("Attempting backtrace. You can use the following information to find out");
SET GLOBAL binlog_error_action=IGNORE_ERROR;
CREATE TABLE t1 (a INT PRIMARY KEY) ENGINE=InnoDB;
RESET MASTER;
SET GLOBAL gtid_purged = CONCAT(@@GLOBAL.server_uuid, ':1-9223372036854775805');
INSERT INTO t1 VALUES (1);
INSERT INTO t1 VALUES (2);
ERROR HY000: Impossible to generate GTID: the integer component reached the maximum value. Restart the server with a new server_uuid.
include/assert_grep.inc [ER_GNO_EXHAUSTED found in server error log]
include/assert.inc [Count of elements in t1 should be 2.]
# restart
RESET MASTER;
DROP TABLE t1;
CREATE TABLE t1 (a INT PRIMARY KEY) ENGINE=InnoDB;
RESET MASTER;
SET GLOBAL gtid_purged = CONCAT(@@GLOBAL.server_uuid, ':1-9223372036854775805');
include/assert.inc [Binlog file must not rotate after SET GTID_PURGED]
INSERT INTO t1 VALUES (1);
SET GLOBAL binlog_error_action=ABORT_SERVER;
INSERT INTO t1 VALUES (2);
ERROR HY000: Binary logging not possible. Message: Impossible to generate GTID: the integer component reached the maximum value. Restart the server with a new server_uuid. Server is being stopped.
RESET MASTER;
DROP TABLE t1;
include/assert_grep.inc [ER_GNO_EXHAUSTED server is being stopped found in server error log]
# restart
RESET MASTER;
CREATE TABLE t1(i INT);
DROP TABLE t1;
include/assert_grep.inc [ER_WARN_GTID_THRESHOLD_BREACH found in server error log]
RESET MASTER;
CREATE TABLE t1(i INT);
include/assert_grep.inc [No new ER_WARN_GTID_THRESHOLD_BREACH found in server error log]
DROP TABLE t1;
# restart
RESET MASTER;
SET GLOBAL gtid_purged = CONCAT('dddddddd-dddd-dddd-dddd-dddddddddddd:1-', (1 << 62));
SET GLOBAL gtid_purged = CONCAT('+SERVER_UUID:1-', (1 << 62));
include/assert_grep.inc [No ER_WARN_GTID_THRESHOLD_BREACH found in server error log]
RESET MASTER;
SET GLOBAL gtid_purged = CONCAT('dddddddd-dddd-dddd-dddd-dddddddddddd:1-', (1 << 63) - 2);
SET GLOBAL gtid_purged = CONCAT('+SERVER_UUID:1-25');
include/assert_grep.inc [No new ER_WARN_GTID_THRESHOLD_BREACH found in server error log]
# restart
