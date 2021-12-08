# Skipping the test temporarily when log-bin is enabled as the test contains a
# statement accessing mysql.general_log in an trxn. The trxn status tracker
# (@@session_track_transaction_info) marks it unsafe only when logbin is on
--source include/not_log_bin.inc

# Skip unless --xa_detach_on_prepare as this affects session tracking output
--let $option_name = xa_detach_on_prepare
--let $option_value = 1
--source include/only_with_option.inc

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
--echo # test "STATE" tracking: transaction type transitions
--echo #

SET SESSION session_track_transaction_info="STATE";

--enable_session_track_info

--echo # 3.1.1.1.1  "explicit transaction active"
# transition: no trx -> explicit trx
START TRANSACTION;
--echo # 3.1.1.1.1  ending explicit transaction explicitly
# transition: explicit trx -> no trx
COMMIT;
--echo # 3.1.1.1.1  ending explicit transaction implicitly
CREATE TABLE t1 (f1 INT) ENGINE="InnoDB";
START TRANSACTION;
# DDL forcing an implicit commit (thereby ending the explicit transaction):
# transition: explicit trx -> no trx
DROP TABLE t1;


--echo # 3.1.1.2    "no work attached"
# no work attached after transaction started (unless WITH CONSISTENT SNAPSHOT)
START TRANSACTION;
# still no work attached as we do not access tables, and still in transaction:
SET @dummy=0;
ROLLBACK;


--echo # 3.1.1.1.2  "implicit transaction active"
--echo #
# autocommit=0 enables implicit transactions
# transition: no trx -> implicit trx
SET autocommit=0;
# DDL forcing an implicit commit
# transition: implicit trx -> no trx
CREATE TABLE t1 (f1 INT) ENGINE="InnoDB";
# attach a (transactional, since t1 is an InnoDB table) write:
# transition: no trx -> implicit trx (with work)
INSERT INTO t1 VALUES (1);
# attach a transaction read; show that we get the union of the accesses so far:
SELECT f1 FROM t1 LIMIT 1 INTO @dummy;
# add a result-set:
SELECT f1 FROM t1;
# force commit by starting an explict transaction:
# transition: implicit trx -> explicit trx
BEGIN WORK;
# end transaction by forcing an implicit commit:
# transition: explicit trx -> no trx
DROP TABLE t1;
# start an implicit transaction
SELECT RAND(22) INTO @dummy;
# implicit transaction, explicit commit:
# transition: implicit trx -> no trx
COMMIT;
#
# SET TRANSACTION is OK during implicit transaction UNTIL tables are accessed
CREATE TABLE t1 (f1 INT) ENGINE="InnoDB";
SET TRANSACTION READ ONLY;
SET TRANSACTION READ WRITE;
SELECT RAND(22) INTO @dummy;
SET TRANSACTION READ WRITE;
INSERT INTO t1 VALUES (1);
--error ER_CANT_CHANGE_TX_CHARACTERISTICS
SET TRANSACTION READ WRITE;
DROP TABLE t1;

# change back to autocommit mode.
# Axiom: This should reset state as the implicit transaction, if any, ends.
# transition: implicit trx -> no trx
SET autocommit=1;

--echo # read with and without result set:
CREATE TABLE t1 (f1 INT) ENGINE="InnoDB";
CREATE TABLE t2 (f1 INT) ENGINE="InnoDB";
INSERT INTO  t1 VALUES (123);

BEGIN;
# read + result set
SELECT f1 FROM t1;
COMMIT AND CHAIN;
# read + write
INSERT INTO t2 SELECT f1 FROM t1;
COMMIT;

DROP TABLE t1;
DROP TABLE t2;
--echo

--echo ########################################################################
--echo #
--echo # test "CHARACTERISTICS" tracking
--echo #

SET SESSION session_track_transaction_info="CHARACTERISTICS";
# side-effect: state will change to "explicit transaction active"
START TRANSACTION;
# side-effect: state -> "explicit transaction active" + "transactional read"
START TRANSACTION WITH CONSISTENT SNAPSHOT;
# side-effect: state -> "explicit transaction active"
START TRANSACTION READ WRITE;
--echo # state is again "we have an empty transaction", so make no state item
START TRANSACTION READ ONLY;
START TRANSACTION READ WRITE, WITH CONSISTENT SNAPSHOT;
START TRANSACTION READ ONLY,  WITH CONSISTENT SNAPSHOT;
--echo # chain read chistics, but not snapshot:
COMMIT AND CHAIN;

# We can't set characteristics one-shots with an explicit transaction ongoing:
--error ER_CANT_CHANGE_TX_CHARACTERISTICS
SET TRANSACTION   READ ONLY;

--echo # will create an empty characteristics item by convention, plus 0 state
ROLLBACK;
--echo


# Let's try the characteristics one-shots again, outside a transaction:
--echo # chistics: READ ONLY
SET TRANSACTION   READ ONLY;
--echo # chistics: READ ONLY + ISOL RR
SET TRANSACTION              ISOLATION LEVEL REPEATABLE READ;
--echo # chistics: READ ONLY + ISOL RR
SET TRANSACTION   READ ONLY;
--echo # chistics: READ WRITE + ISOL RR
SET TRANSACTION   READ WRITE;
--echo # chistics: READ WRITE + ISOL RR
SET TRANSACTION              ISOLATION LEVEL REPEATABLE READ;
--echo # chistics: READ ONLY + ISOL SER
SET TRANSACTION   READ ONLY, ISOLATION LEVEL SERIALIZABLE;
--echo # chistics: READ ONLY + ISOL SER
BEGIN WORK;
COMMIT;
--echo



# Show how the characteristics one-shots interact with the session values:

SET SESSION transaction_read_only=0;
--echo # one-shot (different from session default)
SET TRANSACTION READ ONLY;
START TRANSACTION;
COMMIT;
--echo

--echo # one-shot (repeats session default)
SET TRANSACTION READ WRITE;
START TRANSACTION;
COMMIT;
--echo

--echo # session
SET TRANSACTION ISOLATION LEVEL READ UNCOMMITTED;
SET TRANSACTION ISOLATION LEVEL REPEATABLE READ;
SET SESSION TRANSACTION ISOLATION LEVEL READ UNCOMMITTED;
--echo # "isolation" one-shot is set, and added to chistics tracker (=> 1 item)
SET TRANSACTION ISOLATION LEVEL READ COMMITTED;
--echo # "read-only" one-shot is set, and added to chistics tracker (=> 2 items)
SET TRANSACTION READ ONLY;
--echo # setting the session default:
--echo # - we receive "changed variable" for @@session.transaction_read_only
--echo # - "read-only" one-shot is cleared, disappears from chistics tracker
--echo # - "isolation" one-shot remains set, and remains in chistics tracker
SET SESSION TRANSACTION READ ONLY;
--echo # chistics: isolation level is READ COMMITTED (from one-shot), READ ONLY
--echo #           or READ WRITE not given, as we're using session default again
START TRANSACTION;
COMMIT;
--echo

--echo # chistics: READ ONLY
SET TRANSACTION READ ONLY;
--echo # chistics: READ ONLY, READ COMM
SET TRANSACTION ISOLATION LEVEL READ COMMITTED;
--echo # chistics: SESSION resets ISOL one-shot,  READ ONLY remains
SET SESSION TRANSACTION ISOLATION LEVEL READ UNCOMMITTED;
--echo # chistics: SESSION resets READ one-shot, nothing remains
SET SESSION TRANSACTION READ ONLY;
--echo

SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ;
SET SESSION TRANSACTION READ WRITE;

--echo



--echo # show that START TRANSACTION READ ... overrides SET TRANSACTION READ ..
SET TRANSACTION READ ONLY;
START TRANSACTION READ WRITE;
ROLLBACK;
--echo



--echo # chistics AND CHAIN
# At this point, (COMMIT as well as ROLLBACK) AND CHAIN preserves chistics:
SET TRANSACTION READ ONLY;
START TRANSACTION;
COMMIT AND CHAIN;
ROLLBACK;
--echo

SET TRANSACTION ISOLATION LEVEL READ COMMITTED;
START TRANSACTION READ ONLY;
ROLLBACK AND CHAIN;
COMMIT;
--echo


--echo # show that session_track_transaction_info="STATE" will hide some edges
SET session_track_transaction_info="STATE";
--echo # normal syntax: TR->T->0
START TRANSACTION WITH CONSISTENT SNAPSHOT;
COMMIT AND CHAIN;
COMMIT;
--echo # normal syntax: T->T->0
START TRANSACTION;
--echo # state does not change, and therefore isn't reported
COMMIT AND CHAIN;
COMMIT;
--echo # pathological syntax: TR->TR->0
START TRANSACTION WITH CONSISTENT SNAPSHOT;
--echo # state does not change, and therefore isn't reported
START TRANSACTION WITH CONSISTENT SNAPSHOT;
COMMIT;
--echo

--echo # show that session_track_transaction_info="CHARACTERISTICS" shows more edges
SET session_track_transaction_info="CHARACTERISTICS";
--echo # normal syntax: TR->T->0
START TRANSACTION WITH CONSISTENT SNAPSHOT;
COMMIT AND CHAIN;
COMMIT;
--echo # normal syntax: T->T->0
START TRANSACTION;
COMMIT AND CHAIN;
COMMIT;
--echo # pathological syntax: TR->TR->0
START TRANSACTION WITH CONSISTENT SNAPSHOT;
START TRANSACTION WITH CONSISTENT SNAPSHOT;
COMMIT;
--echo


--echo # chistics and interaction of implicit trx and explicit trx 
CREATE TABLE t1 (f1 INT) ENGINE="InnoDB";
SET autocommit=0;
SET TRANSACTION READ ONLY;
--error ER_CANT_EXECUTE_IN_READ_ONLY_TRANSACTION
INSERT INTO t1 VALUES(1);
ROLLBACK;
SET TRANSACTION READ WRITE;
# does not prevent further one-shots:
SET TRANSACTION READ ONLY;
SET TRANSACTION READ WRITE;
INSERT INTO t1 VALUES(1);
# in implicit transaction, can't set one-shots here using SET TRANSACTION
--error ER_CANT_CHANGE_TX_CHARACTERISTICS
SET TRANSACTION READ WRITE;
# we can set one-shots using START TRANSACTION though, as that commits
# an ongoing implicit transaction
START TRANSACTION READ ONLY;
# implicit COMMIT
DROP TABLE t1;

CREATE TABLE t1 (f1 INT) ENGINE="InnoDB";
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
# start implicit trx
INSERT INTO t1 VALUES(1);
--echo # implicit commit (chistics item here, clearing isolation level):
ALTER TABLE t1 ADD COLUMN f2 INT;
START TRANSACTION;
ROLLBACK;
--echo
# start implicit trx
INSERT INTO t1 VALUES(2,2);
--echo # implicit commit (no chistics item here):
ALTER TABLE t1 ADD COLUMN f3 INT;
START TRANSACTION;
ROLLBACK;
DROP TABLE t1;

SET autocommit=1;

# cleanup
SET session_track_transaction_info="STATE";
--echo

--echo ########################################################################
--echo #
--echo # show that table access reporting works in Stored Functions (SF)
--echo #

CREATE TABLE t1 (f1 INT) ENGINE="InnoDB";
INSERT INTO t1 VALUES (1);
DELIMITER |;

--echo # create func1() in system table:
CREATE FUNCTION func1()
  RETURNS INTEGER
BEGIN
  SET @dummy = 0;
  IF (SELECT * FROM t1) THEN
    SET @dummy = 1;
  END IF;
  RETURN @dummy;
END|

DELIMITER ;|

--echo # func1() reads a trx table (and is read from a system table!):
BEGIN;
SELECT func1();
COMMIT;
DROP TABLE t1;
--echo # drop func1() from system table:
DROP FUNCTION func1;

--echo #
--echo # log tables
--echo #

SET @old_log_output=          @@global.log_output;
SET @old_general_log=         @@global.general_log;
SET @old_general_log_file=    @@global.general_log_file;

TRUNCATE TABLE mysql.general_log;

SET GLOBAL log_output =       'TABLE';
SET GLOBAL general_log=       'ON';

BEGIN;
# Example query to be logged to the log table, general_log.
# That table access does not become part of our state:
SELECT 1 FROM DUAL;
# Show that the above query was actually logged.
# THIS (read) access will be flagged, as it's part of the user's transaction.
SELECT " -> ", argument FROM mysql.general_log WHERE argument LIKE '% DUAL' AND (command_type!='Prepare');
ROLLBACK;

SET GLOBAL general_log_file=  @old_general_log_file;
SET GLOBAL general_log=       @old_general_log;
SET GLOBAL log_output=        @old_log_output;

TRUNCATE TABLE mysql.general_log;


--echo ########################################################################
--echo #
--echo # XA
--echo #

CREATE TABLE t1 (f1 int) ENGINE="InnoDB";
SET SESSION session_track_transaction_info="CHARACTERISTICS";

--echo # XA ROLLBACK
XA START 'test1';
INSERT t1 VALUES (1);
XA END 'test1';
XA PREPARE 'test1';
XA ROLLBACK 'test1';

--echo # XA COMMIT
XA START 'test2', 'yy';
INSERT t1 VALUES (2);
XA END 'test2', 'yy';
XA PREPARE 'test2', 'yy';
XA COMMIT 'test2', 'yy';


--echo # Run XA PREPARE test cases also with xa_detach_on_prepare=OFF.
--let $CURSESS_xa_detach_on_prepare = `SELECT @@SESSION.xa_detach_on_prepare`
SET SESSION xa_detach_on_prepare = false;

--echo # XA ROLLBACK
XA START 'test1_';
INSERT t1 VALUES (1);
XA END 'test1_';
XA PREPARE 'test1_';
XA ROLLBACK 'test1_';

--echo # XA COMMIT
XA START 'test2_', 'yy';
INSERT t1 VALUES (2);
XA END 'test2_', 'yy';
XA PREPARE 'test2_', 'yy';
XA COMMIT 'test2_', 'yy';

--replace_result $CURSESS_xa_detach_on_prepare OLD_VALUE_xa_detach_on_prepare
--eval SET SESSION xa_detach_on_prepare = $CURSESS_xa_detach_on_prepare

--echo # XA COMMIT ONE PHASE
XA START 'test3','xx',5;
INSERT t1 VALUES (3);
XA END 'test3','xx',5;
XA COMMIT 'test3','xx',5 ONE PHASE;

SET SESSION session_track_transaction_info="OFF";
DROP TABLE t1;


--echo ########################################################################
--echo #
--echo # cleanup
--echo #

--disable_session_track_info

SET SESSION session_track_system_variables= @old_track_list;
SET SESSION session_track_state_change=@old_track_enable;
SET SESSION session_track_transaction_info=@old_track_tx;

# ends
