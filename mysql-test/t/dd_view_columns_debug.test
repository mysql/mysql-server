###############################################################################
#                                                                             #
#  Debug Test cases to verify storing view column information in DD.COLUMNS   #
#  table and update view column information and other values on DDL           #
#  operations.                                                                #
#                                                                             #
###############################################################################

--source include/have_debug.inc
--source include/have_debug_sync.inc

--source include/count_sessions.inc

--enable_connect_log

--echo #
--echo # Bug#26322203 - SHOW FIELDS FROM A VALID VIEW FAILS WITH AN INVALID VIEW ERROR
--echo #

CREATE TABLE t1 (f1 int);
CREATE VIEW v1 AS SELECT * FROM t1;

--echo # Wait after listing view "v1" referencing table "t1" for state update.
SET DEBUG_SYNC="after_preparing_view_tables_list SIGNAL alter_view WAIT_FOR go";
--send DROP TABLE t1;

connect (con1, localhost, root,,);
SET DEBUG_SYNC='now WAIT_FOR alter_view';
--echo # Before state of the view v1 is updated ALTER view so that v1 is no more
--echo # dependent on the table t1 being dropped.
ALTER VIEW v1 AS select 13;
SET DEBUG_SYNC='now SIGNAL go';

connection default;
--echo # Without fix even if v1 is not dependent table/view or stored function
--echo # being dropped because of concurrent DDL operation on the view v1, state
--echo # of the view is updated as invalid.
--echo #
--echo # With fix, while updating view state if view is no more dependent on
--echo # the object being dropped then view state is *not* updated.
--reap
FLUSH TABLES;
--echo # Without fix, SHOW FIELDS from valid view "v1" reports invalid view error.
SHOW FIELDS FROM v1;

connection default;
SET DEBUG_SYNC='RESET';
DROP VIEW v1;
disconnect con1;
--disable_connect_log

--source include/wait_until_count_sessions.inc
###########################################################################
