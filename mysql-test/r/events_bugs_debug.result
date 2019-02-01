#
# Bug#21914871 : ASSERTION `! IS_SET()' FOR DIAGNOSTICS_AREA::SET_OK_STATUS
#                  CREATE EVENT
#
SET SESSION DEBUG='+d,thd_killed_injection';
CREATE EVENT event1 ON SCHEDULE EVERY 1 YEAR DO SELECT 1;
ERROR 70100: Query execution was interrupted
SET SESSION DEBUG='-d,thd_killed_injection';
#
# Bug#28122841 - CREATE EVENT/PROCEDURE/FUNCTION CRASHES WITH ACCENT SENSTIVE NAMES.
#
# Case 1: Test case to verify MDL locking from concurrent SHOW CREATE EVENT
#         and ALTER EVENT operation with case and accent insensitive
#         event names.
CREATE EVENT café ON SCHEDULE EVERY 2 YEAR DO SELECT 1;
SET DEBUG_SYNC='after_acquiring_shared_lock_on_the_event SIGNAL locked WAIT_FOR continue';
SHOW CREATE EVENT CaFé;
# At this stage shared lock on the event object is acquired.
CONNECT  con1, localhost, root;
SET DEBUG_SYNC='now WAIT_FOR locked';
ALTER EVENT CaFé COMMENT "comment";
# Exclusive lock on the event is requested by this statement and it is
# blocked till the shared lock is released by the SHOW statement.
CONNECT  con2, localhost, root;
SET DEBUG_SYNC='now SIGNAL continue';
connection con1;
connection default;
Event	sql_mode	time_zone	Create Event	character_set_client	collation_connection	Database Collation
café	#	#	#	#	#	#
# Case 2: Test case to verify MDL locking from concurrent DROP EVENT
#         and SHOW CREATE EVENT operation with case and accent insensitive
#         event name.
SET DEBUG_SYNC='after_acquiring_exclusive_lock_on_the_event SIGNAL locked WAIT_FOR continue';
DROP EVENT cafe;
# At this point we have a exclusive lock on the event.
connection con1;
SET DEBUG_SYNC='now WAIT_FOR locked';
SHOW CREATE EVENT CaFe;
# This statement request for shared lock on the event and it is blocked till
# the DROP EVENT releases the lock.
connection con2;
SET DEBUG_SYNC='now SIGNAL continue';
connection con1;
ERROR HY000: Unknown event 'CaFe'
connection default;
# Cleanup.
SET DEBUG_SYNC='RESET';
disconnect con1;
disconnect con2;
#
#  BUG#29140298 - `OPT_EVENT_SCHEDULER == EVENTS::EVENTS_ON ||
#                  OPT_EVENT_SCHEDULER == EVENTS::EVEN
#  When mysqld is started with --event_scheduler=DISABLED,
#  it asserts on debug build without the fix.
#  With the fix, the event scheduler initialization is skipped
#  if mysqld is started with --event_scheduler=DISABLED.
# restart: --event_scheduler=DISABLED
SELECT @@event_scheduler='DISABLED';
@@event_scheduler='DISABLED'
1
