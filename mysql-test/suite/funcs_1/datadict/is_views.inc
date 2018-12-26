# suite/funcs_1/datadict/is_views.inc
#
# Check the layout of information_schema.views and the impact of
# CREATE/ALTER/DROP TABLE/VIEW/SCHEMA ... on it.
#
# Note:
#    - This test should not check storage engine properties.
#    - Please do not change the storage engines used within this test
#      except you know that the impact is acceptable.
#      Some storage engines might not support the modification of
#      properties like in the following tests.
#
# Author:
# 2008-01-23 mleich WL#4203 Reorganize and fix the data dictionary tests of
#                           testsuite funcs_1
#                   Create this script based on older scripts and new code.
# Last Change:
# 2008-06-11 mleich Move t/is_views.test to this file and
#

let $is_table = VIEWS;

# The table INFORMATION_SCHEMA.VIEWS must exist
eval SHOW TABLES FROM information_schema LIKE '$is_table';

--echo #######################################################################
--echo # Testcase 3.2.1.1: INFORMATION_SCHEMA tables can be queried via SELECT
--echo #######################################################################
# Ensure that every INFORMATION_SCHEMA table can be queried with a SELECT
# statement, just as if it were an ordinary user-defined table.
#
--source suite/funcs_1/datadict/is_table_query.inc


--echo #########################################################################
--echo # Testcase 3.2.13.1: INFORMATION_SCHEMA.VIEWS layout
--echo #########################################################################
# Ensure that the INFORMATION_SCHEMA.VIEWS table has the following columns,
# in the following order:
#
# TABLE_CATALOG (always shows NULL),
# TABLE_SCHEMA (shows the database, or schema, in which an accessible
#       view resides),
# TABLE_NAME (shows the name of a view accessible to the current user),
# VIEW_DEFINITION (shows the SELECT statement that makes up the
#       view's definition),
# CHECK_OPTION (shows the value of the WITH CHECK OPTION clause used to define
#       the view, either NONE, LOCAL or CASCADED),
# IS_UPDATABLE (shows whether the view is an updatable view),
# DEFINER (added with 5.0.14),
# SECURITY_TYPE (added with 5.0.14).
#
--source suite/funcs_1/datadict/datadict_bug_12777.inc
eval DESCRIBE          information_schema.$is_table;
--source suite/funcs_1/datadict/datadict_bug_12777.inc
--replace_result InnoDB TMP_TABLE_ENGINE MyISAM TMP_TABLE_ENGINE 
eval SHOW CREATE TABLE information_schema.$is_table;
--source suite/funcs_1/datadict/datadict_bug_12777.inc
eval SHOW COLUMNS FROM information_schema.$is_table;

# Note: Retrieval of information within information_schema.columns about
#       information_schema.views is in is_columns_is.test.

# Show that TABLE_CATALOG is always NULL.
SELECT table_catalog, table_schema, table_name
FROM information_schema.views WHERE table_catalog IS NOT NULL order by table_name;


--echo ################################################################################
--echo # Testcase 3.2.13.2 + 3.2.13.3: INFORMATION_SCHEMA.VIEWS accessible information
--echo ################################################################################
# 3.2.13.2: Ensure that the table shows the relevant information on every view for
#           which the current user or PUBLIC has the SHOW CREATE VIEW privilege.
# 3.2.13.3: Ensure that the table does not show any information on any views for which
#           the current user and PUBLIC have no SHOW CREATE VIEW privilege.
#
--disable_warnings
DROP DATABASE IF EXISTS db_datadict;
--enable_warnings
CREATE DATABASE db_datadict;

--error 0,ER_CANNOT_USER
DROP   USER 'testuser1'@'localhost';
CREATE USER 'testuser1'@'localhost';
--error 0,ER_CANNOT_USER
DROP   USER 'testuser2'@'localhost';
CREATE USER 'testuser2'@'localhost';
--error 0,ER_CANNOT_USER
DROP   USER 'test_no_views'@'localhost';
CREATE USER 'test_no_views'@'localhost';

--replace_result $engine_type <engine_type>
eval
CREATE TABLE db_datadict.t1(f1 INT, f2 INT, f3 INT)
ENGINE = $engine_type;
CREATE VIEW db_datadict.v_granted_to_1 AS SELECT * FROM db_datadict.t1;
CREATE VIEW db_datadict.v_granted_glob AS SELECT f2, f3 FROM db_datadict.t1;

GRANT SELECT ON db_datadict.t1 TO 'testuser1'@'localhost';
GRANT SELECT ON db_datadict.v_granted_to_1 TO 'testuser1'@'localhost';
GRANT SHOW VIEW, CREATE VIEW ON db_datadict.* TO 'testuser2'@'localhost';

let $select = SELECT * FROM information_schema.views
WHERE table_schema = 'db_datadict' ORDER BY table_name;
eval $select;

--echo # Establish connection testuser1 (user=testuser1)
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (testuser1, localhost, testuser1, , test);
eval $select;

--echo # Establish connection testuser2 (user=testuser2)
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (testuser2, localhost, testuser2, , test);
eval $select;

--echo # Establish connection test_no_views (user=test_no_views)
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (test_no_views, localhost, test_no_views, , test);
eval $select;

# Cleanup
--echo # Switch to connection default and close all other connections
connection default;
disconnect testuser1;
disconnect testuser2;
disconnect test_no_views;
DROP USER 'testuser1'@'localhost';
DROP USER 'testuser2'@'localhost';
DROP USER 'test_no_views'@'localhost';
DROP DATABASE db_datadict;

--echo #########################################################################
--echo # 3.2.1.13+3.2.1.14+3.2.1.15: INFORMATION_SCHEMA.VIEWS modifications
--echo #########################################################################
# 3.2.1.13: Ensure that the creation of any new database object (e.g. table or
#           column) automatically inserts all relevant information on that
#           object into every appropriate INFORMATION_SCHEMA table.
# 3.2.1.14: Ensure that the alteration of any existing database object
#           automatically updates all relevant information on that object in
#           every appropriate INFORMATION_SCHEMA table.
# 3.2.1.15: Ensure that the dropping of any existing database object
#           automatically deletes all relevant information on that object from
#           every appropriate INFORMATION_SCHEMA table.
#
--disable_warnings
DROP TABLE IF EXISTS test.t1_my_table;
DROP DATABASE IF EXISTS db_datadict;
--enable_warnings
CREATE DATABASE db_datadict;
--replace_result $engine_type <engine_type>
eval
CREATE TABLE test.t1_table (f1 BIGINT, f2 CHAR(10))
DEFAULT CHARACTER SET latin1 COLLATE latin1_swedish_ci
ENGINE = $engine_type;
--error 0,ER_CANNOT_USER
DROP   USER 'testuser1'@'localhost';
CREATE USER 'testuser1'@'localhost';

# Check just created VIEW
SELECT * FROM information_schema.views
WHERE table_name LIKE 't1_%';
CREATE VIEW test.t1_view AS SELECT DISTINCT f1 FROM test.t1_table;
SELECT * FROM information_schema.views
WHERE table_name LIKE 't1_%';
#
# Check modification of DEFINER, SECURITY_TYPE, IS_UPDATABLE, VIEW_DEFINITION,
# CHECK_OPTION
SELECT table_name,definer FROM information_schema.views
WHERE table_name = 't1_view';
ALTER DEFINER = 'testuser1'@'localhost' VIEW test.t1_view AS
SELECT DISTINCT f1 FROM test.t1_table;
# The next result set could suffer from
# Bug#22763  	Disrepancy between SHOW CREATE VIEW and I_S.VIEWS
# because the VIEW definition is missing.
# Therefore we exclude the problematic columns from the result set.
SELECT table_name,definer,security_type FROM information_schema.views
WHERE table_name LIKE 't1_%';
ALTER DEFINER = 'root'@'localhost' SQL SECURITY INVOKER VIEW test.t1_view AS
SELECT f1 FROM test.t1_table WITH LOCAL CHECK OPTION;
SELECT table_name,definer,security_type FROM information_schema.views
WHERE table_name LIKE 't1_%';
#
# Check modification of TABLE_SCHEMA
SELECT table_schema,table_name FROM information_schema.views
WHERE table_name LIKE 't1_%'
ORDER BY table_schema,table_name;
--error ER_FORBID_SCHEMA_CHANGE
RENAME TABLE test.t1_view TO db_datadict.t1_view;
# Workaround for missing move to another database
DROP VIEW test.t1_view;
CREATE VIEW db_datadict.t1_view AS SELECT * FROM test.t1_table;
SELECT table_schema,table_name FROM information_schema.views
WHERE table_name LIKE 't1_%'
ORDER BY table_schema,table_name;
#
# Check modification of TABLE_NAME
SELECT table_name FROM information_schema.views
WHERE table_name LIKE 't1_%'
ORDER BY table_name;
RENAME TABLE db_datadict.t1_view TO db_datadict.t1_viewx;
SELECT table_name FROM information_schema.views
WHERE table_name LIKE 't1_%'
ORDER BY table_name;
#
# Check impact of DROP VIEW
SELECT table_name FROM information_schema.views
WHERE table_name LIKE 't1_%'
ORDER BY table_name;
DROP VIEW db_datadict.t1_viewx;
SELECT table_name FROM information_schema.views
WHERE table_name LIKE 't1_%'
ORDER BY table_name;
CREATE VIEW db_datadict.t1_view AS SELECT * FROM test.t1_table;
#
# Check impact of DROP base TABLE of VIEW
SELECT table_name FROM information_schema.views
WHERE table_name LIKE 't1_%'
ORDER BY table_name;
DROP TABLE test.t1_table;
SELECT table_name FROM information_schema.views
WHERE table_name LIKE 't1_%'
ORDER BY table_name;
--replace_result $engine_type <engine_type>
eval
CREATE TABLE test.t1_table (f1 BIGINT, f2 CHAR(10))
DEFAULT CHARACTER SET latin1 COLLATE latin1_swedish_ci COMMENT = 'Initial Comment'
ENGINE = $engine_type;
#
# Check impact of DROP SCHEMA
SELECT table_name FROM information_schema.views
WHERE table_name LIKE 't1_%'
ORDER BY table_name;
DROP DATABASE db_datadict;
SELECT table_name FROM information_schema.views
WHERE table_name LIKE 't1_%'
ORDER BY table_name;

# Cleanup
DROP USER 'testuser1'@'localhost';
DROP TABLE test.t1_table;

--echo ########################################################################
--echo # Testcases 3.2.1.3-3.2.1.5 + 3.2.1.8-3.2.1.12: INSERT/UPDATE/DELETE and
--echo #           DDL on INFORMATION_SCHEMA table are not supported
--echo ########################################################################
# 3.2.1.3:  Ensure that no user may execute an INSERT statement on any
#           INFORMATION_SCHEMA table.
# 3.2.1.4:  Ensure that no user may execute an UPDATE statement on any
#           INFORMATION_SCHEMA table.
# 3.2.1.5:  Ensure that no user may execute a DELETE statement on any
#           INFORMATION_SCHEMA table.
# 3.2.1.8:  Ensure that no user may create an index on an
#           INFORMATION_SCHEMA table.
# 3.2.1.9:  Ensure that no user may alter the definition of an
#           INFORMATION_SCHEMA table.
# 3.2.1.10: Ensure that no user may drop an INFORMATION_SCHEMA table.
# 3.2.1.11: Ensure that no user may move an INFORMATION_SCHEMA table to any
#           other database.
# 3.2.1.12: Ensure that no user may directly add to, alter, or delete any data
#           in an INFORMATION_SCHEMA table.
#
--disable_warnings
DROP DATABASE IF EXISTS db_datadict;
--enable_warnings
CREATE DATABASE db_datadict;
CREATE VIEW db_datadict.v1 AS SELECT 1;

--error ER_DBACCESS_DENIED_ERROR
INSERT INTO information_schema.views
SELECT * FROM information_schema.views;
--error ER_DBACCESS_DENIED_ERROR
INSERT INTO information_schema.views(table_schema, table_name)
VALUES ('db2', 'v2');

# Note:
# 1. We can get different error messages here.
# 2. We do not want to unify the individual messages to the far to unspecific
#    'Got one of the listed errors'.
let $my_error_message =
##### The previous statement must fail ######
# Server type  | expected error name      | expected error message
# --------------------------------------------------------------------------------------------------------------------
# MySQL | ER_DBACCESS_DENIED_ERROR | ERROR 42000: Access denied for user 'root'@'localhost' to database 'information_schema'
;
--disable_abort_on_error
UPDATE information_schema.views SET table_schema = 'test'
WHERE table_name = 't1';
if (!$mysql_errno)
{
   --echo $my_error_message
   exit;
}
--enable_abort_on_error

--error ER_DBACCESS_DENIED_ERROR
DELETE FROM information_schema.views WHERE table_name = 't1';
--error ER_DBACCESS_DENIED_ERROR
TRUNCATE information_schema.views;

--error ER_DBACCESS_DENIED_ERROR
CREATE INDEX my_idx_on_views ON information_schema.views(table_schema);

--error ER_DBACCESS_DENIED_ERROR
ALTER TABLE information_schema.views DROP PRIMARY KEY;
--error ER_DBACCESS_DENIED_ERROR
ALTER TABLE information_schema.views ADD f1 INT;

--error ER_DBACCESS_DENIED_ERROR
DROP TABLE information_schema.views;

--error ER_DBACCESS_DENIED_ERROR
ALTER TABLE information_schema.views RENAME db_datadict.views;
--error ER_DBACCESS_DENIED_ERROR
ALTER TABLE information_schema.views RENAME information_schema.xviews;

# Cleanup
DROP DATABASE db_datadict;

