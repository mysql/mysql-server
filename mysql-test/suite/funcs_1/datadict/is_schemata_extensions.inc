# suite/funcs_1/datadict/is_schemata.inc
#
# Check the layout of information_schema.schemata, permissions and the impact of
# CREATE/ALTER/DROP SCHEMA on it.
#
# Note:
#    This test is not intended
#    - to show information about the all time existing databases
#      information_schema and mysql
#    - for checking storage engine properties
#
# Copied from existing tests of information_schema.schemata.
#

let $is_table = SCHEMATA_EXTENSIONS;

# The table INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS must exist
eval SHOW TABLES FROM information_schema LIKE '$is_table';

--echo #######################################################################
--echo # Testcase 3.2.1.1: INFORMATION_SCHEMA tables can be queried via SELECT
--echo #######################################################################
# Ensure that every INFORMATION_SCHEMA table can be queried with a SELECT
# statement, just as if it were an ordinary user-defined table.
#
--source suite/funcs_1/datadict/is_table_query.inc


--echo #########################################################################
--echo # Testcase 3.2.9.1: INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS layout;
--echo #########################################################################
# Ensure that the INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS table has the following
# columns, in the following order:
#
# CATALOG_NAME (always shows def),
# SCHEMA_NAME (shows the name of a database, or schema, on which the current
#        user or PUBLIC has privileges),
# OPTIONS (shows the READ ONLY option if set)
#
eval DESCRIBE          information_schema.$is_table;
eval SHOW CREATE TABLE information_schema.$is_table;
eval SHOW COLUMNS FROM information_schema.$is_table;

# Note: Retrieval of information within information_schema.columns about
#       information_schema.schemata_extensions is in is_columns_is.test.

# Show that CATALOG_NAME is always 'def'.
SELECT catalog_name, schema_name
FROM information_schema.schemata_extensions
WHERE catalog_name != 'def';


--echo #########################################################################
--echo # Testcases 3.2.9.2+3.2.9.3: INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS
--echo # accessible information
--echo #########################################################################
# 3.2.9.2 Ensure that the table shows the relevant information for every
#         database on which the current user or PUBLIC have privileges.
# 3.2.9.3 Ensure that the table does not show any information on any databases
#         on which the current user and PUBLIC have no privileges.
#
# Note: Check of content within information_schema.schemata_extensions about the
#       databases information_schema and mysql is in
#       suite/funcs_1/t/is_schemata_extensions_is_mysql.test.
#
--disable_warnings
DROP DATABASE IF EXISTS db_datadict_1;
DROP DATABASE IF EXISTS db_datadict_2;
--enable_warnings
CREATE DATABASE db_datadict_1;
CREATE DATABASE db_datadict_2;

--error 0,ER_CANNOT_USER
DROP   USER 'testuser1'@'localhost';
CREATE USER 'testuser1'@'localhost';
--error 0,ER_CANNOT_USER
DROP   USER 'testuser2'@'localhost';
CREATE USER 'testuser2'@'localhost';
--error 0,ER_CANNOT_USER
DROP   USER 'testuser3'@'localhost';
CREATE USER 'testuser3'@'localhost';

GRANT SELECT ON db_datadict_1.* to 'testuser1'@'localhost';
GRANT SELECT ON db_datadict_1.* to 'testuser2'@'localhost';
GRANT SELECT ON db_datadict_2.* to 'testuser2'@'localhost';

let $my_select = SELECT * FROM information_schema.schemata_extensions
WHERE schema_name LIKE 'db_datadict_%' ORDER BY schema_name;
let $my_show = SHOW DATABASES LIKE 'db_datadict_%';

eval $my_select;
--sorted_result
eval $my_show;

--echo # Establish connection testuser1 (user=testuser1)
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (testuser1, localhost, testuser1, , db_datadict_1);
# Shows db_datadict_1
eval $my_select;
--sorted_result
eval $my_show;

--echo # Establish connection testuser2 (user=testuser2)
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (testuser2, localhost, testuser2, , db_datadict_2);
# Shows db_datadict_1 and db_datadict_2
eval $my_select;
--sorted_result
eval $my_show;

--echo # Establish connection testuser3 (user=testuser3)
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (testuser3, localhost, testuser3, , test);
# Shows neither db_datadict_1 nor db_datadict_2
eval $my_select;
--sorted_result
eval $my_show;

# Cleanup
--echo # Switch to connection default and close connections testuser1,testuser2,
--echo # testuser3
connection default;
disconnect testuser1;
disconnect testuser2;
disconnect testuser3;
DROP USER 'testuser1'@'localhost';
DROP USER 'testuser2'@'localhost';
DROP USER 'testuser3'@'localhost';
DROP DATABASE db_datadict_1;
DROP DATABASE db_datadict_2;


--echo #########################################################################
--echo # Testcases 3.2.1.13+3.2.1.14+3.2.1.15:
--echo # INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS modifications
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
DROP DATABASE IF EXISTS db_datadict;
--enable_warnings

SELECT * FROM information_schema.schemata_extensions
  WHERE schema_name = 'db_datadict';
CREATE DATABASE db_datadict;
SELECT * FROM information_schema.schemata_extensions
  WHERE schema_name = 'db_datadict';

# Check modify READ ONLY
SELECT schema_name, options FROM information_schema.schemata_extensions
WHERE schema_name = 'db_datadict';
ALTER SCHEMA db_datadict READ ONLY=1;
SELECT schema_name, options FROM information_schema.schemata_extensions
WHERE schema_name = 'db_datadict';
ALTER SCHEMA db_datadict READ ONLY=0;
SELECT schema_name, options FROM information_schema.schemata_extensions
WHERE schema_name = 'db_datadict';

# Check DROP DATABASE
SELECT schema_name
FROM information_schema.schemata_extensions WHERE schema_name = 'db_datadict';
DROP DATABASE db_datadict;
SELECT schema_name
FROM information_schema.schemata_extensions WHERE schema_name = 'db_datadict';


--echo ########################################################################
--echo # Testcases 3.2.1.3-3.2.1.5 + 3.2.1.8-3.2.1.12: INSERT/UPDATE/DELETE and
--echo #           DDL on INFORMATION_SCHEMA tables are not supported
--echo ########################################################################
# 3.2.1.3:  Ensure that no user may execute an INSERT statement on any
#           INFORMATION_SCHEMA table.
# 3.2.1.4:  Ensure that no user may execute an UPDATE statement on any
#           INFORMATION_SCHEMA table.
# 3.2.1.5:  Ensure that no user may execute a DELETE statement on any
#           INFORMATION_SCHEMA table.
# 3.2.1.8:  Ensure that no user may create an index on an INFORMATION_SCHEMA
#           table.
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

# Note(mleich):
# 1. We can get here different error messages.
# 2. We do not want to unify the individual messages to the far to unspecific
#    'Got one of the listed errors'.
let $my_error_message =
##### The previous statement must fail ######
# Server type  | expected error name      | expected error message
# ------------------------------------------------------------------------------
# MySQL | ER_DBACCESS_DENIED_ERROR | ERROR 42000: Access denied for user ...
;

--disable_abort_on_error
INSERT INTO information_schema.schemata_extensions
       (catalog_name, schema_name, options)
VALUES ('def', 'db1', '');
if (!$mysql_errno)
{
   --echo $my_error_message
   exit;
}
INSERT INTO information_schema.schemata_extensions
SELECT * FROM information_schema.schemata_extensions;
if (!$mysql_errno)
{
   --echo $my_error_message
   exit;
}
UPDATE information_schema.schemata_extensions
SET options = 'abc'
WHERE schema_name = 'db_datadict';
if (!$mysql_errno)
{
   --echo $my_error_message
   exit;
}
UPDATE information_schema.schemata_extensions SET catalog_name = 't_4711';
if (!$mysql_errno)
{
   --echo $my_error_message
   exit;
}
DELETE FROM information_schema.schemata_extensions
  WHERE schema_name = 'db_datadict';
if (!$mysql_errno)
{
   --echo $my_error_message
   exit;
}
TRUNCATE information_schema.schemata_extensions;
if (!$mysql_errno)
{
   --echo $my_error_message
   exit;
}
CREATE INDEX i1 ON information_schema.schemata_extensions(schema_name);
if (!$mysql_errno)
{
   --echo $my_error_message
   exit;
}
ALTER TABLE information_schema.schemata_extensions ADD f1 INT;
if (!$mysql_errno)
{
   --echo $my_error_message
   exit;
}
DROP TABLE information_schema.schemata_extensions;
if (!$mysql_errno)
{
   --echo $my_error_message
   exit;
}
ALTER TABLE information_schema.schemata_extensions
  RENAME db_datadict.schemata_extensions;
if (!$mysql_errno)
{
   --echo $my_error_message
   exit;
}
ALTER TABLE information_schema.schemata_extensions
  RENAME information_schema.xschemata_extensions;
if (!$mysql_errno)
{
   --echo $my_error_message
   exit;
}
--enable_abort_on_error

# Cleanup
DROP DATABASE db_datadict;
