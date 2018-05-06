RESET MASTER;
CREATE TABLE t1 (a int) ENGINE= innodb;
include/include/assert_logical_timestamps.inc [0 1]
INSERT INTO t1 SET a=1;
include/include/assert_logical_timestamps.inc [1 2]
[connection one]
BEGIN;
INSERT INTO t1 SET a=1;
[connection two]
BEGIN;
INSERT INTO t1 SET a=2;
COMMIT;
[connection one]
INSERT INTO t1 SET a=1;
[connection two]
BEGIN;
INSERT INTO t1 SET a=2;
COMMIT;
include/include/assert_logical_timestamps.inc [3 4]
[connection one]
COMMIT;
include/include/assert_logical_timestamps.inc [3 5]
[connection one]
INSERT INTO t1 SET a=1;
[connection two]
INSERT INTO t1 SET a=2;
[connection one]
[connection two]
include/include/assert_logical_timestamps.inc [5 6;[56] 7]
[connection one]
RESET MASTER;
INSERT INTO t1 SET a=1;
[connection two]
BEGIN;
INSERT INTO t1 SET a=2;
[connection one]
BEGIN;
INSERT INTO t1 SET a=3;
[connection two]
COMMIT;
include/include/assert_logical_timestamps.inc [0 1;1 2]
FLUSH LOGS;
[connection one]
COMMIT;
include/include/assert_logical_timestamps.inc [0 1]
[connection one]
RESET MASTER;
INSERT INTO t1 SET a=1;
[connection two]
BEGIN;
INSERT INTO t1 SET a=2;
[connection one]
BEGIN;
INSERT INTO t1 SET a=3;
[connection three]
BEGIN;
INSERT INTO t1 SET a=4;
[connection two]
COMMIT;
include/include/assert_logical_timestamps.inc [0 1;1 2]
RESET MASTER;
[connection one]
COMMIT;
[connection three]
COMMIT;
include/include/assert_logical_timestamps.inc [0 1;0 2]
DROP TABLE t1;
CREATE TABLE t1 (a int) ENGINE= innodb;
CREATE TABLE tm (a int) ENGINE= MyISAM;
RESET MASTER;
CREATE TEMPORARY TABLE ttm1 (a INT) ENGINE=MyISAM;
CREATE TEMPORARY TABLE tti1 (a INT) ENGINE=Innodb;
*** The query is logged in four part.
DROP TABLE tm,t1,ttm1,tti1;
include/include/assert_logical_timestamps.inc [logical_timestamps=0 1;1 2;2 3;3 4;4 5;5 6 logical_timestamps_mix=0 1;1 2]
CALL mtr.add_suppression("Some non-transactional changed tables couldn't be rolled back");
CREATE TABLE t1 (a int) ENGINE= innodb;
CREATE TABLE tm (a int) ENGINE= MyISAM;
CREATE PROCEDURE p1 ()
BEGIN
START TRANSACTION;
INSERT INTO t1 SET a=1;
INSERT INTO tm SET a=2;
COMMIT;
START TRANSACTION;
INSERT INTO t1 SET a=1;
INSERT INTO tm SET a=2;
ROLLBACK;
END|
CREATE PROCEDURE p2 ()
BEGIN
START TRANSACTION;
INSERT INTO t1 SET a=1;
COMMIT;
START TRANSACTION;
INSERT INTO t1 SET a=2;
COMMIT;
START TRANSACTION;
INSERT INTO t1 SET a=3;
COMMIT;
END|
RESET MASTER;
START TRANSACTION;
INSERT INTO t1 SET a=1;
INSERT INTO tm SET a=2;
COMMIT;
include/include/assert_logical_timestamps.inc [0 1;1 2]
RESET MASTER;
START TRANSACTION;
INSERT INTO t1 SET a=1;
INSERT INTO tm SET a=2;
ROLLBACK;
Warnings:
Warning	1196	Some non-transactional changed tables couldn't be rolled back
include/include/assert_logical_timestamps.inc [0 1]
RESET MASTER;
CALL p1();
Warnings:
Warning	1196	Some non-transactional changed tables couldn't be rolled back
include/include/assert_logical_timestamps.inc [0 1;1 2;2 3]
RESET MASTER;
CALL p2();
include/include/assert_logical_timestamps.inc [0 1;1 2;2 3]
DROP PROCEDURE p1;
DROP PROCEDURE p2;
DROP TABLE tm,t1;
