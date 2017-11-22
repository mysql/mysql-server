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
