###################################################
#                                                 #
#  Functions within VIEWs                         #
#                                                 #
###################################################
# 2007-11-09 HHunger enabled all disabled parts belonging to fixed bugs.
# 2006-12-08 mleich  Maintenance + refinements
# 2005-09-14 mleich  Create this test
#
# 0.  Some notes about this test:
# #################################################################
#
# 0.1   This test is unfinished and incomplete, but already useful.
# -----------------------------------------------------------------
# 0.1.1 There will be architectural changes in future.
#       The long sequences with
#           let $col_type= <column to use>;
#           --source suite/funcs_1/views/<file containing the
#                                         select with function>
#       per every column type do not look very smart.
#
#       Ugly combinations of functions and data types must be also checked,
#       because an accidental typo like assigning a string column to an
#       numeric parameter could happen and should not result in a server crash.
#
#       Maybe it is better to change the architecture of this test in such
#       a way:
#       1. A generator script (this one or written in Perl or SP language)
#          generates an prototype of the the final testscript.
#       2. Some manual adjustments because of open bugs (depending on
#          storage engine or function) might be needed (I hope not :)
#       3. The final testscript is pushed to the other regression testscripts.
#       Advantage:    The analysis of bugs, extension and maintenance of this
#                     test will be much easier.
#       Disadvantage: Much redundant code within the final testscript,
#                     but the maintenance of the redundant code will be done
#                     by the script generator.
#
# 0.1.2 The behaviour of SELECTs on VIEWs could be affected by the SQL mode
#       which was valid during VIEW creation time. This means some variations
#       of the SQL mode are needed.
# 0.1.3 There are much more functions to be tested.
# 0.1.4 The result sets of some CAST sub testcases with ugly function parameter
#       column data type combinations must be discussed.
#
#
# 0.2   How to valuate the test results:
# ---------------------------------------------------------------------------
#       Due to the extreme "greedy bug hunting" architecture (combinatorics
#       + heavy use of sourced scripts) of the following tests, there will be
#       - no abort of the test execution, when one statements gets an return
#         code != 0 (The sub testcases are independend.)
#         But statements, which do not make sense like SELECT on non existent
#         VIEW will be omitted. This decreases the amount of useless output.
#       - a file with expected results, which might contain incorrect server
#         responses
#         There are open bugs and I cannot omit statements which reveal these
#         bugs.
#         But there will be a special messages within the protocol files.
#         Example:
#            "Attention: CAST --> SIGNED INTEGER
#            The file with expected results suffers from Bug 5913";
#            means, the file with expected results contains result sets which
#            are known to be wrong.
#            "Attention: The last <whatever> failed"
#            means, a statement which should be successful (bugfree MySQL)
#            failed.
#
#       "Passed" : The behaviour of your MySQL version does not differ from the
#                  version used to generate the files with expected results.
#                  Known bugs affecting these tests could be retrieved by
#                  grep "Attention" r/<testcase>.result .
#
#       "Failed" : The behaviour of your MySQL version differs from the version
#                  used to generate the files with expected results.
#                  These differences could be result of bug fixes or new bugs.
#                  Please compare r/<testcase>.reject and r/<testcase>.result .
#
#      The test will abort if one of the basic preparation statement fails
#      (except ALTER TABLE ADD ...).
#
#
# 0.3   How to debug sub testcases with "unexpected" results:
# ---------------------------------------------------------------------------
#       1. Please execute this test and copy the "reject" file to a save place.
#          Search within the "reject" file for the sub testcase (the SELECT)
#          with the suspicious result set or server response.
#          Now all t1_values records are preloaded.
#       2. Start the server without the initial cleanup of databases etc.
#          This preserves the content of the table t1_values, which
#          might be needed for replaying the situation.
#          Example:
#            ./mysql-test-run.pl   --socket=var/tmp/master.sock --start-dirty
#       3. Issue the statements needed by using "mysql" or "mysqltest".
#
#       Maybe an internal routine of this test fails. Please ask me (mleich) or
#       enable the logging of auxiliary queries and try to analyze the
#       problem.
#
#
# 0.4   How to extend the number of functions to be checked:
# ---------------------------------------------------------------------------
#       Please jump to the paragraphs of the basic preparations
#       1. Extend t1_values with the columns you need
#       2. Insert some predefinded rows
#       3. Add the SELECTs with function which should be used within VIEWs
#          and
#          records which should be used dedicated to the SELECT above
#
#
# 0.5  How to alter the internal routines of this test:
# ---------------------------------------------------------------------------
#      Please try to achieve a state where the protocol
#      - contains ALL statements, which are needed to replay a problem within
#        the field of functions within VIEWs
#      - does not contain too much auxiliary statements, which are not needed
#        to replay a problem (--> "--disable_query_log")
#      Example:
#        Needed for replay:
#        - DROP/CREATE TABLE t1_values
#        - INSERT of records into t1_values
#        - DROP/CREATE/SELECT/SHOW VIEW v1
#        - SELECT direct on base table
#        Not needed for replay:
#        - SET @<uservariable> = <value>
#        - DROP/CREATE/INSERT/SELECT TABLE t1_selects, t1_modes
#
#
# 0.6  A trick for checking results
# ---------------------------------------------------------------------------
#   Standard setting for common execution of this test:
    let $simple_select_result= 1;
    let $view_select_result=   1;
#      The implementation of some additional function tests may lead to
#      masses of result sets, which have to be checked. The result sets of
#      the simple selects on the base table must equal the result sets of the
#      queries on the VIEWs. This step could be made more comfortable by
#      1. Edit this file to
#              let $simple_select_result= 1;
#              let $view_select_result=   0;
#         Please execute this test.
#         The script will omit CREATE/DROP/SHOW/SELECT on VIEW.
#         The "reject" file contains only the simple select result sets.
#      2. Edit this file to
#              let $simple_select_result= 0;
#              let $view_select_result=   1;
#         Please execute this test.
#         The script will work with the VIEWs, but omit the simple selects.
#         The "reject" file contains the view select result sets.
#      3. Compare the "reject" files of 1. and 2. within a graphical diff tool.
#
#

--disable_warnings
DROP TABLE IF EXISTS t1_selects, t1_modes, t1_values;
DROP VIEW  IF EXISTS v1;
--enable_warnings

--disable_query_log
# Storage for the SELECTs to be used for the VIEW definition
# Attention: my_select must be no too small because a statement like
#    SELECT LOAD_FILE(< file in MYSQLTEST_VARDIR >)
#    AS my_col,
#    id FROM t1_values';
#    might be a very long
# Bug#38427 "Data too long" ... tests "<ENGINE>_func_view" fail
CREATE TABLE t1_selects
(
   id BIGINT AUTO_INCREMENT,
   my_select VARCHAR(1000) NOT NULL,
   disable_result ENUM('Yes','No') NOT NULL default 'No',
   PRIMARY KEY(id),
   UNIQUE (my_select)
);

# MODES to be checked
CREATE TABLE t1_modes
(
   id BIGINT AUTO_INCREMENT,
   my_mode VARCHAR(200) NOT NULL,
   PRIMARY KEY(id),
   UNIQUE (my_mode)
);
--enable_query_log

# The table to be used in the FROM parts of the SELECTs
--replace_result $type <engine_to_be_tested>
eval CREATE TABLE t1_values
(
   id BIGINT AUTO_INCREMENT,
   select_id BIGINT,
   PRIMARY KEY(id)
) ENGINE = $type;

##### BEGIN      Basic preparations      #######################################
#
# 1. Extend t1_values with the columns you need
#    - the column name must show the data type
#    - do not add NOT NULL columns
#    - do not worry if the intended column data type is not
#      available for some storage engines
#    Please do not forget to assign values for the new columns (paragraph 2.).
--disable_abort_on_error
ALTER TABLE t1_values ADD my_char_30        CHAR(30);
ALTER TABLE t1_values ADD my_varchar_1000   VARCHAR(1000);
ALTER TABLE t1_values ADD my_binary_30      BINARY(30);
ALTER TABLE t1_values ADD my_varbinary_1000 VARBINARY(1000);
ALTER TABLE t1_values ADD my_datetime       DATETIME;
ALTER TABLE t1_values ADD my_date           DATE;
ALTER TABLE t1_values ADD ts_dummy          TIMESTAMP;
ALTER TABLE t1_values ADD my_timestamp      TIMESTAMP;
ALTER TABLE t1_values ADD my_time           TIME;
ALTER TABLE t1_values ADD my_year           YEAR;
ALTER TABLE t1_values ADD my_bigint         BIGINT;
ALTER TABLE t1_values ADD my_double         DOUBLE;
ALTER TABLE t1_values ADD my_decimal        DECIMAL(64,30);
--enable_abort_on_error

#-------------------------------------------------------------------------------

#
# 2.  Insert some predefinded rows
#     Predefined rows
#     - t1_values.select_id IS NULL
#     - will be selected by every SELECT with function to be tested
#     - have to be inserted when sql_mode = 'traditional' is valid, because
#       we do not want to start with "illegal/unexpected/..." values.
#       Such experiments should be done in other testcases.
#     Please be careful
#     - modifying column values of predefined rows they might change many
#       result sets
#     - additional predefined rows should be really useful for the majority of
#       all sub testcases, since they blow up all result sets.
SET sql_mode = 'traditional';
#
# 2.1 record -- everything to NULL
INSERT INTO t1_values SET id = 0;
#
# 2.2 record -- everything to "minimum"
#     numbers, date/time types -> minimum of range
#     strings, blobs, binaries -> ''
#     FIXME enum, set ??
INSERT INTO t1_values SET
       my_char_30 = '',
       my_varchar_1000 = '',
       my_binary_30 = '',
       my_varbinary_1000 = '',
       my_datetime = '0001-01-01 00:00:00',
       my_date = '0001-01-01',
       my_timestamp = '1970-01-01 03:00:01',
       my_time = '-838:59:59',
       my_year = '1901',
       my_bigint = -9223372036854775808,
       my_decimal = -9999999999999999999999999999999999.999999999999999999999999999999 ,
       my_double = -1.7976931348623E+308;
# shortened due to bug#32285
#       my_double = -1.7976931348623157E+308;
#
# 2.3 record -- everything to "maximum"
#     numbers, date/time types -> maximum of range
#     strings, blobs, binaries -> '<- full length of used data type>'
#     FIXME enum, set ??
INSERT INTO t1_values SET
       my_char_30 = '<--------30 characters------->',
       my_varchar_1000 = CONCAT('<---------1000 characters',
                                RPAD('',965,'-'),'--------->'),
       my_binary_30 = '<--------30 characters------->',
       my_varbinary_1000 = CONCAT('<---------1000 characters',
                                RPAD('',965,'-'),'--------->'),
       my_datetime = '9999-12-31 23:59:59',
       my_date = '9999-12-31',
       my_timestamp = '2038-01-01 02:59:59',
       my_time = '838:59:59',
       my_year = 2155,
       my_bigint = 9223372036854775807,
       my_decimal = +9999999999999999999999999999999999.999999999999999999999999999999 ,
       my_double = 1.7976931348623E+308;
# shortened due to bug#32285
#       my_double = -1.7976931348623157E+308;
#
# 2.4 record -- everything to "magic" value if available or
#               other interesting value
#     numbers   -> 0
#     strings, blobs, binaries -> not full length of used data type, "exotic"
#                                 characters and preceeding and trailing spaces
#     FIXME enum, set ??
INSERT INTO t1_values SET
       my_char_30 = ' ---äÖüß@µ*$-- ',
       my_varchar_1000 = ' ---äÖüß@µ*$-- ',
       my_binary_30 = ' ---äÖüß@µ*$-- ',
       my_varbinary_1000 = ' ---äÖüß@µ*$-- ',
       my_datetime = '2004-02-29 23:59:59',
       my_date = '2004-02-29',
       my_timestamp = '2004-02-29 23:59:59',
       my_time = '13:00:00',
       my_year = 2000,
       my_bigint = 0,  	
       my_decimal = 0.0,
       my_double = 0;
#
# 2.5 record -- everything to "harmless" value if available
#     numbers -> -1 (logical)
#     strings, blobs, binaries -> '-1' useful for numeric functions
#     FIXME enum, set ??
INSERT INTO t1_values SET
       my_char_30 = '-1',
       my_varchar_1000 = '-1',
       my_binary_30 = '-1',
       my_varbinary_1000 = '-1',
       my_datetime = '2005-06-28 10:00:00',
       my_date = '2005-06-28',
       my_timestamp = '2005-06-28 10:00:00',
       my_time = '10:00:00',
       my_year = 2005,
       my_bigint = -1,
       my_decimal = -1.000000000000000000000000000000,
       my_double = -0.1E+1;

#-------------------------------------------------------------------------------

#
# 3.  Add the SELECTs with function which should be used within VIEWs
#     and
#     records which should be used dedicated to the SELECT above
#     - Please avoid WHERE clauses
#     - Include the PRIMARY KEY ("id") of the base table t1_values into the
#       select column list
#     - Include the base table column used as function parameter into the
#       select column list, because it is much easier to check the results
#     - Do not forget to escape single quotes
#       Example:
#        SET @my_select = 'SELECT sqrt(my_bigint), my_bigint, id FROM t1_values'
#        SET @my_select = 'SELECT CONCAT(\'A\',my_char_30), id FROM t1_values';
#     - Statements, which reveal open crashing bugs MUST be disabled.
#     - Result sets must not contain data, which might differ between boxes
#       executing this test.
#       Example: current time, absolute path to some files ...
#     - Please derive the functions to be checked from the MySQL manual
#       and use the same order. This means copy the the function names as
#       comment into this test and start to implement a testcase for your
#       most preferred function.
#       This method avoids that we forget a function and gives a better
#       overview.
#
#     If you have the time to check the result sets do the insert of the
#     SELECT with function via:
#           eval SET @my_select =
#           '<your SELECT>';
#           --source suite/funcs_1/views/fv1.inc
#     fv1.inc sets t1_selects.disable_result to 'No' and the effect will be,
#     that the result set will be logged.
#
#     If you do not have the time to check the result sets do the insert of the
#     SELECT with function via:
#           eval SET @my_select =
#           '<your SELECT>';
#           --source suite/funcs_1/views/fv2.inc
#     fv2.inc sets t1_selects.disable_result to 'Yes' and the effect will be,
#     that the result set will be not logged.
#     This should be only a temporary solution and it does not remove the
#     need to check the server return codes.
#     That means even when we do not have the time to check the correctness of
#     the result sets, we check if
#        - SELECT <function> or
#        - SELECT * FROM <VIEW with function>
#     crash the server or get suspicious server responses.
#
#     - the SELECTs will be applied to the rows defined here (3.) +
#       all predefined rows (2.)
#     - the rows dedicated to the SELECT should contain especially interesting
#       column values or combinations of column values, which are not covered
#       by the predefined records
#     - The records have to be inserted when sql_mode = 'traditional' is valid.
#     - Please do not insert records with column values where the allowed
#       range is exceeded. The SQL mode 'traditional' will prevent such
#       inserts. Such experiments should be done in other tests, because
#       they inflate the current test without giving an appropriate value.
#
#       Example:
#          The function to be tested is "sqrt".
#          The minimum, maximum, default and NULL value are covered by the
#          predefined rows.
#          A value where sqrt(<value>) = <integer value> in strict mathematics
#          would be of interest.
#          --> Add a record with my_bigint = 4
#          --> Just for fun my_bigint = -25 .
#
# Some internal stuff
PREPARE ins_sel_with_result FROM "INSERT INTO t1_selects SET my_select = @my_select,
disable_result = 'No'" ;
PREPARE ins_sel_no_result   FROM "INSERT INTO t1_selects SET my_select = @my_select,
disable_result = 'Yes'" ;
SET sql_mode = 'traditional';
# --disable_query_log

let $col_type= my_bigint;
# Example:
#    eval SET @my_select = 'SELECT CONCAT(''A'', $col_type), $col_type, id';
eval SET @my_select = 'SELECT sqrt($col_type), $col_type, id FROM t1_values';
--source suite/funcs_1/views/fv1.inc
# Content of suite/funcs_1/views/fv1.inc :
#    --disable_query_log
#    EXECUTE ins_sel_with_result;
#    SET @select_id = LAST_INSERT_ID();
#    --enable_query_log

eval INSERT INTO t1_values SET select_id = @select_id,
            $col_type = 4;
eval INSERT INTO t1_values SET select_id = @select_id,
            $col_type = -25;
# SELECT * FROM t1_values;

# 1.    Cast Functions and Operators
# 1.1   CAST
#
# Note(mleich): I guess the CAST routines are used in many other functions.
#               Therefore check also nearly all "ugly" variants like
#               CAST(<string composed of non digits> AS DECIMAL) here.
#
#            suite/funcs_1/views/fv_cast.inc contains
#                SELECT CAST($col_type AS $target_type), ...
#
#
# 1.1.1. CAST --> BINARY
--echo ##### 1.1.1. CAST --> BINARY
let $target_type= BINARY;
#
let $col_type= my_char_30;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_varchar_1000;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_binary_30;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_varbinary_1000;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_bigint;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_decimal;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_double;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_datetime;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_date;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_timestamp;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_time;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_year;
--source suite/funcs_1/views/fv_cast.inc


# 1.1.2. CAST --> CHAR
--echo ##### 1.1.2. CAST --> CHAR
let $target_type= CHAR;
#
let $col_type= my_char_30;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_varchar_1000;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_binary_30;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_varbinary_1000;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_bigint;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_decimal;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_double;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_datetime;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_date;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_timestamp;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_time;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_year;
--source suite/funcs_1/views/fv_cast.inc


# 1.1.3. CAST --> DATE
--echo ##### 1.1.3. CAST --> DATE
let $target_type= DATE;
#
let $col_type= my_char_30;
--source suite/funcs_1/views/fv_cast.inc
eval INSERT INTO t1_values SET select_id = @select_id,
            $col_type = '2005-06-27';
let $col_type= my_varchar_1000;
--source suite/funcs_1/views/fv_cast.inc
eval INSERT INTO t1_values SET select_id = @select_id,
            $col_type = '2005-06-27';
let $col_type= my_binary_30;
--source suite/funcs_1/views/fv_cast.inc
eval INSERT INTO t1_values SET select_id = @select_id,
            $col_type = '2005-06-27';
let $col_type= my_varbinary_1000;
--source suite/funcs_1/views/fv_cast.inc
eval INSERT INTO t1_values SET select_id = @select_id,
            $col_type = '2005-06-27';
let $col_type= my_bigint;
--source suite/funcs_1/views/fv_cast.inc
eval INSERT INTO t1_values SET select_id = @select_id,
            $col_type = 20050627;
let $col_type= my_double;
--source suite/funcs_1/views/fv_cast.inc
eval INSERT INTO t1_values SET select_id = @select_id,
            $col_type = +20.050627E+6;
let $col_type= my_datetime;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_date;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_timestamp;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_time;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_year;
--source suite/funcs_1/views/fv_cast.inc


# 1.1.4. CAST --> DATETIME
--echo ##### 1.1.4. CAST --> DATETIME
let $target_type= DATETIME;
#
let $col_type= my_char_30;
--source suite/funcs_1/views/fv_cast.inc
eval INSERT INTO t1_values SET select_id = @select_id,
            $col_type = '2005-06-27 17:58';
let $col_type= my_varchar_1000;
--source suite/funcs_1/views/fv_cast.inc
eval INSERT INTO t1_values SET select_id = @select_id,
            $col_type = '2005-06-27 17:58';
let $col_type= my_binary_30;
--source suite/funcs_1/views/fv_cast.inc
eval INSERT INTO t1_values SET select_id = @select_id,
            $col_type = '2005-06-27 17:58';
let $col_type= my_varbinary_1000;
--source suite/funcs_1/views/fv_cast.inc
eval INSERT INTO t1_values SET select_id = @select_id,
            $col_type = '2005-06-27 17:58';
let $col_type= my_bigint;
--source suite/funcs_1/views/fv_cast.inc
eval INSERT INTO t1_values SET select_id = @select_id,
            $col_type = 200506271758;
let $col_type= my_double;
--source suite/funcs_1/views/fv_cast.inc
eval INSERT INTO t1_values SET select_id = @select_id,
            $col_type = +0.0200506271758E+13;
let $col_type= my_datetime;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_date;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_timestamp;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_time;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_year;
--source suite/funcs_1/views/fv_cast.inc


# 1.1.5. CAST --> TIME
--echo ##### 1.1.5. CAST --> TIME
let $target_type= TIME;
#
let $col_type= my_char_30;
--source suite/funcs_1/views/fv_cast.inc
eval INSERT INTO t1_values SET select_id = @select_id,
            $col_type = '1 17:58';
let $col_type= my_varchar_1000;
--source suite/funcs_1/views/fv_cast.inc
eval INSERT INTO t1_values SET select_id = @select_id,
            $col_type = '1 17:58';
let $col_type= my_binary_30;
--source suite/funcs_1/views/fv_cast.inc
eval INSERT INTO t1_values SET select_id = @select_id,
            $col_type = '1 17:58';
let $col_type= my_varbinary_1000;
--source suite/funcs_1/views/fv_cast.inc
eval INSERT INTO t1_values SET select_id = @select_id,
            $col_type = '1 17:58';
let $col_type= my_bigint;
--source suite/funcs_1/views/fv_cast.inc
eval INSERT INTO t1_values SET select_id = @select_id,
            $col_type = 1758;
let $col_type= my_double;
# Bug#12440: CAST(data type DOUBLE AS TIME) strange results;
--source suite/funcs_1/views/fv_cast.inc
eval INSERT INTO t1_values SET select_id = @select_id,
            $col_type = +1.758E+3;
let $col_type= my_datetime;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_date;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_timestamp;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_time;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_year;
--source suite/funcs_1/views/fv_cast.inc


# 1.1.6. CAST --> DECIMAL
--echo ##### 1.1.6. CAST --> DECIMAL
# Set the following to (37,2) since the default was changed to (10,0) - OBN
let $target_type= DECIMAL(37,2);
#
let $col_type= my_char_30;
--source suite/funcs_1/views/fv_cast.inc
eval INSERT INTO t1_values SET select_id = @select_id,
            $col_type = '-3333.3333';
let $col_type= my_varchar_1000;
--source suite/funcs_1/views/fv_cast.inc
eval INSERT INTO t1_values SET select_id = @select_id,
            $col_type = '-3333.3333';
let $col_type= my_binary_30;
--source suite/funcs_1/views/fv_cast.inc
eval INSERT INTO t1_values SET select_id = @select_id,
            $col_type = '-3333.3333';
let $col_type= my_varbinary_1000;
--source suite/funcs_1/views/fv_cast.inc
eval INSERT INTO t1_values SET select_id = @select_id,
            $col_type = '-3333.3333';
let $col_type= my_bigint;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_decimal;
--source suite/funcs_1/views/fv_cast.inc
# Bug#13349: CAST(1.0E+300 TO DECIMAL) returns wrong result + diff little/big endian;
let $col_type= my_double;
--source suite/funcs_1/views/fv_cast.inc
eval INSERT INTO t1_values SET select_id = @select_id,
            $col_type = -0.33333333E+4;
let $col_type= my_datetime;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_date;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_timestamp;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_time;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_year;
--source suite/funcs_1/views/fv_cast.inc


# 1.1.7. CAST --> SIGNED INTEGER
--echo ##### 1.1.7. CAST --> SIGNED INTEGER
let $target_type= SIGNED INTEGER;
#
let $message=
"Attention: CAST --> SIGNED INTEGER
            Bug#5913 Traditional mode: BIGINT range not correctly delimited
            Status: To be fixed later";
--source include/show_msg80.inc
let $col_type= my_char_30;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_varchar_1000;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_binary_30;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_varbinary_1000;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_bigint;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_decimal;
--source suite/funcs_1/views/fv_cast.inc
# Bug #13344: CAST(1E+300 TO signed int) on little endian CPU, wrong result;
let $col_type= my_double;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_datetime;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_date;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_timestamp;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_time;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_year;
--source suite/funcs_1/views/fv_cast.inc


# 1.1.8. CAST --> UNSIGNED INTEGER
--echo ##### 1.1.8. CAST --> UNSIGNED INTEGER
let $target_type= UNSIGNED INTEGER;
#
let $message=
"Attention: CAST --> UNSIGNED INTEGER
            The file with expected results suffers from Bug 5913";
--source include/show_msg80.inc
let $col_type= my_char_30;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_varchar_1000;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_binary_30;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_varbinary_1000;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_bigint;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_decimal;
--source suite/funcs_1/views/fv_cast.inc
let $message= some statements disabled because of
Bug#5913 Traditional mode: BIGINT range not correctly delimited;
--source include/show_msg80.inc
# Bug#8663 cant use bgint unsigned as input to cast
let $col_type= my_double;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_datetime;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_date;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_timestamp;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_time;
--source suite/funcs_1/views/fv_cast.inc
let $col_type= my_year;
--source suite/funcs_1/views/fv_cast.inc


# 1.2.   BINARY
#        Manual: BINARY str is a shorthand for CAST(str AS BINARY).
#        Therefore we do not test it here in the moment.
#        FIXME: Add testcases for str in CHAR and VARCHAR only.


# 1.3    CONVERT(expr USING transcoding_name)
#
# 1.3.1  CONVERT(expr USING utf8)
let $target_charset= utf8;
#
let $col_type= my_char_30;
eval SET @my_select = 'SELECT CONVERT($col_type USING $target_charset),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv1.inc
let $col_type= my_varchar_1000;
eval SET @my_select = 'SELECT CONVERT($col_type USING $target_charset),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv1.inc
let $col_type= my_binary_30;
eval SET @my_select = 'SELECT CONVERT($col_type USING $target_charset),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv1.inc
let $col_type= my_varbinary_1000;
eval SET @my_select = 'SELECT CONVERT($col_type USING $target_charset),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv1.inc
#
# 1.3.2  CONVERT(expr USING koi8r)
let $target_charset= koi8r;
let $col_type= my_char_30;
eval SET @my_select = 'SELECT CONVERT($col_type USING $target_charset),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv1.inc
let $col_type= my_varchar_1000;
eval SET @my_select = 'SELECT CONVERT($col_type USING $target_charset),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv1.inc
let $col_type= my_binary_30;
eval SET @my_select = 'SELECT CONVERT($col_type USING $target_charset),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv1.inc
let $col_type= my_varbinary_1000;
eval SET @my_select = 'SELECT CONVERT($col_type USING $target_charset),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv1.inc


# 2.     Control Flow Functions
# 2.1.   CASE value WHEN [compare-value] THEN result [WHEN ...] [ELSE result]
#        END                 or
#        CASE WHEN [condition] THEN result [WHEN ...] [ELSE result] END
#
#        FIXME: to be implemented
#
# 2.2.   IF(expr1,expr2,expr3)
#           expr1 is TRUE when (expr1 <> 0 and expr1 <> NULL) is fulfilled
#
# 2.2.1  IF(expr1,expr2,expr3) with expr1 = <column>
#
#        Note(mleich): Strings, which do not contain a number -> FALSE
#
#        suite/funcs_1/views/fv_if1.inc contains
#                SELECT IF($col_type, 'IS TRUE', 'IS NOT TRUE'), ...
#
let $col_type= my_char_30;
--source suite/funcs_1/views/fv_if1.inc
#
let $col_type= my_varchar_1000;
--source suite/funcs_1/views/fv_if1.inc
#
let $col_type= my_binary_30;
--source suite/funcs_1/views/fv_if1.inc
#
let $col_type= my_varbinary_1000;
--source suite/funcs_1/views/fv_if1.inc
#
let $col_type= my_bigint;
--source suite/funcs_1/views/fv_if1.inc
#
let $col_type= my_decimal;
--source suite/funcs_1/views/fv_if1.inc
#
let $col_type= my_double;
--source suite/funcs_1/views/fv_if1.inc
#
let $col_type= my_datetime;
--source suite/funcs_1/views/fv_if1.inc
#
let $col_type= my_date;
--source suite/funcs_1/views/fv_if1.inc
#
let $col_type= my_timestamp;
--source suite/funcs_1/views/fv_if1.inc
#
let $col_type= my_time;
--source suite/funcs_1/views/fv_if1.inc
#
let $col_type= my_year;
--source suite/funcs_1/views/fv_if1.inc


# 2.2.2  IF(expr1,expr2,expr3) with expr1 != <column>
#
#        suite/funcs_1/views/fv_if2.inc contains
#                SELECT IF($col_type IS NULL, 'IS     NULL', 'IS NOT NULL'), ...
#
# Note(mleich): July 2005
#               IF($col_type IS NULL, ...) is mapped to a VIEW definition
#                  create ... view ... as
#                  select if(isnull(`test`.`t1`.`f1`),_latin1'IS     NULL',
#                  _latin1'IS NOT NULL'),...
#
# Bug#11689 success on Create view .. IF(col1 IS NULL,...), col2 ; but SELECT fails
let $col_type= my_char_30;
--source suite/funcs_1/views/fv_if2.inc
#
let $col_type= my_varchar_1000;
--source suite/funcs_1/views/fv_if2.inc
#
let $col_type= my_binary_30;
--source suite/funcs_1/views/fv_if2.inc
#
let $col_type= my_varbinary_1000;
--source suite/funcs_1/views/fv_if2.inc
#
let $col_type= my_bigint;
--source suite/funcs_1/views/fv_if2.inc
#
let $col_type= my_decimal;
--source suite/funcs_1/views/fv_if2.inc
#
let $col_type= my_double;
--source suite/funcs_1/views/fv_if2.inc
#
let $col_type= my_datetime;
--source suite/funcs_1/views/fv_if2.inc
#
let $col_type= my_date;
--source suite/funcs_1/views/fv_if2.inc
#
let $col_type= my_timestamp;
--source suite/funcs_1/views/fv_if2.inc
#
let $col_type= my_time;
--source suite/funcs_1/views/fv_if2.inc
#
let $col_type= my_year;
--source suite/funcs_1/views/fv_if2.inc


# 2.3.   IFNULL(expr1,expr2)
#           If expr1 is not NULL, IFNULL() returns expr1, else it returns expr2.
#
#        suite/funcs_1/views/fv_ifnull.inc contains
#                SELECT IFNULL($col_type, 'IS_NULL'), ....
#        FIXME: The mixup of non string column values
#               and the string 'IS    NULL' within the first column of the
#               result table is extreme ugly.
#               CAST(IFNULL($col_type, 'IS_NULL') AS CHAR) looks better, but
#               it has the disadvantage, that it involves CAST as additional
#               function.
#
let $col_type= my_char_30;
--source suite/funcs_1/views/fv_ifnull.inc
#
let $col_type= my_varchar_1000;
--source suite/funcs_1/views/fv_ifnull.inc
#
let $col_type= my_binary_30;
--source suite/funcs_1/views/fv_ifnull.inc
#
let $col_type= my_varbinary_1000;
--source suite/funcs_1/views/fv_ifnull.inc
#
let $col_type= my_bigint;
--source suite/funcs_1/views/fv_ifnull.inc
#
let $col_type= my_decimal;
--source suite/funcs_1/views/fv_ifnull.inc
#
let $col_type= my_double;
--source suite/funcs_1/views/fv_ifnull.inc
#
let $col_type= my_datetime;
--source suite/funcs_1/views/fv_ifnull.inc
#
let $col_type= my_date;
--source suite/funcs_1/views/fv_ifnull.inc
#
let $col_type= my_timestamp;
--source suite/funcs_1/views/fv_ifnull.inc
#
let $col_type= my_time;
--source suite/funcs_1/views/fv_ifnull.inc
#
let $col_type= my_year;
--source suite/funcs_1/views/fv_ifnull.inc


# 2.4.    NULLIF(expr1,expr2)
#            Returns NULL if expr1 = expr2 is true, else returns expr1.
#            This is the same as
#                 CASE WHEN expr1 = expr2 THEN NULL ELSE expr1 END.
#
#         FIXME: to be implemented
#


# 3.      String Functions
# 3.1.    ASCII(str)
# 3.2.    BIN(N)
#         FIXME: to be implemented
#
# 3.3.    BIT_LENGTH(str)
#            Returns the length of the string str in bits.
#
let $col_type= my_char_30;
eval SET @my_select = 'SELECT BIT_LENGTH($col_type),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv1.inc
let $col_type= my_varchar_1000;
eval SET @my_select = 'SELECT BIT_LENGTH($col_type),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv1.inc
let $col_type= my_binary_30;
eval SET @my_select = 'SELECT BIT_LENGTH($col_type),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv1.inc
let $col_type= my_varbinary_1000;
eval SET @my_select = 'SELECT BIT_LENGTH($col_type),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv1.inc


# 3.4.    CHAR(N,...)
# 3.5.    CHAR_LENGTH(str)
# 3.6     CHARACTER_LENGTH(str)
#            CHARACTER_LENGTH() is a synonym for CHAR_LENGTH().
# 3.7.    COMPRESS(string_to_compress)
# 3.8.    CONCAT(str1,str2,...)
# 3.9.    CONCAT_WS(separator,str1,str2,...)
# 3.10.   CONV(N,from_base,to_base)
# 3.11.   ELT(N,str1,str2,str3,...)
# 3.12.   EXPORT_SET(bits,on,off[,separator[,number_of_bits]])
# 3.13.   FIELD(str,str1,str2,str3,...)
# 3.14.   FIND_IN_SET(str,strlist)
# 3.15.   HEX(N_or_S
# 3.16.   INSERT(str,pos,len,newstr)
# 3.17.   INSTR(str,substr)
#            This is the same as the two-argument form of LOCATE(),
#            except that the arguments are swapped.
#            The majority of the testcases should be made with LOCATE().
#            Therefore test only one example here.
let $col_type= my_char_30;
eval SET @my_select = 'SELECT INSTR($col_type, ''char''),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc


# 3.18.   LCASE(str)
#         LCASE() is a synonym for LOWER().
#            The majority of the testcases should be made with LOWER().
#            Therefore test only one example here.
let $col_type= my_varchar_1000;
eval SET @my_select = 'SELECT LCASE($col_type),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc


# 3.19.   LEFT(str,len)
#            Returns the leftmost len characters from the string str.
let $col_type= my_char_30;
eval SET @my_select =
'SELECT LEFT($col_type, 2), $col_type, id FROM t1_values';
--source suite/funcs_1/views/fv1.inc
let $col_type= my_varchar_1000;
eval SET @my_select =
'SELECT LEFT($col_type, 2), $col_type, id FROM t1_values';
--source suite/funcs_1/views/fv1.inc
let $col_type= my_binary_30;
eval SET @my_select =
'SELECT LEFT($col_type, 2), $col_type, id FROM t1_values';
--source suite/funcs_1/views/fv1.inc
let $col_type= my_varbinary_1000;
eval SET @my_select =
'SELECT LEFT($col_type, 2), $col_type, id FROM t1_values';
--source suite/funcs_1/views/fv1.inc
# Bug#11728 string function LEFT, strange undocumented behaviour, strict mode
# Bug#10963 LEFT string function returns wrong result with large length
let $col_type= my_bigint;
eval SET @my_select =
'SELECT LEFT(''AaBbCcDdEeFfGgHhIiJjÄäÜüÖö'', $col_type), $col_type, id FROM t1_values';
--source suite/funcs_1/views/fv1.inc
let $col_type= my_decimal;
eval SET @my_select =
'SELECT LEFT(''AaBbCcDdEeFfGgHhIiJjÄäÜüÖö'', $col_type), $col_type, id FROM t1_values';
--source suite/funcs_1/views/fv1.inc
# Bug#10963 LEFT string function returns wrong result with large length
let $col_type= my_double;
eval SET @my_select =
'SELECT LEFT(''AaBbCcDdEeFfGgHhIiJjÄäÜüÖö'', $col_type), $col_type, id FROM t1_values';
--source suite/funcs_1/views/fv1.inc

# 3.20.   LENGTH(str)
let $col_type= my_char_30;
eval SET @my_select = 'SELECT LENGTH($col_type),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type= my_varchar_1000;
eval SET @my_select = 'SELECT LENGTH($col_type),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type= my_binary_30;
eval SET @my_select = 'SELECT LENGTH($col_type),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type= my_varbinary_1000;
eval SET @my_select = 'SELECT LENGTH($col_type),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc


# 3.21.   LOAD_FILE(file_name)
#            Reads the file and returns the file contents as a string.
#            If the file doesn't exist or cannot be read ... ,
#            the function returns NULL.
# SELECT LOADFILE
--replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
eval SET @my_select =
'SELECT LOAD_FILE(''$MYSQLTEST_VARDIR/std_data/funcs_1/load_file.txt'')
           AS my_col,
        id FROM t1_values';
--source suite/funcs_1/views/fv1.inc


# 3.22.    LOCATE(substr,str) , LOCATE(substr,str,pos)
let $col_type= my_char_30;
eval SET @my_select = 'SELECT LOCATE(''char'', $col_type),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type= my_varchar_1000;
eval SET @my_select = 'SELECT LOCATE(''char'', $col_type),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type= my_binary_30;
eval SET @my_select = 'SELECT LOCATE(''char'', $col_type),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type= my_varbinary_1000;
eval SET @my_select = 'SELECT LOCATE(''char'', $col_type),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
#------------------------------------------------------
let $col_type1= my_char_30;
# against all other
eval SET @my_select = 'SELECT LOCATE($col_type1, $col_type1 ),
$col_type1, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type2= my_varchar_1000;
eval SET @my_select = 'SELECT LOCATE($col_type1, $col_type2 ),
$col_type1, $col_type2 id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type2= my_binary_30;
eval SET @my_select = 'SELECT LOCATE($col_type1, $col_type2 ),
$col_type1, $col_type2 id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type2= my_varbinary_1000;
eval SET @my_select = 'SELECT LOCATE($col_type1, $col_type2 ),
$col_type1, $col_type2 id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
#------------------------------------------------------
let $col_type1= my_varchar_1000;
# against all other
eval SET @my_select = 'SELECT LOCATE($col_type1, $col_type1 ),
$col_type1, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type2= my_char_30;
eval SET @my_select = 'SELECT LOCATE($col_type1, $col_type2 ),
$col_type1, $col_type2 id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type2= my_binary_30;
eval SET @my_select = 'SELECT LOCATE($col_type1, $col_type2 ),
$col_type1, $col_type2 id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type2= my_varbinary_1000;
eval SET @my_select = 'SELECT LOCATE($col_type1, $col_type2 ),
$col_type1, $col_type2 id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
#------------------------------------------------------
let $col_type1= my_binary_30;
# against all other
eval SET @my_select = 'SELECT LOCATE($col_type1, $col_type1 ),
$col_type1, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type2= my_char_30;
eval SET @my_select = 'SELECT LOCATE($col_type1, $col_type2 ),
$col_type1, $col_type2 id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type2= my_varchar_1000;
eval SET @my_select = 'SELECT LOCATE($col_type1, $col_type2 ),
$col_type1, $col_type2 id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type2= my_varbinary_1000;
eval SET @my_select = 'SELECT LOCATE($col_type1, $col_type2 ),
$col_type1, $col_type2 id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
#------------------------------------------------------
let $col_type1= my_varbinary_1000;
# against all other
eval SET @my_select = 'SELECT LOCATE($col_type1, $col_type1 ),
$col_type1, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type2= my_char_30;
eval SET @my_select = 'SELECT LOCATE($col_type1, $col_type2 ),
$col_type1, $col_type2 id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type2= my_varchar_1000;
eval SET @my_select = 'SELECT LOCATE($col_type1, $col_type2 ),
$col_type1, $col_type2 id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type2= my_binary_30;
eval SET @my_select = 'SELECT LOCATE($col_type1, $col_type2 ),
$col_type1, $col_type2 id FROM t1_values';
--source suite/funcs_1/views/fv2.inc

# FIXME How to test exotic or interesting substr values like NULL, '', ' '
#       without getting too much result rows
# FIXME Testcases with LOCATE(substr,str,pos)
let $col_type= my_char_30;
eval SET @my_select = 'SELECT LOCATE(''-'', $col_type, 3),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type= my_varchar_1000;
eval SET @my_select = 'SELECT LOCATE(''-'', $col_type, 3),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type= my_binary_30;
eval SET @my_select = 'SELECT LOCATE(''-'', $col_type, 3),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type= my_varbinary_1000;
eval SET @my_select = 'SELECT LOCATE(''-'', $col_type, 3),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
#--------------------------------------------------------
let $col_type= my_bigint;
eval SET @my_select = 'SELECT LOCATE(''-'', '' - -ABC'', $col_type),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type= my_double;
eval SET @my_select = 'SELECT LOCATE(''-'', '' - -ABC'', $col_type),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type= my_decimal;
eval SET @my_select = 'SELECT LOCATE(''-'', '' - -ABC'', $col_type),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc


# 3.23.    LOWER(str)
let $col_type= my_char_30;
eval SET @my_select = 'SELECT LOWER($col_type),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type= my_varchar_1000;
eval SET @my_select = 'SELECT LOWER($col_type),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type= my_binary_30;
eval SET @my_select = 'SELECT LOWER($col_type),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type= my_varbinary_1000;
eval SET @my_select = 'SELECT LOWER($col_type),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc


# 3.24.    LPAD(str,len,padstr)
# 3.25.    LTRIM(str)
let $col_type= my_char_30;
eval SET @my_select = 'SELECT LTRIM($col_type),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type= my_varchar_1000;
eval SET @my_select = 'SELECT LTRIM($col_type),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type= my_binary_30;
eval SET @my_select = 'SELECT LTRIM($col_type),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc
let $col_type= my_varbinary_1000;
eval SET @my_select = 'SELECT LTRIM($col_type),
$col_type, id FROM t1_values';
--source suite/funcs_1/views/fv2.inc


# 3.26.    MAKE_SET(bits,str1,str2,...)
# .....
#         FIXME: to be implemented

################################################################################
# Please do not add SELECTs and interesting records after this line.           #
# These last SELECTs are mostly for checking the testcase code itself.         #
################################################################################
eval SET @my_select =
       'SELECT CONCAT(''A'',my_char_30), my_char_30, id FROM t1_values';       #
--source suite/funcs_1/views/fv1.inc
#
eval SET @my_select = 'SELECT my_char_30, id FROM t1_values';                  #
--source suite/funcs_1/views/fv2.inc
eval INSERT INTO t1_values SET select_id = @select_id,
            my_char_30 = 'Viana do Castelo';
################################################################################
SET sql_mode = '';                                                             #

##### END        Basic preparations      #######################################


let $message= "# The basic preparations end and the main test starts here";
--source include/show_msg80.inc

--disable_ps_protocol

##### The tests start here #####################################################

# Determine the number of different SELECTs to be checked
--disable_query_log
SELECT COUNT(*) INTO @num_selects FROM t1_selects;
--enable_query_log
# Debug statement
# SELECT @num_selects AS "number of SELECTS:";

--disable_abort_on_error
let $select_id= `SELECT @num_selects`;
while ($select_id)
{
    # Determine the SELECT
    --disable_query_log
    eval SELECT my_select, disable_result INTO @my_select, @disable_result
         FROM t1_selects WHERE id = $select_id;
    let $run_no_result= `SELECT @disable_result = 'Yes'`;
    --enable_query_log
    # Debug statement
    # SELECT @my_select AS "SELECT:";
    let $my_select= `SELECT @my_select`;

    let $run0= 0;
    if ($view_select_result)
    {
       # Create the VIEW
       --replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
       eval CREATE VIEW v1 AS $my_select;
       --disable_query_log
       eval set @got_errno= $mysql_errno ;
       let $run0= `SELECT @got_errno = 0`;
       --enable_query_log
       if (!$run0)
       {
          --echo
          --echo Attention: The last CREATE VIEW failed
          --echo
       }
    }

    # FIXME           The loop over the modes will start here.

    if ($simple_select_result)
    {
       # Simple SELECT on the base table of the VIEW for comparison

       if ($run_no_result)
       {
          --disable_result_log
       }
       --replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
       eval $my_select
            WHERE select_id = $select_id OR select_id IS NULL order by id;
       if ($run_no_result)
       {
          --enable_result_log
       }
       if ($mysql_errno)
       {
          --echo
          --echo Attention: The last SELECT on the base table failed
          --echo
       }
    }

    # $run0 is 1, if CREATE VIEW was successful.
    # That means SHOW CREATE VIEW/SELECT/DROP should be executed.
    if ($run0)
    {
       # Check the CREATE VIEW statement
       --replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
       SHOW CREATE VIEW v1;
       if ($mysql_errno)
       {
          --echo
          --echo Attention: The last SHOW CREATE VIEW failed
          --echo
       }

       # Maybe a Join is faster
       if ($run_no_result)
       {
          --disable_result_log
       }
       eval SELECT v1.* FROM v1
            WHERE v1.id IN (SELECT id FROM t1_values
                            WHERE select_id = $select_id OR select_id IS NULL) order by id;
       if ($run_no_result)
       {
          --enable_result_log
       }
       if ($mysql_errno)
       {
          --echo
          --echo Attention: The last SELECT from VIEW failed
          --echo
       }

       DROP VIEW v1;
    }

    # FIXME         The loop over the modes will end here.

    # Produce two empty lines as separator between different SELECTS
    # to be tested.
    --echo
    --echo

    dec $select_id ;
}

--enable_ps_protocol

DROP TABLE t1_selects, t1_modes, t1_values;
