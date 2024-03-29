SET @debug_saved = @@global.debug;

START TRANSACTION;

CREATE TABLE t1(a INT PRIMARY KEY AUTO_INCREMENT, b VARCHAR(255)) engine = innodb;

INSERT INTO t1(b) VALUES ('aaa');
INSERT INTO t1(b) VALUES ('bbb');
INSERT INTO t1(b) VALUES ('ccc');

COMMIT;

# 1. Start outer transaction with SERIALIZABLE isolation level.
# Check that SELECT in inner transaction will not take locks.

# Start transaction.
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
START TRANSACTION;
# Insert a new row into t1.
INSERT INTO t1(b) VALUES ('ddd');
SELECT * FROM t1;
a	b
1	aaa
2	bbb
3	ccc
4	ddd

# [another connection]

SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
START TRANSACTION;
SET @@global.debug = '+d,use_attachable_trx';
SELECT * FROM t1;
a	b
1	aaa
2	bbb
3	ccc
SET @@global.debug = '-d,use_attachable_trx';

# [default connection]

ROLLBACK;

# 2. Check that inner transaction has different visibility scope than
# the outer transaction.
# Start READ ONLY transaction.
SET TRANSACTION ISOLATION LEVEL REPEATABLE READ;
START TRANSACTION READ ONLY;
# SELECT to actually start a transaction.
SELECT * FROM t1;
a	b
1	aaa
2	bbb
3	ccc

# [another connection]

START TRANSACTION;
UPDATE t1 SET b = 'zzz' WHERE a = 2;
COMMIT;

# [default connection]

# SELECT in the outer transaction doesn't see the changes.
SELECT * FROM t1;
a	b
1	aaa
2	bbb
3	ccc

# SELECT in the inner transaction sees the changes.
SET @@global.debug = '+d,use_attachable_trx';
SELECT * FROM t1;
a	b
1	aaa
2	zzz
3	ccc
SET @@global.debug = '-d,use_attachable_trx';

# COMMIT the outer transaction.
COMMIT;
# SELECT in the outer transaction now sees the changes.
SELECT * FROM t1;
a	b
1	aaa
2	zzz
3	ccc
COMMIT;

# 3. Check that the inner transaction does not reset a save point set in
# the outer transaction.

# Start transaction.
SET TRANSACTION ISOLATION LEVEL REPEATABLE READ;
START TRANSACTION;
SELECT * FROM t1;
a	b
1	aaa
2	zzz
3	ccc

# Set save point.
SAVEPOINT sp1;

# Do some changes.
UPDATE t1 SET b = 'xxx' WHERE a = 2;
SELECT * FROM t1;
a	b
1	aaa
2	xxx
3	ccc

# Do anything in the inner transaction.
SET @@global.debug = '+d,use_attachable_trx';
SELECT * FROM t1;
a	b
1	aaa
2	zzz
3	ccc
SET @@global.debug = '-d,use_attachable_trx';

# Just make sure the changes are still there.
SELECT * FROM t1;
a	b
1	aaa
2	xxx
3	ccc

# Rollback to the save point to make sure it was not reset.
ROLLBACK TO SAVEPOINT sp1;

# Check that the changes have been reverted.
SELECT * FROM t1;
a	b
1	aaa
2	zzz
3	ccc

# Commit.
COMMIT;

# Cleanup.
DROP TABLE t1;

SET @@global.debug = @debug_saved;
