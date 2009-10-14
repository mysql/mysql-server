# suite/funcs_1/datadict/is_tables.inc
#
# Check the layout of information_schema.tables and the impact of
# CREATE/ALTER/DROP TABLE/VIEW/SCHEMA ... on it.
#
# Note:
#    This test is not intended
#    - to show information about the all time existing tables
#      within the databases information_schema and mysql
#    - for checking storage engine properties
#      Therefore please do not alter $engine_type and $other_engine_type.
#      Some results of the subtests depend on the storage engines assigned.
#
# Author:
# 2008-01-23 mleich WL#4203 Reorganize and fix the data dictionary tests of
#                           testsuite funcs_1
#                   Create this script based on older scripts and new code.
# Last Change:
# 2008-06-11 mleich Move t/is_tables.test to this file and
#                   create variants for embedded/non embedded server.
#

let $engine_type       = MEMORY;
let $other_engine_type = MyISAM;

let $is_table = TABLES;

# The table INFORMATION_SCHEMA.TABLES must exist
eval SHOW TABLES FROM information_schema LIKE '$is_table';

--echo #######################################################################
--echo # Testcase 3.2.1.1: INFORMATION_SCHEMA tables can be queried via SELECT
--echo #######################################################################
# Ensure that every INFORMATION_SCHEMA table can be queried with a SELECT
# statement, just as if it were an ordinary user-defined table.
#
--source suite/funcs_1/datadict/is_table_query.inc


--echo #########################################################################
--echo # Testcase 3.2.12.1: INFORMATION_SCHEMA.TABLES layout
--echo #########################################################################
# Ensure that the INFORMATION_SCHEMA.TABLES table has the following columns,
# in the following order:
#
# TABLE_CATALOG (always shows NULL),
# TABLE_SCHEMA (shows the name of the database, or schema, in which an
#       accessible table resides),
# TABLE_NAME (shows the name of a table which the current user may access),
# TABLE_TYPE (shows whether the table is a BASE TABLE, a TEMPORARY table,
#       or a VIEW),
# ENGINE (shows the storage engine used for the table),
# VERSION (shows the version number of the table's .frm file),
# ROW_FORMAT (shows the table's row storage format; either FIXED, DYNAMIC
#       or COMPRESSED),
# TABLE_ROWS (shows the number of rows in the table),
# AVG_ROW_LENGTH (shows the average length of the table's rows),
# DATA_LENGTH (shows the length of the table's data file),
# MAX_DATA_LENGTH (shows the maximum length of the table's data file),
# INDEX_LENGTH (shows the length of the index file associated with the table),
# DATA_FREE (shows the number of allocated, unused bytes),
# AUTO_INCREMENT (shows the next AUTO_INCREMENT value, where applicable),
# CREATE_TIME (shows the timestamp of the time the table was created),
# UPDATE_TIME (shows the timestamp of the time the table's data file was
#       last updated),
# CHECK_TIME (shows the timestamp of the time the table was last checked),
# TABLE_COLLATION (shows the table's default collation),
# CHECKSUM (shows the live checksum value for the table, if any; otherwise NULL),
# CREATE_OPTIONS (shows any additional options used in the table's definition;
#       otherwise NULL),
# TABLE_COMMENT (shows the comment added to the table's definition;
#       otherwise NULL).
#
--source suite/funcs_1/datadict/datadict_bug_12777.inc
eval DESCRIBE          information_schema.$is_table;
--source suite/funcs_1/datadict/datadict_bug_12777.inc
eval SHOW CREATE TABLE information_schema.$is_table;
--source suite/funcs_1/datadict/datadict_bug_12777.inc
eval SHOW COLUMNS FROM information_schema.$is_table;

# Note: Retrieval of information within information_schema.columns about
#       information_schema.tables is in is_columns_is.test.

# Show that TABLE_CATALOG is always NULL.
SELECT table_catalog, table_schema, table_name
FROM information_schema.tables WHERE table_catalog IS NOT NULL;


--echo ################################################################################
--echo # Testcase 3.2.12.2 + 3.2.12.3: INFORMATION_SCHEMA.TABLES accessible information
--echo ################################################################################
# 3.2.12.2: Ensure that the table shows the relevant information on every base table
#           and view on which the current user or PUBLIC has privileges.
# 3.2.12.3: Ensure that the table does not show any information on any tables
#           on which the current user and public have no privileges.
#
# Note: Check of content within information_schema.tables about tables within
#       database            is in
#       mysql               is_tables_mysql.test
#       information_schema  is_tables_is.test
#       test%               is_tables_<engine>.test
#
--disable_warnings
DROP DATABASE IF EXISTS db_datadict;
--enable_warnings
CREATE DATABASE db_datadict;

--error 0,ER_CANNOT_USER
DROP   USER 'testuser1'@'localhost';
CREATE USER 'testuser1'@'localhost';
GRANT CREATE, CREATE VIEW, INSERT, SELECT ON db_datadict.*
   TO 'testuser1'@'localhost' WITH GRANT OPTION;
--error 0,ER_CANNOT_USER
DROP   USER 'testuser2'@'localhost';
CREATE USER 'testuser2'@'localhost';
--error 0,ER_CANNOT_USER
DROP   USER 'testuser3'@'localhost';
CREATE USER 'testuser3'@'localhost';

--replace_result $engine_type <engine_type>
eval
CREATE TABLE db_datadict.tb1 (f1 INT, f2 INT, f3 INT)
ENGINE = $engine_type;

GRANT SELECT ON db_datadict.tb1 TO 'testuser1'@'localhost';
GRANT ALL    ON db_datadict.tb1 TO 'testuser2'@'localhost' WITH GRANT OPTION;

let $my_select = SELECT * FROM information_schema.tables
WHERE table_schema = 'db_datadict' ORDER BY table_name;
let $my_show = SHOW TABLES FROM db_datadict;

--echo # Establish connection testuser1 (user=testuser1)
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (testuser1, localhost, testuser1, , db_datadict);
# tb2 is not granted to anyone
--replace_result $engine_type <engine_type>
eval
CREATE TABLE tb2 (f1 DECIMAL)
ENGINE = $engine_type;
--replace_result $engine_type <engine_type>
eval
CREATE TABLE tb3 (f1 VARCHAR(200))
ENGINE = $engine_type;
GRANT SELECT ON db_datadict.tb3 to 'testuser3'@'localhost';
GRANT INSERT ON db_datadict.tb3 to 'testuser2'@'localhost';
CREATE VIEW v3 AS SELECT * FROM tb3;
GRANT SELECT ON db_datadict.v3 to 'testuser3'@'localhost';

if ($have_bug_32285)
{
--disable_ps_protocol
}
# We do not want to check here values affected by
# - the storage engine used
# - Operating system / Filesystem
# - start time of test
# 1 TABLE_CATALOG
# 2 TABLE_SCHEMA
# 3 TABLE_NAME
# 4 TABLE_TYPE
# 5 ENGINE           affected by storage engine used
# 6 VERSION
# 7 ROW_FORMAT       affected by storage engine used
# 8 TABLE_ROWS
# 9 AVG_ROW_LENGTH   affected by storage engine used
# 10 DATA_LENGTH     affected by storage engine used and maybe OS
# 11 MAX_DATA_LENGTH affected by storage engine used and maybe OS
# 12 INDEX_LENGTH    affected by storage engine used and maybe OS
# 13 DATA_FREE       affected by storage engine used and maybe OS
# 14 AUTO_INCREMENT
# 15 CREATE_TIME     depends roughly on start time of test (*)
# 16 UPDATE_TIME     depends roughly on start time of test (*)
# 17 CHECK_TIME      depends roughly on start time of test and storage engine (*)
# 18 TABLE_COLLATION
# 19 CHECKSUM        affected by storage engine used
# 20 CREATE_OPTIONS
# 21 TABLE_COMMENT   affected by some storage engines
# (*) In case of view or temporary table NULL.
--replace_column 5 "#ENG#"  7 "#RF#"  9 "#ARL#"  10 "#DL#"  11 "#MDL#"  12 "#IL#"  13 "#DF#"  15 "#CRT"  16 "#UT#" 17 "#CT#"  19 "#CS#"
eval $my_select;
--enable_ps_protocol
--sorted_result
eval $my_show;

--echo # Establish connection testuser2 (user=testuser2)
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (testuser2, localhost, testuser2, , db_datadict);
if ($have_bug_32285)
{
--disable_ps_protocol
}
--replace_column 5 "#ENG#"  7 "#RF#"  9 "#ARL#"  10 "#DL#"  11 "#MDL#"  12 "#IL#"  13 "#DF#"  15 "#CRT"  16 "#UT#" 17 "#CT#"  19 "#CS#"
eval $my_select;
--enable_ps_protocol
--sorted_result
eval $my_show;

--echo # Establish connection testuser3 (user=testuser3)
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (testuser3, localhost, testuser3, , db_datadict);
if ($have_bug_32285)
{
--disable_ps_protocol
}
--replace_column 5 "#ENG#"  7 "#RF#"  9 "#ARL#"  10 "#DL#"  11 "#MDL#"  12 "#IL#"  13 "#DF#"  15 "#CRT"  16 "#UT#" 17 "#CT#"  19 "#CS#"
eval $my_select;
--enable_ps_protocol
--sorted_result
eval $my_show;

--echo # Switch to connection default (user=root)
connection default;
# we see only 'public' tables
if ($have_bug_32285)
{
--disable_ps_protocol
}
--replace_column 5 "#ENG#"  7 "#RF#"  9 "#ARL#"  10 "#DL#"  11 "#MDL#"  12 "#IL#"  13 "#DF#"  15 "#CRT"  16 "#UT#" 17 "#CT#"  19 "#CS#"
eval $my_select;
--enable_ps_protocol
--sorted_result
eval $my_show;

# Cleanup
--echo # Close connection testuser1, testuser2, testuser3
disconnect testuser1;
disconnect testuser2;
disconnect testuser3;
DROP USER 'testuser1'@'localhost';
DROP USER 'testuser2'@'localhost';
DROP USER 'testuser3'@'localhost';
DROP DATABASE db_datadict;


--echo #########################################################################
--echo # 3.2.1.13+3.2.1.14+3.2.1.15: INFORMATION_SCHEMA.TABLES modifications
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

SELECT table_name FROM information_schema.tables
WHERE table_name LIKE 't1_my_table%';
--replace_result $engine_type <engine_type>
eval
CREATE TABLE test.t1_my_table (f1 BIGINT)
DEFAULT CHARACTER SET latin1 COLLATE latin1_swedish_ci
COMMENT = 'Initial Comment' ENGINE = $engine_type;
# Settings used in CREATE TABLE must be visible in information_schema.tables.
--vertical_results
--replace_column 5 "#ENG#"  7 "#RF#"  9 "#ARL#"  10 "#DL#"  11 "#MDL#"  12 "#IL#"  13 "#DF#"  15 "#CRT"  16 "#UT#" 17 "#CT#"  19 "#CS#"
SELECT * FROM information_schema.tables
WHERE table_name = 't1_my_table';
--horizontal_results
#
# Check modification of TABLE_NAME
SELECT table_name FROM information_schema.tables
WHERE table_name LIKE 't1_my_table%';
RENAME TABLE test.t1_my_table TO test.t1_my_tablex;
SELECT table_name FROM information_schema.tables
WHERE table_name LIKE 't1_my_table%';
#
# Check modification of TABLE_SCHEMA
SELECT table_schema,table_name FROM information_schema.tables
WHERE table_name = 't1_my_tablex';
RENAME TABLE test.t1_my_tablex TO db_datadict.t1_my_tablex;
SELECT table_schema,table_name FROM information_schema.tables
WHERE table_name = 't1_my_tablex';
#
# Check modification of ENGINE
--replace_result $engine_type <engine_type>
SELECT table_name, engine FROM information_schema.tables
WHERE table_name = 't1_my_tablex';
--replace_result $other_engine_type <other_engine_type>
eval
ALTER TABLE db_datadict.t1_my_tablex
ENGINE = $other_engine_type;
--replace_result $other_engine_type <other_engine_type>
SELECT table_name, engine FROM information_schema.tables
WHERE table_name = 't1_my_tablex';
#
# Check modification of TABLE_ROWS
SELECT table_name, table_rows FROM information_schema.tables
WHERE table_name = 't1_my_tablex';
INSERT INTO db_datadict.t1_my_tablex VALUES(1),(2);
SELECT table_name, table_rows FROM information_schema.tables
WHERE table_name = 't1_my_tablex';
#
# Check indirect modification of TABLE_COLLATION
SELECT table_name, table_collation FROM information_schema.tables
WHERE table_name = 't1_my_tablex';
ALTER TABLE db_datadict.t1_my_tablex DEFAULT CHARACTER SET utf8;
SELECT table_name, table_collation FROM information_schema.tables
WHERE table_name = 't1_my_tablex';
# Check direct modification of TABLE_COLLATION
SELECT table_name, table_collation FROM information_schema.tables
WHERE table_name = 't1_my_tablex';
ALTER TABLE db_datadict.t1_my_tablex
DEFAULT CHARACTER SET latin1 COLLATE latin1_german1_ci;
SELECT table_name, table_collation FROM information_schema.tables
WHERE table_name = 't1_my_tablex';
#
# Check modification of TABLE_COMMENT
SELECT table_name, TABLE_COMMENT FROM information_schema.tables
WHERE table_name = 't1_my_tablex';
ALTER TABLE db_datadict.t1_my_tablex COMMENT 'Changed Comment';
SELECT table_name, TABLE_COMMENT FROM information_schema.tables
WHERE table_name = 't1_my_tablex';
#
# Check modification of AUTO_INCREMENT
SELECT table_name, AUTO_INCREMENT FROM information_schema.tables
WHERE table_name = 't1_my_tablex';
ALTER TABLE db_datadict.t1_my_tablex
ADD f2 BIGINT AUTO_INCREMENT, ADD PRIMARY KEY (f2);
SELECT table_name, AUTO_INCREMENT FROM information_schema.tables
WHERE table_name = 't1_my_tablex';
#
# Check modification of ROW_FORMAT
SELECT table_name, ROW_FORMAT FROM information_schema.tables
WHERE table_name = 't1_my_tablex';
ALTER TABLE db_datadict.t1_my_tablex ROW_FORMAT = dynamic;
SELECT table_name, ROW_FORMAT FROM information_schema.tables
WHERE table_name = 't1_my_tablex';
#
# Check "growth" of UPDATE_TIME and modification of CHECKSUM
SELECT table_name, checksum FROM information_schema.tables
WHERE table_name = 't1_my_tablex';
ALTER TABLE db_datadict.t1_my_tablex CHECKSUM = 1;
SELECT table_name, checksum IS NOT NULL FROM information_schema.tables
WHERE table_name = 't1_my_tablex';
SELECT UPDATE_TIME, checksum INTO @UPDATE_TIME, @checksum
FROM information_schema.tables
WHERE table_name = 't1_my_tablex';
#   Enforce a time difference bigger than the smallest unit (1 second).
--real_sleep 1.1
INSERT INTO db_datadict.t1_my_tablex SET f1 = 3;
FLUSH TABLES;
SELECT UPDATE_TIME > @UPDATE_TIME
    AS "Is current UPDATE_TIME bigger than before last INSERT?"
FROM information_schema.tables
WHERE table_name = 't1_my_tablex';
SELECT checksum <> @checksum
    AS "Is current CHECKSUM different than before last INSERT?"
FROM information_schema.tables
WHERE table_name = 't1_my_tablex';
#
# Information is used later
SELECT CREATE_TIME INTO @CREATE_TIME FROM information_schema.tables
WHERE table_name = 't1_my_tablex';
#
# Check impact of DROP TABLE
SELECT table_name FROM information_schema.tables
WHERE table_name LIKE 't1_my_table%';
DROP TABLE db_datadict.t1_my_tablex;
SELECT table_name FROM information_schema.tables
WHERE table_name LIKE 't1_my_table%';
#
# Check "growth" of CREATE_TIME
--replace_result $other_engine_type <other_engine_type>
eval
CREATE TABLE test.t1_my_tablex (f1 BIGINT)
ENGINE = $other_engine_type;
SELECT CREATE_TIME > @CREATE_TIME
    AS "Is current CREATE_TIME bigger than for the old dropped table?"
FROM information_schema.tables
WHERE table_name = 't1_my_tablex';
DROP TABLE test.t1_my_tablex;
#
# Check a VIEW
CREATE VIEW test.t1_my_tablex AS SELECT 1;
--vertical_results
SELECT * FROM information_schema.tables
WHERE table_name = 't1_my_tablex';
--horizontal_results
DROP VIEW test.t1_my_tablex;
SELECT table_name FROM information_schema.tables
WHERE table_name = 't1_my_tablex';
#
# Check a temporary table
--replace_result $other_engine_type <other_engine_type>
eval
CREATE TEMPORARY TABLE test.t1_my_tablex
ENGINE = $other_engine_type
   AS SELECT 1;
--vertical_results
SELECT table_name, table_type FROM information_schema.tables
WHERE table_name = 't1_my_tablex';
--horizontal_results
DROP TEMPORARY TABLE test.t1_my_tablex;
#
# Check impact of DROP SCHEMA
--replace_result $engine_type <engine_type>
eval
CREATE TABLE db_datadict.t1_my_tablex
ENGINE = $engine_type AS
SELECT 1;
SELECT table_name FROM information_schema.tables
WHERE table_name = 't1_my_tablex';
DROP DATABASE db_datadict;
SELECT table_name FROM information_schema.tables
WHERE table_name = 't1_my_tablex';


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
CREATE DATABASE db_datadict;
--replace_result $engine_type <engine_type>
eval
CREATE TABLE db_datadict.t1 (f1 BIGINT)
ENGINE = $engine_type;

--error ER_DBACCESS_DENIED_ERROR
INSERT INTO information_schema.tables
SELECT * FROM information_schema.tables;

--error ER_DBACCESS_DENIED_ERROR
UPDATE information_schema.tables SET table_schema = 'test'
WHERE table_name = 't1';

--error ER_DBACCESS_DENIED_ERROR
DELETE FROM information_schema.tables WHERE table_name = 't1';
--error ER_DBACCESS_DENIED_ERROR
TRUNCATE information_schema.tables;

--error ER_DBACCESS_DENIED_ERROR
CREATE INDEX my_idx_on_tables ON information_schema.tables(table_schema);

--error ER_DBACCESS_DENIED_ERROR
ALTER TABLE information_schema.tables DROP PRIMARY KEY;
--error ER_DBACCESS_DENIED_ERROR
ALTER TABLE information_schema.tables ADD f1 INT;

--error ER_DBACCESS_DENIED_ERROR
DROP TABLE information_schema.tables;

--error ER_DBACCESS_DENIED_ERROR
ALTER TABLE information_schema.tables RENAME db_datadict.tables;
--error ER_DBACCESS_DENIED_ERROR
ALTER TABLE information_schema.tables RENAME information_schema.xtables;

# Cleanup
DROP DATABASE db_datadict;

