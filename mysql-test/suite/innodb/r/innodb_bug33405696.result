# Create a table with rows which can be small or large
# depending on our wish:
CREATE TABLE t1(id INT PRIMARY key, val VARCHAR(16000));
# Create 1024 records with id=0,1000,2000,...
INSERT INTO t1 (id,val) VALUES (0, "x");
INSERT INTO t1 (id,val)
SELECT (SELECT MAX(id) FROM t1)+1000+id, "x" FROM t1;
INSERT INTO t1 (id,val)
SELECT (SELECT MAX(id) FROM t1)+1000+id, "x" FROM t1;
INSERT INTO t1 (id,val)
SELECT (SELECT MAX(id) FROM t1)+1000+id, "x" FROM t1;
INSERT INTO t1 (id,val)
SELECT (SELECT MAX(id) FROM t1)+1000+id, "x" FROM t1;
INSERT INTO t1 (id,val)
SELECT (SELECT MAX(id) FROM t1)+1000+id, "x" FROM t1;
INSERT INTO t1 (id,val)
SELECT (SELECT MAX(id) FROM t1)+1000+id, "x" FROM t1;
INSERT INTO t1 (id,val)
SELECT (SELECT MAX(id) FROM t1)+1000+id, "x" FROM t1;
INSERT INTO t1 (id,val)
SELECT (SELECT MAX(id) FROM t1)+1000+id, "x" FROM t1;
INSERT INTO t1 (id,val)
SELECT (SELECT MAX(id) FROM t1)+1000+id, "x" FROM t1;
INSERT INTO t1 (id,val)
SELECT (SELECT MAX(id) FROM t1)+1000+id, "x" FROM t1;
SELECT COUNT(*) FROM t1;
COUNT(*)
1024
# It so happens that 894000 is the largest record on its page,
# which we can see by looking at engine_lock_id
# being *:space_id:page_id:heap_no:* and verifying that
# 894000 has a different page_id than 895000. We'll call them "left"
# and "right" page.
CREATE FUNCTION lock_page_no (engine_lock_id CHAR(100))
RETURNS CHAR(20) DETERMINISTIC
RETURN SUBSTRING_INDEX(SUBSTRING_INDEX(engine_lock_id, ':', 3), ':', -1);
BEGIN;
SELECT * FROM t1 WHERE id=894000 FOR UPDATE;
id	val
894000	x
SELECT * FROM t1 WHERE id=895000 FOR UPDATE;
id	val
895000	x
SELECT lock_page_no(engine_lock_id)
FROM performance_schema.data_locks
WHERE LOCK_TYPE='RECORD' AND lock_data='894000'
INTO @left;
SELECT lock_page_no(engine_lock_id)
FROM performance_schema.data_locks
WHERE LOCK_TYPE='RECORD' AND lock_data='895000'
INTO @right;
COMMIT;
include/assert.inc [The two records should be on two different pages]
CREATE FUNCTION lock_page_name (engine_lock_id CHAR(100))
RETURNS CHAR(20) DETERMINISTIC
RETURN CASE lock_page_no(engine_lock_id)
WHEN @left THEN "left"
  WHEN @right THEN "right"
  ELSE "other"
END;
# Now, we stop the purge as we want to just delete-mark the record,
# not physically remove it.
SET GLOBAL innodb_purge_stop_now = ON;
# We delete-mark the largest record on "left" page.
DELETE FROM t1 WHERE id = 894000;
SET GLOBAL TRANSACTION ISOLATION LEVEL SERIALIZABLE;
BEGIN;
# This will return no result, as the record is delete-marked.
SELECT * FROM t1 WHERE id=894000 FOR UPDATE;
id	val
# But, should place an X,REC_NOT_GAP lock on the delete-marked record:
SELECT lock_page_name(engine_lock_id),lock_mode,lock_status,lock_data
FROM performance_schema.data_locks WHERE LOCK_TYPE='RECORD';
lock_page_name(engine_lock_id)	lock_mode	lock_status	lock_data
left	X,REC_NOT_GAP	GRANTED	894000
# We now, unpause the purge, so that the delete-marked record gets
# physically removed, and the X,REC_NOT_GAP on it gets
# converted/inherited into a lock on the gap before "next record".
SET GLOBAL innodb_purge_run_now=ON;
# However, 894000 was the largest on its page, so the "next record" is
# the imaginary "supremum pseudo-record", and indeed we
# expect to see a lock on this "supremum pseudo-record" (P_S displays
# it as X, as opposed to X,GAP, because locks on supremum are always
# implicitly "gap" locks, as the record itself doesn't really exist).
# Note that the lock is *not* duplicated on the gap before first
# record of the "right" page, which is "another" gap after 894000.
SELECT lock_page_name(engine_lock_id),lock_mode,lock_status,lock_data
FROM performance_schema.data_locks WHERE LOCK_TYPE='RECORD';
lock_page_name(engine_lock_id)	lock_mode	lock_status	lock_data
left	X	GRANTED	supremum pseudo-record
# In many cases this doesn't cause a problem: for example if some
# other transaction tried to insert 894000 again, it would get
# routed trough the B-tree to the "left" page, where the gap lock on
# supremum would prevent it. And only values larger-equal than
# the split point in the parent (895000) would get routed to the
# "right" page, which doesn't have the GAP lock.
# However, as we shall see, it's possible for us to "move the split
# point to the left", by inserting 893007 to the "right" page,
# which forces update of the split point from 895000 to 893007 - the
# new minimum of "right" page.
# As said before, by default, INSERT of a value smaller than 895000
# gets routed to "left" page, but if the row is so large that it
# doesn't fit on the "left" page, we try to use
# btr_insert_into_right_sibling() heuristic to first push it to the
# "right" page, in hope to avoid the costly spliting of the "left" page
# due to overflow.
# So, let's see what happens if we insert a 2KB record:
INSERT INTO t1 (id,val) VALUES (893007, REPEAT("x",2000));
SELECT lock_page_name(engine_lock_id),lock_mode,lock_status,lock_data
FROM performance_schema.data_locks WHERE LOCK_TYPE='RECORD';
lock_page_name(engine_lock_id)	lock_mode	lock_status	lock_data
left	X	GRANTED	supremum pseudo-record
right	X,GAP	GRANTED	893007
right	X,GAP	GRANTED	895000
SET @@SESSION.innodb_lock_wait_timeout = 1;
# Now we verify ACID properties. The below query is a
# transaction, which should wait to insert 894000 and timeout, because
# of conflicting lock on the gap before 895000.
BEGIN;
INSERT INTO t1 (id,val) VALUES (894000, "x");
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
COMMIT;
# No transaction should see the 894000 record:
BEGIN;
SELECT * FROM t1 WHERE id = 894000 FOR UPDATE;
id	val
COMMIT;
# The default's transaction should still not see the record.
SELECT * FROM t1 WHERE id = 894000;
id	val
# Just to prove that default's transaction hasn't commited meanwhile,
# let's look at its engine_transaction_id:
SELECT lock_page_name(engine_lock_id),lock_mode,lock_status,lock_data
FROM performance_schema.data_locks WHERE LOCK_TYPE='RECORD';
lock_page_name(engine_lock_id)	lock_mode	lock_status	lock_data
left	X	GRANTED	supremum pseudo-record
right	X,GAP	GRANTED	893007
right	X,GAP	GRANTED	895000
COMMIT;
#cleanup
DROP TABLE t1;
SET GLOBAL TRANSACTION ISOLATION LEVEL REPEATABLE READ;
DROP FUNCTION lock_page_name;
DROP FUNCTION lock_page_no;
