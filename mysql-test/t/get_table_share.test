--source include/have_debug.inc
--source include/have_debug_sync.inc
--enable_connect_log

--echo ###########################################################
--echo #
--echo # Test cases for wl#7593: Don't hold LOCK_open... and
--echo # bug #19881395 "FREEZE OF SERVER, FLUSH TABLE VERSUS
--echo # CONCURRENT DDL/DML".
--echo #
--echo # 1) The first basic scenario is based on concurrency and
--echo # sequencing of three threads: Let thread TA1 open table ta,
--echo # while thread TB1 and TB2 open table tb. Now, we have various
--echo # possible situations that are considered in the three first
--echo # test cases below, which are all based on this point of
--echo # departure, referred to as (1) below.
--echo #
--echo # 2) The second basic scenario is based on three threads: Let
--echo # thread TB1 and TB2 open table tb, while thread TA1 issues a
--echo # FLUSH TABLES in order to flush the cache while a share is
--echo # being initialized. The three last test cases are variants of
--echo # this scenario, referred to as (2) below. One of them covers
--echo # scenario in which bug #19881395 has occurred.
--echo #
--echo # 3) The third scenario is based on two threads: One thread
--echo # doing CREATE TABLE while another issues LOCK TABLE on the
--echo # same table. There is one test case based on this scenario.
--echo #
--echo # 4) The fourth scenario is based on two threads: One thread
--echo # TB1 opening table tb, being paused while opening the share,
--echo # while another thread TB2 issues the SQL command SHOW OPEN
--echo # TABLES. Then, we verify that the table being opened is excluded
--echo # from the list of open tables. A related test case is relevant
--echo # in the context of the federated storage engine, and is located
--echo # in suite/federated/federated_get_table_share.test.
--echo #
--echo ###########################################################
--echo #
--echo # Test setup: Create three reusable connections:
--echo #

--connect (con_TA1, localhost, root)
--connect (con_TB1, localhost, root)
--connect (con_TB2, localhost, root)

--echo ###########################################################
--echo #
--echo # Test case 1.1: After 1), verify that if thread TA1
--echo # broadcasts COND_open first, thread TB2 will wake up,
--echo # re-fetch its share and see that m_open_in_progress is
--echo # still true, and then continue waiting for COND_open.
--echo #

--echo #
--connection default
--echo # Create two tables:
CREATE TABLE ta (pk integer primary key);
CREATE TABLE tb (pk integer primary key);

--echo #
--echo # Warm-up data-dictionary cache but keep Table Definition Cache intact.
--echo #
--disable_result_log
--disable_query_log
SHOW CREATE TABLE ta;
SHOW CREATE TABLE tb;
FLUSH TABLE ta, tb;
--enable_query_log
--enable_result_log

--echo #
--connection con_TA1
--echo # Wait after releasing LOCK_open for ta, and make sure we never
--echo # end up at the 'found_share' sync point:
SET DEBUG_SYNC= 'get_share_before_open SIGNAL open_TA1 WAIT_FOR cont_TA1';
SET DEBUG_SYNC= 'get_share_found_share HIT_LIMIT 1';
--send INSERT INTO ta VALUES(1)

--echo #
--connection con_TB1
--echo # Wait for open_TA1, then wait after releasing LOCK_open for tb,
--echo # also make sure we never end up at the 'found_share' sync point:
SET DEBUG_SYNC= 'now WAIT_FOR open_TA1';
SET DEBUG_SYNC= 'get_share_before_open SIGNAL open_TB1 WAIT_FOR cont_TB1';
SET DEBUG_SYNC= 'get_share_found_share HIT_LIMIT 1';
--send INSERT INTO tb VALUES(1)

--echo #
--connection con_TB2
--echo # Wait for open_TB1, then wait after the tb share is found in the
--echo # TDC. Wake up when TA1 broadcasts COND_open, then go back to wait
--echo # since a different share (ta) was opened. Finally stop at the
--echo # 'found_share' sync point to verify that the share being addressed
--echo # is now available. Also make sure we never end up at the
--echo # 'before_open' sync point:
SET DEBUG_SYNC= 'now WAIT_FOR open_TB1';
SET DEBUG_SYNC= 'get_share_found_share SIGNAL found_TB2';
SET DEBUG_SYNC= 'get_share_before_open HIT_LIMIT 1';
--send INSERT INTO tb VALUES(2)

--echo #
--connection default
--echo # Now, we know that TA1 and TB1 are about to open the shares
--echo # for ta and tb concurrently. We also know that TB2 is about to
--echo # wait for COND_open. First issue then is to make sure TB2 waits
--echo # for COND_open (using P_S.events_waits_current, not logged here):
LET $wait_condition=
  SELECT COUNT(*) = 1 FROM performance_schema.events_waits_current
    WHERE event_name LIKE '%COND_open';
--source include/wait_condition.inc

--echo #
--echo # Then we save the event id for later:
SET @first_wait_id= 0;
SELECT event_id FROM performance_schema.events_waits_current
  WHERE event_name LIKE '%COND_open' INTO @first_wait_id;

--echo #
--echo # Next up is to make one of the opening threads read the definition.
--echo # Here, we let TA1 read first:
SET DEBUG_SYNC= 'now SIGNAL cont_TA1';

--echo #
--echo # Then, we make sure TB2 leaves the wait for COND_open, and then
--echo # waits for it once more. Verify this by waiting for the event_id
--echo # to change:
LET $wait_condition=
  SELECT event_id != @first_wait_id
    FROM performance_schema.events_waits_current
    WHERE event_name LIKE '%COND_open';
--source include/wait_condition.inc

--echo #
--echo # Then, we signal TB1 to make it open its def and do its things,
--echo # this will also wake up TB2 (now waiting on the COND_open):
SET DEBUG_SYNC= 'now SIGNAL cont_TB1';

--echo #
--echo # And at last, we wait for TB2 to signal that it found its share:
SET DEBUG_SYNC= 'now WAIT_FOR found_TB2';

--echo #
--echo # Reap the connections, reset DEBUG_SYNC and drop tables:
--connection con_TA1
--reap
--connection con_TB1
--reap
--connection con_TB2
--reap
--connection default
SET DEBUG_SYNC= 'RESET';
DROP TABLE ta, tb;

--echo ###########################################################
--echo #
--echo # Test case 1.2: After 1), verify that if thread TB1
--echo # broadcasts COND_open first, thread TB2 will wake up,
--echo # re-fetch its share and see that m_open_in_progress is
--echo # false, and then continue under the assumption that the
--echo # expected share is found.
--echo #

--echo #
--connection default
--echo # Create two tables:
CREATE TABLE ta (pk integer primary key);
CREATE TABLE tb (pk integer primary key);

--echo #
--echo # Warm-up data-dictionary cache but keep Table Definition Cache intact.
--echo #
--disable_result_log
--disable_query_log
SHOW CREATE TABLE ta;
SHOW CREATE TABLE tb;
FLUSH TABLE ta, tb;
--enable_query_log
--enable_result_log

--echo #
--connection con_TA1
--echo # Wait after releasing LOCK_open for ta, and make sure we never
--echo # end up at the 'found_share' sync point:
SET DEBUG_SYNC= 'get_share_before_open SIGNAL open_TA1 WAIT_FOR cont_TA1';
SET DEBUG_SYNC= 'get_share_found_share HIT_LIMIT 1';
--send INSERT INTO ta VALUES(1)

--echo #
--connection con_TB1
--echo # Wait for open_TA1, then wait after releasing LOCK_open for tb,
--echo # also make sure we never end up at the 'found_share' sync point:
SET DEBUG_SYNC= 'now WAIT_FOR open_TA1';
SET DEBUG_SYNC= 'get_share_before_open SIGNAL open_TB1 WAIT_FOR cont_TB1';
SET DEBUG_SYNC= 'get_share_found_share HIT_LIMIT 1';
--send INSERT INTO tb VALUES(1)

--echo #
--connection con_TB2
--echo # Wait for open_TB1, then wait after the tb share is found in the
--echo # TDC. Wake up when TB1 broadcasts COND_open, jump to 'found' since
--echo # the awaited share (tb) was opened by TB1 before TA1 opened ta.
--echo # Signal when at the 'found_share' sync point so we can verify that the
--echo # thread is at the expected point. Also make sure we never end up
--echo # at the 'before_open' sync point:
SET DEBUG_SYNC= 'now WAIT_FOR open_TB1';
SET DEBUG_SYNC= 'get_share_found_share SIGNAL found_TB2';
SET DEBUG_SYNC= 'get_share_before_open HIT_LIMIT 1';
--send INSERT INTO tb VALUES(2)

--echo #
--connection default
--echo # Now, we know that TA1 and TB1 are about to open the shares
--echo # for ta and tb concurrently. We also know that TB2 is about to
--echo # wait for COND_open. First issue then is to make sure TB2 waits
--echo # for COND_open (using P_S.events_waits_current, not logged here):
LET $wait_condition=
  SELECT COUNT(*) = 1 FROM performance_schema.events_waits_current
    WHERE event_name LIKE '%COND_open';
--source include/wait_condition.inc

--echo #
--echo # Next up is to make one of the opening threads read the def.
--echo # Here, as opposed to the previous test case, we let TB1 read first:
SET DEBUG_SYNC= 'now SIGNAL cont_TB1';

--echo #
--echo # Then, we wait for TB2 to signal that it's at the 'found_share'
--echo # sync point. This means it jumped out of the wait loop in the first
--echo # attempt since a second loop would make it do cond_wait once more:
SET DEBUG_SYNC= 'now WAIT_FOR found_TB2';

--echo #
--echo # Then, we signal TA1 to make it open its def and do its things,
--echo # and then we're done:
SET DEBUG_SYNC= 'now SIGNAL cont_TA1';

--echo #
--echo # Reap the connections, reset DEBUG_SYNC and drop tables:
--connection con_TA1
--reap
--connection con_TB1
--reap
--connection con_TB2
--reap
--connection default
SET DEBUG_SYNC= 'RESET';
DROP TABLE ta, tb;

--echo ###########################################################
--echo #
--echo # Test case 1.3: After 1), verify that if there is an error
--echo # in open_table_def for TB1, the share is deleted from the
--echo # hash table and destroyed. Then, verify that TB2 wakes up,
--echo # discovers that the share is now missing, and then
--echo # continues as if the share never existed in the hash table
--echo # in the first place. This test case does not use connection
--echo # TA1. 
--echo #

--echo #
--connection default
--echo # Create one table:
CREATE TABLE tb (pk integer primary key);

--echo #
--echo # Warm-up data-dictionary cache but keep Table Definition Cache intact.
--echo #
--disable_result_log
--disable_query_log
SHOW CREATE TABLE tb;
FLUSH TABLE tb;
--enable_query_log
--enable_result_log

--echo #
--connection con_TB1
--echo # Wait after releasing LOCK_open when opening table tb.
--echo # Set up a debug label to make the code simulate an error when
--echo # opening the table definition, hence making TB1 delete the share
--echo # from the hash table and destroy the share. Let TB1 signal at the
--echo # 'after_destroy' sync point to verify this behavior. Also make
--echo # sure we never end up at the 'found_share' sync point.
SET SESSION debug= '+d,set_open_table_err';
SET DEBUG_SYNC= 'get_share_before_open SIGNAL open_TB1 WAIT_FOR cont_TB1';
SET DEBUG_SYNC= 'get_share_after_destroy SIGNAL del_TB1';
SET DEBUG_SYNC= 'get_share_found_share HIT_LIMIT 1';
--send INSERT INTO tb VALUES(1)

--echo #
--connection con_TB2
--echo # Wait for open_TB1, then wait after the tb share is found in the
--echo # TDC. Wake up when TB1 broadcasts COND_open, this happens after
--echo # the share is destroyed by TB1. Then make sure TB2 gets to the
--echo # 'before_open' sync point, and ensure it does not get to neither
--echo # the 'after_destroy' nor 'found_share' sync points:
SET DEBUG_SYNC= 'now WAIT_FOR open_TB1';
SET DEBUG_SYNC= 'get_share_before_open SIGNAL open_TB2';
SET DEBUG_SYNC= 'get_share_after_destroy HIT_LIMIT 1';
SET DEBUG_SYNC= 'get_share_found_share HIT_LIMIT 1';
--send INSERT INTO tb VALUES(2)

--echo #
--connection default
--echo # Now, we know that TB1 is about to open the share for tb.
--echo # We also know that TB2 is about to wait for COND_open. 
--echo # First issue then is to make sure TB2 waits for COND_open
--echo # (using P_S.events_waits_current, not logged here):
LET $wait_condition=
  SELECT COUNT(*) = 1 FROM performance_schema.events_waits_current
    WHERE event_name LIKE '%COND_open';
--source include/wait_condition.inc

--echo #
--echo # Next up is to make TB1 continue and read the table definition.
--echo # Then, TB1 will "see" an error from open_table_def (by means of
--echo # debug instrumentation in the source code). Thus, we wait for TB1
--echo # to delete the share, and then we can let it finish:
SET DEBUG_SYNC= 'now SIGNAL cont_TB1';
SET DEBUG_SYNC= 'now WAIT_FOR del_TB1';

--echo #
--echo # Then, we wait for TB2 to signal that it's at the 'before_open'
--echo # sync point. This means it jumped out of the wait loop in the first
--echo # attempt since a second loop would make it do another cond_wait, 
--echo # and it also means it ended the loop because the share was NOT
--echo # found in the TDC anymore. Then, we're done:
SET DEBUG_SYNC= 'now WAIT_FOR open_TB2';

--echo #
--echo # Reap the connections, reset DEBUG_SYNC and drop tables:
--connection con_TB1
--error ER_NO_SUCH_TABLE
--reap
SET SESSION debug= '-d,set_open_table_err';
--connection con_TB2
--reap
--connection default
SET DEBUG_SYNC= 'RESET';
DROP TABLE tb;

--echo ###########################################################
--echo #
--echo # Test case 2.1: After 2), verify that if thread TB1 is
--echo # stopped before open, and TA1 is starting its flushing,
--echo # when TB1 continues, then TA1 will be able to complete.
--echo # This test case has only one thread accessing table tb.
--echo #

--echo #
--connection default
--echo # Create one table:
CREATE TABLE tb (pk integer primary key);

--echo #
--echo # Warm-up data-dictionary cache but keep Table Definition Cache intact.
--echo #
--disable_result_log
--disable_query_log
SHOW CREATE TABLE tb;
FLUSH TABLE tb;
--enable_query_log
--enable_result_log

--echo #
--connection con_TB1
--echo # Do an insert, wait after releasing LOCK_open for tb. Due to
--echo # a concurrent pending FLUSH TABLES, the first share will be
--echo # rejected due to wrong version number, and the share will be
--echo # retrieved once more. Also, we make sure we never end up
--echo # at the 'found_share' nor the 'after_destroy' sync point:
SET DEBUG_SYNC= 'get_share_before_open SIGNAL open_TB1 WAIT_FOR cont_TB1';
SET DEBUG_SYNC= 'get_share_after_destroy HIT_LIMIT 1';
SET DEBUG_SYNC= 'get_share_found_share HIT_LIMIT 1';
--send INSERT INTO tb VALUES(1)

--echo #
--connection con_TA1
--echo # Wait for TB1 to signal 'open_TB1', then issue a 'FLUSH TABLES'
--echo # command:
SET DEBUG_SYNC= 'now WAIT_FOR open_TB1';
--send FLUSH TABLES

--echo #
--connection default
--echo # Wait until the flush has started waiting for the share,
--echo # use I_S.processlist for this purpose (not logged here):
LET $wait_condition=
  SELECT COUNT(*) = 1 FROM information_schema.processlist
    WHERE state LIKE 'Waiting for table flush' AND info LIKE 'FLUSH TABLES';
--source include/wait_condition.inc

--echo #
--echo # Then we know TA1 is waiting for TB1 to finish. Next, we signal
--echo # TB1 to continue. This will make it retry getting the share:
SET DEBUG_SYNC= 'now SIGNAL cont_TB1';

--echo #
--echo # Reap the connections, reset DEBUG_SYNC and drop tables:
--connection con_TA1
--reap
--connection con_TB1
--reap
--connection default
SET DEBUG_SYNC= 'RESET';
DROP TABLE tb;

--echo ###########################################################
--echo #
--echo # Test case 2.2: After 2), verify that if thread TB1 is
--echo # stopped before open, and TA1 is starting its flushing,
--echo # when TB1 continues, then TA1 will be able to complete.
--echo # This test case has only one thread accessing table tb.
--echo # This case is similar to 2.1, but we simulate an error in
--echo # open_table_def. We also do 'FLUSH TABLES tb' to test
--echo # another variant of the FLUSH TABLES statement.
--echo # This test also covers bug #19881395 "FREEZE OF SERVER,
--echo # FLUSH TABLE VERSUS CONCURRENT DDL/DML".

--echo #
--connection default
--echo # Create one table:
CREATE TABLE tb (pk integer primary key);

--echo #
--echo # Warm-up data-dictionary cache but keep Table Definition Cache intact.
--echo #
--disable_result_log
--disable_query_log
SHOW CREATE TABLE tb;
FLUSH TABLE tb;
--enable_query_log
--enable_result_log

--echo #
--connection con_TB1
--echo # Do an insert, wait after releasing LOCK_open for tb. 
--echo # Simulate a failing open_table_def to verify that a
--echo # concurrent flush table operation handles this situation. Due
--echo # to a concurrent pending FLUSH TABLES, the first share will be
--echo # rejected due to wrong version number, but since it fails,
--echo # anyway, it will not be retrieved once more. Make sure we don't
--echo # end up more than once at the 'before_open' sync point. Also, make
--echo # sure we never end up at the 'found_share' sync point:
SET SESSION debug= '+d,set_open_table_err';
SET DEBUG_SYNC= 'get_share_before_open SIGNAL open_TB1 \
                         WAIT_FOR cont_TB1 HIT_LIMIT 2';
SET DEBUG_SYNC= 'get_share_after_destroy SIGNAL del_TB1 HIT_LIMIT 2';
SET DEBUG_SYNC= 'get_share_found_share HIT_LIMIT 1';
--send INSERT INTO tb VALUES(1)

--echo #
--connection con_TA1
--echo # Wait for TB1 to signal 'open_TB1', then issue a 'FLUSH TABLES'
--echo # command.
SET DEBUG_SYNC= 'now WAIT_FOR open_TB1';
--send FLUSH TABLES tb

--echo #
--connection default
--echo # Wait until the flush has started waiting for the share,
--echo # use I_S.processlist for this purpose (not logged here):
LET $wait_condition=
  SELECT COUNT(*) = 1 FROM information_schema.processlist
    WHERE state LIKE 'Waiting for table flush' AND info LIKE 'FLUSH TABLES tb';
--source include/wait_condition.inc

--echo #
--echo # Then we know TA1 is waiting for TB1 to finish. Next, we signal
--echo # TB1 to continue. It should be able to do so because "FLUSH TABLE
--echo # tb" is not supposed to hold any lock on table cache (as it did
--echo # before bug#19881395 was fixed). Since we simulate a failing open,
--echo # TB1 should end up signalling 'del_TB1'. We tell it to continue,
--echo # and then we're done:
SET DEBUG_SYNC= 'now SIGNAL cont_TB1';
SET DEBUG_SYNC= 'now WAIT_FOR del_TB1';

--echo #
--echo # Reap the connections, reset DEBUG_SYNC and drop tables:
--connection con_TA1
--reap
--connection con_TB1
--error ER_NO_SUCH_TABLE
--reap
SET SESSION debug= '-d,set_open_table_err';
--connection default
SET DEBUG_SYNC= 'RESET';
DROP TABLE tb;

--echo ###########################################################
--echo #
--echo # Test case 2.3: After 2), verify that if thread TB1 is
--echo # stopped before open, and TA1 is starting its flushing,
--echo # and TB2 has found a share and waits, then if we let
--echo # TB1 continue, then TA1 will be able to complete, and TB2
--echo # gets to open the table def because the "first" share is
--echo # flushed due to wrong version number. Also, verify that
--echo # TB1 finds the share to exist (since TB2 opened it) when it
--echo # retries. 
--echo #

--echo #
--connection default
--echo # Create one table:
CREATE TABLE tb (pk integer primary key);

--echo #
--echo # Warm-up data-dictionary cache but keep Table Definition Cache intact.
--echo #
--disable_result_log
--disable_query_log
SHOW CREATE TABLE tb;
FLUSH TABLE tb;
--enable_query_log
--enable_result_log

--echo #
--connection con_TB1
--echo # Do an insert, wait after releasing LOCK_open for tb. After
--echo # being signaled to continue, stop again before
--echo # retrying. After TB2 has opened the share, continue, and signal
--echo # when the share is found:
SET DEBUG_SYNC= 'get_share_before_open SIGNAL open_TB1
                                       WAIT_FOR cont_open_TB1 HIT_LIMIT 2';
SET DEBUG_SYNC= 'open_table_before_retry SIGNAL retry_TB1
                                         WAIT_FOR cont_retry_TB1';
SET DEBUG_SYNC= 'get_share_found_share SIGNAL found_TB1';
--send INSERT INTO tb VALUES(1)

--echo #
--connection con_TB2
--echo # Wait for TB1 to start opening, then do an insert, and wait for
--echo # COND_open after finding the share. After TB1 broadcasts COND_open,
--echo # the share will be missing, so TB2 will open it. Stop after opening
--echo # the share to make sure TB1 will also call get_table_share() when
--echo # retrying (to get predictable behavior):
SET DEBUG_SYNC= 'now WAIT_FOR open_TB1';
SET DEBUG_SYNC= 'get_share_found_share HIT_LIMIT 1';
SET DEBUG_SYNC= 'open_table_found_share SIGNAL found_TB2 WAIT_FOR finish_TB2';
--send INSERT INTO tb VALUES(2)

--echo #
--connection con_TA1
--echo # Issue a 'FLUSH TABLES' command:
--send FLUSH TABLES

--echo #
--connection default
--echo # Wait until the flush has started waiting for the share,
--echo # use I_S.processlist for this purpose (not logged here):
LET $wait_condition=
  SELECT COUNT(*) = 1 FROM information_schema.processlist
    WHERE state LIKE 'Waiting for table flush' AND info LIKE 'FLUSH TABLES';
--source include/wait_condition.inc

--echo #
--echo # Next issue then is to make sure TB2 waits for COND_open
--echo # (using P_S.events_waits_current, not logged here):
LET $wait_condition=
  SELECT COUNT(*) = 1 FROM performance_schema.events_waits_current
    WHERE event_name LIKE '%COND_open';
--source include/wait_condition.inc

--echo #
--echo # Then we know TA1 is waiting for the tb share, and we know TB2
--echo # is waiting for COND_open. Now, we signal TB1 to continue opening
--echo # the table:
SET DEBUG_SYNC= 'now SIGNAL cont_open_TB1';

--echo # 
--echo # Then we wait for the flush to complete (by an I_S wait condition,
--echo # not logged):
LET $wait_condition=
  SELECT COUNT(*) = 0 FROM information_schema.processlist
    WHERE state LIKE 'Waiting for table flush' AND info LIKE 'FLUSH TABLES';
--source include/wait_condition.inc

--echo #
--echo # When TB1 finished opening tb, we know that TB2 was signaled,
--echo # and since TB1 has eventually unlocked LOCK_open, TB2 will be opening
--echo # the table share (because the tb share is removed by TB1 since
--echo # it has too old version), but we must stop TB1 before retrying to
--echo # avoid the situation where TB1 manages to open tb before TB2
--echo # (to make sure the test is deterministic):
SET DEBUG_SYNC= 'now WAIT_FOR retry_TB1';
SET DEBUG_SYNC= 'now WAIT_FOR found_TB2';
SET DEBUG_SYNC= 'now SIGNAL cont_retry_TB1';

--echo # 
--echo # Now, we know that TB2 has opened the "new" version of the share,
--echo # and we know it's stopped at 'open_table_share_found'.
--echo # The only issue left then is to make sure TB1 drops by the
--echo # 'found_share' sync point, then signal TB2 to finish, and we're done:
SET DEBUG_SYNC= 'now SIGNAL finish_TB2';
SET DEBUG_SYNC= 'now WAIT_FOR found_TB1';
SET DEBUG_SYNC= 'now SIGNAL finish_TB2';

--echo #
--echo # Reap the connections, reset DEBUG_SYNC and drop tables:
--connection con_TA1
--reap
--connection con_TB1
--reap
--connection con_TB2
--reap
--connection con_TB1
SET DEBUG_SYNC= 'RESET';
--connection con_TB2
SET DEBUG_SYNC= 'RESET';
--connection default
SET DEBUG_SYNC= 'RESET';
DROP TABLE tb;

--echo ###########################################################
--echo #
--echo # Test case 3.1: Let thread TB1 issue LOCK TABLES tb
--echo # while thread TB2 issues CREATE TABLE tb. Do LOCK TABLES
--echo # first, and stop in get_share_before_open. Then run CREATE
--echo # TABLE, which does check_if_table_exists. The latter called
--echo # get_cached_table_share in the past, so could have been
--echo # confused by presence of open-in-progress share.
--echo #
--echo # If the CREATE mistakenly does conclude that tb
--echo # exists, it will bypass the MDL lock upgrade from S to X,
--echo # and instead go ahead and open the table. Then, it will
--echo # wait in get_table_share until TB1 is done, and finally,
--echo # TB2 will attempt to open_table_def (since TB1 failed and
--echo # destroyed the share). Thus, TB2 will fail while opening
--echo # too, since tb doesn't exist, making TB2 return the
--echo # error message "Table test.tb doesn't exist".
--echo #

--echo #
connection con_TB1;
--echo # Issue 'LOCK TABLES tb', and stop after allocating a share
--echo # for the table, before trying to actually open it:
SET DEBUG_SYNC= 'get_share_before_open SIGNAL open_TB1 WAIT_FOR cont_TB1';
--send LOCK TABLES tb WRITE;

--echo #
connection con_TB2;
--echo # Create the table which is being locked by TB1. The execution
--echo # will do check_if_table_exists() before create. This function
--echo # should not be confused by share being opened by TB1. It should
--echo # detect that able is absent and proceed to upgrading metadata
--echo # lock on the table to X.
SET DEBUG_SYNC= 'now WAIT_FOR open_TB1';
--send CREATE TABLE tb (pk integer primary key);

--echo #
connection default;
--echo # Wait until the CREATE TABLE execution is waiting for
--echo # metadata lock upgrade.
LET $wait_condition=
  SELECT COUNT(*) = 1 FROM information_schema.processlist
    WHERE state LIKE 'Waiting for table metadata lock'
      AND info LIKE 'CREATE TABLE tb%';
--source include/wait_condition.inc
SET DEBUG_SYNC='now SIGNAL cont_TB1';

--echo #
--echo # Reap the connections, reset DEBUG_SYNC and drop tables:
--connection con_TB1
--error ER_NO_SUCH_TABLE
--reap
--connection con_TB2
--reap
--connection default
SET DEBUG_SYNC= 'RESET';
DROP TABLE tb;

--echo ###########################################################
--echo #
--echo # Test case 4.1: After 4), issue a SHOW OPEN TABLES command
--echo # and verify that the table being opened is excluded from
--echo # the list of open tables.
--echo #

--echo #
--connection default
--echo # Create two tables:
CREATE TABLE ta (pk integer primary key);
CREATE TABLE tb (pk integer primary key);

--echo #
--echo # Warm-up data-dictionary cache but keep Table Definition Cache intact.
--echo #
--disable_result_log
--disable_query_log
SHOW CREATE TABLE tb;
FLUSH TABLE tb;
--enable_query_log
--enable_result_log

--echo #
--echo # Insert into ta to make sure it is open and in the cache:
INSERT INTO ta VALUES(0);

--echo #
--connection con_TB1
--echo # Wait after releasing LOCK_open for tb, and make sure we never
--echo # end up at the 'found_share' sync point:
SET DEBUG_SYNC= 'get_share_before_open SIGNAL open_TB1 WAIT_FOR cont_TB1';
SET DEBUG_SYNC= 'get_share_found_share HIT_LIMIT 1';
--send INSERT INTO tb VALUES(1)

--echo #
--connection default
--echo # Wait for open_TB1, then issue a SHOW OPEN TABLES command
--echo # where tb should not be included:
SET DEBUG_SYNC= 'now WAIT_FOR open_TB1';
SHOW OPEN TABLES FROM test;

--echo #
--echo # Next up is to let TB1 read the share, and do the insert:
SET DEBUG_SYNC= 'now SIGNAL cont_TB1';

--echo #
--echo # Reap connection TB1, and do another SHOW OPEN TABLES
--echo # where tb should now be included:
--connection con_TB1
--reap
--connection default
SHOW OPEN TABLES FROM test;

--echo #
--echo # Connection TB1 has already been reaped. Reset DEBUG_SYNC
--echo # and drop tables:
--connection default
SET DEBUG_SYNC= 'RESET';
DROP TABLE ta, tb;

--echo ###########################################################
--echo #
--echo # Test teardown: Disconnect
--echo #

--connection con_TA1
--disconnect con_TA1
--source include/wait_until_disconnected.inc

--connection con_TB1
--disconnect con_TB1
--source include/wait_until_disconnected.inc

--connection con_TB2
--disconnect con_TB2
--source include/wait_until_disconnected.inc

--connection default
--disable_connect_log
