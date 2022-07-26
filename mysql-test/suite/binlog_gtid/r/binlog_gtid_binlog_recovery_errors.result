include/suppress_messages.inc
# Connection 1 suppresses message <.*Found invalid event sequence while recovering from binary log file.*>.
# Connection 1 suppresses message <.*Found.*prepared XA transactions.*>.
# Connection 1 suppresses message <.*Can.t init tc log.*>.
# Connection 1 suppresses message <.*Aborting.*>.
CREATE TABLE t1 (c1 INT PRIMARY KEY);
RESET MASTER;
INSERT INTO t1 VALUES (1);
INSERT INTO t1 VALUES (2);
extra/binlog_tests/recovery/kill_copy_malformed_restart.inc [orphan_xid]
# Kill the server
# restart: --debug=+d,eval_force_bin_log_recovery
include/wait_for_removed_expect_file.inc
include/wait_for_pattern_in_file.inc [.*between positions 430 and 540: Xid_log_event outside the boundary of a sequence of events representing an active transaction.*]
extra/binlog_tests/recovery/end_scenario.inc [orphan_xid]
# restart
DELETE FROM t1;
RESET MASTER;
INSERT INTO t1 VALUES (1);
INSERT INTO t1 VALUES (2);
extra/binlog_tests/recovery/kill_copy_malformed_restart.inc [duplicate_xid]
# Kill the server
# restart: --debug=+d,eval_force_bin_log_recovery
include/wait_for_removed_expect_file.inc
include/wait_for_pattern_in_file.inc [.*between positions 430 and 703: Xid_log_event holds an invalid XID.*]
extra/binlog_tests/recovery/end_scenario.inc [duplicate_xid]
# restart
DELETE FROM t1;
RESET MASTER;
XA START 'x';
INSERT INTO t1 VALUES (1);
XA END 'x';
XA PREPARE 'x';
extra/binlog_tests/recovery/kill_copy_malformed_restart.inc [orphan_xa_prepare]
# Kill the server
# restart: --debug=+d,eval_force_bin_log_recovery
include/wait_for_removed_expect_file.inc
include/wait_for_pattern_in_file.inc [.*between positions 157 and 273: XA_prepare_log_event outside the boundary of a sequence of events representing an active transaction.*]
extra/binlog_tests/recovery/end_scenario.inc [orphan_xa_prepare]
# restart
XA ROLLBACK 'x';
DELETE FROM t1;
RESET MASTER;
XA START 'x';
INSERT INTO t1 VALUES (1);
XA END 'x';
XA PREPARE 'x';
XA START 'y';
INSERT INTO t1 VALUES (2);
XA END 'y';
XA PREPARE 'y';
extra/binlog_tests/recovery/kill_copy_malformed_restart.inc [duplicate_xid_xa_prepare]
# Kill the server
# restart: --debug=+d,eval_force_bin_log_recovery
include/wait_for_removed_expect_file.inc
include/wait_for_pattern_in_file.inc [.*between positions 539 and 921: XA_prepare_log_event holds an invalid XID.*]
extra/binlog_tests/recovery/end_scenario.inc [duplicate_xid_xa_prepare]
# restart
XA ROLLBACK 'x'; XA ROLLBACK 'y';
DELETE FROM t1;
RESET MASTER;
BEGIN;
INSERT INTO t1 VALUES (1);
INSERT INTO t1 VALUES (2);
COMMIT;
XA START 'x';
INSERT INTO t1 VALUES (3);
XA END 'x';
XA PREPARE 'x';
extra/binlog_tests/recovery/kill_copy_malformed_restart.inc [orphan_begin_start]
# Kill the server
# restart: --debug=+d,eval_force_bin_log_recovery
include/wait_for_removed_expect_file.inc
include/wait_for_pattern_in_file.inc [.*between positions 157 and 656: Query_log_event containing `BEGIN/XA START` inside the boundary of a sequence of events representing an active transaction.*]
extra/binlog_tests/recovery/end_scenario.inc [orphan_begin_start]
# restart
XA ROLLBACK 'x';
DELETE FROM t1;
RESET MASTER;
CREATE TABLE tmp1 (i INT) ENGINE=MyISAM;
SET @@SESSION.binlog_format = STATEMENT;
BEGIN;
INSERT INTO tmp1 (SELECT c1 FROM t1);
COMMIT;
SET @@SESSION.binlog_format = ROW;
extra/binlog_tests/recovery/kill_copy_malformed_restart.inc [orphan_commit]
# Kill the server
# restart: --debug=+d,eval_force_bin_log_recovery
include/wait_for_removed_expect_file.inc
include/wait_for_pattern_in_file.inc [.*between positions 352 and 514: Query_log_event containing `COMMIT` outside the boundary of a sequence of events representing an active transaction.*]
extra/binlog_tests/recovery/end_scenario.inc [orphan_commit]
# restart
DROP TABLE tmp1;
DELETE FROM t1;
RESET MASTER;
CREATE FUNCTION func1 () RETURNS INT
BEGIN
CREATE TEMPORARY TABLE tmp1(i INT) ENGINE=MyISAM;
DROP TEMPORARY TABLE IF EXISTS tmp1;
RETURN 0;
END|
SET @@SESSION.binlog_format = STATEMENT;
BEGIN;
SELECT func1();
func1()
0
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET @@SESSION.binlog_format = ROW;
extra/binlog_tests/recovery/kill_copy_malformed_restart.inc [orphan_rollback]
# Kill the server
# restart: --debug=+d,eval_force_bin_log_recovery
include/wait_for_removed_expect_file.inc
include/wait_for_pattern_in_file.inc [.*between positions 510 and 667: Query_log_event containing `ROLLBACK` outside the boundary of a sequence of events representing an active transaction.*]
extra/binlog_tests/recovery/end_scenario.inc [orphan_rollback]
# restart
DROP FUNCTION func1;
DELETE FROM t1;
RESET MASTER;
INSERT INTO t1 VALUES (1);
CREATE TABLE tmp1 (i INT);
INSERT INTO tmp1 (SELECT c1 FROM t1);
extra/binlog_tests/recovery/kill_copy_malformed_restart.inc [ddl_in_transaction]
# Kill the server
# restart: --debug=+d,eval_force_bin_log_recovery
include/wait_for_removed_expect_file.inc
include/wait_for_pattern_in_file.inc [.*between positions 157 and 589: Query_log event containing a DDL inside the boundary of a sequence of events representing an active transaction.*]
extra/binlog_tests/recovery/end_scenario.inc [ddl_in_transaction]
# restart
DROP TABLE tmp1;
DELETE FROM t1;
RESET MASTER;
INSERT INTO t1 VALUES (1);
CREATE TABLE tmp1 (i INT);
extra/binlog_tests/recovery/kill_copy_malformed_restart.inc [duplicate_xid_ddl]
# Kill the server
# restart: --debug=+d,eval_force_bin_log_recovery
include/wait_for_removed_expect_file.inc
include/wait_for_pattern_in_file.inc [.*between positions 430 and 620: Query_log_event containing a DDL holds an invalid XID.*]
extra/binlog_tests/recovery/end_scenario.inc [duplicate_xid_ddl]
# restart
DROP TABLE tmp1;
DELETE FROM t1;
RESET MASTER;
INSERT INTO t1 VALUES (1);
XA START 'x';
INSERT INTO t1 VALUES (2);
XA END 'x';
XA PREPARE 'x';
XA COMMIT 'x' ;
extra/binlog_tests/recovery/kill_copy_malformed_restart.inc [orphan_xa_commit]
# Kill the server
# restart: --debug=+d,eval_force_bin_log_recovery
include/wait_for_removed_expect_file.inc
include/wait_for_pattern_in_file.inc [.*between positions 430 and 943: Query_log_event containing `XA COMMIT` inside the boundary of a sequence of events representing a transaction not yet in prepared state.*]
extra/binlog_tests/recovery/end_scenario.inc [orphan_xa_commit]
# restart
DELETE FROM t1;
RESET MASTER;
INSERT INTO t1 VALUES (1);
XA START 'x';
INSERT INTO t1 VALUES (2);
XA END 'x';
XA PREPARE 'x';
XA COMMIT 'x' ;
extra/binlog_tests/recovery/kill_copy_malformed_restart.inc [malformed_xid_xa_commit]
# Kill the server
# restart: --debug=+d,eval_force_bin_log_recovery
include/wait_for_removed_expect_file.inc
include/wait_for_pattern_in_file.inc [.*between positions 812 and 980: Query_log_event containing `XA COMMIT` holds an invalid XID.*]
extra/binlog_tests/recovery/end_scenario.inc [malformed_xid_xa_commit]
# restart
DELETE FROM t1;
RESET MASTER;
INSERT INTO t1 VALUES (1);
XA START 'x';
INSERT INTO t1 VALUES (2);
XA END 'x';
XA PREPARE 'x';
XA COMMIT 'x' ;
XA START 'x';
INSERT INTO t1 VALUES (3);
XA END 'x';
XA PREPARE 'x';
XA COMMIT 'x' ;
extra/binlog_tests/recovery/kill_copy_malformed_restart.inc [duplicate_xid_xa_commit]
# Kill the server
# restart: --debug=+d,eval_force_bin_log_recovery
include/wait_for_removed_expect_file.inc
include/wait_for_pattern_in_file.inc [.*between positions 980 and 1148: Query_log_event containing `XA COMMIT` holds an invalid XID.*]
extra/binlog_tests/recovery/end_scenario.inc [duplicate_xid_xa_commit]
# restart
DELETE FROM t1;
RESET MASTER;
INSERT INTO t1 VALUES (1);
XA START 'x';
INSERT INTO t1 VALUES (2);
XA END 'x';
XA PREPARE 'x';
XA ROLLBACK 'x' ;
extra/binlog_tests/recovery/kill_copy_malformed_restart.inc [orphan_xa_rollback]
# Kill the server
# restart: --debug=+d,eval_force_bin_log_recovery
include/wait_for_removed_expect_file.inc
include/wait_for_pattern_in_file.inc [.*between positions 430 and 945: Query_log_event containing `XA ROLLBACK` inside the boundary of a sequence of events representing a transaction not yet in prepared state.*]
extra/binlog_tests/recovery/end_scenario.inc [orphan_xa_rollback]
# restart
DELETE FROM t1;
RESET MASTER;
INSERT INTO t1 VALUES (1);
XA START 'x';
INSERT INTO t1 VALUES (2);
XA END 'x';
XA PREPARE 'x';
XA ROLLBACK 'x' ;
extra/binlog_tests/recovery/kill_copy_malformed_restart.inc [malformed_xid_xa_rollback]
# Kill the server
# restart: --debug=+d,eval_force_bin_log_recovery
include/wait_for_removed_expect_file.inc
include/wait_for_pattern_in_file.inc [.*between positions 812 and 982: Query_log_event containing `XA ROLLBACK` holds an invalid XID.*]
extra/binlog_tests/recovery/end_scenario.inc [malformed_xid_xa_rollback]
# restart
DELETE FROM t1;
RESET MASTER;
INSERT INTO t1 VALUES (1);
XA START 'x';
INSERT INTO t1 VALUES (2);
XA END 'x';
XA PREPARE 'x';
XA COMMIT 'x' ;
XA START 'x';
INSERT INTO t1 VALUES (3);
XA END 'x';
XA PREPARE 'x';
XA ROLLBACK 'x' ;
extra/binlog_tests/recovery/kill_copy_malformed_restart.inc [duplicate_xid_xa_rollback]
# Kill the server
# restart: --debug=+d,eval_force_bin_log_recovery
include/wait_for_removed_expect_file.inc
include/wait_for_pattern_in_file.inc [.*between positions 980 and 1150: Query_log_event containing `XA ROLLBACK` holds an invalid XID.*]
extra/binlog_tests/recovery/end_scenario.inc [duplicate_xid_xa_rollback]
# restart
DELETE FROM t1;
RESET MASTER;
DROP TABLE t1;
