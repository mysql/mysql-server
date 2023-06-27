# This script reverts what has been modified by stop_dirty_page_flushing_and_background_redo_producers.inc
SET GLOBAL innodb_dict_stats_disabled_debug = OFF;

SET GLOBAL innodb_master_thread_disabled_debug = OFF;

SET GLOBAL debug = "-d,gtid_persist_flush_disable";

SET GLOBAL debug = "-d,disable_se_persists_gtid";

--source resume_dirty_page_flushing_and_redo_log_truncating.inc

SET GLOBAL innodb_purge_stop_now = OFF;

SET GLOBAL innodb_purge_run_now = ON;
