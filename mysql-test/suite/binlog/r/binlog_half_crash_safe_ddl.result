include/gtid_utils.inc
include/gtid_step_reset.inc
FLUSH LOGS;
*** Crash right after 'CREATE TABLE t_2 (a int auto_increment primary key)' has been binary-logged before committed in the engine ***
SET @@SESSION.debug="+d,crash_commit_after_log";
CREATE TABLE t_2 (a int auto_increment primary key);
ERROR HY000: Lost connection to MySQL server during query
# restart
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
#	#	Query	#	#	use `test`; CREATE TABLE t_2 (a int auto_increment primary key) /* XID # */
include/gtid_step_assert.inc [count=1, only_count=0]
include/gtid_step_reset.inc
FLUSH LOGS;
*** Crash right after 'ALTER TABLE t_2 ADD COLUMN b INT NOT NULL, ALGORITHM=COPY, COMMENT 'Col b copy-added'' has been binary-logged before committed in the engine ***
SET @@SESSION.debug="+d,crash_commit_after_log";
ALTER TABLE t_2 ADD COLUMN b INT NOT NULL, ALGORITHM=COPY, COMMENT 'Col b copy-added';
ERROR HY000: Lost connection to MySQL server during query
# restart
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
#	#	Query	#	#	use `test`; ALTER TABLE t_2 ADD COLUMN b INT NOT NULL, ALGORITHM=COPY, COMMENT 'Col b copy-added' /* XID # */
include/gtid_step_assert.inc [count=1, only_count=0]
include/gtid_step_reset.inc
FLUSH LOGS;
*** Crash right after 'ALTER TABLE t_2 ADD INDEX idx(b), ALGORITHM= INPLACE, COMMENT 'Index on b inplace-added'' has been binary-logged before committed in the engine ***
SET @@SESSION.debug="+d,crash_commit_after_log";
ALTER TABLE t_2 ADD INDEX idx(b), ALGORITHM= INPLACE, COMMENT 'Index on b inplace-added';
ERROR HY000: Lost connection to MySQL server during query
# restart
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
#	#	Query	#	#	use `test`; ALTER TABLE t_2 ADD INDEX idx(b), ALGORITHM= INPLACE, COMMENT 'Index on b inplace-added' /* XID # */
include/gtid_step_assert.inc [count=1, only_count=0]
include/gtid_step_reset.inc
FLUSH LOGS;
*** Crash right after 'ALTER TABLE t_2 RENAME t_3' has been binary-logged before committed in the engine ***
SET @@SESSION.debug="+d,crash_commit_after_log";
ALTER TABLE t_2 RENAME t_3;
ERROR HY000: Lost connection to MySQL server during query
# restart
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
#	#	Query	#	#	use `test`; ALTER TABLE t_2 RENAME t_3 /* XID # */
include/gtid_step_assert.inc [count=1, only_count=0]
include/gtid_step_reset.inc
FLUSH LOGS;
*** Crash right after 'RENAME TABLE t_3 TO t_2 /* Name restored */' has been binary-logged before committed in the engine ***
SET @@SESSION.debug="+d,crash_commit_after_log";
RENAME TABLE t_3 TO t_2 /* Name restored */;
ERROR HY000: Lost connection to MySQL server during query
# restart
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
#	#	Query	#	#	use `test`; RENAME TABLE t_3 TO t_2 /* Name restored */ /* XID # */
include/gtid_step_assert.inc [count=1, only_count=0]
include/gtid_step_reset.inc
FLUSH LOGS;
*** Crash right after 'INSERT INTO t_2 SET a=1,b=1' has been binary-logged before committed in the engine ***
SET @@SESSION.debug="+d,crash_commit_after_log";
INSERT INTO t_2 SET a=1,b=1;
ERROR HY000: Lost connection to MySQL server during query
# restart
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
#	#	Query	#	#	BEGIN
#	#	Query	#	#	use `test`; INSERT INTO t_2 SET a=1,b=1
#	#	Xid	#	#	COMMIT /* XID */
include/gtid_step_assert.inc [count=1, only_count=0]
include/gtid_step_reset.inc
FLUSH LOGS;
*** Crash right after 'TRUNCATE TABLE t_2' has been binary-logged before committed in the engine ***
SET @@SESSION.debug="+d,crash_commit_after_log";
TRUNCATE TABLE t_2;
ERROR HY000: Lost connection to MySQL server during query
# restart
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
#	#	Query	#	#	use `test`; TRUNCATE TABLE t_2 /* XID # */
include/gtid_step_assert.inc [count=1, only_count=0]
include/gtid_step_reset.inc
FLUSH LOGS;
*** Crash right after 'CREATE TABLESPACE ts_0 ADD DATAFILE 'ts_0.ibd'' has been binary-logged before committed in the engine ***
SET @@SESSION.debug="+d,crash_commit_after_log";
CREATE TABLESPACE ts_0 ADD DATAFILE 'ts_0.ibd';
ERROR HY000: Lost connection to MySQL server during query
# restart
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
#	#	Query	#	#	use `test`; CREATE TABLESPACE ts_0 ADD DATAFILE 'ts_0.ibd' /* XID # */
include/gtid_step_assert.inc [count=1, only_count=0]
include/gtid_step_reset.inc
FLUSH LOGS;
*** Crash right after 'DROP TABLESPACE ts_0' has been binary-logged before committed in the engine ***
SET @@SESSION.debug="+d,crash_commit_after_log";
DROP TABLESPACE ts_0;
ERROR HY000: Lost connection to MySQL server during query
# restart
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
#	#	Query	#	#	use `test`; DROP TABLESPACE ts_0 /* XID # */
include/gtid_step_assert.inc [count=1, only_count=0]
include/gtid_step_reset.inc
FLUSH LOGS;
*** Crash right after 'CREATE DATABASE db_0 DEFAULT COLLATE latin1_general_ci' has been binary-logged before committed in the engine ***
SET @@SESSION.debug="+d,crash_commit_after_log";
CREATE DATABASE db_0 DEFAULT COLLATE latin1_general_ci;
ERROR HY000: Lost connection to MySQL server during query
# restart
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
#	#	Query	#	#	CREATE DATABASE db_0 DEFAULT COLLATE latin1_general_ci /* XID # */
include/gtid_step_assert.inc [count=1, only_count=0]
include/gtid_step_reset.inc
FLUSH LOGS;
*** Crash right after 'ALTER DATABASE db_0 DEFAULT COLLATE utf8mb3_general_ci' has been binary-logged before committed in the engine ***
SET @@SESSION.debug="+d,crash_commit_after_log";
ALTER DATABASE db_0 DEFAULT COLLATE utf8mb3_general_ci;
ERROR HY000: Lost connection to MySQL server during query
# restart
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
#	#	Query	#	#	ALTER DATABASE db_0 DEFAULT COLLATE utf8mb3_general_ci /* XID # */
include/gtid_step_assert.inc [count=1, only_count=0]
include/gtid_step_reset.inc
FLUSH LOGS;
*** Crash right after 'DROP DATABASE db_0' has been binary-logged before committed in the engine ***
SET @@SESSION.debug="+d,crash_commit_after_log";
DROP DATABASE db_0;
ERROR HY000: Lost connection to MySQL server during query
# restart
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
#	#	Query	#	#	DROP DATABASE db_0 /* XID # */
include/gtid_step_assert.inc [count=1, only_count=0]
include/gtid_step_reset.inc
FLUSH LOGS;
*** Crash right after 'DROP TABLE t_2' has been binary-logged before committed in the engine ***
SET @@SESSION.debug="+d,crash_commit_after_log";
DROP TABLE t_2;
ERROR HY000: Lost connection to MySQL server during query
# restart
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
#	#	Query	#	#	use `test`; DROP TABLE `t_2` /* generated by server */ /* XID # */
include/gtid_step_assert.inc [count=1, only_count=0]
include/gtid_utils_end.inc
