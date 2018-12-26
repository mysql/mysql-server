###########################################################
#
# Test cases for wl#7593: Don't hold LOCK_open... and
# bug #19881395 "FREEZE OF SERVER, FLUSH TABLE VERSUS
# CONCURRENT DDL/DML".
#
# 1) The first basic scenario is based on concurrency and
# sequencing of three threads: Let thread TA1 open table ta,
# while thread TB1 and TB2 open table tb. Now, we have various
# possible situations that are considered in the three first
# test cases below, which are all based on this point of
# departure, referred to as (1) below.
#
# 2) The second basic scenario is based on three threads: Let
# thread TB1 and TB2 open table tb, while thread TA1 issues a
# FLUSH TABLES in order to flush the cache while a share is
# being initialized. The three last test cases are variants of
# this scenario, referred to as (2) below. One of them covers
# scenario in which bug #19881395 has occurred.
#
# 3) The third scenario is based on two threads: One thread
# doing CREATE TABLE while another issues LOCK TABLE on the
# same table. There is one test case based on this scenario.
#
# 4) The fourth scenario is based on two threads: One thread
# TB1 opening table tb, being paused while opening the share,
# while another thread TB2 issues the SQL command SHOW OPEN
# TABLES. Then, we verify that the table being opened is excluded
# from the list of open tables. A related test case is relevant
# in the context of the federated storage engine, and is located
# in suite/federated/federated_get_table_share.test.
#
###########################################################
#
# Test setup: Create three reusable connections:
#
connect  con_TA1, localhost, root;
connect  con_TB1, localhost, root;
connect  con_TB2, localhost, root;
###########################################################
#
# Test case 1.1: After 1), verify that if thread TA1
# broadcasts COND_open first, thread TB2 will wake up,
# re-fetch its share and see that m_open_in_progress is
# still true, and then continue waiting for COND_open.
#
#
connection default;
# Create two tables:
CREATE TABLE ta (pk integer primary key);
CREATE TABLE tb (pk integer primary key);
#
# Warm-up data-dictionary cache but keep Table Definition Cache intact.
#
#
connection con_TA1;
# Wait after releasing LOCK_open for ta, and make sure we never
# end up at the 'found_share' sync point:
SET DEBUG_SYNC= 'get_share_before_open SIGNAL open_TA1 WAIT_FOR cont_TA1';
SET DEBUG_SYNC= 'get_share_found_share HIT_LIMIT 1';
INSERT INTO ta VALUES(1);
#
connection con_TB1;
# Wait for open_TA1, then wait after releasing LOCK_open for tb,
# also make sure we never end up at the 'found_share' sync point:
SET DEBUG_SYNC= 'now WAIT_FOR open_TA1';
SET DEBUG_SYNC= 'get_share_before_open SIGNAL open_TB1 WAIT_FOR cont_TB1';
SET DEBUG_SYNC= 'get_share_found_share HIT_LIMIT 1';
INSERT INTO tb VALUES(1);
#
connection con_TB2;
# Wait for open_TB1, then wait after the tb share is found in the
# TDC. Wake up when TA1 broadcasts COND_open, then go back to wait
# since a different share (ta) was opened. Finally stop at the
# 'found_share' sync point to verify that the share being addressed
# is now available. Also make sure we never end up at the
# 'before_open' sync point:
SET DEBUG_SYNC= 'now WAIT_FOR open_TB1';
SET DEBUG_SYNC= 'get_share_found_share SIGNAL found_TB2';
SET DEBUG_SYNC= 'get_share_before_open HIT_LIMIT 1';
INSERT INTO tb VALUES(2);
#
connection default;
# Now, we know that TA1 and TB1 are about to open the shares
# for ta and tb concurrently. We also know that TB2 is about to
# wait for COND_open. First issue then is to make sure TB2 waits
# for COND_open (using P_S.events_waits_current, not logged here):
#
# Then we save the event id for later:
SET @first_wait_id= 0;
SELECT event_id FROM performance_schema.events_waits_current
WHERE event_name LIKE '%COND_open' INTO @first_wait_id;
#
# Next up is to make one of the opening threads read the definition.
# Here, we let TA1 read first:
SET DEBUG_SYNC= 'now SIGNAL cont_TA1';
#
# Then, we make sure TB2 leaves the wait for COND_open, and then
# waits for it once more. Verify this by waiting for the event_id
# to change:
#
# Then, we signal TB1 to make it open its def and do its things,
# this will also wake up TB2 (now waiting on the COND_open):
SET DEBUG_SYNC= 'now SIGNAL cont_TB1';
#
# And at last, we wait for TB2 to signal that it found its share:
SET DEBUG_SYNC= 'now WAIT_FOR found_TB2';
#
# Reap the connections, reset DEBUG_SYNC and drop tables:
connection con_TA1;
connection con_TB1;
connection con_TB2;
connection default;
SET DEBUG_SYNC= 'RESET';
DROP TABLE ta, tb;
###########################################################
#
# Test case 1.2: After 1), verify that if thread TB1
# broadcasts COND_open first, thread TB2 will wake up,
# re-fetch its share and see that m_open_in_progress is
# false, and then continue under the assumption that the
# expected share is found.
#
#
connection default;
# Create two tables:
CREATE TABLE ta (pk integer primary key);
CREATE TABLE tb (pk integer primary key);
#
# Warm-up data-dictionary cache but keep Table Definition Cache intact.
#
#
connection con_TA1;
# Wait after releasing LOCK_open for ta, and make sure we never
# end up at the 'found_share' sync point:
SET DEBUG_SYNC= 'get_share_before_open SIGNAL open_TA1 WAIT_FOR cont_TA1';
SET DEBUG_SYNC= 'get_share_found_share HIT_LIMIT 1';
INSERT INTO ta VALUES(1);
#
connection con_TB1;
# Wait for open_TA1, then wait after releasing LOCK_open for tb,
# also make sure we never end up at the 'found_share' sync point:
SET DEBUG_SYNC= 'now WAIT_FOR open_TA1';
SET DEBUG_SYNC= 'get_share_before_open SIGNAL open_TB1 WAIT_FOR cont_TB1';
SET DEBUG_SYNC= 'get_share_found_share HIT_LIMIT 1';
INSERT INTO tb VALUES(1);
#
connection con_TB2;
# Wait for open_TB1, then wait after the tb share is found in the
# TDC. Wake up when TB1 broadcasts COND_open, jump to 'found' since
# the awaited share (tb) was opened by TB1 before TA1 opened ta.
# Signal when at the 'found_share' sync point so we can verify that the
# thread is at the expected point. Also make sure we never end up
# at the 'before_open' sync point:
SET DEBUG_SYNC= 'now WAIT_FOR open_TB1';
SET DEBUG_SYNC= 'get_share_found_share SIGNAL found_TB2';
SET DEBUG_SYNC= 'get_share_before_open HIT_LIMIT 1';
INSERT INTO tb VALUES(2);
#
connection default;
# Now, we know that TA1 and TB1 are about to open the shares
# for ta and tb concurrently. We also know that TB2 is about to
# wait for COND_open. First issue then is to make sure TB2 waits
# for COND_open (using P_S.events_waits_current, not logged here):
#
# Next up is to make one of the opening threads read the def.
# Here, as opposed to the previous test case, we let TB1 read first:
SET DEBUG_SYNC= 'now SIGNAL cont_TB1';
#
# Then, we wait for TB2 to signal that it's at the 'found_share'
# sync point. This means it jumped out of the wait loop in the first
# attempt since a second loop would make it do cond_wait once more:
SET DEBUG_SYNC= 'now WAIT_FOR found_TB2';
#
# Then, we signal TA1 to make it open its def and do its things,
# and then we're done:
SET DEBUG_SYNC= 'now SIGNAL cont_TA1';
#
# Reap the connections, reset DEBUG_SYNC and drop tables:
connection con_TA1;
connection con_TB1;
connection con_TB2;
connection default;
SET DEBUG_SYNC= 'RESET';
DROP TABLE ta, tb;
###########################################################
#
# Test case 1.3: After 1), verify that if there is an error
# in open_table_def for TB1, the share is deleted from the
# hash table and destroyed. Then, verify that TB2 wakes up,
# discovers that the share is now missing, and then
# continues as if the share never existed in the hash table
# in the first place. This test case does not use connection
# TA1. 
#
#
connection default;
# Create one table:
CREATE TABLE tb (pk integer primary key);
#
# Warm-up data-dictionary cache but keep Table Definition Cache intact.
#
#
connection con_TB1;
# Wait after releasing LOCK_open when opening table tb.
# Set up a debug label to make the code simulate an error when
# opening the table definition, hence making TB1 delete the share
# from the hash table and destroy the share. Let TB1 signal at the
# 'after_destroy' sync point to verify this behavior. Also make
# sure we never end up at the 'found_share' sync point.
SET SESSION debug= '+d,set_open_table_err';
SET DEBUG_SYNC= 'get_share_before_open SIGNAL open_TB1 WAIT_FOR cont_TB1';
SET DEBUG_SYNC= 'get_share_after_destroy SIGNAL del_TB1';
SET DEBUG_SYNC= 'get_share_found_share HIT_LIMIT 1';
INSERT INTO tb VALUES(1);
#
connection con_TB2;
# Wait for open_TB1, then wait after the tb share is found in the
# TDC. Wake up when TB1 broadcasts COND_open, this happens after
# the share is destroyed by TB1. Then make sure TB2 gets to the
# 'before_open' sync point, and ensure it does not get to neither
# the 'after_destroy' nor 'found_share' sync points:
SET DEBUG_SYNC= 'now WAIT_FOR open_TB1';
SET DEBUG_SYNC= 'get_share_before_open SIGNAL open_TB2';
SET DEBUG_SYNC= 'get_share_after_destroy HIT_LIMIT 1';
SET DEBUG_SYNC= 'get_share_found_share HIT_LIMIT 1';
INSERT INTO tb VALUES(2);
#
connection default;
# Now, we know that TB1 is about to open the share for tb.
# We also know that TB2 is about to wait for COND_open. 
# First issue then is to make sure TB2 waits for COND_open
# (using P_S.events_waits_current, not logged here):
#
# Next up is to make TB1 continue and read the table definition.
# Then, TB1 will "see" an error from open_table_def (by means of
# debug instrumentation in the source code). Thus, we wait for TB1
# to delete the share, and then we can let it finish:
SET DEBUG_SYNC= 'now SIGNAL cont_TB1';
SET DEBUG_SYNC= 'now WAIT_FOR del_TB1';
#
# Then, we wait for TB2 to signal that it's at the 'before_open'
# sync point. This means it jumped out of the wait loop in the first
# attempt since a second loop would make it do another cond_wait, 
# and it also means it ended the loop because the share was NOT
# found in the TDC anymore. Then, we're done:
SET DEBUG_SYNC= 'now WAIT_FOR open_TB2';
#
# Reap the connections, reset DEBUG_SYNC and drop tables:
connection con_TB1;
ERROR 42S02: Table 'test.tb' doesn't exist
SET SESSION debug= '-d,set_open_table_err';
connection con_TB2;
connection default;
SET DEBUG_SYNC= 'RESET';
DROP TABLE tb;
###########################################################
#
# Test case 2.1: After 2), verify that if thread TB1 is
# stopped before open, and TA1 is starting its flushing,
# when TB1 continues, then TA1 will be able to complete.
# This test case has only one thread accessing table tb.
#
#
connection default;
# Create one table:
CREATE TABLE tb (pk integer primary key);
#
# Warm-up data-dictionary cache but keep Table Definition Cache intact.
#
#
connection con_TB1;
# Do an insert, wait after releasing LOCK_open for tb. Due to
# a concurrent pending FLUSH TABLES, the first share will be
# rejected due to wrong version number, and the share will be
# retrieved once more. Also, we make sure we never end up
# at the 'found_share' nor the 'after_destroy' sync point:
SET DEBUG_SYNC= 'get_share_before_open SIGNAL open_TB1 WAIT_FOR cont_TB1';
SET DEBUG_SYNC= 'get_share_after_destroy HIT_LIMIT 1';
SET DEBUG_SYNC= 'get_share_found_share HIT_LIMIT 1';
INSERT INTO tb VALUES(1);
#
connection con_TA1;
# Wait for TB1 to signal 'open_TB1', then issue a 'FLUSH TABLES'
# command:
SET DEBUG_SYNC= 'now WAIT_FOR open_TB1';
FLUSH TABLES;
#
connection default;
# Wait until the flush has started waiting for the share,
# use I_S.processlist for this purpose (not logged here):
#
# Then we know TA1 is waiting for TB1 to finish. Next, we signal
# TB1 to continue. This will make it retry getting the share:
SET DEBUG_SYNC= 'now SIGNAL cont_TB1';
#
# Reap the connections, reset DEBUG_SYNC and drop tables:
connection con_TA1;
connection con_TB1;
connection default;
SET DEBUG_SYNC= 'RESET';
DROP TABLE tb;
###########################################################
#
# Test case 2.2: After 2), verify that if thread TB1 is
# stopped before open, and TA1 is starting its flushing,
# when TB1 continues, then TA1 will be able to complete.
# This test case has only one thread accessing table tb.
# This case is similar to 2.1, but we simulate an error in
# open_table_def. We also do 'FLUSH TABLES tb' to test
# another variant of the FLUSH TABLES statement.
# This test also covers bug #19881395 "FREEZE OF SERVER,
# FLUSH TABLE VERSUS CONCURRENT DDL/DML".
#
connection default;
# Create one table:
CREATE TABLE tb (pk integer primary key);
#
# Warm-up data-dictionary cache but keep Table Definition Cache intact.
#
#
connection con_TB1;
# Do an insert, wait after releasing LOCK_open for tb. 
# Simulate a failing open_table_def to verify that a
# concurrent flush table operation handles this situation. Due
# to a concurrent pending FLUSH TABLES, the first share will be
# rejected due to wrong version number, but since it fails,
# anyway, it will not be retrieved once more. Make sure we don't
# end up more than once at the 'before_open' sync point. Also, make
# sure we never end up at the 'found_share' sync point:
SET SESSION debug= '+d,set_open_table_err';
SET DEBUG_SYNC= 'get_share_before_open SIGNAL open_TB1 \
                         WAIT_FOR cont_TB1 HIT_LIMIT 2';
SET DEBUG_SYNC= 'get_share_after_destroy SIGNAL del_TB1 HIT_LIMIT 2';
SET DEBUG_SYNC= 'get_share_found_share HIT_LIMIT 1';
INSERT INTO tb VALUES(1);
#
connection con_TA1;
# Wait for TB1 to signal 'open_TB1', then issue a 'FLUSH TABLES'
# command.
SET DEBUG_SYNC= 'now WAIT_FOR open_TB1';
FLUSH TABLES tb;
#
connection default;
# Wait until the flush has started waiting for the share,
# use I_S.processlist for this purpose (not logged here):
#
# Then we know TA1 is waiting for TB1 to finish. Next, we signal
# TB1 to continue. It should be able to do so because "FLUSH TABLE
# tb" is not supposed to hold any lock on table cache (as it did
# before bug#19881395 was fixed). Since we simulate a failing open,
# TB1 should end up signalling 'del_TB1'. We tell it to continue,
# and then we're done:
SET DEBUG_SYNC= 'now SIGNAL cont_TB1';
SET DEBUG_SYNC= 'now WAIT_FOR del_TB1';
#
# Reap the connections, reset DEBUG_SYNC and drop tables:
connection con_TA1;
connection con_TB1;
ERROR 42S02: Table 'test.tb' doesn't exist
SET SESSION debug= '-d,set_open_table_err';
connection default;
SET DEBUG_SYNC= 'RESET';
DROP TABLE tb;
###########################################################
#
# Test case 2.3: After 2), verify that if thread TB1 is
# stopped before open, and TA1 is starting its flushing,
# and TB2 has found a share and waits, then if we let
# TB1 continue, then TA1 will be able to complete, and TB2
# gets to open the table def because the "first" share is
# flushed due to wrong version number. Also, verify that
# TB1 finds the share to exist (since TB2 opened it) when it
# retries. 
#
#
connection default;
# Create one table:
CREATE TABLE tb (pk integer primary key);
#
# Warm-up data-dictionary cache but keep Table Definition Cache intact.
#
#
connection con_TB1;
# Do an insert, wait after releasing LOCK_open for tb. After
# being signaled to continue, stop again before
# retrying. After TB2 has opened the share, continue, and signal
# when the share is found:
SET DEBUG_SYNC= 'get_share_before_open SIGNAL open_TB1
                                       WAIT_FOR cont_open_TB1 HIT_LIMIT 2';
SET DEBUG_SYNC= 'open_table_before_retry SIGNAL retry_TB1
                                         WAIT_FOR cont_retry_TB1';
SET DEBUG_SYNC= 'get_share_found_share SIGNAL found_TB1';
INSERT INTO tb VALUES(1);
#
connection con_TB2;
# Wait for TB1 to start opening, then do an insert, and wait for
# COND_open after finding the share. After TB1 broadcasts COND_open,
# the share will be missing, so TB2 will open it. Stop after opening
# the share to make sure TB1 will also call get_table_share() when
# retrying (to get predictable behavior):
SET DEBUG_SYNC= 'now WAIT_FOR open_TB1';
SET DEBUG_SYNC= 'get_share_found_share HIT_LIMIT 1';
SET DEBUG_SYNC= 'open_table_found_share SIGNAL found_TB2 WAIT_FOR finish_TB2';
INSERT INTO tb VALUES(2);
#
connection con_TA1;
# Issue a 'FLUSH TABLES' command:
FLUSH TABLES;
#
connection default;
# Wait until the flush has started waiting for the share,
# use I_S.processlist for this purpose (not logged here):
#
# Next issue then is to make sure TB2 waits for COND_open
# (using P_S.events_waits_current, not logged here):
#
# Then we know TA1 is waiting for the tb share, and we know TB2
# is waiting for COND_open. Now, we signal TB1 to continue opening
# the table:
SET DEBUG_SYNC= 'now SIGNAL cont_open_TB1';
# 
# Then we wait for the flush to complete (by an I_S wait condition,
# not logged):
#
# When TB1 finished opening tb, we know that TB2 was signaled,
# and since TB1 has eventually unlocked LOCK_open, TB2 will be opening
# the table share (because the tb share is removed by TB1 since
# it has too old version), but we must stop TB1 before retrying to
# avoid the situation where TB1 manages to open tb before TB2
# (to make sure the test is deterministic):
SET DEBUG_SYNC= 'now WAIT_FOR retry_TB1';
SET DEBUG_SYNC= 'now WAIT_FOR found_TB2';
SET DEBUG_SYNC= 'now SIGNAL cont_retry_TB1';
# 
# Now, we know that TB2 has opened the "new" version of the share,
# and we know it's stopped at 'open_table_share_found'.
# The only issue left then is to make sure TB1 drops by the
# 'found_share' sync point, then signal TB2 to finish, and we're done:
SET DEBUG_SYNC= 'now SIGNAL finish_TB2';
SET DEBUG_SYNC= 'now WAIT_FOR found_TB1';
SET DEBUG_SYNC= 'now SIGNAL finish_TB2';
#
# Reap the connections, reset DEBUG_SYNC and drop tables:
connection con_TA1;
connection con_TB1;
connection con_TB2;
connection con_TB1;
SET DEBUG_SYNC= 'RESET';
connection con_TB2;
SET DEBUG_SYNC= 'RESET';
connection default;
SET DEBUG_SYNC= 'RESET';
DROP TABLE tb;
###########################################################
#
# Test case 3.1: Let thread TB1 issue LOCK TABLES tb
# while thread TB2 issues CREATE TABLE tb. Do LOCK TABLES
# first, and stop in get_share_before_open. Then run CREATE
# TABLE, which does check_if_table_exists. The latter called
# get_cached_table_share in the past, so could have been
# confused by presence of open-in-progress share.
#
# If the CREATE mistakenly does conclude that tb
# exists, it will bypass the MDL lock upgrade from S to X,
# and instead go ahead and open the table. Then, it will
# wait in get_table_share until TB1 is done, and finally,
# TB2 will attempt to open_table_def (since TB1 failed and
# destroyed the share). Thus, TB2 will fail while opening
# too, since tb doesn't exist, making TB2 return the
# error message "Table test.tb doesn't exist".
#
#
connection con_TB1;
# Issue 'LOCK TABLES tb', and stop after allocating a share
# for the table, before trying to actually open it:
SET DEBUG_SYNC= 'get_share_before_open SIGNAL open_TB1 WAIT_FOR cont_TB1';
LOCK TABLES tb WRITE;;
#
connection con_TB2;
# Create the table which is being locked by TB1. The execution
# will do check_if_table_exists() before create. This function
# should not be confused by share being opened by TB1. It should
# detect that able is absent and proceed to upgrading metadata
# lock on the table to X.
SET DEBUG_SYNC= 'now WAIT_FOR open_TB1';
CREATE TABLE tb (pk integer primary key);;
#
connection default;
# Wait until the CREATE TABLE execution is waiting for
# metadata lock upgrade.
SET DEBUG_SYNC='now SIGNAL cont_TB1';
#
# Reap the connections, reset DEBUG_SYNC and drop tables:
connection con_TB1;
ERROR 42S02: Table 'test.tb' doesn't exist
connection con_TB2;
connection default;
SET DEBUG_SYNC= 'RESET';
DROP TABLE tb;
###########################################################
#
# Test case 4.1: After 4), issue a SHOW OPEN TABLES command
# and verify that the table being opened is excluded from
# the list of open tables.
#
#
connection default;
# Create two tables:
CREATE TABLE ta (pk integer primary key);
CREATE TABLE tb (pk integer primary key);
#
# Warm-up data-dictionary cache but keep Table Definition Cache intact.
#
#
# Insert into ta to make sure it is open and in the cache:
INSERT INTO ta VALUES(0);
#
connection con_TB1;
# Wait after releasing LOCK_open for tb, and make sure we never
# end up at the 'found_share' sync point:
SET DEBUG_SYNC= 'get_share_before_open SIGNAL open_TB1 WAIT_FOR cont_TB1';
SET DEBUG_SYNC= 'get_share_found_share HIT_LIMIT 1';
INSERT INTO tb VALUES(1);
#
connection default;
# Wait for open_TB1, then issue a SHOW OPEN TABLES command
# where tb should not be included:
SET DEBUG_SYNC= 'now WAIT_FOR open_TB1';
SHOW OPEN TABLES FROM test;
Database	Table	In_use	Name_locked
test	ta	0	0
#
# Next up is to let TB1 read the share, and do the insert:
SET DEBUG_SYNC= 'now SIGNAL cont_TB1';
#
# Reap connection TB1, and do another SHOW OPEN TABLES
# where tb should now be included:
connection con_TB1;
connection default;
SHOW OPEN TABLES FROM test;
Database	Table	In_use	Name_locked
test	tb	0	0
test	ta	0	0
#
# Connection TB1 has already been reaped. Reset DEBUG_SYNC
# and drop tables:
connection default;
SET DEBUG_SYNC= 'RESET';
DROP TABLE ta, tb;
###########################################################
#
# Test teardown: Disconnect
#
connection con_TA1;
disconnect con_TA1;
connection con_TB1;
disconnect con_TB1;
connection con_TB2;
disconnect con_TB2;
connection default;
