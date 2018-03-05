CALL mtr.add_suppression(".*Could not find a valid tablespace file for");
CREATE TABLE t2 (a INT);
INSERT INTO t2 VALUES(10);
INSERT INTO t2 VALUES(11);
SET SESSION DEBUG = "+d,ib_truncate_crash_after_rename";
TRUNCATE TABLE t2;
ERROR HY000: Lost connection to MySQL server during query
# restart
SELECT * FROM t2;
a
10
11
SET SESSION DEBUG = "+d,ib_truncate_crash_after_drop_old_table";
TRUNCATE TABLE t2;
ERROR HY000: Lost connection to MySQL server during query
# restart
SELECT * FROM t2;
a
10
11
SET SESSION DEBUG = "+d,ib_truncate_crash_after_create_new_table";
TRUNCATE TABLE t2;
ERROR HY000: Lost connection to MySQL server during query
# restart
SELECT * FROM t2;
a
10
11
SET SESSION DEBUG = "+d,ib_truncate_crash_after_innodb_complete";
TRUNCATE TABLE t2;
ERROR HY000: Lost connection to MySQL server during query
# restart
SELECT * FROM t2;
a
10
11
SET SESSION DEBUG = "+d,ib_truncate_rollback_test";
TRUNCATE TABLE t2;
ERROR HY000: Got error 168 - 'Unknown (generic) error from engine' from storage engine
SELECT * FROM t2;
a
10
11
DROP TABLE t2;
