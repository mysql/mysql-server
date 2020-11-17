#
# Tests for INFORMATION_SCHEMA system views requiring debug build of server.
#
--source include/have_debug.inc

# Warning is generated when default file (NULL) is used
CALL mtr.add_suppression("Could not parse key-value pairs in property string.*");

--echo #
--echo # Bug#26318759 ASSERT IN ROW_DROP_TABLE_FOR_MYSQL IN ROW/ROW0MYSQL.CC
--echo #

SET SESSION information_schema_stats_expiry=0;
SET SESSION debug= "+d,information_schema_fetch_table_stats";
CREATE TABLE t1 (a VARCHAR(200), b TEXT, FULLTEXT (a,b));
INSERT INTO t1 VALUES ('a','b');
# This SELECT should dump if it end-ups calling SE API. (Without fix).
SELECT table_name, cardinality  FROM INFORMATION_SCHEMA.STATISTICS
  WHERE cardinality > 0 and table_schema='test';
DROP TABLE t1;

SET SESSION debug= "-d,information_schema_fetch_table_stats";
SET SESSION information_schema_stats_expiry=default;


--echo #
--echo # Bug #27569314: ASSERTION `(UCHAR *)TABLE->DEF_READ_SET.BITMAP +
--echo # TABLE->S->COLUMN_BITMAP_SIZE
--echo #
--echo # RQG bug, not directly re-producible. Provoking same issue using
--echo # fault injection. Without fix, this would trigger same assert as seen
--echo # in RQG. Triggered by a failure to call tmp_restore_column_map in case
--echo # of errors.

CREATE TABLE t1(i INT);

SET SESSION debug="+d,sim_acq_fail_in_store_ci";
--error ER_DA_UNKNOWN_ERROR_NUMBER
SHOW CREATE TABLE t1;

SET SESSION debug="";
DROP TABLE t1;

--echo #
--echo # Bug#28460158 SIG 11 IN ITEM_FUNC_GET_DD_CREATE_OPTIONS::VAL_STR AT SQL/ITEM_STRFUNC.CC:4167
--echo #
CREATE TABLE t1(f1 INT, s VARCHAR(10));
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
  WHERE TABLE_NAME='t1';
SET debug = '+d,skip_dd_table_access_check';
update mysql.tables set options=concat(options,"abc") where name='t1';
SET debug = '+d,continue_on_property_string_parse_failure';
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
  WHERE TABLE_NAME='t1';
SET debug = DEFAULT;
DROP TABLE t1;
let SEARCH_FILE= $MYSQLTEST_VARDIR/log/mysqld.1.err;
--let SEARCH_PATTERN= Could not parse key-value pairs in property string.*
--source include/search_pattern.inc

--echo #
--echo # Bug#28875646 - ASSERTION `THD->GET_TRANSACTION()->IS_EMPTY(TRANSACTION_CTX::STMT) || (THD->STAT
--echo #
CREATE TABLE t1 (f1 INT );

--echo # Case 1: Test case to verify re-prepare of a prepared statement using
--echo #         INFORMATION_SCHEMA table in LOCK TABLE mode does not add a SE
--echo #         to the transaction while opening query tables.
--echo #         (Scenario reported in the bug page)
PREPARE stmt FROM 'show events';
EXECUTE stmt;
FLUSH TABLES;
LOCK TABLE t1 READ;
SET DEBUG_SYNC="after_statement_reprepare SIGNAL flush_tables WAIT_FOR continue";
--SEND EXECUTE stmt

CONNECT (con1, localhost, root);
SET DEBUG_SYNC="now WAIT_FOR flush_tables";
SET DEBUG="+d,skip_dd_table_access_check";
FLUSH TABLES mysql.events;
SET DEBUG="-d,skip_dd_table_access_check";
SET DEBUG_SYNC="now SIGNAL continue";

CONNECTION default;
--echo # Without fix, execution of a prepared statement in the debug build will
--echo # hit the assert condition to check no SEs added to the transaction while
--echo # opening query tables. In non-debug build execution continues without
--echo # any issues.
--echo # With fix, in debug build "stmt" execution succeeds.
--reap

--echo # Case 2: Test case added for the coverage.
--echo #         Test case to verify no SEs added to the transaction when open
--echo #         table fails after opening INFORMATION_SCHEMA tables in LOCK TABLE
--echo #         mode. In debug build, assert condition mentioned in the bug
--echo #         report fails if there are any SEs added to the transaction.
--error ER_TABLE_NOT_LOCKED
SELECT * FROM INFORMATION_SCHEMA.EVENTS, t2;
UNLOCK TABLES;

--echo # Case 3: Test case added for the coverage.
--echo #         Test case to verify no SEs added to the transaction when open
--echo #         table fails after opening INFORMATION_SCHEMA tables. In debug
--echo #         build, assert condition mentioned in the bug report fails if
--echo #         there are any SEs added to the transaction.
--error ER_NO_SUCH_TABLE
SELECT * FROM INFORMATION_SCHEMA.EVENTS, t2;

# Cleanup
DISCONNECT con1;
DROP TABLE t1;
DROP PREPARE stmt;
SET DEBUG_SYNC=RESET;

--echo #
--echo # WL#13369: Read only schema
--echo #
--echo # Test parsing of corrupted property string.
--echo #

CREATE SCHEMA s;
SELECT SCHEMA_NAME, OPTIONS FROM INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS
  WHERE SCHEMA_NAME = 's';

SET debug = '+d,skip_dd_table_access_check';
UPDATE mysql.schemata SET options = 'abc' WHERE name = 's';
SELECT options FROM mysql.schemata WHERE name = 's';

SET debug = '+d,continue_on_property_string_parse_failure';
SELECT SCHEMA_NAME, OPTIONS FROM INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS
  WHERE SCHEMA_NAME = 's';

SET debug = DEFAULT;
DROP SCHEMA s;
let SEARCH_FILE= $MYSQLTEST_VARDIR/log/mysqld.1.err;
--let SEARCH_PATTERN= Could not parse key-value pairs in property string 'abc'
--source include/search_pattern.inc
