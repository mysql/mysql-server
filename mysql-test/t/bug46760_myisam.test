--source include/force_myisam_default.inc
--source include/have_myisam.inc

--echo #
--echo # MySQL Bug#39200: optimize table does not recognize
--echo # ROW_FORMAT=COMPRESSED
--echo #

CREATE TABLE t1 (a INT) ROW_FORMAT=compressed, ENGINE=MyISAM;
SHOW CREATE TABLE t1;
OPTIMIZE TABLE t1;
SHOW CREATE TABLE t1;
DROP TABLE t1;
