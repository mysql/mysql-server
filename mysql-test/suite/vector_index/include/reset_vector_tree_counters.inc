# Copyright 2026 Google LLC

--disable_query_log
set global innodb_monitor_disable='num_tree_%';
set global innodb_monitor_reset_all='num_tree_%';
set global innodb_monitor_enable='num_tree_%';
--enable_query_log
