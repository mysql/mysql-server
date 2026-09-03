################################################################################
#
# Exercise redo encryption across crash recovery and a post-recovery
# change-buffer merge.
#
# Parameters:
#
#   $REDO_RECOVERY_ENCRYPTION
#     Whether redo encryption is enabled during crash recovery.
#
#   REDO_MARKERS_NOT_VISIBLE_IN_PLAINTEXT
#     Comma-separated markers that must remain encrypted.
#
#   REDO_MARKERS_VISIBLE_IN_PLAINTEXT
#     Comma-separated markers that must be visible in plaintext.
#
################################################################################

--source include/have_debug.inc
--source include/have_perl_64bit_int.inc
--source include/have_component_keyring_file.inc
--source suite/component_keyring_file/inc/setup_component.inc

if ($REDO_RECOVERY_ENCRYPTION) {
  --let $REDO_RECOVERY_RESTART_PARAMETERS = restart: $PLUGIN_DIR_OPT --innodb_redo_log_encrypt=ON --innodb-disable-background-merge=ON
}
if (!$REDO_RECOVERY_ENCRYPTION) {
  --let $REDO_RECOVERY_RESTART_PARAMETERS = restart: $PLUGIN_DIR_OPT --innodb-disable-background-merge=ON
}

DO innodb_redo_log_consumer_register();
CREATE TABLE t1 (a INT PRIMARY KEY, b LONGBLOB) ENGINE = InnoDB;
CREATE TABLE t2(
  a INT AUTO_INCREMENT PRIMARY KEY,
  b CHAR(16) CHARACTER SET latin1 COLLATE latin1_swedish_ci,
  c INT,
  INDEX(b))
ENGINE = InnoDB STATS_PERSISTENT = 0;

# Place the populated secondary-index pages after the recovery checkpoint.
# Keep the checkpoint fixed and prevent background change-buffer merges before
# and after the crash, until the raw redo has been inspected.
SET GLOBAL innodb_log_checkpoint_now = ON;
SET GLOBAL innodb_checkpoint_disabled = ON;
SET GLOBAL innodb_disable_background_merge = ON;

# Force change buffering whenever possible and populate multiple secondary
# index pages. Changes to the root page itself cannot be buffered.
SET GLOBAL innodb_change_buffering_debug = 1;
INSERT INTO t2 VALUES (0, REPEAT('X', 16), 1);
INSERT INTO t2 SELECT 0,b,c FROM t2;
INSERT INTO t2 SELECT 0,b,c FROM t2;
INSERT INTO t2 SELECT 0,b,c FROM t2;
INSERT INTO t2 SELECT 0,b,c FROM t2;
INSERT INTO t2 SELECT 0,b,c FROM t2;
INSERT INTO t2 SELECT 0,b,c FROM t2;
INSERT INTO t2 SELECT 0,b,c FROM t2;
INSERT INTO t2 SELECT 0,b,c FROM t2;
INSERT INTO t2 SELECT 0,b,c FROM t2;
INSERT INTO t2 SELECT 0,b,c FROM t2;
INSERT INTO t2 SELECT 0,b,c FROM t2;
INSERT INTO t2 SELECT 0,b,c FROM t2;
INSERT INTO t2 SELECT 0,b,c FROM t2;

SET GLOBAL innodb_redo_log_encrypt = ON;
INSERT INTO t1 VALUES (1, REPEAT('b', 6*512*512));

BEGIN;
SELECT b FROM t2 LIMIT 3;

--connect(con1,localhost,root,,)
BEGIN;
DELETE FROM t2 WHERE a = 1;
# The case-insensitive index treats X and x as equal, while their different
# physical bytes make the recovery-generated redo detectable.
INSERT INTO t2 VALUES (1, REPEAT('x', 16), 1);
COMMIT;

# The COMMIT above persisted the changes in the redo log; now kill the server
# while the buffered secondary-index change is still pending.
--source include/kill_mysqld.inc

--let $restart_parameters = $REDO_RECOVERY_RESTART_PARAMETERS
--replace_result $PLUGIN_DIR_OPT --plugin-dir=PLUGIN_DIR_OPT
--source include/start_mysqld.inc

# Change-buffer merges are disabled during redo recovery, so both counters
# remain zero.
SELECT COUNT(*) = 2 AS ibuf_counters_unchanged_during_recovery
FROM INFORMATION_SCHEMA.INNODB_METRICS
WHERE NAME IN ('ibuf_merges_delete_mark', 'ibuf_merges_insert')
AND COUNT = 0;

# Force the pending buffered operations to be applied after recovery.
SELECT COUNT(*) FROM t2 FORCE INDEX (b)
WHERE b = REPEAT('x', 16);

SELECT COUNT(*) = 2 AS ibuf_counters_increased_after_foreground_access
FROM INFORMATION_SCHEMA.INNODB_METRICS
WHERE NAME IN ('ibuf_merges_delete_mark', 'ibuf_merges_insert')
AND COUNT > 0;

SELECT a, LEFT(b, 8) FROM t1;
INSERT INTO t1 VALUES (2, REPEAT('c', 6*512*512));

--let MYSQLD_DATADIR = `SELECT @@datadir`
--source include/kill_mysqld.inc

--source ../include/verify_redo_log_marker_encryption.inc

--let $restart_parameters = $REDO_RECOVERY_RESTART_PARAMETERS
--replace_result $PLUGIN_DIR_OPT --plugin-dir=PLUGIN_DIR_OPT
--source include/start_mysqld.inc

CHECK TABLE t2;
DROP TABLE t1, t2;
--source suite/component_keyring_file/inc/teardown_component.inc
