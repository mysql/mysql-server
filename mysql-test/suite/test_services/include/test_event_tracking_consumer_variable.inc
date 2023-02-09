#
# Usage:
# --let $EVENT_TYPE=<event type>
# --let $COUNTER_NAME=<name of event tracking counter>
# --source ../include/test_event_tracking_variable.inc

SELECT reset_event_tracking_counter("all");

--echo # Execute query to populate event tracker
SELECT @@global.default_password_lifetime;
--echo # Should only show event markers for following events:
--echo # command events
--echo # $EVENT_TYPE
SELECT display_session_data();

--echo # Execute query to populate event tracker
SET GLOBAL default_password_lifetime=100;
--echo # Should only show event markers for following events:
--echo # command events
--echo # $EVENT_TYPE
SELECT display_session_data();

--echo # Should show following counters:
--echo # test_event_tracking_consumer.counter_command
--echo # $COUNTER_NAME
SELECT VARIABLE_NAME, VARIABLE_VALUE FROM performance_schema.global_status
  WHERE VARIABLE_NAME like 'test_event_tracking_consumer%' AND
        VARIABLE_VALUE > 0;
