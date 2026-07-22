# --let $wait_log_pattern=LOG PATTERN
# --let $wait_log_expected_count=COUNT
# --let $wait_timeout=SECONDS   # optional (defaults to 10)
# --let $wait_interval=SECONDS  # optional (defaults to 0.5)
# --source ../include/cmrs/compare_log_timeout.inc
--let $wait_timeout=`SELECT COALESCE('$wait_timeout', 10)`
--let $wait_interval=`SELECT COALESCE('$wait_interval', 0.5)`
--let $wait_condition=SELECT COUNT(*) FROM performance_schema.error_log WHERE DATA LIKE '%$wait_log_pattern%' = $wait_log_expected_count
--source include/wait_condition.inc

--let $wait_log_pattern=
--let $wait_log_expected_count=
