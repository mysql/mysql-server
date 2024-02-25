##########################################################################
# Test script to test UPGRADE from older version table having
# INSTANT ADD columns and try UPDATE/ROLLBACK on them. The tables have
# row fitting exactly to page bounday and UPDATE will cause fields to be
# stored off page. Whereas ROLLBACK shall bring it back to in page.

# NOTE : This behavior isn't there for tables with row versions.
#
# $row_format is to be set to the row_format on which test is to be run.
##########################################################################

--echo # ------------------------------------------------------------
--echo # Start server with old datadir with
--echo #   table t1_$row_format having
--echo #     c1 normal column
--echo #     c2 INSTANT ADD column
--echo #
--echo #   table t2_$row_format having
--echo #     id, c1 normal columns
--echo #     c2 INSTANT ADD column
--echo # ------------------------------------------------------------

# -------------------
# Table t1 with no PK
# -------------------
eval SHOW CREATE TABLE t1_$row_format;

BEGIN;
eval UPDATE t1_$row_format SET c2 = 0;
ROLLBACK;

eval SELECT SUBSTRING(c1, 1000, 32) FROM t1_$row_format;

# -------------------
# Table t2 with PK
# -------------------
eval SHOW CREATE TABLE t2_$row_format;

BEGIN;
eval UPDATE t2_$row_format SET c2 = 0;
ROLLBACK;

eval SELECT SUBSTRING(c1, 1000, 32) FROM t2_$row_format;


