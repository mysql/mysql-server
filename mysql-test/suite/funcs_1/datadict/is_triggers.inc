# suite/funcs_1/datadict/is_triggers.inc
#
# Check the layout of information_schema.triggers and the impact of
# CREATE/ALTER/DROP TABLE/VIEW/SCHEMA ... on it.
#
# Note:
#    This test is not intended
#    - to show information about the all time existing triggers
#      (there are no in the moment) within the databases information_schema
#      and mysql
#    - for checking storage engine properties
#      Therefore please do not alter $engine_type and $other_engine_type.
#
# Author:
# 2008-01-23 mleich WL#4203 Reorganize and fix the data dictionary tests of
#                           testsuite funcs_1
#                   Create this script based on older scripts and new code.
# Last Change:
# 2008-06-11 mleich Move t/is_triggers.test to this file and
#

let $is_table = TRIGGERS;

# The table INFORMATION_SCHEMA.TRIGGERS must exist
eval SHOW TABLES FROM information_schema LIKE '$is_table';

--echo #######################################################################
--echo # Testcase 3.2.1.1: INFORMATION_SCHEMA tables can be queried via SELECT
--echo #######################################################################
# Ensure that every INFORMATION_SCHEMA table can be queried with a SELECT
# statement, just as if it were an ordinary user-defined table.
#
--source suite/funcs_1/datadict/is_table_query.inc


--echo #########################################################################
--echo # Testcase 3.2.12.1: INFORMATION_SCHEMA.TRIGGERS layout
--echo #########################################################################
# Ensure that the INFORMATION_SCHEMA.TRIGGERS table has the following columns,
# in the following order:
#
# TRIGGER_CATALOG             NULL
# TRIGGER_SCHEMA              name of the database in which the trigger occurs
# TRIGGER_NAME
# EVENT_MANIPULATION          event associated with the trigger
#                             ('INSERT', 'DELETE', or 'UPDATE')
# EVENT_OBJECT_CATALOG        NULL
# EVENT_OBJECT_SCHEMA         database in which the table associated with the
#                             trigger occurs
# EVENT_OBJECT_TABLE          name of the table associated with the trigger
# ACTION_ORDER                0
# ACTION_CONDITION            NULL
# ACTION_STATEMENT
# ACTION_ORIENTATION          ROW
# ACTION_TIMING               'BEFORE' or 'AFTER'
# ACTION_REFERENCE_OLD_TABLE  NULL
# ACTION_REFERENCE_NEW_TABLE  NULL
# ACTION_REFERENCE_OLD_ROW    OLD
# ACTION_REFERENCE_NEW_ROW    NEW
# CREATED                     NULL (0)
# SQL_MODE                    server SQL mode that was in effect at the time
#                             when the trigger was created
#                             (also used during trigger execution)
# DEFINER                     who defined the trigger
#
--source suite/funcs_1/datadict/datadict_bug_12777.inc
eval DESCRIBE          information_schema.$is_table;
--source suite/funcs_1/datadict/datadict_bug_12777.inc
--replace_result InnoDB TMP_TABLE_ENGINE MyISAM TMP_TABLE_ENGINE 
eval SHOW CREATE TABLE information_schema.$is_table;
--source suite/funcs_1/datadict/datadict_bug_12777.inc
eval SHOW COLUMNS FROM information_schema.$is_table;


# Note: Retrieval of information within information_schema.columns about
#       information_schema.tables is in is_columns_is.test.

# Note: Do not use mtr --sorted_result:
# the result set contains the trigger body (sql statements),
# which itself contains several lines.
# This confuses --sorted_result

--replace_column 17 #
SELECT * FROM information_schema.triggers
WHERE trigger_catalog IS NOT NULL OR event_object_catalog IS NOT NULL
   OR action_condition IS NOT NULL OR action_reference_old_table IS NOT NULL
   OR action_reference_new_table IS NOT NULL
   ORDER BY TRIGGER_CATALOG, TRIGGER_SCHEMA, TRIGGER_NAME;


--echo ##################################################################################
--echo # Testcase 3.2.18.2 + 3.2.18.3: INFORMATION_SCHEMA.TRIGGERS accessible information
--echo ##################################################################################
# 3.2.18.2: Ensure that the table shows the relevant information on every
#           trigger on which the current user or PUBLIC has privileges.
# 3.2.18.3: Ensure that the table does not show any information on any trigger
#           on which the current user and public have no privileges.
# The SUPER (before 5.1.22) or TRIGGER (since 5.1.22) privilege is required for
# - creation of triggers
# - retrieval in INFORMATION_SCHEMA.TRIGGERS (affects size of result set)
#
--disable_warnings
DROP DATABASE IF EXISTS db_datadict;
--enable_warnings
CREATE DATABASE db_datadict charset latin1;
--error 0,ER_CANNOT_USER
DROP   USER 'testuser1'@'localhost';
CREATE USER 'testuser1'@'localhost';
--error 0,ER_CANNOT_USER
DROP   USER 'testuser2'@'localhost';
CREATE USER 'testuser2'@'localhost';
--error 0,ER_CANNOT_USER
DROP   USER 'testuser3'@'localhost';
CREATE USER 'testuser3'@'localhost';
--error 0,ER_CANNOT_USER
DROP   USER 'testuser4'@'localhost';
CREATE USER 'testuser4'@'localhost';

GRANT TRIGGER ON *.* TO 'testuser1'@'localhost';
GRANT TRIGGER ON *.* TO 'testuser3'@'localhost';
GRANT TRIGGER ON *.* TO 'testuser4'@'localhost';
GRANT ALL ON db_datadict.* TO 'testuser1'@'localhost' WITH GRANT OPTION;

let $my_select = SELECT * FROM information_schema.triggers
WHERE trigger_name = 'trg1';
let $my_show =   SHOW TRIGGERS FROM db_datadict;
--echo # Establish connection testuser1 (user=testuser1)
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (testuser1, localhost, testuser1, , db_datadict);
--replace_result $engine_type <engine_type>
eval
CREATE TABLE db_datadict.t1 (f1 INT, f2 INT, f3 INT)
ENGINE = $engine_type;
CREATE TRIGGER trg1 BEFORE INSERT
ON db_datadict.t1 FOR EACH ROW SET @test_before = 2, new.f1 = @test_before;
GRANT ALL ON db_datadict.t1 TO 'testuser2'@'localhost';
REVOKE TRIGGER ON db_datadict.t1 FROM 'testuser2'@'localhost';
GRANT SELECT ON db_datadict.t1 TO 'testuser3'@'localhost';
--replace_column 17 #
eval $my_select;
--replace_column 6 #
eval $my_show;

--echo # Establish connection testuser2 (user=testuser2)
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (testuser2, localhost, testuser2, , db_datadict);
SHOW GRANTS FOR 'testuser2'@'localhost';
--echo # No TRIGGER Privilege --> no result for query
--replace_column 17 #
eval $my_select;
--replace_column 6 #
eval $my_show;

--echo # Establish connection testuser3 (user=testuser3)
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (testuser3, localhost, testuser3, , test);
SHOW GRANTS FOR 'testuser3'@'localhost';
--echo # TRIGGER Privilege + SELECT Privilege on t1 --> result for query
--replace_column 17 #
eval $my_select;
--replace_column 6 #
eval $my_show;

--echo # Establish connection testuser4 (user=testuser4)
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (testuser4, localhost, testuser4, , test);
SHOW GRANTS FOR 'testuser4'@'localhost';
--echo # TRIGGER Privilege + no SELECT Privilege on t1 --> result for query
--disable_abort_on_error
SELECT * FROM db_datadict.t1;
DESC db_datadict.t1;
--replace_column 17 #
eval $my_select;
--replace_column 6 #
eval $my_show;

--echo # Switch to connection default and close connections testuser1 - testuser4
connection default;
disconnect testuser1;
disconnect testuser2;
disconnect testuser3;
disconnect testuser4;
--replace_column 17 #
eval $my_select;
--replace_column 6 #
eval $my_show;
DROP USER 'testuser1'@'localhost';
DROP USER 'testuser2'@'localhost';
DROP USER 'testuser3'@'localhost';
DROP USER 'testuser4'@'localhost';
DROP DATABASE db_datadict;


--echo #########################################################################
--echo # 3.2.1.13+3.2.1.14+3.2.1.15: INFORMATION_SCHEMA.TRIGGERS modifications
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
# FIXME: To be implemented


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
CREATE DATABASE db_datadict charset latin1;
--replace_result $engine_type <engine_type>
eval
CREATE TABLE db_datadict.t1 (f1 BIGINT)
ENGINE = $engine_type;
CREATE TRIGGER db_datadict.trg1 BEFORE INSERT
ON db_datadict.t1 FOR EACH ROW SET @test_before = 2, new.f1 = @test_before;

--error ER_DBACCESS_DENIED_ERROR
INSERT INTO information_schema.triggers
SELECT * FROM information_schema.triggers;

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
UPDATE information_schema.triggers SET trigger_schema = 'test'
WHERE table_name = 't1';
if (!$mysql_errno)
{
   --echo $my_error_message
   exit;
}
--enable_abort_on_error

--error ER_DBACCESS_DENIED_ERROR
DELETE FROM information_schema.triggers WHERE trigger_name = 't1';
--error ER_DBACCESS_DENIED_ERROR
TRUNCATE information_schema.triggers;

--error ER_DBACCESS_DENIED_ERROR
CREATE INDEX my_idx_on_triggers ON information_schema.triggers(trigger_schema);

--error ER_DBACCESS_DENIED_ERROR
ALTER TABLE information_schema.triggers DROP PRIMARY KEY;
--error ER_DBACCESS_DENIED_ERROR
ALTER TABLE information_schema.triggers ADD f1 INT;

--error ER_DBACCESS_DENIED_ERROR
DROP TABLE information_schema.triggers;

--error ER_DBACCESS_DENIED_ERROR
ALTER TABLE information_schema.triggers RENAME db_datadict.triggers;
--error ER_DBACCESS_DENIED_ERROR
ALTER TABLE information_schema.triggers RENAME information_schema.xtriggers;

# Cleanup
DROP DATABASE db_datadict;

