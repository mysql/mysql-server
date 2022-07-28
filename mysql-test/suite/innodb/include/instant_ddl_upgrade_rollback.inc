##########################################################################
# Test script to test UPGRADE from older version table having
# INSTANT ADD columns.
#
# $row_format is to be set to the row_format on which test is to be run.
##########################################################################

--echo # ------------------------------------------------------------
--echo # Start server with old datadir with
--echo #   table t1_$row_format having
--echo #     id, c1, c2 normal columns
--echo #     c3 INSTANT ADD column with default.
--echo # ------------------------------------------------------------

--disable_warnings
eval SHOW CREATE TABLE t1_$row_format;
--enable_warnings

--echo # ------------------------------------------------------------
--echo # Read rows from upgraded table t1_$row_format
--echo # ------------------------------------------------------------
--let $table_name=t1_$row_format
--source suite/innodb/include/print_instant_metadata.inc
eval SELECT id, LEFT(c1 , 10), LEFT(c2, 10), LEFT(c3, 10) FROM t1_$row_format ORDER BY id;

--echo # ------------------------------------------------------------
--echo # Run an UPDATE and ROLLBACK. 
--echo # ------------------------------------------------------------
START TRANSACTION;
eval UPDATE t1_$row_format SET c1 = repeat('x', 200) WHERE id = 1;
eval SELECT id, LEFT(c1 , 10), LEFT(c2, 10), LEFT(c3, 10) FROM t1_$row_format ORDER BY id;
ROLLBACK;
eval SELECT id, LEFT(c1 , 10), LEFT(c2, 10), LEFT(c3, 10) FROM t1_$row_format ORDER BY id;
