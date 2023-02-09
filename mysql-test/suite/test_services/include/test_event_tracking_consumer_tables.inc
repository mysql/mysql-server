#
# Usage:
# --let $EVENT_TYPE=<event type>
# --let $COUNTER_NAME=<name of event tracking counter>
# --source ../include/test_event_tracking_consumer_tables.inc

SELECT reset_event_tracking_counter("all");

--echo # Execute query to populate event tracker
SELECT * FROM event_tracking_db.event_tracking_table;
--echo # Should only show event markers for following events:
--echo # command events
--echo # $EVENT_TYPE
SELECT display_session_data();

--echo # Execute query to populate event tracker
INSERT INTO event_tracking_db.event_tracking_table VALUES (4);
--echo # Should only show event markers for following events:
--echo # command events
--echo # $EVENT_TYPE
SELECT display_session_data();

--echo # Execute query to populate event tracker
UPDATE event_tracking_db.event_tracking_table SET c1 = 5 WHERE c1 = 4;
--echo # Should only show event markers for following events:
--echo # command events
--echo # $EVENT_TYPE
SELECT display_session_data();

--echo # Execute query to populate event tracker
DELETE FROM event_tracking_db.event_tracking_table WHERE c1 = 5;
--echo # Should only show event markers for following events:
--echo # command events
--echo # $EVENT_TYPE
SELECT display_session_data();

--echo # Execute query to populate event tracker
--error ER_NO_SUCH_TABLE
SELECT * FROM event_tracking_db.non_existing_table;
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
