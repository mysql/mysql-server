#
# Usage:
# --let $EVENT_TYPE=<event type>
# --let $COUNTER_NAME=<name of event tracking counter>
# --source ../include/test_event_tracking_service.inc

--source test_event_tracking_consumer_tables.inc

--source test_event_tracking_consumer_procedure.inc

--source test_event_tracking_consumer_variable.inc

--source test_event_tracking_consumer_authentication.inc
