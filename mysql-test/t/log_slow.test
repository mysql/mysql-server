--echo #
--echo # Bug#28268680: REGRESSION: SLOW_QUERIES STATUS VARIABLE ISN'T
--echo #               INCREMENTED IF SLOW QUERY IS OFF
--echo #

# PS protocol skews our counters with its prepare/execute cycles.
# Cursor protocol doesn't support everything we need.
--source include/no_protocol.inc

# Test does not depend on MyISAM. Can possibly be merged into
# show_check.test or log_state.test once their dependence on
# MyISAM has been removed.

SELECT @@global.slow_query_log INTO @save_sql;

--echo # enable slow query log
SET @@global.slow_query_log=1;

--echo # Should be none yet.
SHOW SESSION STATUS LIKE 'Slow_queries';
SELECT variable_name,variable_value
  FROM performance_schema.session_status
 WHERE variable_name = "Slow_queries";

SET SESSION long_query_time=0;

--echo # expect "1"
SELECT variable_name,variable_value
  FROM performance_schema.session_status
 WHERE variable_name = "Slow_queries";

--echo # expect "2"
SHOW SESSION STATUS LIKE 'Slow_queries';

--echo # disable slow query log. slow queries should still be counted!
# (This is slow query #3.)
SET @@global.slow_query_log=0;

--echo # expect "4"
SELECT variable_name,variable_value
  FROM performance_schema.session_status
 WHERE variable_name = "Slow_queries";

--echo # expect "5"
SHOW SESSION STATUS LIKE 'Slow_queries';

SET SESSION long_query_time=DEFAULT;
SET @@global.slow_query_log=@save_sql;
