# This script reverts what has been modified by truncate_redo_log_and_stop_truncating.inc

SET GLOBAL innodb_checkpoint_disabled = OFF;
