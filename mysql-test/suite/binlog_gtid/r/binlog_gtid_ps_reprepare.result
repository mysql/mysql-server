# ==== Initialize ====
CREATE TABLE t1 (a INT);
CREATE TABLE t2 (a INT);
PREPARE stmt1 FROM 'INSERT INTO t1 VALUES (1)';
CREATE PROCEDURE p1 ()
BEGIN
INSERT INTO t2 VALUES(2);
END
$
CREATE PROCEDURE p2 ()
BEGIN
EXECUTE stmt1;
END
$
# ==== Case 1 ====
# Test case to verify gtid_next update with prepared statement re-prepare.
SET gtid_next = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';
BEGIN;
EXECUTE stmt1;
COMMIT;
SET gtid_next = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2';
TRUNCATE TABLE t1;
SET gtid_next = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:3';
# Without fix, following statement execution fails.
EXECUTE stmt1;
# ==== Case 2 ====
# Test case to verify gtid_next update with stored procedure statement
# re-prepare.
SET gtid_next = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:4';
CALL p1();
SET gtid_next = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:5';
TRUNCATE TABLE t2;
SET gtid_next = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:6';
# Without fix, following statement execution fails.
CALL p1();
# ==== Case 3 ====
# Test case to verify gtid_next update with prepared statement re-prepare
# in stored procedure.
SET gtid_next = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:7';
CALL p2();
SET gtid_next = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:8';
TRUNCATE TABLE t1;
SET gtid_next = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:9';
# Without fix, following statement execution fails.
CALL p2();
# === Case 4 ===
# Test case to verify gtid_next update with prepared statement max
# re-prepare attempt hits.
SET gtid_next = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:10';
TRUNCATE TABLE t1;
# Adding debug point 'simulate_max_reprepare_attempts_hit_case' to @@SESSION.debug
SET gtid_next = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:11';
# Error is reported when max re-prepare limit is hit. Transaction is
# rolled back and gtid_next is updated.
EXECUTE stmt1;
ERROR HY000: Prepared statement needs to be re-prepared
INSERT INTO t1 VALUES(10);
ERROR HY000: When @@SESSION.GTID_NEXT is set to a GTID, you must explicitly set it to a different value after a COMMIT or ROLLBACK. Please check GTID_NEXT variable manual page for detailed explanation. Current @@SESSION.GTID_NEXT is 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:11'.
# === Case 5 ===
# Test case to verify gtid_next update with stored procedure statement
# max re-prepare attempt hits.
SET gtid_next = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:12';
TRUNCATE TABLE t2;
SET gtid_next = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:13';
# Error is reported when max re-prepare limit is hit. Transaction is
# rolled back and gtid_next is updated.
CALL p1;
ERROR HY000: Prepared statement needs to be re-prepared
INSERT INTO t2 VALUES(10);
ERROR HY000: When @@SESSION.GTID_NEXT is set to a GTID, you must explicitly set it to a different value after a COMMIT or ROLLBACK. Please check GTID_NEXT variable manual page for detailed explanation. Current @@SESSION.GTID_NEXT is 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:13'.
# ==== Cleanup ====
SET gtid_next = DEFAULT;
# Removing debug point 'simulate_max_reprepare_attempts_hit_case' from @@SESSION.debug
DEALLOCATE PREPARE stmt1;
DROP TABLE t1, t2;
DROP PROCEDURE p1;
DROP PROCEDURE p2;
