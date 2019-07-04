--echo #
--echo # Cleanup
--echo #
drop PROCEDURE populate_t1;

--disable_query_log
eval set @@global.innodb_undo_log_truncate = $undo_log_truncate_start_value;
eval set @@global.innodb_purge_rseg_truncate_frequency = $truncate_frequency_start_value;
eval set @@global.innodb_rollback_segments = $rollback_segments_start_value;
eval set @@global.innodb_max_undo_log_size = $max_undo_log_size_start_value;
eval set @@global.log_error_verbosity = $log_error_verbosity_start_value;
--enable_query_log

# This is in case the truncate got started before the settings above were reset.
# chtest will see any leftover trunc file.
let $wait_timeout= 300; # Override default 30 seconds with 300.
let $wait_condition= SELECT count(*) = 2 FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
                     WHERE SPACE_TYPE = 'Undo' AND STATE = 'active';
source include/wait_condition.inc;

