--source include/have_debug.inc

# Save the initial number of concurrent sessions.
--source include/count_sessions.inc

--echo #
--echo # Bug#21914871 : ASSERTION `! IS_SET()' FOR DIAGNOSTICS_AREA::SET_OK_STATUS
--echo #                  CREATE EVENT
--echo #

SET SESSION DEBUG='+d,thd_killed_injection';
--error ER_QUERY_INTERRUPTED
CREATE EVENT event1 ON SCHEDULE EVERY 1 YEAR DO SELECT 1;
SET SESSION DEBUG='-d,thd_killed_injection';

--echo #
--echo # Bug#28122841 - CREATE EVENT/PROCEDURE/FUNCTION CRASHES WITH ACCENT SENSTIVE NAMES.
--echo #

--enable_connect_log

--echo # Case 1: Test case to verify MDL locking from concurrent SHOW CREATE EVENT
--echo #         and ALTER EVENT operation with case and accent insensitive
--echo #         event names.

CREATE EVENT café ON SCHEDULE EVERY 2 YEAR DO SELECT 1;
SET DEBUG_SYNC='after_acquiring_shared_lock_on_the_event SIGNAL locked WAIT_FOR continue';
--SEND SHOW CREATE EVENT CaFé
--echo # At this stage shared lock on the event object is acquired.

--CONNECT (con1, localhost, root)
SET DEBUG_SYNC='now WAIT_FOR locked';
--SEND ALTER EVENT CaFé COMMENT "comment"
--echo # Exclusive lock on the event is requested by this statement and it is
--echo # blocked till the shared lock is released by the SHOW statement.

--CONNECT (con2, localhost, root)
let $wait_condition= SELECT COUNT(*) > 0 FROM information_schema.processlist
                     WHERE info LIKE 'ALTER EVENT%' AND
                     state='Waiting for event metadata lock';
source include/wait_condition.inc;
SET DEBUG_SYNC='now SIGNAL continue';

--CONNECTION con1
--REAP

--CONNECTION default
--replace_column 2 # 3 # 4 # 5 # 6 # 7 #
--REAp

--echo # Case 2: Test case to verify MDL locking from concurrent DROP EVENT
--echo #         and SHOW CREATE EVENT operation with case and accent insensitive
--echo #         event name.

SET DEBUG_SYNC='after_acquiring_exclusive_lock_on_the_event SIGNAL locked WAIT_FOR continue';
--SEND DROP EVENT cafe
--echo # At this point we have a exclusive lock on the event.

--CONNECTION con1
SET DEBUG_SYNC='now WAIT_FOR locked';
--SEND SHOW CREATE EVENT CaFe
--echo # This statement request for shared lock on the event and it is blocked till
--echo # the DROP EVENT releases the lock.

--CONNECTION con2
let $wait_condition= SELECT COUNT(*) > 0 FROM information_schema.processlist
                     WHERE info LIKE 'SHOW CREATE EVENT%' AND
                     state='Waiting for event metadata lock';
source include/wait_condition.inc;
SET DEBUG_SYNC='now SIGNAL continue';

--CONNECTION con1
--error ER_EVENT_DOES_NOT_EXIST
--REAP

--CONNECTION default
--REAP

--echo # Cleanup.
SET DEBUG_SYNC='RESET';
DISCONNECT con1;
DISCONNECT con2;
--disable_connect_log


# Check that all connections opened by test cases in this file are really gone
# so execution of other tests won't be affected by their presence.
--source include/wait_until_count_sessions.inc


--echo #
--echo #  BUG#29140298 - `OPT_EVENT_SCHEDULER == EVENTS::EVENTS_ON ||
--echo #                  OPT_EVENT_SCHEDULER == EVENTS::EVEN
--echo #  When mysqld is started with --event_scheduler=DISABLED,
--echo #  it asserts on debug build without the fix.
--echo #  With the fix, the event scheduler initialization is skipped
--echo #  if mysqld is started with --event_scheduler=DISABLED.

--let $restart_parameters = restart: --event_scheduler=DISABLED
--source include/restart_mysqld.inc
# Ensure the event scheduler is OFF.
SELECT @@event_scheduler='DISABLED';


# Clean up, clear option
--let $restart_parameters = restart:
--source include/restart_mysqld.inc
