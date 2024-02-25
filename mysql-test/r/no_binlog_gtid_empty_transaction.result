include/gtid_utils.inc
RESET MASTER;
include/gtid_step_reset.inc
SET @@SESSION.GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';
COMMIT;
SET @@SESSION.GTID_NEXT = 'AUTOMATIC';
include/gtid_step_assert.inc [count=1, only_count=1]
CREATE TABLE t1 (a INT);
CREATE PROCEDURE p1()
BEGIN
SET @@SESSION.GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2';
START TRANSACTION;
COMMIT;
SET @@SESSION.GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:3';
START TRANSACTION;
COMMIT;
END|
include/gtid_step_reset.inc
CALL p1();
SET @@SESSION.GTID_NEXT = 'AUTOMATIC';
include/gtid_step_assert.inc [count=2, only_count=1]
DROP TABLE t1;
DROP PROCEDURE IF EXISTS p1;
include/gtid_utils_end.inc
