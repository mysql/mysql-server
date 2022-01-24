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
# View using only table t2;
CREATE VIEW v1 AS SELECT * FROM t1;
# Verify view v1 columns information.
EXECUTE check_view_columns USING @view;
TABLE_NAME	COLUMN_NAME	COLUMN_TYPE
# View using only table t2
CREATE VIEW v2 AS SELECT * FROM t2;
SET @view='v2';
# Verify view v2 columns information.
EXECUTE check_view_columns USING @view;
TABLE_NAME	COLUMN_NAME	COLUMN_TYPE
v2	f2	int
# View using only table t3 and view v2.
CREATE VIEW v3 AS SELECT * FROM v2, t3;
SET @view='v3';
# Verify view v3 columns information.
EXECUTE check_view_columns USING @view;
TABLE_NAME	COLUMN_NAME	COLUMN_TYPE
v3	f2	int
v3	f3	int
# View using stored function sf1.
CREATE VIEW v4 AS SELECT sf1() AS sf;
SET @view='v4';
# Verify view v4 columns information.
EXECUTE check_view_columns USING @view;
TABLE_NAME	COLUMN_NAME	COLUMN_TYPE
v4	sf	int
# View using only view v4.
create view v5 as select * from v4;
SET @view='v5';
# Verify view v5 columns information.
EXECUTE check_view_columns USING @view;
TABLE_NAME	COLUMN_NAME	COLUMN_TYPE
v5	sf	int
#
# CASE 1 -  Add new column to the table t1. Verify view v1 column
#           information.
# EXPECTATION: No change in the view columns information.
#
ALTER TABLE t1 ADD f2 INT;
SET @view='v1';
EXECUTE check_view_columns USING @view;
TABLE_NAME	COLUMN_NAME	COLUMN_TYPE
v1	f1	int
#
#
# 
# CASE 2 - Modify type of a column t1.f1. Verify view v1 column information.
# EXPECTATION: Change in view column v1.f1 type.
#
ALTER TABLE t1 CHANGE f1 f1 CHAR(100);
SET @view='v1';
EXECUTE check_view_columns USING @view;
TABLE_NAME	COLUMN_NAME	COLUMN_TYPE
v1	f1	char(100)
#
#
#
# CASE 3 - Modify type of column a t2.f2. Verify view v2 and v3 column
#          information.
# EXPECTATION: Change in view column v2.f2 and v3.f2 type.
#
ALTER TABLE t2 CHANGE f2 f2 CHAR(100);
SET @view='v2';
EXECUTE check_view_columns USING @view;
TABLE_NAME	COLUMN_NAME	COLUMN_TYPE
v2	f2	char(100)
SET @view='v3';
EXECUTE check_view_columns USING @view;
TABLE_NAME	COLUMN_NAME	COLUMN_TYPE
v3	f2	char(100)
v3	f3	int
#
#
#
# CASE 4 - Drop column used by a view from its base table. Verify view 
#          v1's status.
# EXPECTATION: Invalid view warning should be reported.
#
SET @view='v1';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v1	VIEW
ALTER TABLE t1 DROP f1;
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v1	View 'test.v1' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
Warnings:
Warning	1356	View 'test.v1' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
#
#
#
# CASE 5 - Add column used by a view with same name but different type to
#          its base table. Verify view v1's status.
# EXPECTATION: Invalid view warning should be reported.
#
SET @view='v1';
ALTER TABLE t2 ADD f1 DATETIME;
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v1	View 'test.v1' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
Warnings:
Warning	1356	View 'test.v1' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
ALTER TABLE t2 DROP f1;
#
#
#
# CASE 6 - Add column used by a view to its base table. Verify view
#          v1's status.
# EXPECTATION: v1 view becomes valid again. Invalid view warning is
#              *not* reported.
#
SET @view='v1';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v1	View 'test.v1' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
Warnings:
Warning	1356	View 'test.v1' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
ALTER TABLE t1 ADD f1 int;
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v1	VIEW
EXECUTE check_view_columns USING @view;
TABLE_NAME	COLUMN_NAME	COLUMN_TYPE
v1	f1	int
#
#
#
# CASE 7 - Rename column used by a view from its base table. Verify view 
#          v2 and v3 status.
# EXPECTATION: Invalid view warning should be reported.
#
ALTER TABLE t2 CHANGE f2 f5 int;
SET @view='v2';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v2	View 'test.v2' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
Warnings:
Warning	1356	View 'test.v2' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
SET @view='v3';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v3	View 'test.v3' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
Warnings:
Warning	1356	View 'test.v3' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
EXECUTE check_view_columns USING @view;
TABLE_NAME	COLUMN_NAME	COLUMN_TYPE
Warnings:
Warning	1356	View 'test.v3' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
#
#
#
# CASE 8 - Rename back to original column names used by a view from its
#          base table.
#          Verify view v2 and v3 status.
# EXPECTATION: v2 and v3 becomes valid again. Invalid view warning is
#              *not* reported.
#
ALTER TABLE t2 CHANGE f5 f2 int;
SET @view='v2';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v2	VIEW
EXECUTE check_view_columns USING @view;
TABLE_NAME	COLUMN_NAME	COLUMN_TYPE
v2	f2	int
SET @view='v3';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v3	VIEW
EXECUTE check_view_columns USING @view;
TABLE_NAME	COLUMN_NAME	COLUMN_TYPE
v3	f2	int
v3	f3	int
#
#
#
# CASE 9 - Drop base table used by a view. Verify view v2 and v3 status.
# EXPECTATION: Invalid view warning should be reported.
#
DROP TABLE t2;
SET @view='v2';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v2	View 'test.v2' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
Warnings:
Warning	1356	View 'test.v2' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
SET @view='v3';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v3	View 'test.v3' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
Warnings:
Warning	1356	View 'test.v3' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
#
#
#
# CASE 10 - Create table t2 with different column name.
#           Verify view v2 and v3 status.
# EXPECTATION: Invalid view warning should be reported.
#
CREATE TABLE t2(f4 int);
SET @view='v2';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v2	View 'test.v2' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
Warnings:
Warning	1356	View 'test.v2' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
SET @view='v3';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v3	View 'test.v3' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
Warnings:
Warning	1356	View 'test.v3' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
DROP TABLE t2;
#
#
#
# CASE 11: Create table t2 with original column name.
#          Verify view v2 and v3 status.
# EXPECTATION: v2 and v3 becomes valid again. Invalid view warning
#              is *not* reported.
#
CREATE TABLE t2(f2 int);
SET @view='v2';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v2	VIEW
SET @view='v3';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v3	VIEW
#
#
#
# CASE 12 - Rename base table used by a view. Verify view v2 and v3
#           status.
# EXPECTATION: Invalid view warning should be reported.
#
RENAME TABLE t2 TO t5;
SET @view='v2';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v2	View 'test.v2' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
Warnings:
Warning	1356	View 'test.v2' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
SET @view='v3';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v3	View 'test.v3' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
Warnings:
Warning	1356	View 'test.v3' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
CREATE VIEW vw AS SELECT * FROM t5;
SET @view='vw';
# View "vw" is valid. Invalid view warning is *not* reported for it.
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
vw	VIEW
#
#
#
# CASE 13 - Rename table t5 back to t2. Verify view v2 and v3 status.
# EXPECTATION: v2 and v3 becomes valid again. Invalid view warning is
#              *not* reported.
#
RENAME TABLE t5 to t2;
SET @view='v2';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v2	VIEW
SET @view='v3';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v3	VIEW
SET @view='vw';
# Invalid view warning should be reported for vw.
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
vw	View 'test.vw' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
Warnings:
Warning	1356	View 'test.vw' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
DROP VIEW vw;
#
#
#
# CASE 14 - Rename base table used by a view using ALTER TABLE statement.
#           Verify view v2 and v3 status.
# EXPECTATION: Invalid view warning should be reported.
#
ALTER TABLE t2 RENAME t5;
SET @view='v2';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v2	View 'test.v2' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
Warnings:
Warning	1356	View 'test.v2' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
SET @view='v3';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v3	View 'test.v3' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
Warnings:
Warning	1356	View 'test.v3' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
CREATE VIEW vw AS SELECT * FROM t5;
SET @view='vw';
# View "vw" is valid. Invalid view warning is *not* reported for it.
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
vw	VIEW
#
#
#
# CASE 15 - Rename table t5 back to t2. Verify view v2 and v3 status.
# EXPECTATION: v2 and v3 becomes valid again. Invalid view warning
#              is *not* reported.
#
ALTER TABLE t5 RENAME t2;
SET @view='v2';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v2	VIEW
SET @view='v3';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v3	VIEW
SET @view='vw';
# Invalid view warning should be reported for vw.
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
vw	View 'test.vw' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
Warnings:
Warning	1356	View 'test.vw' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
DROP VIEW vw;
#
#
#
# CASE 16 - Drop function sf1. Verify view v4 and v5 status.
# EXPECTATION: Invalid view warning should be reported.
#
DROP FUNCTION sf1;
SET @view='v4';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v4	View 'test.v4' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
Warnings:
Warning	1356	View 'test.v4' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
SET @view='v5';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v5	View 'test.v5' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
Warnings:
Warning	1356	View 'test.v5' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
#
#
#
# CASE 17 - Recreate function sf1. Verify view v4 and v5 status.
# EXPECTATION: v4 and v5 becomes valid again. Invalid view warning
#              is *not* reported.
#
CREATE FUNCTION sf1() RETURNS INT return 0;
SET @view='v4';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v4	VIEW
SET @view='v5';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v5	VIEW
#
#
#
# CASE 18 - Drop view used in a view query. Verify view v3 status
# EXPECTATION: Invalid view warning should be reported.
#
SET @view='v3';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v3	VIEW
DROP VIEW v2;
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v3	View 'test.v3' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
Warnings:
Warning	1356	View 'test.v3' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
#
# CASE 19 - Recreate view v2. Verify view v3 status.
# EXPECTATION: v3 view becomes valid again. Invalid view warning is
#              *not* reported.
#
CREATE VIEW v2 AS SELECT * FROM t2;
SET @view='v3';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v3	VIEW
#
# CASE 20 - Alter view v2 to non-updatable. Verify view v2 and vw status.
# EXPECTATION: v2 and vw should be set as non-updatable.
#
SELECT table_schema, table_name, is_updatable FROM information_schema.views
WHERE table_name='v2';
TABLE_SCHEMA	TABLE_NAME	IS_UPDATABLE
test	v2	YES
ALTER VIEW v2 AS SELECT * FROM t2 GROUP BY(f2);
CREATE VIEW vw AS SELECT * FROM v2;
SELECT table_schema, table_name, is_updatable FROM information_schema.views
WHERE table_name='v2' OR table_name='vw';
TABLE_SCHEMA	TABLE_NAME	IS_UPDATABLE
test	v2	NO
test	vw	NO
#
# CASE 21 - Alter view v2 to updatable. Verify view v2 and vw status.
# EXPECTATION - v2 and vw should be set as updatable.
#
ALTER VIEW v2 AS SELECT * FROM t2;
SELECT table_schema, table_name, is_updatable FROM information_schema.views
WHERE table_name='v2' OR table_name='vw';
TABLE_SCHEMA	TABLE_NAME	IS_UPDATABLE
test	v2	YES
test	vw	YES
DROP VIEW vw;
#
# CASE 22 - Lock table t1 and add new column to it. Verify view v1
#          columns.
# EXPECTATION: There should not be any errors related locking and no 
#              change in view v1 columns.
#
LOCK TABLE t1 WRITE;
ALTER TABLE t1 ADD f3 INT;
UNLOCK TABLES;
SET @view='v1';
EXECUTE check_view_columns USING @view;
TABLE_NAME	COLUMN_NAME	COLUMN_TYPE
v1	f1	int
#
#
#
# CASE 23 - Lock table t1 and modify type of column t1.f1.
#           Verify view v1's columns.
# EXPECTATION: There should not be any errors related locking and 
#              change in view v1.f1 column type.
#
SET @view='v1';
EXECUTE check_view_columns using @view;
TABLE_NAME	COLUMN_NAME	COLUMN_TYPE
v1	f1	int
LOCK TABLE t1 WRITE;
ALTER TABLE t1 CHANGE f1 f1 INT;
UNLOCK TABLES;
EXECUTE check_view_columns using @view;
TABLE_NAME	COLUMN_NAME	COLUMN_TYPE
v1	f1	int
#
#
#
# CASE 24 - Lock table t2 and modify type of column t2.f2.
#           Verify view v2 and v3's columns.
# EXPECTATION: There should not be any errors related locking and 
#              change in view v2.f2 and v3.f2 types.
#
SET @view='v2';
EXECUTE check_view_columns USING @view;
TABLE_NAME	COLUMN_NAME	COLUMN_TYPE
v2	f2	int
SET @view='v3';
EXECUTE check_view_columns USING @view;
TABLE_NAME	COLUMN_NAME	COLUMN_TYPE
v3	f2	int
v3	f3	int
LOCK TABLE t2 WRITE;
ALTER TABLE t2 CHANGE f2 f2 INT;
UNLOCK TABLES;
SET @view='v2';
EXECUTE check_view_columns USING @view;
TABLE_NAME	COLUMN_NAME	COLUMN_TYPE
v2	f2	int
SET @view='v3';
EXECUTE check_view_columns USING @view;
TABLE_NAME	COLUMN_NAME	COLUMN_TYPE
v3	f2	int
v3	f3	int
#
#
#
# CASE 25 - Lock table t1 and drop column from it.
#           Verify view v1's status.
# EXPECTATION: There should not be any errors related locking and 
#              invalid view warning should be reported for view v1.
#
SET @view='v1';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v1	VIEW
LOCK TABLE t1 WRITE;
ALTER TABLE t1 DROP f1;
UNLOCK TABLES;
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v1	View 'test.v1' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
Warnings:
Warning	1356	View 'test.v1' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
#
#
#
# CASE 26 - Lock table t1 and add column back to the table.
#           Verify view v1's status.
# EXPECTATION: There should not be any errors related locking and 
#              View v1 becomes valid again. Invalid view error should *not* be
#              reported for view v1.
#
SET @view='v1';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v1	View 'test.v1' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
Warnings:
Warning	1356	View 'test.v1' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
LOCK TABLE t1 WRITE;
ALTER TABLE t1 ADD f1 int;
UNLOCK TABLES;
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v1	VIEW
#
#
#
# CASE 27 - Lock table t2 and rename column used by a view from its base
#           table. Verify view v2 and v3's status.
# EXPECTATION: There should not be any errors related locking and 
#              invalid view warning should be reported for views v2 and v3.
#
LOCK TABLE t2 WRITE;
ALTER TABLE t2 CHANGE f2 f5 int;
UNLOCK TABLES;
SET @view='v2';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v2	View 'test.v2' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
Warnings:
Warning	1356	View 'test.v2' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
SET @view='v3';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v3	View 'test.v3' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
Warnings:
Warning	1356	View 'test.v3' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
#
#
#
# CASE 28 - Lock table t2 and rename column used by a view from its base
#           table to original name. Verify view v2 and v3's status.
# EXPECTATION: There should not be any errors related locking and 
#              View v2 and v3 becomes valid again. Invalid view error should
#              *not* be reported for view v2 and v3.
#
LOCK TABLE t2 WRITE;
ALTER TABLE t2 CHANGE f5 f2 int;
UNLOCK TABLES;
SET @view='v2';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v2	VIEW
SET @view='v3';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v3	VIEW
#
#
#
# CASE 29 - Lock table t2 and drop table used by view.
#           Verify view v2 and v3's status.
# EXPECTATION: There should not be any errors related locking and 
#              invalid view warning should be reported for views v2 and v3.
#
LOCK TABLE t2 WRITE;
DROP TABLE t2;
UNLOCK TABLES;
SET @view='v2';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v2	View 'test.v2' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
Warnings:
Warning	1356	View 'test.v2' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
SET @view='v3';
EXECUTE check_view_status USING @view;
TABLE_NAME	TABLE_COMMENT
v3	View 'test.v3' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
Warnings:
Warning	1356	View 'test.v3' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
#
#
DROP VIEW v1,v2,v3,v4,v5;
DROP TABLES t1,t3;
DROP FUNCTION sf1;
DEALLOCATE PREPARE check_view_columns;
DEALLOCATE PREPARE check_view_status;
#
# Bug#24834622 - ASSERT IN UPDATING VIEW METADATA FOR SYS SCHEMA VIEW.
#
# Creating views with dependency on view/base table/stored functions
# as below,
#    View v2 is dependent on stored function f2.
#    View v1 is dependent on stored function f1 and view v2.
#    View z1 is dependent on view v2 and table t1 (having datetime column).
CREATE TABLE t1 (f1 DATETIME default '2016-11-01');
CREATE FUNCTION f1() RETURNS INT return 1;
CREATE FUNCTION f2() RETURNS INT return 2;
CREATE VIEW v2 AS SELECT f2() AS f2;
CREATE VIEW v1 AS SELECT v2.f2 AS f2,
a3.x as f3 from v2,
(SELECT a.x FROM (SELECT f1() AS x)
as a HAVING a.x=1) as a3;
CREATE VIEW z1 AS SELECT t1.f1 AS f1, v2.f2 AS f2 FROM t1, v2;
# View v1 is dependent on stored function f1 and view v2.
# Dropping stored function f1 leaves view v1 in invalid state.
DROP FUNCTION f1;
# View v2 is dependent on stored function f2.
# Dropping stored function f2 leaves view v2 and views using v2
# (i.e v1 and z1) in invalid state.
DROP FUNCTION f2;
# Recreating the stored function f2 updates columns of dependent view v2
# and views dependent on v2 (i.e. v1 and z1) in DD tables COLUMNS.
# View v1 is dependent on f1 too. So parsing view v1 fails while
# updating views column metadata.
# Without fix, THD::derived_tables_processing is not unset to false
# on view v1 parse failure. This causes an assert condition failure while
# updating metadata of datatime type column of view z1 as read flag
# for column is not set with the THD::derived_tables_processing on.
CREATE FUNCTION f2() RETURNS INT return 2;
DROP FUNCTION f2;
DROP VIEW v1, v2, z1;
DROP TABLE t1;
#
# Bug #27041536 "TRANS_TABLE || !CHANGED || THD->GET_TRANSACTION()->
#                CANNOT_SAFELY_ROLLBACK..."
#
# Create view with dependency on table to be created by
# CREATE TABLE SELECT.
CREATE TABLE t1 (i INT);
CREATE VIEW v1 AS SELECT * FROM t1;
DROP TABLE t1;
# The below statement caused assertion failure before the fix.
CREATE TABLE t1 ENGINE=InnoDB SELECT 1 AS i;
DROP TABLE t1;
DROP VIEW v1;
#
# Bug#24619634 - mtr's check of main.mysqldump fails.
#
CREATE TABLE t1(f1 LONGTEXT);
CREATE TABLE t2 (f2 INT);
CREATE FUNCTION func(param LONGTEXT) RETURNS LONGTEXT RETURN param;
CREATE VIEW v1 AS SELECT f2 FROM t2;
CREATE VIEW v2 AS SELECT func(f1), f2
FROM t1 AS stmt
JOIN v1 AS tab1;
SELECT table_name, view_definition FROM information_schema.views
WHERE table_name='v2';
TABLE_NAME	VIEW_DEFINITION
v2	select `func`(`stmt`.`f1`) AS `func(f1)`,`test`.`tab1`.`f2` AS `f2` from (`test`.`t1` `stmt` join `test`.`v1` `tab1`)
# Flush cached TABLE objects.
FLUSH TABLES;
# View v1 is dependent on t2 and view v2 is dependent on v1. Altering
# table t2 updates view v1 and v2 metadata too.
ALTER TABLE t2 MODIFY f2 LONGTEXT;
# Without fix schema name is prefixed to the argument in function "func"
# of view v2. View definition looks like below without the fix,
# select `func`(`test`.`stmt`.`f1`) AS `func(f1)`,`tab1`.`f2` AS `f2` ..
#                ^^^^^^
SELECT table_name, view_definition FROM information_schema.views
WHERE table_name='v2';
TABLE_NAME	VIEW_DEFINITION
v2	select `func`(`stmt`.`f1`) AS `func(f1)`,`test`.`tab1`.`f2` AS `f2` from (`test`.`t1` `stmt` join `test`.`v1` `tab1`)
DROP TABLE t1, t2;
DROP VIEW v1, v2;
DROP FUNCTION func;
