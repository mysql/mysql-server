CREATE TEMPORARY TABLE tmp_t (a INT PRIMARY KEY, b VARCHAR(10000)) ENGINE = InnoDB;
INSERT INTO tmp_t (a, b) VALUES (0, '');

# Enable all METRICS within log module

SET GLOBAL innodb_monitor_enable = module_log;

if ($freeze_background_mtr == 1) {

  # Disable background threads that could modify pages

  SET GLOBAL innodb_dict_stats_disabled_debug = 1;
  SET GLOBAL innodb_master_thread_disabled_debug = 1;

  # Flush all dirty pages and write checkpoint

  SET GLOBAL innodb_log_checkpoint_now = 1;

  # Disable page cleaner threads

  SET GLOBAL innodb_page_cleaner_disabled_debug = 1;

  # Disable checkpoint writes

  SET GLOBAL innodb_checkpoint_disabled = 1;

  # Disable undo purge coordinator thread, because it could try to use mtr
  # and lead to deadlock because it keeps some rw-lock from DD.

  SET GLOBAL innodb_purge_stop_now = 1;
}

# Always validate order in flush list - after each insertion

SET GLOBAL DEBUG = "+d,buf_flush_list_validate";

SET DEBUG_SYNC = "RESET";
