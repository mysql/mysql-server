RESET MASTER;
#
# USE-CASE 1 - rollback empty transaction as first statement and GTID_NEXT
# set to autotmatic:
#
include/save_binlog_position.inc
#
# S1. Set GTID_NEXT to 'AUTOMATIC'.
#
SET @@SESSION.GTID_NEXT= 'AUTOMATIC';
#
# S2. Start and end an XA transaction.
#
XA START 'trx';
XA END 'trx';
#
# S3. Rollback the XA transaction.
#
XA ROLLBACK 'trx';
#
# S4. Check binlog for related event (should not be found).
#
include/save_binlog_position.inc
include/assert.inc [Empty transaction, binlog position is the same]
#
# S5. Check for an unchanged GTID_EXECUTED set.
#
include/assert.inc [GTID_EXECUTED has not changed]
#
# USE-CASE 2 - rollback empty transaction as first statement and GTID_NEXT
# set to specific UUID:
#
include/save_binlog_position.inc
#
# S1. Set GTID_NEXT to a specific UUID.
#
SET @@SESSION.GTID_NEXT= 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';
#
# S2. Start and end an XA transaction.
#
XA START 'trx';
XA END 'trx';
#
# S3. Rollback the XA transaction.
#
XA ROLLBACK 'trx';
SET @@SESSION.GTID_NEXT= 'AUTOMATIC';
#
# S4. Check binlog for related event (should not be found).
#
include/save_binlog_position.inc
include/assert.inc [Empty transaction, binlog position is the same]
#
# S5. Check GTID_EXECUTED for the specific UUID (should not be found).
#
include/assert.inc [Transaction is NOT present in GTID_EXECUTED]
#
# USE-CASE 3 - start with GTID_NEXT set to a specific UUID and rollback empty
# transaction after XA COMMIT ... ONE PHASE and GTID_NEXT set to a specific
# UUID:
#
#
# S1. Set GTID_NEXT to a specific UUID.
#
SET @@SESSION.GTID_NEXT= 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2';
#
# S2. Start and end an XA transaction.
#
XA START 'trx';
XA END 'trx';
#
# S3. Commit the XA transaction with 'XA COMMIT ... ONE PHASE'.
#
XA COMMIT 'trx' ONE PHASE;
include/save_binlog_position.inc
#
# S4. Set GTID_NEXT to a specific UUID.
#
SET @@SESSION.GTID_NEXT= 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:3';
#
# S5. Start and end an XA transaction.
#
XA START 'trx';
XA END 'trx';
#
# S6. Rollback the XA transaction.
#
XA ROLLBACK 'trx';
SET @@SESSION.GTID_NEXT= 'AUTOMATIC';
#
# S7. Check binlog for related event (should not be found).
#
include/save_binlog_position.inc
include/assert.inc [Empty transaction, binlog position is the same]
#
# S8. Check GTID_EXECUTED for the specific UUID (should not be found).
#
include/assert.inc [Transaction is NOT present in GTID_EXECUTED]
#
# USE-CASE 4 - start with GTID_NEXT set to 'AUTOMATIC' and rollback empty
# transaction after XA COMMIT ... ONE PHASE and GTID_NEXT set to a specific
# UUID:
#
#
# S1. Set GTID_NEXT to 'AUTOMATIC'.
#
SET @@SESSION.GTID_NEXT= 'AUTOMATIC';
#
# S2. Start and end an XA transaction.
#
XA START 'trx';
XA END 'trx';
#
# S3. Commit the XA transaction with 'XA COMMIT ... ONE PHASE'.
#
XA COMMIT 'trx' ONE PHASE;
include/save_binlog_position.inc
#
# S4. Set GTID_NEXT to a specific UUID.
#
SET @@SESSION.GTID_NEXT= 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:4';
#
# S5. Start and end an XA transaction.
#
XA START 'trx';
XA END 'trx';
#
# S6. Rollback the XA transaction.
#
XA ROLLBACK 'trx';
SET @@SESSION.GTID_NEXT= 'AUTOMATIC';
#
# S7. Check binlog for related event (should not be found).
#
include/save_binlog_position.inc
include/assert.inc [Empty transaction, binlog position is the same]
#
# S8. Check GTID_EXECUTED for the specific UUID (should not be found).
#
include/assert.inc [Transaction is NOT present in GTID_EXECUTED]
#
# USE-CASE 5 - rollback empty transaction after XA PREPARE and XA COMMIT and
# GTID_NEXT set to a specific UUID:
#
#
# S1. Set GTID_NEXT to 'AUTOMATIC'.
#
SET @@SESSION.GTID_NEXT= 'AUTOMATIC';
#
# S2. Start and end an XA transaction.
#
XA START 'trx';
XA END 'trx';
#
# S3. Commit the XA transaction with 'XA PREPARE' and 'XA COMMIT'.
#
XA PREPARE 'trx';
XA COMMIT 'trx';
include/save_binlog_position.inc
#
# S4. Set GTID_NEXT to a specific UUID.
#
SET @@SESSION.GTID_NEXT= 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:5';
#
# S5. Start and end an XA transaction.
#
XA START 'trx';
XA END 'trx';
#
# S6. Rollback the XA transaction.
#
XA ROLLBACK 'trx';
SET @@SESSION.GTID_NEXT= 'AUTOMATIC';
#
# S7. Check binlog for related event (should not be found).
#
include/save_binlog_position.inc
include/assert.inc [Empty transaction, binlog position is the same]
#
# S8. Check GTID_EXECUTED for the specific UUID (should not be found).
#
include/assert.inc [Transaction is NOT present in GTID_EXECUTED]
#
# USE-CASE 6 - ensure that non-empty XA transactions are logged and
# GITD-executed even if they are rolled back with XA ROLLBACK:
#
#
# S1. Create a table
#
CREATE TABLE t (a INT);
#
# S2. Set GTID_NEXT to a 'AUTOMATIC'.
#
SET @@SESSION.GTID_NEXT= 'AUTOMATIC';
#
# S3. Start an XA transaction.
#
XA START 'trx';
#
# s4. Insert values into created table.
#
INSERT INTO t VALUES (1), (2), (3);
#
# S5. End the XA transaction.
#
XA END 'trx';
#
# S6. Prepare the XA transaction.
#
XA PREPARE 'trx';
#
# S7. Rollback the XA transaction.
#
XA ROLLBACK 'trx';
#
# S8. Check binlog for related event (should be found).
#
include/assert_binlog_events.inc [Gtid/SET.* # Query/use.* # Gtid/SET.* # Query/XA START.* # Table_map/.* # Write_rows/.* # Query/XA END.* # XA_prepare/XA PREPARE.* # Gtid/SET.* # Query/XA ROLLBACK.*]
#
# S9. Check GTID_EXECUTED for the executed UUID (should be found).
#
include/assert.inc [Transaction is present in GTID_EXECUTED]
DROP TABLE t;
