call mtr.add_suppression("Found 1 prepared XA transactions");
RESET MASTER;
CREATE TABLE t1 (a INT) ENGINE=InnoDB;
FLUSH LOGS;
#
# Verify that selecting from mysql.gtid_executed table
# explicitly by a XA transaction does not cause an error.
#
XA START '1';
INSERT INTO t1 VALUES(1);
INSERT INTO t1 VALUES(2);
SELECT * FROM mysql.gtid_executed;
source_uuid	interval_start	interval_end
MASTER_UUID	1	1
XA END '1';
XA PREPARE '1';
#
# Crash the server after preparing a XA transaction
# and right before committing the XA transaction
#
SET @@SESSION.DEBUG= '+d, simulate_crash_on_commit_xa_trx';
XA COMMIT '1';
ERROR HY000: Lost connection to MySQL server during query
#
# Verify that the following recovery is not aborting due to an
# innodb_lock_wait_timeout error when it is reading the system
# gtid_executed table.
#
# Restart the master server
# Rollback the XA transaction with xid '1' in order to release
# MDL lock for the table t1 and so allow the statement DROP TABLE t1
# to be executed successfully.
XA ROLLBACK '1';
DROP TABLE t1;
