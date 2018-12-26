SET @save_session_debug= @@SESSION.debug;
SET @@SESSION.debug = "+d,transaction_write_set_savepoint_add_savepoint";
CREATE TABLE t1 (c1 INT PRIMARY KEY);
BEGIN;
SAVEPOINT S0;
INSERT INTO t1 VALUES (0);
SAVEPOINT S0;
ROLLBACK TO S0;
COMMIT;
include/assert.inc ['There is one value in table t1']
SET @@SESSION.debug= @save_session_debug;
DROP TABLE t1;
