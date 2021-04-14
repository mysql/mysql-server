#
# 0. Initial setup. Create two connections (applier_thread, xa_commit_thread).
#
#
# 1. Make a thread as "applier_thread" by setting pseudo_replica_mode and
#    by executing BINLOG'' query.
#
SET @@SESSION.pseudo_replica_mode=1;
BINLOG '0';
ERROR HY000: Decoding of base64 string failed
#
# 2. Prepare a XA transaction on applier_thread that involves FTS index.
#
CREATE TABLE t1(i TEXT, FULLTEXT INDEX tix (i)) ENGINE=InnoDB;
XA START 'xa1','';
INSERT INTO t1 VALUES ('abc');
XA END 'xa1','';
XA PREPARE 'xa1','';
#
# 3. A 'XA COMMIT' is pending on table t1 which is holding MDL lock.
# Hence executing 'DROP TABLE' on t1 should be blocked by the server.
DROP TABLE t1;;
#
# 4. From a different thread, check that DROP TABLE is waiting for MDL
# lock. Then execute XA COMMIT which releases the MDL lock.
#
XA COMMIT 'xa1';
#
# 5. After XA COMMIT (after releasing MDL lock on table t1),
# Drop table should continue and be successful.
#
#
# 6. Cleanup.
#
