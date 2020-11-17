CALL mtr.add_suppression("Could not parse key-value pairs in property string.*");
#
# Bug#26318759 ASSERT IN ROW_DROP_TABLE_FOR_MYSQL IN ROW/ROW0MYSQL.CC
#
SET SESSION information_schema_stats_expiry=0;
SET SESSION debug= "+d,information_schema_fetch_table_stats";
CREATE TABLE t1 (a VARCHAR(200), b TEXT, FULLTEXT (a,b));
INSERT INTO t1 VALUES ('a','b');
SELECT table_name, cardinality  FROM INFORMATION_SCHEMA.STATISTICS
WHERE cardinality > 0 and table_schema='test';
TABLE_NAME	CARDINALITY
t1	1
t1	1
DROP TABLE t1;
SET SESSION debug= "-d,information_schema_fetch_table_stats";
SET SESSION information_schema_stats_expiry=default;
#
# Bug #27569314: ASSERTION `(UCHAR *)TABLE->DEF_READ_SET.BITMAP +
# TABLE->S->COLUMN_BITMAP_SIZE
#
# RQG bug, not directly re-producible. Provoking same issue using
# fault injection. Without fix, this would trigger same assert as seen
# in RQG. Triggered by a failure to call tmp_restore_column_map in case
# of errors.
CREATE TABLE t1(i INT);
SET SESSION debug="+d,sim_acq_fail_in_store_ci";
SHOW CREATE TABLE t1;
ERROR HY000: Got unknown error: 42
SET SESSION debug="";
DROP TABLE t1;
#
# Bug#28460158 SIG 11 IN ITEM_FUNC_GET_DD_CREATE_OPTIONS::VAL_STR AT SQL/ITEM_STRFUNC.CC:4167
#
CREATE TABLE t1(f1 INT, s VARCHAR(10));
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	
SET debug = '+d,skip_dd_table_access_check';
update mysql.tables set options=concat(options,"abc") where name='t1';
SET debug = '+d,continue_on_property_string_parse_failure';
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	
SET debug = DEFAULT;
DROP TABLE t1;
Pattern "Could not parse key-value pairs in property string.*" found
#
# Bug#28875646 - ASSERTION `THD->GET_TRANSACTION()->IS_EMPTY(TRANSACTION_CTX::STMT) || (THD->STAT
#
CREATE TABLE t1 (f1 INT );
# Case 1: Test case to verify re-prepare of a prepared statement using
#         INFORMATION_SCHEMA table in LOCK TABLE mode does not add a SE
#         to the transaction while opening query tables.
#         (Scenario reported in the bug page)
PREPARE stmt FROM 'show events';
EXECUTE stmt;
Db	Name	Definer	Time zone	Type	Execute at	Interval value	Interval field	Starts	Ends	Status	Originator	character_set_client	collation_connection	Database Collation
FLUSH TABLES;
LOCK TABLE t1 READ;
SET DEBUG_SYNC="after_statement_reprepare SIGNAL flush_tables WAIT_FOR continue";
EXECUTE stmt;
SET DEBUG_SYNC="now WAIT_FOR flush_tables";
SET DEBUG="+d,skip_dd_table_access_check";
FLUSH TABLES mysql.events;
SET DEBUG="-d,skip_dd_table_access_check";
SET DEBUG_SYNC="now SIGNAL continue";
# Without fix, execution of a prepared statement in the debug build will
# hit the assert condition to check no SEs added to the transaction while
# opening query tables. In non-debug build execution continues without
# any issues.
# With fix, in debug build "stmt" execution succeeds.
Db	Name	Definer	Time zone	Type	Execute at	Interval value	Interval field	Starts	Ends	Status	Originator	character_set_client	collation_connection	Database Collation
# Case 2: Test case added for the coverage.
#         Test case to verify no SEs added to the transaction when open
#         table fails after opening INFORMATION_SCHEMA tables in LOCK TABLE
#         mode. In debug build, assert condition mentioned in the bug
#         report fails if there are any SEs added to the transaction.
SELECT * FROM INFORMATION_SCHEMA.EVENTS, t2;
ERROR HY000: Table 't2' was not locked with LOCK TABLES
UNLOCK TABLES;
# Case 3: Test case added for the coverage.
#         Test case to verify no SEs added to the transaction when open
#         table fails after opening INFORMATION_SCHEMA tables. In debug
#         build, assert condition mentioned in the bug report fails if
#         there are any SEs added to the transaction.
SELECT * FROM INFORMATION_SCHEMA.EVENTS, t2;
ERROR 42S02: Table 'test.t2' doesn't exist
DROP TABLE t1;
DROP PREPARE stmt;
SET DEBUG_SYNC=RESET;
#
# WL#13369: Read only schema
#
# Test parsing of corrupted property string.
#
CREATE SCHEMA s;
SELECT SCHEMA_NAME, OPTIONS FROM INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS
WHERE SCHEMA_NAME = 's';
SCHEMA_NAME	OPTIONS
s	
SET debug = '+d,skip_dd_table_access_check';
UPDATE mysql.schemata SET options = 'abc' WHERE name = 's';
SELECT options FROM mysql.schemata WHERE name = 's';
options
abc
SET debug = '+d,continue_on_property_string_parse_failure';
SELECT SCHEMA_NAME, OPTIONS FROM INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS
WHERE SCHEMA_NAME = 's';
SCHEMA_NAME	OPTIONS
s	
SET debug = DEFAULT;
DROP SCHEMA s;
Pattern "Could not parse key-value pairs in property string 'abc'" found
