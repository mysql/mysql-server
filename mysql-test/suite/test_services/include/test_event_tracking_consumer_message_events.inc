#
# Usage:
# --let $EVENT_TYPE=<event type>
# --let $COUNTER_NAME=<name of event tracking counter>
# --source ../include/test_event_tracking_consumer_message_events.inc

SELECT reset_event_tracking_counter("all");

--echo # Install audit api message emit component
INSTALL COMPONENT "file://component_audit_api_message_emit";

SELECT audit_api_message_emit_udf('component_text', 'producer_text',
                                  'message_text',
                                  'key1', 'value1',
                                  'key2', 123,
                                  'key3', NULL);

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

--echo # Uninstall audit api message emit component
UNINSTALL COMPONENT "file://component_audit_api_message_emit";
