call mtr.add_suppression("An error occurred during flush stage of the commit");
call mtr.add_suppression("Attempting backtrace. You can use the following information to find out");
SET GLOBAL binlog_error_action=IGNORE_ERROR;
CREATE TABLE t1 (a INT PRIMARY KEY) ENGINE=InnoDB;
RESET MASTER;
SET GLOBAL gtid_purged = CONCAT(@@GLOBAL.server_uuid, ':1-9223372036854775805');
INSERT INTO t1 VALUES (1);
INSERT INTO t1 VALUES (2);
ERROR HY000: Impossible to generate Global Transaction Identifier: the integer component reached the maximal value. Restart the server with a new server_uuid.
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
ERROR HY000: Binary logging not possible. Message: An error occurred during flush stage of the commit. 'binlog_error_action' is set to 'ABORT_SERVER'. Hence aborting the server.
RESET MASTER;
DROP TABLE t1;
