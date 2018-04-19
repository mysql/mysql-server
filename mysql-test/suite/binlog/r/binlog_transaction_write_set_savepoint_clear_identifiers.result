SET @save_session_debug= @@SESSION.debug;
SET @@SESSION.debug = "+d,transaction_write_set_savepoint_clear_on_commit_rollback";
CREATE TABLE t1 (c1 INT PRIMARY KEY);
BEGIN;
SAVEPOINT S0;
INSERT INTO t1 VALUES (0);
COMMIT;
BEGIN;
SAVEPOINT S1;
INSERT INTO t1 VALUES (1);
ROLLBACK;
BEGIN;
SAVEPOINT S2;
INSERT INTO t1 VALUES (2);
COMMIT;
include/assert.inc ['There are two values in table t1']
SET @@SESSION.debug= @save_session_debug;
DROP TABLE t1;
