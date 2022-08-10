SET DEBUG_SYNC = "RESET";

CREATE TEMPORARY TABLE tmp_t (a INT PRIMARY KEY, b VARCHAR(10000)) ENGINE = InnoDB;
INSERT INTO tmp_t (a, b) VALUES (0, '');

# Enable all METRICS within log module

SET GLOBAL innodb_monitor_enable = module_log;

if ($freeze_background_mtr == 1) {
  --source log_disable_page_cleaners.inc
}

# Always validate order in flush list - after each insertion

SET GLOBAL DEBUG = "+d,buf_flush_list_validate";
