# This script reverts what has been modified by log_disable_page_cleaners.inc

SET GLOBAL debug = "-d,gtid_persist_flush_disable";
SET GLOBAL debug = "-d,disable_se_persists_gtid";
SET GLOBAL innodb_dict_stats_disabled_debug = 0;
SET GLOBAL innodb_master_thread_disabled_debug = 0;
SET GLOBAL innodb_page_cleaner_disabled_debug = 0;
SET GLOBAL innodb_checkpoint_disabled = 0;
SET GLOBAL innodb_purge_stop_now = 0;
SET GLOBAL innodb_purge_run_now = 1;
