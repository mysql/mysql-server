--source include/force_myisam_default.inc
--source include/have_myisam.inc
# Skipping the test temporarily when log-bin is enabled as the test contains a
# statement accessing mysql.general_log in an trxn. The trxn status tracker
# (@@session_track_transaction_info) marks it unsafe only when logbin is on
--source include/not_log_bin.inc

--echo #
--echo # WL#6631: Detect transaction boundaries
--echo #

###############################################################################
# The main functionality implemented by WL#6631 is enhanced reporting
# on transaction state. Historically, the server has already reported
# with a flag whether we're inside a transaction, but on one hand,
# BEGIN..COMMIT AND CHAIN..COMMIT AND CHAIN..COMMIT AND RELEASE would
# look like a single very long transaction to users of that flag; on
# the other, we could still re-locate the session elsewhere (e.g. load-
# balance it) as long as no actual reads or writes have been done.
# A client subscribing to WL#6631 reporting will see this more granular
# state (implicit transaction, explicit transaction, work attached, etc.).
# it may additionally subscribe to reporting on the characteristics of
# the transaction (READ ONLY/READ WRITE; WITH CONSISTENT SNAPSHOT;
# ISOLATION LEVEL) so it may restart the transaction elsewhere with the
# same characteristics as the original transaction.
#
# We can switch a connection:
#
# a) if no transaction is active
#
# b) if a  transaction is active, but has no "work" attached to it yet,
#    in which case we'll want to know its characteristics to move it:
#
#    - was START TRANSACTION "WITH CONSISTENT SNAPSHOT" used?
#
#    - was START TRANSACTION used with "READ ONLY" or "READ WRITE"?
#      (if neither was given in the statement, we won't flag either,
#      so the default will be used -- it's up to the client to
#      replicate that setting from SET TRANSACTION (i.e. GLOBAL and
#      SESSION transaction_isolation / transaction_read_only) as needed!
#
#    - was "SET TRANSACTION ISOLATION LEVEL" one-shot set for this
#      transaction?
#
#    - was "SET TRANSACTION READ [WRITE|ONLY]" one-shot used?
#
#
# A transaction may be "explicit" (started with BEGIN WORK /
# START TRANSACTION) or "implicit" (autocommit==0 && not in an
# explicit transaction). A transaction of either type will end
# when a statement causes an implicit or explicit commit.
# In both cases, we'll see the union of any reads or writes
# (transactional and non-transactional) that happened up to
# that point in the transaction.
#
# In this test, we will document various state transitions between
# no transaction, implicit transaction, and explict transaction active.
# We will also show that "work attached" (read/write, transactional/
# non-transactional) as flagged as expected when a transaction is active.
# Next, we will show that CHARACTERISTICS tracking supplies the correct
# SQL statement or sequence of SQL statements to start a new transaction
# with characteristics identital to that of the on-going transaction.
# Finally, we'll explore some interesting situations -- reads within
# a stored function, within LOCK, etc.



--echo ########################################################################
--echo #
--echo # set up: save settings
--echo #

SET autocommit=1;
--echo # if we track CHARACTERISTICS, we must also track the tx_* sysvars!
SELECT @@session.session_track_system_variables INTO @old_track_list;
SET @track_list= CONCAT(@old_track_list, ",transaction_isolation,
                                           transaction_read_only");
SET SESSION session_track_system_variables=@track_list;

SELECT @@session.session_track_state_change INTO @old_track_enable;
SET SESSION session_track_state_change=TRUE;

SELECT @@session.session_track_transaction_info INTO @old_track_tx;

FLUSH STATUS;

--echo ########################################################################
--echo #
--echo # test "STATE" tracking: transaction state reporting
--echo #
CREATE TABLE t1 (f1 INT) ENGINE="InnoDB";
CREATE TABLE t2 (f1 INT) ENGINE="MyISAM";
--echo # We don't report when in autocommit mode and outside a transaction:
INSERT INTO t1 VALUES(1);
SELECT 1 FROM DUAL;
DELETE FROM t1;
# Now start a transaction so we can actually report things!
START TRANSACTION;
--echo # add "unsafe stmt" to the set:
SET @x=UUID();
--echo # next stmt is safe, but unsafe flag should stick:
SET @x=1;
--echo # however, after C/A/C, the status should revert to "empty trx":
COMMIT AND CHAIN;
--echo # SELECT with no tables gives us just a result set
SELECT 1 FROM DUAL;
COMMIT AND CHAIN;
--echo # SELECT with no tables and no result set
--replace_result $MYSQLTEST_VARDIR VARDIR
eval SELECT 1 FROM DUAL INTO OUTFILE '$MYSQLTEST_VARDIR/tmp/wl6631.csv';
--remove_file $MYSQLTEST_VARDIR/tmp/wl6631.csv
COMMIT AND CHAIN;
--echo # SELECT with trx tables but no result set
--replace_result $MYSQLTEST_VARDIR VARDIR
--eval SELECT f1 FROM t1 INTO OUTFILE '$MYSQLTEST_VARDIR/tmp/wl6631.csv';
--remove_file $MYSQLTEST_VARDIR/tmp/wl6631.csv
COMMIT AND CHAIN;
--echo # SELECT with non-trx tables but no result set
--replace_result $MYSQLTEST_VARDIR VARDIR
--eval SELECT f1 FROM t2 INTO OUTFILE '$MYSQLTEST_VARDIR/tmp/wl6631.csv';
--remove_file $MYSQLTEST_VARDIR/tmp/wl6631.csv
--echo # show that "unsafe read" sticks (isn't cleared by the dummy SELECT)
SELECT 1 FROM DUAL INTO @x;
--echo # clear state
COMMIT;
DROP TABLE t1;
DROP TABLE t2;
 --echo


--echo # states: read + write; transactional + non-transactional
--echo # state should be "no transaction":
CREATE TABLE t1 (f1 INT) ENGINE="InnoDB";
CREATE TABLE t2 (f1 INT) ENGINE="MyISAM";
--echo # resulting state should be "T", transaction started, no data modified:
START TRANSACTION;
--echo # resulting state should be "W", transactional write pending:
INSERT INTO t1 VALUES (1);
--echo # resulting state should be "wW", both safe and unsafe writes happened:
INSERT INTO t2 VALUES (1);
--echo # resulting state should STILL be "wW"!
INSERT INTO t1 VALUES (1);
ROLLBACK;
START TRANSACTION;
--echo # resulting state should be "w", unsafe non-transaction write happened:
INSERT INTO t2 VALUES (1);
--echo # resulting state should be "wW", both safe and unsafe writes happened:
INSERT INTO t1 VALUES (1);
--echo # resulting state should be  "RwW" (adding transactional read):
SELECT f1 FROM t1;
--echo # resulting state should be "rRwW" (adding non-transactional read):
SELECT f1 FROM t2;
	
# ROLLBACK will throw "couldn't roll back some tables" here.
# Prevent an implicit, hidden "SHOW WARNINGS" here that would
# lead to an extra result set, and thereby to a hidden state edge
# (and a seemingly nonsensical logged change from TX_EMPTY to TX_EMPTY).
ROLLBACK;
--enable_warnings
DROP TABLE t1, t2;
--echo

--echo # autocommit (0)
CREATE TABLE t1 (f1 INT) ENGINE="InnoDB";
SET autocommit=0;
# unsafe stmt
	
SET @x=UUID();
SET @x=1;
SET @x=UUID();
--echo # SET TRANSACTION one-shot should generate a tracker item when
--echo # tracking statements. Transaction state however should not change.
--echo # We can still set chistics here because in_active_multi_stmt_transaction()
--echo # is still false (!SERVER_STATUS_IN_TRANS).
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
INSERT INTO t1 VALUES(1);
--echo # Now that we've involved tables in the implicit transaction, we're
--echo # no longer allowed to change its chistics:
--error ER_CANT_CHANGE_TX_CHARACTERISTICS
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
# implicit commit (because of DROP TABLE)
BEGIN;
INSERT INTO t1 VALUES(3);
DROP TABLE t1;
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
BEGIN;
SELECT 1 FROM DUAL;
SELECT 1 FROM DUAL INTO @dummy;
COMMIT;
CREATE TABLE t2 (f1 INT) ENGINE="MyISAM";
SELECT f1 FROM t2;
DROP TABLE t2;
SET autocommit=1;
--echo

	
--echo ########################################################################
--echo #
--echo # show that table access reporting works in multi-statements
--echo #
--echo # multi-stmt #1
CREATE TABLE t2 (f1 INT) ENGINE="MyISAM";
BEGIN;

	
# COMMIT clears state, table write is not recorded as outside transaction,
# then transaction start is recorded:
delimiter |;
COMMIT; INSERT INTO t2 VALUES (1); BEGIN; |
delimiter ;|
COMMIT;
DROP TABLE t2;
--echo # multi-stmt #2
CREATE TABLE t1 (f1 INT) ENGINE="InnoDB";
CREATE TABLE t2 (f1 INT) ENGINE="MyISAM";
BEGIN;


# COMMIT clears state, table write is not recorded as outside transaction,
# then transaction start is recorded, finally a write is added:
delimiter |;
COMMIT; INSERT INTO t2 VALUES (1); BEGIN; INSERT INTO t1 VALUES (99); |
delimiter ;|
COMMIT;
DROP TABLE t1;
DROP TABLE t2;
--echo


--echo ########################################################################
--echo #
--echo # show that reporting works for Stored Procedures (SP)
--echo #
CREATE TABLE t1 (f1 INT) ENGINE="InnoDB";
CREATE TABLE t2 (f1 INT) ENGINE="MyISAM";
INSERT INTO t1 VALUES (1);
INSERT INTO t2 VALUES (2);
DELIMITER |;
CREATE PROCEDURE proc1()
BEGIN
  SET @dummy = 0;
  IF (SELECT f1 FROM t1) THEN
    SET @dummy = 1;
  END IF;
END|
CREATE PROCEDURE proc2()
BEGIN
  CALL proc1();
  UPDATE t1 SET f1=4;
END|
CREATE PROCEDURE proc3()
BEGIN
  DECLARE x CHAR(36);
  SET x=UUID();
END|
CREATE PROCEDURE proc4(x CHAR(36))
BEGIN
END|
CREATE PROCEDURE proc5()
BEGIN
  SELECT f1 FROM t1;
  SELECT f1 FROM t2;
END|
CREATE PROCEDURE proc6a()
BEGIN
  IF (SELECT f1 FROM t1) THEN
    SET @dummy = 1;
  END IF;
  ALTER TABLE t1 ADD COLUMN f2 INT;
  IF (SELECT f1 FROM t2) THEN
    SET @dummy = 1;
  END IF;
END|
CREATE PROCEDURE proc6b()
BEGIN
  SELECT f1 FROM t1;
  ALTER TABLE t1 ADD COLUMN f3 INT;
  SELECT f1 FROM t2;
END|
CREATE PROCEDURE proc7(x INT)
BEGIN
  SELECT f1   FROM t1;
  SELECT f1*2 FROM t1;
END|
CREATE PROCEDURE proc8(x INT)
BEGIN
  SELECT f1   FROM t1;
  IF (SELECT f1 FROM t2) THEN
    SET @dummy = 1;
  END IF;
END|
CREATE PROCEDURE proc9(x INT)
BEGIN
  SELECT f1   FROM t1;
  IF (SELECT f1 FROM t1) THEN
    SET @dummy = 1;
  END IF;
END|
DELIMITER ;|
BEGIN;
--echo # example unsafe call:
CALL proc3();
--echo # report on tables accessed within SP:
CALL proc1();
--echo # report on tables accessed within all (nested) SP:
CALL proc2();
COMMIT;
BEGIN;
CALL proc4(UUID());
COMMIT;
--echo # multiple result sets:
--echo # autocommit=1, not in a transaction, no tracker item:
CALL proc5();
--echo
BEGIN;
--echo # first  SELECT adds R/S to present T and renders a tracker item;
--echo # second SELECT add r flag and renders another tracker item
CALL proc5();
COMMIT;
SET autocommit=0;
--echo # first  SELECT renders I/R/S tracker item;
--echo # second SELECT add r flag and renders another tracker item
CALL proc5();
COMMIT;
--echo
BEGIN;
--echo # first  SELECT adds R/S to present T and renders a tracker item;
--echo # second SELECT add r flag and renders another tracker item
CALL proc5();
COMMIT;
--echo
--echo # multiple result sets; implicit commit between them
BEGIN;
--echo # first  SELECT adds R/S to present T and renders a tracker item;
--echo # second SELECT add r flag and renders another tracker item
CALL proc6b();
COMMIT;
BEGIN;
--echo # first  SELECT adds R/S to present T and renders a tracker item;
--echo # second SELECT add r flag and renders another tracker item
CALL proc6a();
COMMIT;
--echo
--echo # multiple result sets; sub-query as argument, autocommit==0
BEGIN;
# add S to tracker item
SELECT 1 FROM DUAL;
CALL proc7((SELECT f1 FROM t2));
COMMIT;
SET autocommit=1;
--echo # multiple result sets; sub-query as argument, autocommit==1
BEGIN;
# add S to tracker item
SELECT 1 FROM DUAL;
CALL proc7((SELECT f1 FROM t2));
COMMIT;
--echo # not in a transaction, no tracking
CALL proc8((SELECT f1 FROM t2));
--echo
BEGIN;
--echo # body sets R/S in select, and r in sub-select
CALL proc8((SELECT f1 FROM t2));
COMMIT;
--echo
BEGIN;
--echo # argument sets r, body sets R
CALL proc9((SELECT f1 FROM t2));
COMMIT;
--echo
DROP PROCEDURE proc1;
DROP PROCEDURE proc2;
DROP PROCEDURE proc3;
DROP PROCEDURE proc4;
DROP PROCEDURE proc5;
DROP PROCEDURE proc6a;
DROP PROCEDURE proc6b;
DROP PROCEDURE proc7;
DROP PROCEDURE proc8;
DROP PROCEDURE proc9;
DROP TABLE t1;
DROP TABLE t2;
--echo ########################################################################
--echo #
--echo # interaction with system tables
--echo #
CREATE TABLE t2 (f1 INT) ENGINE="MyISAM";
BEGIN;
	
# CONVERT_TZ() accesses a transactional system table in an attached
# transaction. This is an implementation detail / artifact that does
# not concern the user transaction, so we hide it (as we do all state
# from attached transactions).
SELECT CONVERT_TZ('2004-01-01 12:00:00','GMT','MET');
COMMIT;
DROP TABLE t2;

--echo ########################################################################
--echo #
--echo #
--echo # show that table access reporting works with LOCK TABLES
--echo #
CREATE TABLE t1 (f1 INT) ENGINE="InnoDB";
CREATE TABLE t2 (f1 INT) ENGINE="MyISAM";
SET autocommit=0;
SELECT 1  FROM DUAL;
SELECT f1 FROM t1;
--echo # no LOCK held this time, so no implicit commit:
UNLOCK TABLES;
--echo # LOCK TABLES pre-commits:
LOCK TABLES t1 WRITE;
INSERT INTO t1 VALUES (1);
INSERT INTO t1 VALUES (2);
SELECT f1 FROM t1;
COMMIT;
SET TRANSACTION READ ONLY;
SET TRANSACTION READ WRITE;
UNLOCK TABLES;
--echo
SET autocommit=1;
LOCK TABLE t1 WRITE, t2 WRITE;
INSERT INTO t2 VALUES (3);
INSERT INTO t1 VALUES (3);
SELECT f1 FROM t1 WHERE f1 > 2;
UNLOCK TABLES;
--echo # don't report chistics for autocommits while LOCK is held
SET SESSION session_track_transaction_info="CHARACTERISTICS";
LOCK TABLE t1 WRITE;
INSERT INTO t1 VALUES (3);
SELECT f1 FROM t1 WHERE f1 > 2;
UNLOCK TABLES;
SET session_track_transaction_info="STATE";
DROP TABLE t1;
DROP TABLE t2;

--echo ########################################################################
--echo #
--echo # cleanup
--echo #

--disable_session_track_info

SET SESSION session_track_system_variables= @old_track_list;
SET SESSION session_track_state_change=@old_track_enable;
SET SESSION session_track_transaction_info=@old_track_tx;

# ends
