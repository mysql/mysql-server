###############################################################################
#                                                                             #
# WL7167- Change DDL to update rows for view columns in DD.COLUMNS and other  #
#         dependent values.                                                   #
#                                                                             #
#         Test cases to verify storing view column information in DD.COLUMNS  #
#         table and update view column information and other values on DDL    #
#         operations.                                                         #
#                                                                             #
###############################################################################

CREATE FUNCTION sf1() RETURNS INT return 0;
CREATE TABLE t1 (f1 INT);
CREATE TABLE t2 (f2 INT);
CREATE TABLE t3 (f3 INT);

PREPARE check_view_columns FROM
  'SELECT table_name, column_name, column_type FROM information_schema.columns
   WHERE table_name= ? ORDER BY table_name, column_name';
PREPARE check_view_status FROM
  'SELECT table_name, table_comment FROM information_schema.tables WHERE
   table_name= ?';

--echo # View using only table t2;
CREATE VIEW v1 AS SELECT * FROM t1;
--echo # Verify view v1 columns information.
EXECUTE check_view_columns USING @view;

--echo # View using only table t2
CREATE VIEW v2 AS SELECT * FROM t2;
SET @view='v2';
--echo # Verify view v2 columns information.
EXECUTE check_view_columns USING @view;

--echo # View using only table t3 and view v2.
CREATE VIEW v3 AS SELECT * FROM v2, t3;
SET @view='v3';
--echo # Verify view v3 columns information.
EXECUTE check_view_columns USING @view;

--echo # View using stored function sf1.
CREATE VIEW v4 AS SELECT sf1() AS sf;
SET @view='v4';
--echo # Verify view v4 columns information.
EXECUTE check_view_columns USING @view;

--echo # View using only view v4.
create view v5 as select * from v4;
SET @view='v5';
--echo # Verify view v5 columns information.
EXECUTE check_view_columns USING @view;

--echo #
--echo # CASE 1 -  Add new column to the table t1. Verify view v1 column
--echo #           information.
--echo # EXPECTATION: No change in the view columns information.
--echo #
ALTER TABLE t1 ADD f2 INT;
SET @view='v1';
EXECUTE check_view_columns USING @view;
--echo #
--echo #

--echo # 
--echo # CASE 2 - Modify type of a column t1.f1. Verify view v1 column information.
--echo # EXPECTATION: Change in view column v1.f1 type.
--echo #
ALTER TABLE t1 CHANGE f1 f1 CHAR(100);
SET @view='v1';
EXECUTE check_view_columns USING @view;
--echo #
--echo #

--echo #
--echo # CASE 3 - Modify type of column a t2.f2. Verify view v2 and v3 column
--echo #          information.
--echo # EXPECTATION: Change in view column v2.f2 and v3.f2 type.
--echo #
ALTER TABLE t2 CHANGE f2 f2 CHAR(100);
SET @view='v2';
EXECUTE check_view_columns USING @view;
SET @view='v3';
--sorted_result
EXECUTE check_view_columns USING @view;
--echo #
--echo #

--echo #
--echo # CASE 4 - Drop column used by a view from its base table. Verify view 
--echo #          v1's status.
--echo # EXPECTATION: Invalid view warning should be reported.
--echo #
SET @view='v1';
EXECUTE check_view_status USING @view;
ALTER TABLE t1 DROP f1;
EXECUTE check_view_status USING @view;
--echo #
--echo #

--echo #
--echo # CASE 5 - Add column used by a view with same name but different type to
--echo #          its base table. Verify view v1's status.
--echo # EXPECTATION: Invalid view warning should be reported.
--echo #
SET @view='v1';
ALTER TABLE t2 ADD f1 DATETIME;
EXECUTE check_view_status USING @view;
ALTER TABLE t2 DROP f1;
--echo #
--echo #

--echo #
--echo # CASE 6 - Add column used by a view to its base table. Verify view
--echo #          v1's status.
--echo # EXPECTATION: v1 view becomes valid again. Invalid view warning is
--echo #              *not* reported.
--echo #
SET @view='v1';
EXECUTE check_view_status USING @view;
ALTER TABLE t1 ADD f1 int;
EXECUTE check_view_status USING @view;
EXECUTE check_view_columns USING @view;
--echo #
--echo #

--echo #
--echo # CASE 7 - Rename column used by a view from its base table. Verify view 
--echo #          v2 and v3 status.
--echo # EXPECTATION: Invalid view warning should be reported.
--echo #
ALTER TABLE t2 CHANGE f2 f5 int;
SET @view='v2';
EXECUTE check_view_status USING @view;
SET @view='v3';
EXECUTE check_view_status USING @view;
--skip_if_hypergraph   # Different warnings.
EXECUTE check_view_columns USING @view;
--echo #
--echo #

--echo #
--echo # CASE 8 - Rename back to original column names used by a view from its
--echo #          base table.
--echo #          Verify view v2 and v3 status.
--echo # EXPECTATION: v2 and v3 becomes valid again. Invalid view warning is
--echo #              *not* reported.
--echo #
ALTER TABLE t2 CHANGE f5 f2 int;
SET @view='v2';
EXECUTE check_view_status USING @view;
EXECUTE check_view_columns USING @view;
SET @view='v3';
EXECUTE check_view_status USING @view;
EXECUTE check_view_columns USING @view;
--echo #
--echo #

--echo #
--echo # CASE 9 - Drop base table used by a view. Verify view v2 and v3 status.
--echo # EXPECTATION: Invalid view warning should be reported.
--echo #
DROP TABLE t2;
SET @view='v2';
EXECUTE check_view_status USING @view;
SET @view='v3';
EXECUTE check_view_status USING @view;
--echo #
--echo #

--echo #
--echo # CASE 10 - Create table t2 with different column name.
--echo #           Verify view v2 and v3 status.
--echo # EXPECTATION: Invalid view warning should be reported.
--echo #
CREATE TABLE t2(f4 int);
SET @view='v2';
EXECUTE check_view_status USING @view;
SET @view='v3';
EXECUTE check_view_status USING @view;
DROP TABLE t2;
--echo #
--echo #

--echo #
--echo # CASE 11: Create table t2 with original column name.
--echo #          Verify view v2 and v3 status.
--echo # EXPECTATION: v2 and v3 becomes valid again. Invalid view warning
--echo #              is *not* reported.
--echo #
CREATE TABLE t2(f2 int);
SET @view='v2';
EXECUTE check_view_status USING @view;
SET @view='v3';
EXECUTE check_view_status USING @view;
--echo #
--echo #

--echo #
--echo # CASE 12 - Rename base table used by a view. Verify view v2 and v3
--echo #           status.
--echo # EXPECTATION: Invalid view warning should be reported.
--echo #
RENAME TABLE t2 TO t5;
SET @view='v2';
EXECUTE check_view_status USING @view;
SET @view='v3';
EXECUTE check_view_status USING @view;

CREATE VIEW vw AS SELECT * FROM t5;
SET @view='vw';
--echo # View "vw" is valid. Invalid view warning is *not* reported for it.
EXECUTE check_view_status USING @view;
--echo #
--echo #

--echo #
--echo # CASE 13 - Rename table t5 back to t2. Verify view v2 and v3 status.
--echo # EXPECTATION: v2 and v3 becomes valid again. Invalid view warning is
--echo #              *not* reported.
--echo #
RENAME TABLE t5 to t2;
SET @view='v2';
EXECUTE check_view_status USING @view;
SET @view='v3';
EXECUTE check_view_status USING @view;
SET @view='vw';

--echo # Invalid view warning should be reported for vw.
EXECUTE check_view_status USING @view;
DROP VIEW vw;
--echo #
--echo #

--echo #
--echo # CASE 14 - Rename base table used by a view using ALTER TABLE statement.
--echo #           Verify view v2 and v3 status.
--echo # EXPECTATION: Invalid view warning should be reported.
--echo #
ALTER TABLE t2 RENAME t5;
SET @view='v2';
EXECUTE check_view_status USING @view;
SET @view='v3';
EXECUTE check_view_status USING @view;

CREATE VIEW vw AS SELECT * FROM t5;
SET @view='vw';
--echo # View "vw" is valid. Invalid view warning is *not* reported for it.
EXECUTE check_view_status USING @view;
--echo #
--echo #

--echo #
--echo # CASE 15 - Rename table t5 back to t2. Verify view v2 and v3 status.
--echo # EXPECTATION: v2 and v3 becomes valid again. Invalid view warning
--echo #              is *not* reported.
--echo #
ALTER TABLE t5 RENAME t2;
SET @view='v2';
EXECUTE check_view_status USING @view;
SET @view='v3';
EXECUTE check_view_status USING @view;
SET @view='vw';

--echo # Invalid view warning should be reported for vw.
EXECUTE check_view_status USING @view;
DROP VIEW vw;
--echo #
--echo #

--echo #
--echo # CASE 16 - Drop function sf1. Verify view v4 and v5 status.
--echo # EXPECTATION: Invalid view warning should be reported.
--echo #
DROP FUNCTION sf1;
SET @view='v4';
EXECUTE check_view_status USING @view;
SET @view='v5';
EXECUTE check_view_status USING @view;
--echo #
--echo #

--echo #
--echo # CASE 17 - Recreate function sf1. Verify view v4 and v5 status.
--echo # EXPECTATION: v4 and v5 becomes valid again. Invalid view warning
--echo #              is *not* reported.
--echo #
CREATE FUNCTION sf1() RETURNS INT return 0;
SET @view='v4';
EXECUTE check_view_status USING @view;
SET @view='v5';
EXECUTE check_view_status USING @view;
--echo #
--echo #

--echo #
--echo # CASE 18 - Drop view used in a view query. Verify view v3 status
--echo # EXPECTATION: Invalid view warning should be reported.
--echo #
SET @view='v3';
EXECUTE check_view_status USING @view;
DROP VIEW v2;
EXECUTE check_view_status USING @view;

--echo #
--echo # CASE 19 - Recreate view v2. Verify view v3 status.
--echo # EXPECTATION: v3 view becomes valid again. Invalid view warning is
--echo #              *not* reported.
--echo #
CREATE VIEW v2 AS SELECT * FROM t2;
SET @view='v3';
EXECUTE check_view_status USING @view;

--echo #
--echo # CASE 20 - Alter view v2 to non-updatable. Verify view v2 and vw status.
--echo # EXPECTATION: v2 and vw should be set as non-updatable.
--echo #
--sorted_result
SELECT table_schema, table_name, is_updatable FROM information_schema.views
  WHERE table_name='v2';

ALTER VIEW v2 AS SELECT * FROM t2 GROUP BY(f2);
CREATE VIEW vw AS SELECT * FROM v2;

--sorted_result
SELECT table_schema, table_name, is_updatable FROM information_schema.views
  WHERE table_name='v2' OR table_name='vw';

--echo #
--echo # CASE 21 - Alter view v2 to updatable. Verify view v2 and vw status.
--echo # EXPECTATION - v2 and vw should be set as updatable.
--echo #
ALTER VIEW v2 AS SELECT * FROM t2;

--sorted_result
SELECT table_schema, table_name, is_updatable FROM information_schema.views
  WHERE table_name='v2' OR table_name='vw';
DROP VIEW vw;

--echo #
--echo # CASE 22 - Lock table t1 and add new column to it. Verify view v1
--echo #          columns.
--echo # EXPECTATION: There should not be any errors related locking and no 
--echo #              change in view v1 columns.
--echo #
LOCK TABLE t1 WRITE;
ALTER TABLE t1 ADD f3 INT;
UNLOCK TABLES;
SET @view='v1';
EXECUTE check_view_columns USING @view;
--echo #
--echo #

--echo #
--echo # CASE 23 - Lock table t1 and modify type of column t1.f1.
--echo #           Verify view v1's columns.
--echo # EXPECTATION: There should not be any errors related locking and 
--echo #              change in view v1.f1 column type.
--echo #
SET @view='v1';
EXECUTE check_view_columns using @view;
LOCK TABLE t1 WRITE;
ALTER TABLE t1 CHANGE f1 f1 INT;
UNLOCK TABLES;
EXECUTE check_view_columns using @view;
--echo #
--echo #

--echo #
--echo # CASE 24 - Lock table t2 and modify type of column t2.f2.
--echo #           Verify view v2 and v3's columns.
--echo # EXPECTATION: There should not be any errors related locking and 
--echo #              change in view v2.f2 and v3.f2 types.
--echo #
SET @view='v2';
EXECUTE check_view_columns USING @view;
SET @view='v3';
EXECUTE check_view_columns USING @view;
LOCK TABLE t2 WRITE;
ALTER TABLE t2 CHANGE f2 f2 INT;
UNLOCK TABLES;
SET @view='v2';
EXECUTE check_view_columns USING @view;
SET @view='v3';
EXECUTE check_view_columns USING @view;
--echo #
--echo #

--echo #
--echo # CASE 25 - Lock table t1 and drop column from it.
--echo #           Verify view v1's status.
--echo # EXPECTATION: There should not be any errors related locking and 
--echo #              invalid view warning should be reported for view v1.
--echo #
SET @view='v1';
EXECUTE check_view_status USING @view;
LOCK TABLE t1 WRITE;
ALTER TABLE t1 DROP f1;
UNLOCK TABLES;
EXECUTE check_view_status USING @view;
--echo #
--echo #

--echo #
--echo # CASE 26 - Lock table t1 and add column back to the table.
--echo #           Verify view v1's status.
--echo # EXPECTATION: There should not be any errors related locking and 
--echo #              View v1 becomes valid again. Invalid view error should *not* be
--echo #              reported for view v1.
--echo #
SET @view='v1';
EXECUTE check_view_status USING @view;
LOCK TABLE t1 WRITE;
ALTER TABLE t1 ADD f1 int;
UNLOCK TABLES;
EXECUTE check_view_status USING @view;
--echo #
--echo #

--echo #
--echo # CASE 27 - Lock table t2 and rename column used by a view from its base
--echo #           table. Verify view v2 and v3's status.
--echo # EXPECTATION: There should not be any errors related locking and 
--echo #              invalid view warning should be reported for views v2 and v3.
--echo #
LOCK TABLE t2 WRITE;
ALTER TABLE t2 CHANGE f2 f5 int;
UNLOCK TABLES;
SET @view='v2';
EXECUTE check_view_status USING @view;
SET @view='v3';
EXECUTE check_view_status USING @view;
--echo #
--echo #

--echo #
--echo # CASE 28 - Lock table t2 and rename column used by a view from its base
--echo #           table to original name. Verify view v2 and v3's status.
--echo # EXPECTATION: There should not be any errors related locking and 
--echo #              View v2 and v3 becomes valid again. Invalid view error should
--echo #              *not* be reported for view v2 and v3.
--echo #
LOCK TABLE t2 WRITE;
ALTER TABLE t2 CHANGE f5 f2 int;
UNLOCK TABLES;
SET @view='v2';
EXECUTE check_view_status USING @view;
SET @view='v3';
EXECUTE check_view_status USING @view;
--echo #
--echo #

--echo #
--echo # CASE 29 - Lock table t2 and drop table used by view.
--echo #           Verify view v2 and v3's status.
--echo # EXPECTATION: There should not be any errors related locking and 
--echo #              invalid view warning should be reported for views v2 and v3.
--echo #
LOCK TABLE t2 WRITE;
DROP TABLE t2;
UNLOCK TABLES;
SET @view='v2';
EXECUTE check_view_status USING @view;
SET @view='v3';
EXECUTE check_view_status USING @view;
--echo #
--echo #

# Cleanup
DROP VIEW v1,v2,v3,v4,v5;
DROP TABLES t1,t3;
DROP FUNCTION sf1;
DEALLOCATE PREPARE check_view_columns;
DEALLOCATE PREPARE check_view_status;


--echo #
--echo # Bug#24834622 - ASSERT IN UPDATING VIEW METADATA FOR SYS SCHEMA VIEW.
--echo #

--echo # Creating views with dependency on view/base table/stored functions
--echo # as below,
--echo #    View v2 is dependent on stored function f2.
--echo #    View v1 is dependent on stored function f1 and view v2.
--echo #    View z1 is dependent on view v2 and table t1 (having datetime column).

CREATE TABLE t1 (f1 DATETIME default '2016-11-01');

CREATE FUNCTION f1() RETURNS INT return 1;
CREATE FUNCTION f2() RETURNS INT return 2;

CREATE VIEW v2 AS SELECT f2() AS f2;
CREATE VIEW v1 AS SELECT v2.f2 AS f2,
                         a3.x as f3 from v2,
                  (SELECT a.x FROM (SELECT f1() AS x)
                          as a HAVING a.x=1) as a3;
CREATE VIEW z1 AS SELECT t1.f1 AS f1, v2.f2 AS f2 FROM t1, v2;

--echo # View v1 is dependent on stored function f1 and view v2.
--echo # Dropping stored function f1 leaves view v1 in invalid state.
DROP FUNCTION f1;

--echo # View v2 is dependent on stored function f2.
--echo # Dropping stored function f2 leaves view v2 and views using v2
--echo # (i.e v1 and z1) in invalid state.
DROP FUNCTION f2;

--echo # Recreating the stored function f2 updates columns of dependent view v2
--echo # and views dependent on v2 (i.e. v1 and z1) in DD tables COLUMNS.
--echo # View v1 is dependent on f1 too. So parsing view v1 fails while
--echo # updating views column metadata.
--echo # Without fix, THD::derived_tables_processing is not unset to false
--echo # on view v1 parse failure. This causes an assert condition failure while
--echo # updating metadata of datatime type column of view z1 as read flag
--echo # for column is not set with the THD::derived_tables_processing on.
CREATE FUNCTION f2() RETURNS INT return 2;

# Cleanup
DROP FUNCTION f2;
DROP VIEW v1, v2, z1;
DROP TABLE t1;


--echo #
--echo # Bug #27041536 "TRANS_TABLE || !CHANGED || THD->GET_TRANSACTION()->
--echo #                CANNOT_SAFELY_ROLLBACK..."
--echo #
--echo # Create view with dependency on table to be created by
--echo # CREATE TABLE SELECT.
CREATE TABLE t1 (i INT);
CREATE VIEW v1 AS SELECT * FROM t1;
DROP TABLE t1;
--echo # The below statement caused assertion failure before the fix.
CREATE TABLE t1 ENGINE=InnoDB SELECT 1 AS i;
DROP TABLE t1;
DROP VIEW v1;


--echo #
--echo # Bug#24619634 - mtr's check of main.mysqldump fails.
--echo #
CREATE TABLE t1(f1 LONGTEXT);
CREATE TABLE t2 (f2 INT);
CREATE FUNCTION func(param LONGTEXT) RETURNS LONGTEXT RETURN param;
CREATE VIEW v1 AS SELECT f2 FROM t2;
CREATE VIEW v2 AS SELECT func(f1), f2
                         FROM t1 AS stmt
                         JOIN v1 AS tab1;

SELECT table_name, view_definition FROM information_schema.views
                                   WHERE table_name='v2';
--echo # Flush cached TABLE objects.
FLUSH TABLES;
--echo # View v1 is dependent on t2 and view v2 is dependent on v1. Altering
--echo # table t2 updates view v1 and v2 metadata too.
ALTER TABLE t2 MODIFY f2 LONGTEXT;

--echo # Without fix schema name is prefixed to the argument in function "func"
--echo # of view v2. View definition looks like below without the fix,
--echo # select `func`(`test`.`stmt`.`f1`) AS `func(f1)`,`tab1`.`f2` AS `f2` ..
--echo #                ^^^^^^
SELECT table_name, view_definition FROM information_schema.views
                                   WHERE table_name='v2';

# Cleanup
DROP TABLE t1, t2;
DROP VIEW v1, v2;
DROP FUNCTION func;
