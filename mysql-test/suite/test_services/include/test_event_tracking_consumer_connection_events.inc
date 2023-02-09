#
# Usage:
# --let $COUNTER_NAME=<name of event tracking counter>
# --source ../include/test_event_tracking_consumer_connection_events.inc

SELECT reset_event_tracking_counter("all");

--source include/count_sessions.inc
--echo # Connect with test_user_connection
--connect(test_user_connection, localhost, event_tracking_user, abcd)
SELECT current_user();

--echo # Execute COM_CHANGE_USER
--change_user other_event_tracking_user, efgh
SELECT current_user();

--echo # Disconnect test_user_connection
--connection default
--disconnect test_user_connection
--source include/wait_until_count_sessions.inc

# Note: We are not displaying counter values because wait_until_count_sessions
# checks connected session status in a loop and depending on time taken to
# disconnect test_user_connection, command event counter may show different
# values for different test runs

--echo # Should show following counters:
--echo # test_event_tracking_consumer.counter_command
--echo # $COUNTER_NAME
SELECT VARIABLE_NAME FROM performance_schema.global_status
  WHERE VARIABLE_NAME like 'test_event_tracking_consumer%' AND
        VARIABLE_VALUE > 0;
