# suite/funcs_1/datadict/is_columns.inc
#
# Auxiliary script to be sourced by
#    is_columns_is
#    is_columns_mysql
#    is_columns_<engine>
#
# Purpose:
#    Check the content of information_schema.columns about tables within certain
#    database/s.
#
# Usage:
#    The variable $my_where has to
#    - be set before sourcing this script.
#    - contain the first part of the WHERE qualification
#    Example:
#       let $my_where = WHERE table_schema = 'information_schema'
#       AND table_name <> 'profiling';
#       --source suite/funcs_1/datadict/is_columns.inc
#
# Author:
# 2008-01-23 mleich WL#4203 Reorganize and fix the data dictionary tests of
#                           testsuite funcs_1
#                   Create this script based on older scripts and new code.
#

--source suite/funcs_1/datadict/datadict_bug_12777.inc
eval
SELECT * FROM information_schema.columns
$my_where
ORDER BY table_schema, table_name, column_name;

--echo ##########################################################################
--echo # Show the quotient of CHARACTER_OCTET_LENGTH and CHARACTER_MAXIMUM_LENGTH
--echo ##########################################################################
eval
SELECT DISTINCT
       CHARACTER_OCTET_LENGTH / CHARACTER_MAXIMUM_LENGTH AS COL_CML,
       DATA_TYPE,
       CHARACTER_SET_NAME,
       COLLATION_NAME
FROM information_schema.columns
$my_where
AND CHARACTER_OCTET_LENGTH / CHARACTER_MAXIMUM_LENGTH = 1
ORDER BY CHARACTER_SET_NAME, COLLATION_NAME, COL_CML;

#FIXME 3.2.6.2: check the value 2.0079 tinytext ucs2 ucs2_general_ci
eval
SELECT DISTINCT
       CHARACTER_OCTET_LENGTH / CHARACTER_MAXIMUM_LENGTH AS COL_CML,
       DATA_TYPE,
       CHARACTER_SET_NAME,
       COLLATION_NAME
FROM information_schema.columns
$my_where
AND CHARACTER_OCTET_LENGTH / CHARACTER_MAXIMUM_LENGTH <> 1
ORDER BY CHARACTER_SET_NAME, COLLATION_NAME, COL_CML;

eval
SELECT DISTINCT
       CHARACTER_OCTET_LENGTH / CHARACTER_MAXIMUM_LENGTH AS COL_CML,
       DATA_TYPE,
       CHARACTER_SET_NAME,
       COLLATION_NAME
FROM information_schema.columns
$my_where
      AND CHARACTER_OCTET_LENGTH / CHARACTER_MAXIMUM_LENGTH IS NULL
ORDER BY CHARACTER_SET_NAME, COLLATION_NAME, COL_CML;

echo --> CHAR(0) is allowed (see manual), and here both CHARACHTER_* values;
echo --> are 0, which is intended behavior, and the result of 0 / 0 IS NULL;
--source suite/funcs_1/datadict/datadict_bug_12777.inc
eval
SELECT CHARACTER_OCTET_LENGTH / CHARACTER_MAXIMUM_LENGTH AS COL_CML,
       TABLE_SCHEMA,
       TABLE_NAME,
       COLUMN_NAME,
       DATA_TYPE,
       CHARACTER_MAXIMUM_LENGTH,
       CHARACTER_OCTET_LENGTH,
       CHARACTER_SET_NAME,
       COLLATION_NAME,
       COLUMN_TYPE
FROM information_schema.columns
$my_where
ORDER BY TABLE_SCHEMA, TABLE_NAME, ORDINAL_POSITION;
