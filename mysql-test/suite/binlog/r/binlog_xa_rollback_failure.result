CREATE TABLE t1(id INT PRIMARY KEY);
#
# 1) Prepare XA transaction with some sample data with
#    XA global Trx id '1'.
#
XA START '1';
INSERT INTO t1 VALUES (1);
XA END '1';
XA PREPARE '1';
#
# 2) Try to do 'XA ROLLBACK' with XA global Trx id '2' and check that
#    it fails.
#
XA ROLLBACK '2';
ERROR XAE07: XAER_RMFAIL: The command cannot be executed when global transaction is in the  PREPARED state
#
# 3) Check that nothing is written to binary log.
#
include/assert.inc [Nothing should be written in binlog for failed XA ROLLBACK.]
#
# 4) Cleanup.
#
XA COMMIT '1';
DROP TABLE t1;
