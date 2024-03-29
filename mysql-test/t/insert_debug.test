source include/have_debug.inc;
source include/have_debug_sync.inc;

SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED;

connect (con1, localhost, root,,);
SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED;

connection default;

let $conn0_id= `SELECT CONNECTION_ID()`;

CREATE TABLE t1(c1 VARCHAR(10) NOT NULL, c2 VARCHAR(10) NOT NULL, c3 VARCHAR(10) NOT NULL);
INSERT INTO t1(c1, c2, c3) VALUES('A1','B1','IT1'), ('A2','B2','IT1'), ('A3','B3','IT1'), ('A4','B4','IT1'), ('A5','B5','IT1'), ('A6','B6','IT1'), ('A7','B7','IT1');

CREATE TABLE t2(c1 VARCHAR(10) NOT NULL, c2 VARCHAR(10) NOT NULL, c3 VARCHAR(10) NOT NULL);
INSERT INTO t2(c1, c2, c3) VALUES ('A3','B3','IT2'), ('A2','B2','IT2'), ('A4','B4','IT2'), ('A5','B5','II2');

CREATE TABLE result(id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT, c1 VARCHAR(10) NOT NULL, c2 VARCHAR(10),
c3 VARCHAR(10), update_count INT DEFAULT 0, UNIQUE KEY uniq_idx (c1,c2), PRIMARY KEY (id)) ENGINE = innodb;

# Insert one row from 't1' into the 'result' table and wait on a debug sync
# point. The next insert statement from an session 2 inserts values that would
# lead to unique key clash, when this insert resumes.
# The subsequent inserts of this statement(after resume) will fail because of a
# clash with the unique index, and are expected to update the row which clashes
# with the unique key.
# Without the fix for bug#30194841 a stale auto increment value, would cause a
# collision with existing auto increment column value and ends up updating that
# colliding row, instead of the row colliding with the unique index.
SET DEBUG_SYNC = "ha_write_row_end WAIT_FOR flushed EXECUTE 1";
send INSERT INTO result(c1, c2, c3) SELECT * FROM t1 ON DUPLICATE KEY UPDATE c2=t1.c2, c3='UT1', update_count=update_count+1;

# While session 1 is waiting (after one insert), insert rows that will cause a clash
# with the inserts of session 1 on the unique key.
connection con1;

# Wait for the session 1 to hit the debug sync point.
let $wait_condition=SELECT 1 FROM information_schema.processlist WHERE id = $conn0_id AND state LIKE '%ha_write_row_end%';
--source include/wait_condition.inc

INSERT INTO result(c1, c2, c3) SELECT * FROM t2 ON DUPLICATE KEY UPDATE c2=t2.c2, c3='UT2', update_count=update_count+1;

# Signal to resume the insert statement in session 1
SET DEBUG_SYNC = "now SIGNAL flushed";
connection default;
reap;
SELECT * FROM result;

DROP TABLE t1;
DROP TABLE t2;
DROP TABLE result;
