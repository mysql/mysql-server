# suite/funcs_1/datadict/is_key_column_usage.inc
#
# Check the layout of information_schema.key_column_usage and the impact of
# CREATE/ALTER/DROP TABLE/VIEW/SCHEMA/COLUMN ... on it.
#
# Note:
#    This test is not intended
#    - to show information about the all time existing tables
#      within the databases information_schema and mysql
#    - for checking storage engine properties
#      Therefore please do not alter $engine_type.
#
# Author:
# 2008-01-23 mleich WL#4203 Reorganize and fix the data dictionary tests of
#                           testsuite funcs_1
#                   Create this script based on older scripts and new code.
# Last Change:
# 2008-06-11 mleich Move t/is_key_column_usage.test to this file and
#                   create variants for embedded/non embedded server.
#

let $engine_type = MEMORY;

let $is_table = KEY_COLUMN_USAGE;

# The table INFORMATION_SCHEMA.KEY_COLUMN_USAGE must exist
eval SHOW TABLES FROM information_schema LIKE '$is_table';

--echo #######################################################################
--echo # Testcase 3.2.1.1: INFORMATION_SCHEMA tables can be queried via SELECT
--echo #######################################################################
# Ensure that every INFORMATION_SCHEMA table can be queried with a SELECT
# statement, just as if it were an ordinary user-defined table.
#
--source suite/funcs_1/datadict/is_table_query.inc


--echo #########################################################################
--echo # Testcase 3.2.7.1: INFORMATION_SCHEMA.KEY_COLUMN_USAGE layout
--echo #########################################################################
# Ensure that the INFORMATION_SCHEMA.KEY_COLUMN_USAGE table has the following
# columns, in the following order:
#
# CONSTRAINT_CATALOG (always shows NULL),
# CONSTRAINT_SCHEMA (shows the database, or schema, in which an accessible
#       constraint, or index, resides),
# CONSTRAINT_NAME (shows the name of the accessible constraint),
# TABLE_CATALOG (always shows NULL),
# TABLE_SCHEMA (shows the database, or schema, in which the table constrained
#       by that constraint resides),
# TABLE_NAME (shows the name of the table constrained by the constraint),
# COLUMN_NAME (shows the name of a column that is the index key, or part of
#       the index key),
# ORDINAL_POSITION (shows the ordinal position of the column within the
#       constraint index),
# POSITION_IN_UNIQUE_CONSTRAINT (shows, for a foreign key column, the ordinal
#       position of the referenced column within the referenced unique index;
#       otherwise NULL).
# added with 5.0.6:
# REFERENCED_TABLE_SCHEMA,
# REFERENCED_TABLE_NAME,
# REFERENCED_COLUMN_NAME
#
--source suite/funcs_1/datadict/datadict_bug_12777.inc
eval DESCRIBE          information_schema.$is_table;
--source suite/funcs_1/datadict/datadict_bug_12777.inc
eval SHOW CREATE TABLE information_schema.$is_table;
--source suite/funcs_1/datadict/datadict_bug_12777.inc
eval SHOW COLUMNS FROM information_schema.$is_table;

# Note: Retrieval of information within information_schema.columns about
#       information_schema.key_column_usage is in is_columns_is.test.

# Show that CONSTRAINT_CATALOG and TABLE_CATALOG are always NULL.
SELECT constraint_catalog, constraint_schema, constraint_name, table_catalog,
       table_schema, table_name, column_name
FROM information_schema.key_column_usage
WHERE constraint_catalog IS NOT NULL OR table_catalog IS NOT NULL;


--echo ########################################################################################
--echo # Testcase 3.2.7.2 + 3.2.7.3: INFORMATION_SCHEMA.KEY_COLUMN_USAGE accessible information
--echo ########################################################################################
# 3.2.7.2:  Ensure that the table shows the relevant information on every column, defined to
#           be part of an index key, which is accessible to the current user or to PUBLIC.
# 3.2.7.3:  Ensure that the table does not show any information on any indexed column that is
#           not accessible to the current user or PUBLIC.
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

USE db_datadict;

--replace_result $engine_type <engine_type>
eval
CREATE TABLE t1_1
  (f1 INT NOT NULL, PRIMARY KEY(f1),
   f2 INT,          INDEX f2_ind(f2))
ENGINE = $engine_type;
GRANT SELECT ON t1_1 to 'testuser1'@'localhost';

--replace_result $engine_type <engine_type>
eval
CREATE TABLE t1_2
  (f1 INT NOT NULL, PRIMARY KEY(f1),
   f2 INT,          INDEX f2_ind(f2))
ENGINE = $engine_type;
GRANT SELECT ON t1_2 to 'testuser2'@'localhost';
#FIXME: add foreign keys

let $select= SELECT * FROM information_schema.key_column_usage
WHERE table_name LIKE 't1_%'
ORDER BY constraint_catalog, constraint_schema, constraint_name,
         table_catalog, table_schema, table_name, ordinal_position;

# show view of user root
eval $select;

--echo # Establish connection testuser1 (user=testuser1)
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (testuser1, localhost, testuser1, , db_datadict);
eval $select;

--echo # Establish connection testuser2 (user=testuser2)
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (testuser2, localhost, testuser2, , db_datadict);
eval $select;

# Cleanup
--echo # Switch to connection default and close connections testuser1, testuser2
connection default;
disconnect testuser1;
disconnect testuser2;
DROP USER 'testuser1'@'localhost';
DROP USER 'testuser2'@'localhost';
DROP TABLE t1_1;
DROP TABLE t1_2;
DROP DATABASE IF EXISTS db_datadict;


--echo ########################################################################################
--echo # Testcase 3.2.1.13+3.2.1.14+3.2.1.15: INFORMATION_SCHEMA.KEY_COLUMN_USAGE modifications
--echo ########################################################################################
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
DROP DATABASE IF EXISTS db_datadict;
DROP TABLE IF EXISTS test.t1_my_table;
--enable_warnings
CREATE DATABASE db_datadict;

SELECT table_name FROM information_schema.key_column_usage
WHERE table_name LIKE 't1_my_table%';
--replace_result $engine_type <engine_type>
eval
CREATE TABLE test.t1_my_table
   (f1 CHAR(12), f2 TIMESTAMP, f4 BIGINT, PRIMARY KEY(f1,f2))
DEFAULT CHARACTER SET latin1 COLLATE latin1_swedish_ci
ENGINE = $engine_type;
# Settings used in CREATE TABLE must be visible
# in information_schema.key_column_usage.
--vertical_results
SELECT * FROM information_schema.key_column_usage
WHERE table_name = 't1_my_table';
--horizontal_results
#
# Check modification of TABLE_NAME
SELECT DISTINCT table_name FROM information_schema.key_column_usage
WHERE table_name LIKE 't1_my_table%';
RENAME TABLE test.t1_my_table TO test.t1_my_tablex;
SELECT DISTINCT table_name FROM information_schema.key_column_usage
WHERE table_name LIKE 't1_my_table%';
#
# Check modification of TABLE_SCHEMA
SELECT DISTINCT table_schema,table_name FROM information_schema.key_column_usage
WHERE table_name = 't1_my_tablex';
RENAME TABLE test.t1_my_tablex TO db_datadict.t1_my_tablex;
SELECT DISTINCT table_schema,table_name FROM information_schema.key_column_usage
WHERE table_name = 't1_my_tablex';
#
# Check modification of COLUMN_NAME
SELECT DISTINCT table_name, column_name FROM information_schema.key_column_usage
WHERE table_name = 't1_my_tablex'
ORDER BY table_name, column_name;
ALTER TABLE db_datadict.t1_my_tablex CHANGE COLUMN f1 first_col CHAR(12);
SELECT DISTINCT table_name, column_name FROM information_schema.key_column_usage
WHERE table_name = 't1_my_tablex'
ORDER BY table_name, column_name;
#
# Note: The size of the column list and the not very selective qualification
#       is intended. I want to see that the schema names are equal and
#       all records about 't1_my_tablex'.
let $my_select = SELECT constraint_schema, constraint_name, table_schema,
table_name, column_name, ordinal_position
FROM information_schema.key_column_usage
WHERE table_name = 't1_my_tablex'
ORDER BY constraint_schema, constraint_name, table_schema,
         table_name, ordinal_position;
#
# Check ADD INDEX being not UNIQUE (does not show up in key_column_usage)
eval $my_select;
CREATE INDEX f2 ON db_datadict.t1_my_tablex(f2);
eval $my_select;
DROP INDEX f2 ON db_datadict.t1_my_tablex;
#
# Check ADD UNIQUE INDEX without name explicit assigned
eval $my_select;
ALTER TABLE db_datadict.t1_my_tablex ADD UNIQUE (f2);
eval $my_select;
DROP INDEX f2 ON db_datadict.t1_my_tablex;
#
# Check ADD UNIQUE INDEX with name explicit assigned
eval $my_select;
ALTER TABLE db_datadict.t1_my_tablex ADD UNIQUE my_idx (f2);
eval $my_select;
DROP INDEX my_idx ON db_datadict.t1_my_tablex;
eval $my_select;
ALTER TABLE db_datadict.t1_my_tablex ADD UNIQUE my_idx (f4,first_col);
eval $my_select;
#
# Check DROP COLUMN
eval $my_select;
ALTER TABLE db_datadict.t1_my_tablex
DROP COLUMN first_col;
eval $my_select;
#
# Check impact of DROP TABLE
SELECT table_name, column_name
FROM information_schema.key_column_usage
WHERE table_name = 't1_my_tablex'
ORDER BY table_name, column_name;
DROP TABLE db_datadict.t1_my_tablex;
SELECT table_name, column_name
FROM information_schema.key_column_usage
WHERE table_name = 't1_my_tablex';
#
# No UNIQUE CONSTRAINT -> no entry in key_column_usage
SELECT table_name FROM information_schema.key_column_usage
WHERE table_name = 't1_my_tablex';
--replace_result $engine_type <engine_type>
eval
CREATE TABLE db_datadict.t1_my_tablex
ENGINE = $engine_type AS
SELECT 1 AS f1;
SELECT table_name FROM information_schema.key_column_usage
WHERE table_name = 't1_my_tablex';
# UNIQUE CONSTRAINT -> entry in key_column_usage
ALTER TABLE db_datadict.t1_my_tablex ADD PRIMARY KEY(f1);
SELECT table_name FROM information_schema.key_column_usage
WHERE table_name = 't1_my_tablex';
#
# Check impact of DROP SCHEMA
SELECT table_name FROM information_schema.key_column_usage
WHERE table_name = 't1_my_tablex';
DROP DATABASE db_datadict;
SELECT table_name FROM information_schema.key_column_usage
WHERE table_name = 't1_my_tablex';
#


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
DROP TABLE IF EXISTS db_datadict.t1;
--enable_warnings
CREATE DATABASE db_datadict;
--replace_result $engine_type <engine_type>
eval
CREATE TABLE db_datadict.t1 (f1 BIGINT)
ENGINE = $engine_type;

# Note(mleich):
# 1. We can get here different error messages.
# 2. We do not want to unify the individual messages to the far to unspecific
#    'Got one of the listed errors'.
let $my_error_message =
##### The previous statement must fail ######
# Server type  | expected error name      | expected error message
# --------------------------------------------------------------------------------------------------------------------
# not embedded | ER_DBACCESS_DENIED_ERROR | ERROR 42000: Access denied for user 'root'@'localhost' to database 'information_schema'
# embedded     | ER_NON_INSERTABLE_TABLE  | ERROR HY000: The target table schemata of the INSERT is not insertable-into
#              | or similar               |              or similar
;

--disable_abort_on_error
INSERT INTO information_schema.key_column_usage
       (constraint_schema, constraint_name, table_name)
VALUES (          'mysql',       'primary',       'db');
if (!$mysql_errno)
{
   --echo $my_error_message
   exit;
}
--error ER_DBACCESS_DENIED_ERROR
INSERT INTO information_schema.key_column_usage
SELECT * FROM information_schema.key_column_usage;

--error ER_DBACCESS_DENIED_ERROR
UPDATE information_schema.key_column_usage
SET  table_name = 'db1' WHERE constraint_name = 'primary';

--error ER_DBACCESS_DENIED_ERROR
DELETE FROM information_schema.key_column_usage WHERE table_name = 't1';
--error ER_DBACCESS_DENIED_ERROR
TRUNCATE information_schema.key_column_usage;

--error ER_DBACCESS_DENIED_ERROR
CREATE INDEX i3 ON information_schema.key_column_usage(table_name);

--error ER_DBACCESS_DENIED_ERROR
ALTER TABLE information_schema.key_column_usage ADD f1 INT;

--error ER_DBACCESS_DENIED_ERROR
DROP TABLE information_schema.key_column_usage;

--error ER_DBACCESS_DENIED_ERROR
ALTER TABLE information_schema.key_column_usage
RENAME db_datadict.key_column_usage;
--error ER_DBACCESS_DENIED_ERROR
ALTER TABLE information_schema.key_column_usage
RENAME information_schema.xkey_column_usage;
--enable_abort_on_error

# Cleanup
DROP TABLE db_datadict.t1;
DROP DATABASE db_datadict;
