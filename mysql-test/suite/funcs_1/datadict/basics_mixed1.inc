# suite/funcs_1/datadict/basics_mixed1.inc
#
# Auxiliary script to be sourced by suite/funcs_1/t/is_basics_mixed.test
#
# Author:
# 2008-01-23 mleich WL#4203 Reorganize and fix the data dictionary tests of
#                           testsuite funcs_1
#                   Create this script based on older scripts and new code.
#

# 1 Attempt to create tables and views when residing in information_schema
# 1.1 CREATE TABLE
USE information_schema;
let $message= root: create a table with a name of an IS table directly in IS;
let $dd_part1= CREATE TABLE;
let $dd_part2= ( c1 INT );
--source suite/funcs_1/datadict/basics_mixed2.inc
# FIXME 3.2.1.6: error message ER_UNKNOWN_TABLE is misleading
--error ER_UNKNOWN_TABLE
CREATE TABLE t1 (f1 INT, f2 INT, f3 INT);
#
# 1.2 CREATE VIEW
# 1.2.1 Hit on existing INFORMATION_SCHEMA table
--error ER_DBACCESS_DENIED_ERROR
CREATE VIEW tables AS SELECT 'garbage';
--error ER_DBACCESS_DENIED_ERROR
CREATE VIEW tables AS SELECT * FROM information_schema.tables;
# 1.2.2 New view
# ER_DBACCESS_DENIED_ERROR would be better.
--error ER_UNKNOWN_TABLE
CREATE VIEW v1 AS SELECT 'garbage';

# 2 Attempt to create tables and views when residing in information_schema
# 1.1 CREATE TABLE
USE test;
let $message= root: create a table with a name of an IS table from other db;
let $dd_part1= CREATE TABLE information_schema.;
let $dd_part2= ( c1 INT );
--source suite/funcs_1/datadict/basics_mixed2.inc
# FIXME 3.2.1.6: error message ER_UNKNOWN_TABLE is misleading
--error ER_UNKNOWN_TABLE
CREATE TABLE information_schema.t1 (f1 INT, f2 INT, f3 INT);
#
# Hit on existing INFORMATION_SCHEMA table
--error ER_DBACCESS_DENIED_ERROR
CREATE VIEW information_schema.tables AS SELECT 'garbage';
--error ER_DBACCESS_DENIED_ERROR
CREATE VIEW information_schema.tables AS
SELECT * FROM information_schema.tables;
# New table
# ER_DBACCESS_DENIED_ERROR would be better.
--error ER_UNKNOWN_TABLE
CREATE VIEW information_schema.v1 AS SELECT 'garbage';
