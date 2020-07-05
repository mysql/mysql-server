--disable_query_log

# MTR's check of test cases ensures that the state of innodb
# monitors(enabled/disabled) are not changed by tests.
# Some InnoDB monitors are enabled by default. In case some
# monitors are enabled during a test, there is no direct way
# to get back to the initial state of enabled monitors
# other than disabling them on a case-by-case basis.
#
# The purpose of this .inc file is to only retain those
# monitors which are enabled by default and disable the
# rest so that monitors changed in a test do not affect
# other tests.

SET GLOBAL innodb_monitor_disable=all;
SET GLOBAL innodb_monitor_enable="adaptive_hash_searches";
SET GLOBAL innodb_monitor_enable="adaptive_hash_searches_btree";
SET GLOBAL innodb_monitor_enable="buffer_data_reads";
SET GLOBAL innodb_monitor_enable="buffer_data_written";
SET GLOBAL innodb_monitor_enable="buffer_pages_created";
SET GLOBAL innodb_monitor_enable="buffer_pages_read";
SET GLOBAL innodb_monitor_enable="buffer_pages_written";
SET GLOBAL innodb_monitor_enable="buffer_pool_bytes_data";
SET GLOBAL innodb_monitor_enable="buffer_pool_bytes_dirty";
SET GLOBAL innodb_monitor_enable="buffer_pool_pages_data";
SET GLOBAL innodb_monitor_enable="buffer_pool_pages_dirty";
SET GLOBAL innodb_monitor_enable="buffer_pool_pages_free";
SET GLOBAL innodb_monitor_enable="buffer_pool_pages_misc";
SET GLOBAL innodb_monitor_enable="buffer_pool_pages_total";
SET GLOBAL innodb_monitor_enable="buffer_pool_reads";
SET GLOBAL innodb_monitor_enable="buffer_pool_read_ahead";
SET GLOBAL innodb_monitor_enable="buffer_pool_read_ahead_evicted";
SET GLOBAL innodb_monitor_enable="buffer_pool_read_requests";
SET GLOBAL innodb_monitor_enable="buffer_pool_size";
SET GLOBAL innodb_monitor_enable="buffer_pool_wait_free";
SET GLOBAL innodb_monitor_enable="buffer_pool_write_requests";
SET GLOBAL innodb_monitor_enable="dml_deletes";
SET GLOBAL innodb_monitor_enable="dml_inserts";
SET GLOBAL innodb_monitor_enable="dml_system_deletes";
SET GLOBAL innodb_monitor_enable="dml_system_inserts";
SET GLOBAL innodb_monitor_enable="dml_system_reads";
SET GLOBAL innodb_monitor_enable="dml_system_updates";
SET GLOBAL innodb_monitor_enable="dml_updates";
SET GLOBAL innodb_monitor_enable="file_num_open_files";
SET GLOBAL innodb_monitor_enable="ibuf_merges";
SET GLOBAL innodb_monitor_enable="ibuf_merges_delete";
SET GLOBAL innodb_monitor_enable="ibuf_merges_delete_mark";
SET GLOBAL innodb_monitor_enable="ibuf_merges_discard_delete";
SET GLOBAL innodb_monitor_enable="ibuf_merges_discard_delete_mark";
SET GLOBAL innodb_monitor_enable="ibuf_merges_discard_insert";
SET GLOBAL innodb_monitor_enable="ibuf_merges_insert";
SET GLOBAL innodb_monitor_enable="ibuf_size";
SET GLOBAL innodb_monitor_enable="innodb_activity_count";
SET GLOBAL innodb_monitor_enable="innodb_dblwr_pages_written";
SET GLOBAL innodb_monitor_enable="innodb_dblwr_writes";
SET GLOBAL innodb_monitor_enable="innodb_page_size";
SET GLOBAL innodb_monitor_enable="innodb_rwlock_sx_os_waits";
SET GLOBAL innodb_monitor_enable="innodb_rwlock_sx_spin_rounds";
SET GLOBAL innodb_monitor_enable="innodb_rwlock_sx_spin_waits";
SET GLOBAL innodb_monitor_enable="innodb_rwlock_s_os_waits";
SET GLOBAL innodb_monitor_enable="innodb_rwlock_s_spin_rounds";
SET GLOBAL innodb_monitor_enable="innodb_rwlock_s_spin_waits";
SET GLOBAL innodb_monitor_enable="innodb_rwlock_x_os_waits";
SET GLOBAL innodb_monitor_enable="innodb_rwlock_x_spin_rounds";
SET GLOBAL innodb_monitor_enable="innodb_rwlock_x_spin_waits";
SET GLOBAL innodb_monitor_enable="lock_deadlocks";
SET GLOBAL innodb_monitor_enable="lock_deadlock_false_positives";
SET GLOBAL innodb_monitor_enable="lock_deadlock_rounds";
SET GLOBAL innodb_monitor_enable="lock_rec_grant_attempts";
SET GLOBAL innodb_monitor_enable="lock_rec_release_attempts";
SET GLOBAL innodb_monitor_enable="lock_row_lock_current_waits";
SET GLOBAL innodb_monitor_enable="lock_row_lock_time";
SET GLOBAL innodb_monitor_enable="lock_row_lock_time_avg";
SET GLOBAL innodb_monitor_enable="lock_row_lock_time_max";
SET GLOBAL innodb_monitor_enable="lock_row_lock_waits";
SET GLOBAL innodb_monitor_enable="lock_schedule_refreshes";
SET GLOBAL innodb_monitor_enable="lock_threads_waiting";
SET GLOBAL innodb_monitor_enable="lock_timeouts";
SET GLOBAL innodb_monitor_enable="log_waits";
SET GLOBAL innodb_monitor_enable="log_writes";
SET GLOBAL innodb_monitor_enable="log_write_requests";
SET GLOBAL innodb_monitor_enable="os_data_fsyncs";
SET GLOBAL innodb_monitor_enable="os_data_reads";
SET GLOBAL innodb_monitor_enable="os_data_writes";
SET GLOBAL innodb_monitor_enable="os_log_bytes_written";
SET GLOBAL innodb_monitor_enable="os_log_fsyncs";
SET GLOBAL innodb_monitor_enable="os_log_pending_fsyncs";
SET GLOBAL innodb_monitor_enable="os_log_pending_writes";
SET GLOBAL innodb_monitor_enable="trx_rseg_history_len";

# NOTE:
# The default value for below variables is an empty string
# it assigning an empty string using the SET statement results
# in an error. The only way we can restore the value of the
# variable and avoid check_testcase failures is by trying to
# set the variable to its default value. Even though the
# variable has no default value, the value gets reset to an
# empty string and throws the below warning:
# Warning NNNN Default value is not defined for this set option.
#              Please specify correct counter or module name.

--disable_warnings ER_NO_DEFAULT
SET GLOBAL innodb_monitor_enable=default;
SET GLOBAL innodb_monitor_disable=default;
SET GLOBAL innodb_monitor_reset=default;
SET GLOBAL innodb_monitor_reset_all=default;
--enable_warnings ER_NO_DEFAULT

--enable_query_log
