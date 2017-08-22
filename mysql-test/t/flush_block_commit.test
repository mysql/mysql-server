# Let us see if FLUSH TABLES WITH READ LOCK blocks COMMIT of existing
# transactions.
# We verify that we did not introduce a deadlock.
# This is intended to mimick how mysqldump and innobackup work.

# And it requires InnoDB

--echo # Save the initial number of concurrent sessions
--source include/count_sessions.inc

--echo # Establish connection con1 (user=root)
connect (con1,localhost,root,,);
--echo # Establish connection con2 (user=root)
connect (con2,localhost,root,,);
--echo # Establish connection con3 (user=root)
connect (con3,localhost,root,,);
--echo # Switch to connection con1
connection con1;

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings
CREATE TABLE t1 (a INT) ENGINE=innodb;

# blocks COMMIT ?

BEGIN;
INSERT INTO t1 VALUES(1);
--echo # Switch to connection con2
connection con2;
FLUSH TABLES WITH READ LOCK;
--echo # Switch to connection con1
connection con1;
--echo # Sending:
--send COMMIT
--echo # Switch to connection con2
connection con2;
--echo # Wait until COMMIT gets blocked.
let $wait_condition=
  select count(*) = 1 from information_schema.processlist
  where state = "Waiting for commit lock" and info = "COMMIT";
--source include/wait_condition.inc
--echo # Verify that 'con1' was blocked and data did not move.
SELECT * FROM t1;
UNLOCK TABLES;
--echo # Switch to connection con1
connection con1;
--echo # Reaping COMMIT
--reap

# No deadlock ?

--echo # Switch to connection con1
connection con1;
BEGIN;
SELECT * FROM t1 FOR UPDATE;
--echo # Switch to connection con2
connection con2;
BEGIN;
send SELECT * FROM t1 FOR UPDATE; # blocked by con1
sleep 1;
--echo # Switch to connection con3
connection con3;
send FLUSH TABLES WITH READ LOCK; # blocked by con2
--echo # Switch to connection con1
connection con1;
COMMIT; # should not be blocked by con3
--echo # Switch to connection con2
connection con2;
reap;
COMMIT;
--echo # Switch to connection con3
connection con3;
reap;
UNLOCK TABLES;

# Bug#6732 FLUSH TABLES WITH READ LOCK + COMMIT hangs later FLUSH TABLES
#          WITH READ LOCK

--echo # Switch to connection con2
connection con2;
COMMIT; # unlock InnoDB row locks to allow insertions
--echo # Switch to connection con1
connection con1;
BEGIN;
INSERT INTO t1 VALUES(10);
FLUSH TABLES WITH READ LOCK;
--echo # Switch to connection con2
connection con2;
FLUSH TABLES WITH READ LOCK; # bug caused hang here
UNLOCK TABLES;

# Bug#7358 SHOW CREATE DATABASE fails if open transaction

BEGIN;
SELECT * FROM t1;
SHOW CREATE DATABASE test;
COMMIT;


--echo # Cleanup
--echo # Switch to connection default and close connections con1, con2, con3
connection default;
disconnect con1;
disconnect con2;
disconnect con3;

--echo # We commit open transactions when we disconnect: only then we can
--echo # drop the table.
DROP TABLE t1;
--echo # End of 4.1 tests

--echo # Wait till all disconnects are completed
--source include/wait_until_count_sessions.inc
