
if ( !$LOCKING_SERVICE ) {
  skip Locking service plugin requires the environment variable \$LOCKING_SERVICE to be set (normally done by mtr);
}

--echo #
--echo # WL#8161 Locking service for read/write named locks
--echo #

--enable_connect_log

# Check the service locks via UDF's which invoke the functions for the
# locking service.
# Some main test for this WL is the locking_service unit test.
# The checks here are partially
# - duplicates of checks in the unit test
# - integration tests
#
# Note:
# Some sub tests check the impact of service locks on the content of
# performance_schema tables. They are placed here and not in the suite
# 'perfschema' because
# 1. The UDF's are some non trivial infrastructure which we need here anyway.
# 2. Some of the checks related to our service locks already use the content of
#    performance_schema tables for internal purposes.
# 3. In order to avoid frequent maintenance in case the layout or the content
#    of performance_schema tables gets changed intentionally we check only
#    some minimal set of content of the corresponding performance_schema tables.
#    The omitted content is covered for other types of locks in
#    performance_schema test anyway.
#

--replace_result $LOCKING_SERVICE LOCKING_SERVICE_LIB
eval
CREATE FUNCTION service_get_read_locks  RETURNS INT SONAME "$LOCKING_SERVICE";
--replace_result $LOCKING_SERVICE LOCKING_SERVICE_LIB
eval
CREATE FUNCTION service_get_write_locks RETURNS INT SONAME "$LOCKING_SERVICE";
--replace_result $LOCKING_SERVICE LOCKING_SERVICE_LIB
eval
CREATE FUNCTION service_release_locks   RETURNS INT SONAME "$LOCKING_SERVICE";


--echo #
--echo # 1: Test that calling the functions works, number of arguments,
--echo #    invalid argument type, invalid content (empty string)
--echo #
# service_get_*_locks(<name_space>, n times <lock_name>, <timeout>)
# service_release_locks(<name_space>)
# <name_space>, <lock_name> of type string, empty string is disallowed
# <timeout> of type integer
#

# Positive case checked later
# SELECT service_get_read_locks('positive', 'lock1', 'lock2', 0);
#
# Negative cases
# Already missing parameter <name_space>, no parameter at all
--error ER_CANT_INITIALIZE_UDF
SELECT service_get_read_locks();
# Already missing parameter <lock_name>
--error ER_CANT_INITIALIZE_UDF
SELECT service_get_read_locks('negative');
# Missing parameter <timeout>
--error ER_CANT_INITIALIZE_UDF
SELECT service_get_read_locks('negative', 'lock1');
#
# <name_space> of wrong type
--error ER_CANT_INITIALIZE_UDF
SELECT service_get_read_locks(         1, 'lock1', 'lock2', 0);
# <name_space> wrong content
--error ER_LOCKING_SERVICE_WRONG_NAME
SELECT service_get_read_locks(        '', 'lock1', 'lock2', 0);
# First <lock_name> of wrong type
--error ER_CANT_INITIALIZE_UDF
SELECT service_get_read_locks('negative',       1, 'lock2', 0);
# First <lock_name> wrong content
--error ER_LOCKING_SERVICE_WRONG_NAME

# Positive case checked later
# SELECT service_get_write_locks('positive', 'lock3', 'lock4', 0);
#
# Negative cases
# Already missing parameter <name_space>, no parameter at all
--error ER_CANT_INITIALIZE_UDF
SELECT service_get_write_locks();
# Already missing parameter <lock_name>
--error ER_CANT_INITIALIZE_UDF
SELECT service_get_write_locks('negative');
# Missing parameter <timeout>
--error ER_CANT_INITIALIZE_UDF
SELECT service_get_write_locks('negative', 'lock1');
#
# <name_space> of wrong type
--error ER_CANT_INITIALIZE_UDF
SELECT service_get_write_locks(         1, 'lock1', 'lock2', 0);
# <name_space> wrong content
--error ER_LOCKING_SERVICE_WRONG_NAME
SELECT service_get_write_locks(        '', 'lock1', 'lock2', 0);
# First <lock_name> of wrong type
--error ER_CANT_INITIALIZE_UDF
SELECT service_get_write_locks('negative',       1, 'lock2', 0);
# First <lock_name> wrong content
--error ER_LOCKING_SERVICE_WRONG_NAME
SELECT service_get_write_locks('negative',      '', 'lock2', 0);
# Last <lock_name> of wrong type
--error ER_CANT_INITIALIZE_UDF
SELECT service_get_write_locks('negative', 'lock1',       1, 0);
# Last <lock_name> wrong content
--error ER_LOCKING_SERVICE_WRONG_NAME
SELECT service_get_write_locks('negative', 'lock1',      '', 0);
# <timeout> of wrong type
--error ER_CANT_INITIALIZE_UDF
SELECT service_get_write_locks('negative', 'lock1', 'lock2', 'hello');

# Positive case checked later
# SELECT service_release_locks('positive');
#
# Negative cases
# Missing parameter <name_space>
--error ER_CANT_INITIALIZE_UDF
SELECT service_release_locks();
# Too many parameters
--error ER_CANT_INITIALIZE_UDF
SELECT service_release_locks('negative', 'hello');
#
# <name_space> of wrong type
--error ER_CANT_INITIALIZE_UDF
SELECT service_release_locks(         1);
# <name_space> wrong content
--error ER_LOCKING_SERVICE_WRONG_NAME
SELECT service_release_locks(        '');


--echo
--echo # 2: Check simple positive cases and that service locks taken are roughly
--echo #    correct represented in performance_schema.metadata_locks
--echo #
UPDATE performance_schema.setup_instruments SET enabled = 'NO', timed = 'YES';
TRUNCATE TABLE performance_schema.events_waits_current;
TRUNCATE TABLE performance_schema.events_waits_history_long;

# Enable only metadata wait event to prevent other wait events filling history
UPDATE performance_schema.setup_instruments SET enabled = 'YES'
WHERE name = 'wait/lock/metadata/sql/mdl';

SELECT COUNT(*) = 0 AS expect_1 FROM performance_schema.metadata_locks
WHERE OBJECT_TYPE = 'LOCKING SERVICE';

SELECT service_get_read_locks('pfs_check', 'lock1', 'lock2', 0);
SELECT service_get_write_locks('pfs_check', 'lock3', 'lock4', 0);

--sorted_result
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, LOCK_TYPE, LOCK_DURATION,
       LOCK_STATUS
FROM performance_schema.metadata_locks
WHERE OBJECT_TYPE = 'LOCKING SERVICE';

SELECT service_release_locks('pfs_check');
SELECT COUNT(*) = 0 AS expect_1 FROM performance_schema.metadata_locks
WHERE OBJECT_TYPE = 'LOCKING SERVICE';

--echo #
--echo # 3: Test taking and releasing of locks in two namespaces

SELECT service_get_write_locks("namespace1", "lock1", 60);
SELECT service_get_write_locks("namespace2", "lock1", "lock2", 60);
SELECT service_release_locks("namespace1");
SELECT service_release_locks("namespace2");

--echo #
--echo # 4: Test that another connection can be blocked and
--echo #    impact on certain tables in performance_schema
--echo # 4.0: Baseline checks
--echo #      Per performance_schema content no service locks in the moment
SELECT COUNT(*) = 0 AS expect_1 FROM performance_schema.metadata_locks
WHERE OBJECT_TYPE = 'LOCKING SERVICE';
--echo #      Per performance_schema no current wait for getting a service lock
SELECT COUNT(*) = 0 AS expect_1 FROM performance_schema.events_waits_current
WHERE OBJECT_TYPE = 'LOCKING SERVICE';
--echo # 4.1: Test that another connection gets blocked.
SELECT service_get_write_locks("namespace1", "lock1", "lock2", 60);
--connect(con1,localhost,root)
# First check timeout
--error ER_LOCKING_SERVICE_TIMEOUT
SELECT service_get_write_locks("namespace1", "lock1", 1);
--error ER_LOCKING_SERVICE_TIMEOUT
SELECT service_get_write_locks("namespace1", "lock2", 1);
--error ER_LOCKING_SERVICE_TIMEOUT
SELECT service_get_read_locks("namespace1", "lock1", 1);
--error ER_LOCKING_SERVICE_TIMEOUT
SELECT service_get_write_locks("namespace1", "lock1", 0);
--error ER_LOCKING_SERVICE_TIMEOUT
SELECT service_get_write_locks("namespace1", "lock2", 0);
--error ER_LOCKING_SERVICE_TIMEOUT
SELECT service_get_read_locks("namespace1", "lock1", 0);
--echo # 4.2: The content of performance_schema.events_waits_history_long
--echo #      should show that we have waited three times including correct
--echo #      service lock namespace and lock name.
--sorted_result
SELECT OBJECT_SCHEMA, OBJECT_NAME, OBJECT_TYPE, OPERATION
FROM performance_schema.events_waits_history_long
WHERE OBJECT_TYPE = 'LOCKING SERVICE';
--echo #      Per performance_schema keeps the last monitored wait for
--echo #      service lock even after our attempt timed out.
SELECT COUNT(*) = 1 AS expect_1 FROM performance_schema.events_waits_current
WHERE OBJECT_TYPE = 'LOCKING SERVICE';
--echo # 4.3: Show the impact of waiting for getting a service lock on
--echo #      content of the processlist and performance_schema
--send
SELECT service_get_write_locks("namespace1", "lock1", 60);
--connection default
let $wait_condition=
  SELECT COUNT(*) = 1 FROM information_schema.processlist
  WHERE state = "Waiting for locking service lock";
--source include/wait_condition.inc
--echo #      One waiting session in information_schema.processlist
SELECT COUNT(*) = 1 AS expect_1 FROM information_schema.processlist
WHERE state = 'Waiting for locking service lock';
--echo #      Per performance_schema one current wait for getting a service lock
SELECT OBJECT_SCHEMA, OBJECT_NAME, OBJECT_TYPE, OPERATION
FROM performance_schema.events_waits_current
WHERE OBJECT_TYPE = 'LOCKING SERVICE';
--echo # 4.4: Show the impact of releasing and getting a service lock on
--echo #      content of the processlist and performance_schema
# Release locks and cleanup
SELECT service_release_locks("namespace1");
--connection con1
--reap
--echo #      No waiting session in information_schema.processlist
SELECT COUNT(*) = 0 AS expect_1 FROM information_schema.processlist
WHERE state = 'Waiting for locking service lock';
--echo #      performance_schema keeps the last monitored wait for
--echo #      service lock even if no current wait
SELECT COUNT(*) = 1 AS expect_1 FROM performance_schema.events_waits_current
WHERE OBJECT_TYPE = 'LOCKING SERVICE';
SELECT service_release_locks("namespace1");

--echo #
--echo # 5: Test atomic acquire.
--connection default
SELECT service_get_write_locks('holder_name_space',
                               'first', 'middle', 'last', 1);
--connection con1
--error ER_LOCKING_SERVICE_TIMEOUT
SELECT service_get_write_locks('holder_name_space',
                               'first', 'l2_1', 'l3_1', 0);
--error ER_LOCKING_SERVICE_TIMEOUT
SELECT service_get_write_locks('holder_name_space',
                               'l1_2', 'middle', 'l3_2', 0);
--error ER_LOCKING_SERVICE_TIMEOUT
SELECT service_get_write_locks('holder_name_space',
                               'l1_3', 'l2_3', 'last', 0);
# There should still be only 3 locks held.
# This also checks that P_S output it sensible for locking service locks.
--sorted_result
SELECT object_schema, object_name, lock_type, lock_status
  FROM performance_schema.metadata_locks
  WHERE OBJECT_TYPE = 'LOCKING SERVICE';

# Release locks and cleanup
--disconnect con1
--source include/wait_until_disconnected.inc
--connection default
SELECT service_release_locks("holder_name_space");


--echo #
--echo # 6: Mixed tests
--echo #    They are partial logical duplicates of the corresponding unit test.
--connection default
--enable_connect_log
--connect(holder, localhost, root)
--connection holder
let $holder_id= `SELECT CONNECTION_ID()`;
--connect(requestor, localhost, root)
--connection requestor
let $requestor_id= `SELECT CONNECTION_ID()`;

--echo #
--echo # 6.1: Check maximum supported name_space length.
--connection holder
SELECT service_get_write_locks(RPAD('holder_name_space_', 64, 'a'),
                              'l1', 'l2', 'l3', 0) AS col1;
SELECT service_release_locks(RPAD('holder_name_space_', 64, 'a')) AS col1;
--error ER_LOCKING_SERVICE_WRONG_NAME
SELECT service_get_write_locks(RPAD('holder_name_space_', 65, 'a'),
                              'l1', 'l2', 'l3', 0) AS col1;

--echo #
--echo # 6.2: Check maximum supported lock name length at several positions
SELECT service_get_write_locks('holder_name_space',
       RPAD('l1_', 64, 'a'), RPAD('l2_', 64, 'a'), RPAD('l3_', 64, 'a'), 0) AS col1;
SELECT service_release_locks('holder_name_space') AS col1;
--error ER_LOCKING_SERVICE_WRONG_NAME
SELECT service_get_write_locks('holder_name_space',
                              RPAD('l1_', 65, 'a'), 'l2_1', 'l3_1', 0) AS col1;
--error ER_LOCKING_SERVICE_WRONG_NAME
SELECT service_get_write_locks('holder_name_space',
                              'l1_2', RPAD('l2_', 65, 'a'), 'l3_2', 0) AS col1;
--error ER_LOCKING_SERVICE_WRONG_NAME
SELECT service_get_write_locks('holder_name_space',
                              'l1_3', 'l2_3', RPAD('l3_', 65, 'a'), 0) AS col1;

--echo #
--echo # 6.3: Check that none of the failing (ER_LOCKING_SERVICE_WRONG_NAME)
--echo #      statements above requesting several locks got any lock at all.
--connection requestor
SELECT service_get_write_locks('holder_name_space',
       'l1_1', 'l2_1', 'l3_1',
       'l1_2', 'l2_2', 'l3_2',
       'l1_3', 'l2_3', 'l3_3', 1) AS col1;
SELECT service_release_locks('holder_name_space') AS col1;

--echo # Reveal that there are no locks left over.
SELECT COUNT(*) = 0 AS expect_1 FROM performance_schema.metadata_locks
WHERE OBJECT_TYPE = 'LOCKING SERVICE';

--echo #
--echo # 6.4: Check that comparison of locks works correct.
--connection holder
SELECT service_get_write_locks('holder_name_space', 'lock', 0) AS col1;
--connection requestor
--error ER_LOCKING_SERVICE_TIMEOUT
SELECT service_get_write_locks('holder_name_space', 'lock', 0) AS col1;
#
--echo # 6.4.1: Variation of lock namespace in get_write_lock.
SELECT service_get_write_locks('holder_name_space ', 'lock', 0) AS col1;
SELECT service_release_locks('holder_name_space ') AS col1;
SELECT service_get_write_locks(' holder_name_space', 'lock', 0) AS col1;
SELECT service_release_locks(' holder_name_space') AS col1;
SELECT service_get_write_locks('holder_name_spac', 'lock', 0) AS col1;
SELECT service_release_locks('holder_name_spac') AS col1;
SELECT service_get_write_locks('older_name_space', 'lock', 0) AS col1;
SELECT service_release_locks('older_name_space') AS col1;
#
--echo # 6.4.2: Variation of lock name in get_write_lock.
SELECT service_get_write_locks('holder_name_space', 'lock ', 0) AS col1;
SELECT service_release_locks('holder_name_space') AS col1;
SELECT service_get_write_locks('holder_name_space', ' lock', 0) AS col1;
SELECT service_release_locks('holder_name_space') AS col1;
SELECT service_get_write_locks('holder_name_space', 'loc', 0) AS col1;
SELECT service_release_locks('holder_name_space') AS col1;
SELECT service_get_write_locks('holder_name_space', 'ock', 0) AS col1;
SELECT service_release_locks('holder_name_space') AS col1;
#

--echo # 6.4.3: Variation of lock namespace in release_locks
--connection holder
SELECT service_release_locks('holder_name_space ') AS col1;
--connection requestor
--error ER_LOCKING_SERVICE_TIMEOUT
SELECT service_get_write_locks('holder_name_space', 'lock', 0) AS col1;
--connection holder
SELECT service_release_locks(' holder_name_space') AS col1;
--connection requestor
--error ER_LOCKING_SERVICE_TIMEOUT
SELECT service_get_write_locks('holder_name_space', 'lock', 0) AS col1;
--connection holder
SELECT service_release_locks('holder_name_spac') AS col1;
--connection requestor
--error ER_LOCKING_SERVICE_TIMEOUT
SELECT service_get_write_locks('holder_name_space', 'lock', 0) AS col1;
--connection holder
SELECT service_release_locks('older_name_spac') AS col1;
--connection requestor
--error ER_LOCKING_SERVICE_TIMEOUT
SELECT service_get_write_locks('holder_name_space', 'lock', 0) AS col1;
#
--connection holder
SELECT service_release_locks('holder_name_space') AS col1;

--echo # Reveal that there are no locks left over.
SELECT COUNT(*) = 0 AS expect_1 FROM performance_schema.metadata_locks
WHERE OBJECT_TYPE = 'LOCKING SERVICE';
# Attention:
# It is intentional that $requestor_id is not added at the end of $one_waits.
# Better a statement failing because of wrong syntax than some statement
# delivering whatever because the $requestor_id is wrong like outdated.
# The latter could easy happen by disconnect requestor + connect requestor.
let $one_waits=
SELECT COUNT(*) = 1 FROM information_schema.processlist
WHERE state = 'Waiting for locking service lock' AND id = ;

--echo #
--echo # 6.5: How get values assigned to timeout interpreted.
let $timeout= 0;
--echo # 6.5.1: timeout = $timeout -- No waiting
--connection holder
SELECT service_get_write_locks('holder_name_space', 'lock', 0) AS col1;
--connection requestor
--error ER_LOCKING_SERVICE_TIMEOUT
eval
SELECT service_get_write_locks('holder_name_space', 'lock', $timeout) AS col1;
let $timeout= -1;
--echo # 6.5.2: timeout = $timeout -- Wait endless similar to user locks
--connection requestor
--echo Send the statement and reap the result later.
--send
eval
SELECT service_get_write_locks('holder_name_space', 'lock', $timeout) AS col1;
--connection default
--echo # Check that "requestor" waits longer than one second.
let $wait_condition= $one_waits $requestor_id AND time > 1;
--source include/wait_condition.inc
--connection holder
SELECT service_release_locks('holder_name_space') AS col1;
--connection requestor
--echo Reap the result of the statement sent earlier.
--reap
SELECT service_release_locks('holder_name_space') AS col1;
let $timeout= 1.0E-5;
--echo # 6.5.3: timeout = $timeout -- Value does not get accepted.
--connection holder
SELECT service_get_write_locks('holder_name_space', 'lock', 0) AS col1;
--connection requestor
--error ER_CANT_INITIALIZE_UDF
eval
SELECT service_get_write_locks('holder_name_space', 'lock', $timeout) AS col1;
--connection holder
SELECT service_release_locks('holder_name_space') AS col1;
--echo # Reveal that there are no locks left over.
SELECT COUNT(*) = 0 AS expect_1 FROM performance_schema.metadata_locks
WHERE OBJECT_TYPE = 'LOCKING SERVICE';

--echo #
--echo # 6.6: Check that the same locks can be requested several times.
# == We do not collide with our own locks.
--connection holder
--echo # 6.6.1: Two times in same UDF call.
SELECT service_get_write_locks('holder_name_space', 'l1', 'l1', 0) AS col1;
--echo # We have two entries for the same lock.
--echo # Reveal that there are two locks.
SELECT COUNT(*) = 2 AS expect_1 FROM performance_schema.metadata_locks
WHERE OBJECT_TYPE = 'LOCKING SERVICE';
--echo # 6.6.2: Two times per two UDF calls in same statement.
SELECT service_get_write_locks('holder_name_space', 'l2', 0) AS col1,
       service_get_write_locks('holder_name_space', 'l2', 0);
--echo # 6.6.3: Two times per one UDF call per statement.
SELECT service_get_write_locks('holder_name_space', 'l3', 0) AS col1;
SELECT service_get_write_locks('holder_name_space', 'l3', 0) AS col1;
--echo # Reveal that there are six locks.
SELECT COUNT(*) = 6 AS expect_1 FROM performance_schema.metadata_locks
WHERE OBJECT_TYPE = 'LOCKING SERVICE';
SELECT service_release_locks('holder_name_space') AS col1;

--echo # Reveal that there are no locks left over.
SELECT COUNT(*) = 0 AS expect_1 FROM performance_schema.metadata_locks
WHERE OBJECT_TYPE = 'LOCKING SERVICE';

--echo #
--echo # 6.7: Taking several locks in one statement is supported and "holder"
--echo #      will do that now. Reveal that all are blocked.
--connection holder
SELECT service_get_write_locks('holder_name_space',
                              'first', 'middle', 'last', 0) AS col1;
--connection requestor
--error ER_LOCKING_SERVICE_TIMEOUT
SELECT service_get_write_locks('holder_name_space',
                              'first', 'l2_1', 'l3_1', 0) AS col1;
--error ER_LOCKING_SERVICE_TIMEOUT
SELECT service_get_write_locks('holder_name_space',
                              'l1_2', 'middle', 'l3_2', 0) AS col1;
--error ER_LOCKING_SERVICE_TIMEOUT
SELECT service_get_write_locks('holder_name_space',
                              'l1_3', 'l2_3', 'last', 0) AS col1;

--echo #
--echo # 6.8: Check that none of the failing (ER_LOCKING_SERVICE_TIMEOUT)
--echo #      statements above requesting several locks got any lock at all.
--connection holder
SELECT service_get_write_locks('holder_name_space',
       'l1_1', 'l2_1', 'l3_1',
       'l1_2', 'l2_2', 'l3_2',
       'l1_3', 'l2_3', 'l3_3', 0) AS col1;
SELECT service_release_locks('holder_name_space') AS col1;
--connection requestor
SELECT service_release_locks('holder_name_space') AS col1;

--echo # Reveal that there are no locks left over.
SELECT COUNT(*) = 0 AS expect_1 FROM performance_schema.metadata_locks
WHERE OBJECT_TYPE = 'LOCKING SERVICE';

--echo #
--echo # 6.9: Reveal that a
--echo #      - more or less smooth loss of the connection frees the special lock
--echo #      - ROLLBACK of some transaction during that a special lock was taken
--echo #        does not free that special lock
--echo #      held by that connection.
# Typical design of sub tests:
# 1. "requestor" sends requesting locks with timeout of 10s
# 2. "holder" or "default" polls on the processlist till "requestor" is
#    waiting for the lock.
# 3. "holder" and/or "default" does some operation.
# Maybe
# 4. "holder" or "default" polls on the processlist if "requestor" is
#    waiting for the lock.
# 5. "holder" and/or "default" does some operation which should finally lead
#    to freeing the lock
# 6. "requestor" reaps his result (success).
# 7. "requestor" frees his locks.
--echo # 6.9.1: ROLLBACK of "holder".
--connection holder
SET SESSION AUTOCOMMIT = OFF;
COMMIT;
SELECT service_get_write_locks('holder_name_space', 'lock', 0) AS col1;
--connection requestor
--echo Send the statement and reap the result later.
--send
SELECT service_get_write_locks('holder_name_space', 'lock', 10) AS col1;
--connection holder
--echo # Check that "requestor" waits.
let $wait_condition= $one_waits $requestor_id;
--source include/wait_condition.inc
ROLLBACK;
# Do something in order to ensure that the ROLLBACK impact is finished.
SET @aux = 1;
# Check that "requestor" waits.
let $wait_condition= $one_waits $requestor_id;
--source include/wait_condition.inc
SELECT service_release_locks('holder_name_space') AS col1;
--connection requestor
--echo Reap the result of the statement sent earlier.
--reap
SELECT service_release_locks('holder_name_space') AS col1;

--echo # 6.9.2: Disconnect of "holder" in mysqltest.
--connection holder
SELECT service_get_write_locks('holder_name_space', 'lock', 0) AS col1;
--connection requestor
--echo Send the statement and reap the result later.
--send
SELECT service_get_write_locks('holder_name_space', 'lock', 10) AS col1;
--connection holder
--echo # Check that "requestor" waits.
let $wait_condition= $one_waits $requestor_id;
--source include/wait_condition.inc
--disconnect holder
--source include/wait_until_disconnected.inc
--connection requestor
--echo Reap the result of the statement sent earlier.
--reap
SELECT service_release_locks('holder_name_space') AS col1;

--echo # 6.9.3: COMMMIT WORK RELEASE of "holder".
--connect(holder, localhost, root)
--connection holder
let $holder_id= `SELECT CONNECTION_ID()`;
SELECT service_get_write_locks('holder_name_space', 'lock', 0) AS col1;
--connection requestor
--echo Send the statement and reap the result later.
--send
SELECT service_get_write_locks('holder_name_space', 'lock', 10) AS col1;
--connection holder
--echo # Check that "requestor" waits.
let $wait_condition= $one_waits $requestor_id;
--source include/wait_condition.inc
COMMIT WORK RELEASE;
--connection requestor
--echo Reap the result of the statement sent earlier.
--reap
SELECT service_release_locks('holder_name_space') AS col1;
# Get rid of the connection handle.
--disconnect holder

--echo # 6.9.4: KILL <session of "holder"> issued by "holder"
--connect(holder, localhost, root)
--connection holder
let $holder_id= `SELECT CONNECTION_ID()`;
SELECT service_get_write_locks('holder_name_space', 'lock', 0) AS col1;
--connection requestor
--echo Send the statement and reap the result later.
--send
SELECT service_get_write_locks('holder_name_space', 'lock', 10) AS col1;
--connection holder
--echo # Check that "requestor" waits.
let $wait_condition= $one_waits $requestor_id;
--source include/wait_condition.inc
# There is the risk that $holder_id is a small number and part of the error
# message too. So we have to go via @aux.
--replace_result $holder_id <holder_id>
eval SET @aux = $holder_id;
--error ER_QUERY_INTERRUPTED
KILL @aux;
--connection requestor
--echo Reap the result of the statement sent earlier.
--reap
SELECT service_release_locks('holder_name_space') AS col1;
# Get rid of the connection handle.
--disconnect holder

--echo # 6.9.5: KILL <session of "holder"> issued by "default"
--connect(holder, localhost, root)
--connection holder
let $holder_id= `SELECT CONNECTION_ID()`;
SELECT service_get_write_locks('holder_name_space', 'lock', 0) AS col1;
--connection requestor
--echo Send the statement and reap the result later.
--send
SELECT service_get_write_locks('holder_name_space', 'lock', 10) AS col1;
--connection default
--echo # Check that "requestor" waits.
let $wait_condition= $one_waits $requestor_id;
--source include/wait_condition.inc
--replace_result $holder_id <holder_id>
eval KILL $holder_id;
--connection requestor
--echo Reap the result of the statement sent earlier.
--reap
SELECT service_release_locks('holder_name_space') AS col1;
# Get rid of the connection handle.
--disconnect holder

--echo #
--echo # 6.10: Are all locks in the same space == We take them "read" or "write"
--echo #       or is there a space for "read" lock and one for "write" locks.
--echo #       The first is valid.
--echo #       Can we upgrade (read->write, yes) or downgrade (write-> read, no)
--echo #       such locks.
--echo # 6.10.1: holder:    get_read_lock
--echo #                    get_write_lock
--echo #         requestor: get_read_lock -- will wait
--connect(holder, localhost, root)
--connection holder
let $holder_id= `SELECT CONNECTION_ID()`;
SELECT service_get_read_locks('holder_name_space', 'lock', 0) AS col1;
SELECT service_get_write_locks('holder_name_space', 'lock', 0) AS col1;
--connection requestor
--echo Send the statement and reap the result later.
--send
SELECT service_get_read_locks('holder_name_space', 'lock', 10) AS col1;
--connection default
--echo # Check that "requestor" waits.
let $wait_condition= $one_waits $requestor_id;
--source include/wait_condition.inc
# Conclusion: One space only. Upgrade read->write is supported.
--connection holder
SELECT service_release_locks('holder_name_space') AS col1;
--connection requestor
--echo Reap the result of the statement sent earlier.
--reap
SELECT service_release_locks('holder_name_space') AS col1;
--echo # 6.10.2: holder:    get_write_lock
--echo #                    get_read_lock
--echo #         requestor: get_read_lock -- will wait
--connection holder
SELECT service_get_write_locks('holder_name_space', 'lock', 0) AS col1;
SELECT service_get_read_locks('holder_name_space', 'lock', 0) AS col1;
--connection requestor
--echo Send the statement and reap the result later.
--send
SELECT service_get_read_locks('holder_name_space', 'lock', 10) AS col1;
--connection default
--echo # Check that "requestor" waits.
let $wait_condition= $one_waits $requestor_id;
--source include/wait_condition.inc
# Conclusion: Downgrade write->read is not supported.
--connection holder
SELECT service_release_locks('holder_name_space') AS col1;
--connection requestor
--echo Reap the result of the statement sent earlier.
--reap
SELECT service_release_locks('holder_name_space') AS col1;

--echo #
--echo # 6.11: Interleaved stuff
--echo # 6.11.1: It does not count who got the first "read" lock when trying to
--echo #         to get a "write" lock later.
--echo #         requestor: get_read_lock
--echo #         holder:    get_read_lock
--echo #         requestor: get_write_lock -- will wait
--connection requestor
SELECT service_get_read_locks('holder_name_space', 'lock', 0) AS col1;
--connection holder
SELECT service_get_read_locks('holder_name_space', 'lock', 0) AS col1;
--connection requestor
--echo Send the statement and reap the result later.
--send
SELECT service_get_write_locks('holder_name_space', 'lock', 10) AS col1;
--connection default
--echo # Check that "requestor" waits.
let $wait_condition= $one_waits $requestor_id;
--source include/wait_condition.inc
--connection holder
SELECT service_release_locks('holder_name_space') AS col1;
--connection requestor
--echo Reap the result of the statement sent earlier.
--reap
SELECT service_release_locks('holder_name_space') AS col1;
--echo # 6.11.2: It does not count who got the last "read" lock when trying to
--echo #         to get a "write" lock later.
--echo #         holder:    get_read_lock
--echo #         requestor: get_read_lock
--echo #         requestor: get_write_lock -- will wait
--connection holder
SELECT service_get_read_locks('holder_name_space', 'lock', 0) AS col1;
--connection requestor
SELECT service_get_read_locks('holder_name_space', 'lock', 0) AS col1;
--connection requestor
--echo Send the statement and reap the result later.
--send
SELECT service_get_write_locks('holder_name_space', 'lock', 10) AS col1;
--connection default
--echo # Check that "requestor" waits.
let $wait_condition= $one_waits $requestor_id;
--source include/wait_condition.inc
--connection holder
SELECT service_release_locks('holder_name_space') AS col1;
--connection requestor
--echo Reap the result of the statement sent earlier.
--reap
SELECT service_release_locks('holder_name_space') AS col1;
--echo # 6.11.3: There seems to be no lock free (micro) phase during upgrading.
--echo #         holder:    get_read_lock
--echo #         requestor: get_write_lock -- will wait
--echo #         holder:    get_write_lock
--echo #         requestor:                -- will stay waiting
--connection holder
SELECT service_get_read_locks('holder_name_space', 'lock', 0) AS col1;
--connection requestor
--echo Send the statement and reap the result later.
--send
SELECT service_get_write_locks('holder_name_space', 'lock', 10) AS col1;
--connection default
--echo # Check that "requestor" waits.
let $wait_condition= $one_waits $requestor_id;
--source include/wait_condition.inc
--connection holder
SELECT service_get_write_locks('holder_name_space', 'lock', 10) AS col1;
# Do something in order to ensure that the impact is finished.
SET @aux = 1;
--connection default
--echo # Check that "requestor" waits.
let $wait_condition= $one_waits $requestor_id;
--source include/wait_condition.inc
--connection holder
SELECT service_release_locks('holder_name_space') AS col1;
--connection requestor
--echo Reap the result of the statement sent earlier.
--reap
SELECT service_release_locks('holder_name_space') AS col1;

--echo #
--echo # 6.12: A session is able to free own locks but not locks held by
--echo #       by other sessions.
--echo #       holder:    get_read_lock l1
--echo #       default:   get_read_lock l1, l2
--echo #       requestor: get_write_lock l1 -- will wait
--echo #       default:   free locks (l1, l2)
--echo #       requestor:                   -- will stay waiting
--connection holder
SELECT service_get_read_locks('holder_name_space', 'l1', 0) AS col1;
--connection default
SELECT service_get_read_locks('holder_name_space', 'l1', 'l2', 0) AS col1;
--connection requestor
--echo Send the statement and reap the result later.
--send
SELECT service_get_write_locks('holder_name_space', 'l1', 10) AS col1;
--connection default
--echo # Check that "requestor" waits.
let $wait_condition= $one_waits $requestor_id;
--source include/wait_condition.inc
SELECT service_release_locks('holder_name_space') AS col1;
# Do something in order to ensure that the impact is finished.
SET @aux = 1;
--echo # Check that "requestor" waits.
let $wait_condition= $one_waits $requestor_id;
--source include/wait_condition.inc
--connection holder
SELECT service_release_locks('holder_name_space') AS col1;
--connection requestor
--echo Reap the result of the statement sent earlier.
--reap
SELECT service_release_locks('holder_name_space') AS col1;

--echo #
--echo # 6.13: The statement running the UDF fails because of reason outside UDF.
--echo #       No lock is taken.
--connection holder
CREATE TEMPORARY TABLE t1 AS SELECT 1 AS col1;
--error ER_TABLE_EXISTS_ERROR
CREATE TEMPORARY TABLE t1
AS SELECT service_get_write_locks('holder_name_space', 'lock', 0) AS col1;
--connection requestor
SELECT service_get_write_locks('holder_name_space', 'lock', 0) AS col1;
SELECT service_release_locks('holder_name_space') AS col1;
--connection holder
DROP TEMPORARY TABLE t1;
SELECT COUNT(*) = 0 AS expect_1 FROM performance_schema.metadata_locks
WHERE OBJECT_TYPE = 'LOCKING SERVICE';

--echo #
--echo # 6.14: The UDF's are atomic per call and not per statement where they
--echo #       are invoked. Locks are taken as long as it works and they will
--echo #       be not freed if the statement finally fails.
--connection holder
SELECT service_get_write_locks('holder_name_space', 'l2', 0) AS col1;
--connection default
SELECT COUNT(*) = 1 AS expect_1 FROM performance_schema.metadata_locks
WHERE OBJECT_TYPE = 'LOCKING SERVICE';
--connection requestor
--error ER_LOCKING_SERVICE_TIMEOUT
SELECT service_get_write_locks('holder_name_space', 'l1', 0) AS col1,
       service_get_write_locks('holder_name_space', 'l2', 0);
--connection default
SELECT COUNT(*) = 2 AS expect_1 FROM performance_schema.metadata_locks
WHERE OBJECT_TYPE = 'LOCKING SERVICE';
--connection requestor
SELECT service_release_locks('holder_name_space') AS col1;
--connection holder
SELECT service_release_locks('holder_name_space') AS col1;

--echo #
--echo # 6.15: The statement invoking a failing UDF call needs to fail too.
--echo #       The specific is here that we run the UDF for several rows.
--echo #       The statement should have zero impact on data (InnoDB assumed).
--connection holder
CREATE TABLE t1 (col1 VARCHAR(10));
INSERT INTO t1 VALUES ('l1'), ('l2'), ('l3');
SELECT service_get_write_locks('holder_name_space', 'l2', 0);
--echo # 6.15.1: CREATE TEMPORARY TABLE ... AS SELECT ... UDF ... FROM ...
--connection requestor
--error ER_LOCKING_SERVICE_TIMEOUT
CREATE TEMPORARY TABLE t2 ENGINE = InnoDB AS
SELECT service_get_write_locks('holder_name_space', col1, 0) AS col1 FROM t1
ORDER BY col1;
--error ER_BAD_TABLE_ERROR
DROP TEMPORARY TABLE t2;
--echo # There are two locks left over.
--echo # One taken by holder and one by requestor though his statement failed.
--echo # The latter is caused by some UDF limitation.
SELECT COUNT(*) = 2 AS expect_1 FROM performance_schema.metadata_locks
WHERE OBJECT_TYPE = 'LOCKING SERVICE';
SELECT service_release_locks('holder_name_space') AS col1;
if (0)
{
   # In case the UDF limitation is lifted than maybe implement and than enable.
   --echo # 6.15.2: INSERT INTO ... SELECT ... UDF ... FROM ...
   --echo # 6.15.3: INSERT INTO ... SELECT ... FROM ... WHERE ... UDF ...
}
--connection requestor
SELECT service_release_locks('holder_name_space') AS col1;
--connection holder
SELECT service_release_locks('holder_name_space') AS col1;
DROP TABLE t1;

--echo # Reveal that there are no locks left over.
SELECT COUNT(*) = 0 AS expect_1 FROM performance_schema.metadata_locks
WHERE OBJECT_TYPE = 'LOCKING SERVICE';

connection default;
--disconnect holder
--disconnect requestor
--echo # Check that "requestor" and "holder" are gone.
let $wait_condition=
SELECT COUNT(*) = 0 FROM information_schema.processlist
WHERE id IN ($requestor_id, $holder_id);
--source include/wait_condition.inc


--echo #
--echo # Bug#21286221: DEBUG SERVER CRASHES ON RESET SLAVE WITH
--echo #               LOCKED VERSION TOKEN
--echo #

SELECT service_get_write_locks('holder_name_space', 'lock', 0);
# This triggered an assert
FLUSH TABLES;

SELECT service_release_locks('holder_name_space');

DROP FUNCTION service_get_read_locks;
DROP FUNCTION service_get_write_locks;
DROP FUNCTION service_release_locks;

# Restore previous state of performance_schema instruments
UPDATE performance_schema.setup_instruments SET enabled = 'YES';

--disable_connect_log
