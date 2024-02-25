################################################################################
# inc/partition_check.inc                                                      #
#                                                                              #
# Purpose:                                                                     #
#   Do some basic usability checks on table t1.                                #
#   This routine is only useful for the partition_<feature>_<engine> tests.    #
#                                                                              #
# Some Notes:                                                                  #
#   It is intended that in many testcases (statements) more than one partition #
#   or subpartition is affected.                                               #
#   Without analysis of the partitioning function used during CREATE TABLE     #
#   we cannot be 100% sure that this goal is reached.                          #
#   But statements affecting many rows give a good probability that this       #
#   appears.                                                                   #
#                                                                              #
#   It is expected that the table to be checked contains at the beginning      #
#   of this script records following the scheme                                #
#       f_int1  f_int2  f_char1  f_char2  f_charbig                            #
#          1        1     '1'      '1'    '###1###'                            #
#          2        2     '2'      '1'    '###2###'                            #
#         ...      ...    ...      ...       ...                               #
#          x        x     'x'      'x'    '###x###'                            #
#   x = @max_row                                                               #
#                                                                              #
#   The table content must be equal to the content of the table t0_template.   #
#   Attention: Please be careful when modiying the data.                       #
#              Records can be deleted or inserted, but the content of the      #
#              records after a test/testsequence should follow this scheme.    #
#                                                                              #
#   All checks of preceding statements via Select are so written,              #
#   that they deliver a                                                        #
#       # check <n> success:        1                                          #
#   when everything is like expected.                                          #
# - f_charbig is typically used for showing if something was changed.          #
#                      This is useful for cleanups.                            #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: mleich                                                      #
# Original Date:   2006-03-05                                                  #
# Change Author:   mleich                                                      #
# Change Date:     2007-10-08                                                  #
# Change:          Around fix for                                              #
#                  Bug#31243 Test "partition_basic_myisam" truncates path names#
#                  Adjustments of expected error codes:                        #
#                  ER_NO_PARTITION_FOR_GIVEN_VALUE is now 1525                 #
#                  ER_SAME_NAME_PARTITION is now 1516                          #
################################################################################


--echo # Start usability test (inc/partition_check.inc)
# Print the CREATE TABLE STATEMENT and store the current layout of the table
--source suite/parts/inc/partition_layout_check1.inc


#-------------------------------------------------------------------------------
## 1.   Check the prerequisites for the following tests
#             (sideeffect some SELECT functionality is also tested)
#       Determine if we have PRIMARY KEYs or UNIQUE INDEXes
## 1.1  Check if the content of the records is like expected
#       Sideeffect: mass SELECT, all records/partitions/subpartitions have to be
#                   read, because at least f_charbig is not part of any
#                   INDEX/PRIMARY KEY
if ($no_debug)
{
--disable_query_log
}
let $my_stmt= SELECT COUNT(*) <> 0 INTO @aux FROM t1
WHERE f_int1 <> f_int2 OR f_char1 <> CAST(f_int1 AS CHAR) OR f_char1 <> f_char2
   OR f_charbig <> CONCAT('===',f_char1,'===')
   OR f_int1 IS NULL OR f_int2 IS NULL OR f_char1 IS NULL OR f_char2 IS NULL
   OR f_charbig IS NULL;
eval $my_stmt;
let $run= `SELECT @aux`;
--enable_query_log
if ($run)
{
   --echo #      Prerequisites for following tests not fullfilled.
   --echo #      The content of the table t1 is unexpected
   eval $my_stmt;
   SELECT @aux;
   --echo #      Sorry, have to abort.
   exit;
}
# Give a success message like in the other following tests
--echo # check prerequisites-1 success:    1
#-------------------------------------------------------------------------------
## 1.2  Check if the number of records and the maximum and minimum values are
#       like expected
#       Sideeffect: Check
#            - COUNT(*)
#            - MIN/MAX on all columns possibly used in part. function
#              The optimizer might decide to run on INDEX only, if available.
#
## 1.2.1 Check COUNT(*)
if ($no_debug)
{
--disable_query_log
}
let $my_stmt= SELECT COUNT(*) <> @max_row INTO @aux FROM t1;
let $run= `SELECT @aux`;
--enable_query_log
if ($run)
{
   --echo #      Prerequisites for following tests not fullfilled.
   --echo #      The content of the table t1 is unexpected
   eval $my_stmt;
   SELECT @aux;
   --echo #      Sorry, have to abort.
   exit;
}
# Give a success message like in the other following tests
--echo # check COUNT(*) success:    1
## 1.2.2 Check MAX(f_int1),MIN(f_int1)
if ($no_debug)
{
--disable_query_log
}
let $my_stmt= SELECT MIN(f_int1) <> 1 AND MAX(f_int1) <> @max_row INTO @aux
FROM t1;
let $run= `SELECT @aux`;
--enable_query_log
if ($run)
{
   --echo #      Prerequisites for following tests not fullfilled.
   --echo #      The content of the table t1 is unexpected
   eval $my_stmt;
   SELECT @aux;
   --echo #      Sorry, have to abort.
   exit;
}
# Give a success message like in the other following tests
--echo # check MIN/MAX(f_int1) success:    1
## 1.2.3 Check MAX(f_int2),MIN(f_int2)
if ($no_debug)
{
--disable_query_log
}
let $my_stmt= SELECT MIN(f_int2) <> 1 AND MAX(f_int2) <> @max_row INTO @aux
FROM t1;
let $run= `SELECT @aux`;
--enable_query_log
if ($run)
{
   --echo #      Prerequisites for following tests not fullfilled.
   --echo #      The content of the table t1 is unexpected
   eval $my_stmt;
   SELECT @aux;
   --echo #      Sorry, have to abort.
   exit;
}
# Give a success message like in the other following tests
--echo # check MIN/MAX(f_int2) success:    1

#-------------------------------------------------------------------------------
## 1.3  Check, if f_int1 and/or f_char2 and/or (f_char1,f_char2) is UNIQUE
#       by testing if any DUPLICATE KEY might appear
#       Note: INFORMATION_SCHEMA SELECTs could be also used, but testing the
#             table via INSERT and SELECT is better because is stresses the
#             partitioning mechanism.
#       Sideeffect: Attempt to INSERT one record
#             DUPLICATE KEY will appear if we have UNIQUE columns
#             ER_DUP_KEY, ER_DUP_ENTRY
--disable_abort_on_error
--replace_regex /'3-3'/'2-2'/   # Don't care which row is reported.
INSERT INTO t1 (f_int1, f_int2, f_char1, f_char2, f_charbig)
SELECT f_int1, f_int1, CAST(f_int1 AS CHAR),
       CAST(f_int1 AS CHAR), 'delete me' FROM t0_template
WHERE f_int1 IN (2,3);
--enable_abort_on_error
if ($no_debug)
{
--disable_query_log
}
eval SET @my_errno = $mysql_errno;
let $run_delete= `SELECT @my_errno = 0`;
let $any_unique= `SELECT @my_errno IN ($ER_DUP_KEY,$ER_DUP_ENTRY)`;
# DEBUG eval SELECT $run_delete AS run_delete, $any_unique AS any_unique,
#                   @my_errno AS sql_errno;
if (`SELECT @my_errno NOT IN (0,$ER_DUP_KEY,$ER_DUP_ENTRY)`)
{
   --echo #      The last command got an unexpected error response.
   --echo #      Expected/handled SQL codes are 0,$ER_DUP_KEY,$ER_DUP_ENTRY
   SELECT '#      SQL code we got was: ' AS "", @my_errno AS "";
   --echo #      Sorry, have to abort.
   exit;
   --echo
}
# Give a success message like in the other following tests
--echo # check prerequisites-3 success:    1
--enable_query_log
# DEBUG eval SELECT $run_delete AS run_delete, $any_unique AS any_unique;
if ($run_delete)
{
   # INSERT was successful -> DELETE this new record
   DELETE FROM t1 WHERE f_charbig = 'delete me';
   --echo # INFO: Neither f_int1 nor f_int2 nor (f_int1,f_int2) is UNIQUE
}
if ($any_unique)
{
   --echo # INFO: f_int1 AND/OR f_int2 AND/OR (f_int1,f_int2) is UNIQUE

   ## 1.3.1  Check, if f_int1 is UNIQUE
   #       Sideeffect: Attempt to INSERT one record
   #             DUPLICATE KEY will appear if we have UNIQUE columns
   #             ER_DUP_KEY, ER_DUP_ENTRY
   --disable_abort_on_error
   INSERT INTO t1 (f_int1, f_int2, f_char1, f_char2, f_charbig)
   SELECT f_int1, 2 * @max_row + f_int1, CAST((2 * @max_row + f_int1) AS CHAR),
          CAST((2 * @max_row + f_int1) AS CHAR), 'delete me' FROM t0_template
   WHERE f_int1 IN (2,3);
   --enable_abort_on_error
   if ($no_debug)
   {
   --disable_query_log
   }
   eval SET @my_errno = $mysql_errno;
   let $run_delete= `SELECT @my_errno = 0`;
   let $f_int1_is_unique= `SELECT @my_errno IN ($ER_DUP_KEY,$ER_DUP_ENTRY)`;
   # DEBUG eval SELECT $run_delete AS run_delete, $f_int1_is_unique AS any_unique,
   #                   @my_errno AS sql_errno;
   if (`SELECT @my_errno NOT IN (0,$ER_DUP_KEY,$ER_DUP_ENTRY)`)
   {
      --echo #      The last command got an unexpected error response.
      --echo #      Expected/handled SQL codes are 0,$ER_DUP_KEY,$ER_DUP_ENTRY
      SELECT '#      SQL code we got was: ' AS "", @my_errno AS "";
      --echo #      Sorry, have to abort.
      exit;
      --echo
   }
   --enable_query_log
   if ($f_int1_is_unique)
   {
      --echo # INFO: f_int1 is UNIQUE
   }
   if ($run_delete)
   {
      # INSERT was successful -> DELETE this new record
      DELETE FROM t1 WHERE f_charbig = 'delete me';
   }

   ## 1.3.2  Check, if f_int2 is UNIQUE (get ER_DUP_KEY or ER_DUP_ENTRY
   --disable_abort_on_error
   INSERT INTO t1 (f_int1, f_int2, f_char1, f_char2, f_charbig)
   SELECT 2 * @max_row + f_int1, f_int1, CAST((2 * @max_row + f_int1) AS CHAR),
          CAST((2 * @max_row + f_int1) AS CHAR), 'delete me' FROM t0_template
   WHERE f_int1 IN (2,3);
   --enable_abort_on_error
   if ($no_debug)
   {
   --disable_query_log
   }
   eval SET @my_errno = $mysql_errno;
   let $run_delete= `SELECT @my_errno = 0`;
   let $f_int1_is_unique= `SELECT @my_errno IN ($ER_DUP_KEY,$ER_DUP_ENTRY)`;
   # DEBUG eval SELECT $run_delete AS run_delete, $f_int1_is_unique AS any_unique,
   #                   @my_errno AS sql_errno;
   if (`SELECT @my_errno NOT IN (0,$ER_DUP_KEY,$ER_DUP_ENTRY)`)
   {
      --echo #      The last command got an unexpected error response.
      --echo #      Expected/handled SQL codes are 0,$ER_DUP_KEY,$ER_DUP_ENTRY
      SELECT '#      SQL code we got was: ' AS "", @my_errno AS "";
      --echo #      Sorry, have to abort.
      exit;
      --echo
   }
   if ($f_int2_is_unique)
   {
      --echo # INFO: f_int2 is UNIQUE
   }
   --enable_query_log
   if ($run_delete)
   {
      # INSERT was successful -> DELETE this new record
      DELETE FROM t1 WHERE f_charbig = 'delete me';
   }
}


#-------------------------------------------------------------------------------
## 2.   Read the table row by row
#       Note: There were crashes in history when reading a partitioned table
#             PRIMARY KEY AND/OR UNIQUE INDEXes
## 2.1  Read all existing and some not existing records of table
#       per f_int1 used in partitioning function
let $col_to_check= f_int1;
--source suite/parts/inc/partition_check_read.inc
## 2.2  Read all existing and some not existing records of table
#       per f_int2 used in partitioning function
let $col_to_check= f_int2;
--source suite/parts/inc/partition_check_read.inc


#-------------------------------------------------------------------------------
# 3    Some operations with multiple records
# 3.1  Select on "full" table
if ($no_debug)
{
--disable_query_log
}
SELECT '# check multiple-1 success: ' AS "",COUNT(*) = @max_row AS "" FROM t1;
--enable_query_log
#
# 3.2  (mass) DELETE of @max_row_div3 records
DELETE FROM t1 WHERE MOD(f_int1,3) = 0;
if ($no_debug)
{
--disable_query_log
}
SELECT '# check multiple-2 success: ' AS "",COUNT(*) = @max_row - @max_row_div3 AS "" FROM t1;
--enable_query_log
#
# 3.3  (mass) Insert of @max_row_div3 records
#             (Insert the records deleted in 3.2)
INSERT INTO t1 SELECT * FROM t0_template
WHERE MOD(f_int1,3) = 0;
#      Check of preceding statement via Select
if ($no_debug)
{
--disable_query_log
}
SELECT '# check multiple-3 success: ' AS "",
(COUNT(*) = @max_row) AND (MIN(f_int1) = 1) AND (MAX(f_int1) = @max_row) AS ""
FROM t1;
--enable_query_log
# DEBUG SELECT COUNT(*),MIN(f_int1),MAX(f_int1) FROM t1;
#
# 3.4  (mass) Update @max_row_div4 * 2 + 1 records
# Bug#18735: Partitions: NDB, UNIQUE INDEX, UPDATE, strange server response
UPDATE t1 SET f_int1 = f_int1 + @max_row
WHERE f_int1 BETWEEN @max_row_div2 - @max_row_div4
  AND @max_row_div2 + @max_row_div4;
#      Check of preceding statement via Select
if ($no_debug)
{
--disable_query_log
}
SELECT '# check multiple-4 success: ' AS "",(COUNT(*) = @max_row) AND (MIN(f_int1) = 1) AND
(MAX(f_int1) = @max_row_div2 + @max_row_div4 + @max_row ) AS "" FROM t1;
--enable_query_log
# DEBUG SELECT COUNT(*),MIN(f_int1),MAX(f_int1) FROM t1;
#
# 3.5  (mass) Delete @max_row_div4 * 2 + 1 records
#             (Delete the records updated in 3.4)
DELETE FROM t1
WHERE f_int1 BETWEEN @max_row_div2 - @max_row_div4 + @max_row
  AND @max_row_div2 + @max_row_div4 + @max_row;
#      Check of preceding statement via Select
if ($no_debug)
{
--disable_query_log
}
SELECT '# check multiple-5 success: ' AS "",
(COUNT(*) = @max_row - @max_row_div4 - @max_row_div4 - 1)
AND (MIN(f_int1) = 1) AND (MAX(f_int1) = @max_row) AS "" FROM t1;
--enable_query_log
# DEBUG SELECT COUNT(*),MIN(f_int1),MAX(f_int1) FROM t1;

#-------------------------------------------------------------------------------
# Attention: After this section all modification on the table have to be reverted !
#            Current content of t1 follows the rule:
#                    <value>,<value>,'<value>','<value>',===<value>===
#            <value> contains all INTEGER values
#                    between 1 and @max_row_div2 - @max_row_div4 - 1
#                    and
#                    between @max_row_div2 + @max_row_div4 + 1 and @max_row
#            With other words the values between @max_row_div2 - @max_row_div4
#            and @max_row_div2 + @max_row_div4 are "missing".
#-------------------------------------------------------------------------------
# The following is only needed for tests of UNIQUE CONSTRAINTs.
if ($any_unique)
{
   # Calculate the number of records, where we will try INSERT ..... or REPLACE
   SELECT COUNT(*) INTO @try_count FROM t0_template
   WHERE MOD(f_int1,3) = 0
     AND f_int1 BETWEEN @max_row_div2 AND @max_row;
   #
   # Calculate the number of records, where we will get DUPLICATE KEY
   #           f_int1 is sufficient for calculating this, because 1.1
   #           checks, that f_int1 = f_int2 is valid for all rows.
   SELECT COUNT(*) INTO @clash_count
   FROM t1 INNER JOIN t0_template USING(f_int1)
   WHERE MOD(f_int1,3) = 0
     AND f_int1 BETWEEN @max_row_div2 AND @max_row;
   if ($debug)
   {
      SELECT @try_count, @clash_count;
   }
}


#-------------------------------------------------------------------------------
# 4    Some operations with single records
# 4.1  Insert one record with a value for f_int1 which is lower than in all
#      existing records.
SELECT MIN(f_int1) - 1 INTO @cur_value FROM t1;
INSERT INTO t1
SET f_int1 = @cur_value , f_int2 = @cur_value,
    f_char1 = CAST(@cur_value AS CHAR), f_char2 = CAST(@cur_value AS CHAR),
    f_charbig = '#SINGLE#';
#      Check of preceding statement via Select
if ($no_debug)
{
--disable_query_log
}
SELECT '# check single-1 success: ' AS "",COUNT(*) = 1 AS "" FROM t1
WHERE f_int1 = @cur_value AND f_int2 = @cur_value
  AND f_char1 = CAST(@cur_value AS CHAR)
  AND f_char2 = CAST(@cur_value AS CHAR) AND f_charbig = '#SINGLE#';
--enable_query_log
#
# 4.2  Insert one record with a value for f_int1 which is higher than in all
#      existing records.
SELECT MAX(f_int1) + 1 INTO @cur_value FROM t1;
INSERT INTO t1
SET f_int1 = @cur_value , f_int2 = @cur_value,
    f_char1 = CAST(@cur_value AS CHAR), f_char2 = CAST(@cur_value AS CHAR),
    f_charbig = '#SINGLE#';
#      Check of preceding statement via Select
if ($no_debug)
{
--disable_query_log
}
SELECT '# check single-2 success: ' AS "",COUNT(*) = 1 AS "" FROM t1
WHERE f_int1 = @cur_value AND f_int2 = @cur_value
  AND f_char1 = CAST(@cur_value AS CHAR)
  AND f_char2 = CAST(@cur_value AS CHAR) AND f_charbig = '#SINGLE#';
--enable_query_log
#
# 4.3  Update one record. The value of f_int1 is altered from the lowest to
#      the highest value of all existing records.
#      If f_int1 is used for the partitioning expression a movement of the
#      record to another partition/subpartition might appear.
SELECT MIN(f_int1) INTO @cur_value1 FROM t1;
SELECT MAX(f_int1) + 1 INTO @cur_value2 FROM t1;
# Bug#18735: Partitions: NDB, UNIQUE INDEX, UPDATE, strange server response
UPDATE t1 SET f_int1 = @cur_value2
WHERE  f_int1 = @cur_value1 AND f_charbig = '#SINGLE#';
#      Check of preceding statement via Select
if ($no_debug)
{
--disable_query_log
}
SELECT '# check single-3 success: ' AS "",COUNT(*) = 1 AS "" FROM t1
WHERE f_int1 = @cur_value2 AND f_charbig = '#SINGLE#';
--enable_query_log
#
# 4.4  Update one record. The value of f_int1 is altered from the highest value
#      to a value lower than in all existing records.
#      If f_int1 is used for the partitioning expression a movement of the
#      record to another partition/subpartition might appear.
#      f_int1 gets the delicate value '-1'.
SET @cur_value1= -1;
SELECT MAX(f_int1) INTO @cur_value2 FROM t1;
# Bug#15968: Partitions: crash when INSERT with f_int1 = -1 into PARTITION BY HASH(f_int1)
# Bug#16385: Partitions: crash when updating a range partitioned NDB table
# Bug#18735: Partitions: NDB, UNIQUE INDEX, UPDATE, strange server response
UPDATE t1 SET f_int1 = @cur_value1
WHERE  f_int1 = @cur_value2 AND f_charbig = '#SINGLE#';
#      Check of preceding statement via Select
if ($no_debug)
{
--disable_query_log
}
SELECT '# check single-4 success: ' AS "",COUNT(*) AS "" FROM t1
WHERE f_int1 = @cur_value1 AND f_charbig = '#SINGLE#';
--enable_query_log
#
# 4.5  Delete the record with the highest value of f_int1.
SELECT MAX(f_int1) INTO @cur_value FROM t1;
DELETE FROM t1 WHERE f_int1 = @cur_value AND f_charbig = '#SINGLE#';
#      Check of preceding statements via Select
if ($no_debug)
{
--disable_query_log
}
SELECT '# check single-5 success: ' AS "",COUNT(*) = 0 AS "" FROM t1
WHERE f_charbig = '#SINGLE#' AND f_int1 = f_int1 = @cur_value;
--enable_query_log
#
# 4.6  Delete the record with f_int1 = -1
DELETE FROM t1 WHERE f_int1 = -1 AND f_charbig = '#SINGLE#';
#      Check of preceding statements via Select
if ($no_debug)
{
--disable_query_log
}
SELECT '# check single-6 success: ' AS "",COUNT(*) = 0 AS "" FROM t1
WHERE f_charbig = '#SINGLE#' AND f_int1 IN (-1,@cur_value);
--enable_query_log
#
# 4.7  Insert one record with such a big value for f_int1, so that in case
#      - f_int1 is used within the partitioning algorithm
#      - we use range partitioning
#      we get error ER_NO_PARTITION_FOR_GIVEN_VALUE
#         "Table has no partition for value ...."
#      or ER_SAME_NAME_PARTITION
--disable_abort_on_error
eval INSERT INTO t1 SET f_int1 = @max_int_4 , f_int2 = @max_int_4, f_charbig = '#$max_int_4##';
--enable_abort_on_error
if ($no_debug)
{
--disable_query_log
}
eval SET @my_errno = $mysql_errno;
if (`SELECT @my_errno NOT IN (0,$ER_SAME_NAME_PARTITION,$ER_NO_PARTITION_FOR_GIVEN_VALUE)`)
{
   --echo #      The last command got an unexpected error response.
   --echo #      Expected/handled SQL codes are 0,$ER_SAME_NAME_PARTITION,$ER_NO_PARTITION_FOR_GIVEN_VALUE
   SELECT '#      SQL code we got was: ' AS "", @my_errno AS "";
   --echo #      Sorry, have to abort.
   exit;
   --echo
}
#      Check of preceding statement via Select, if the INSERT was successful
let $run= `SELECT @my_errno = 0`;
if ($run)
{
# Attention: There are some tests where the column type is changed from
#            INTEGER to MEDIUMINT. MEDIUMINT has a smaller range and the
#            inserted value is automatically adjusted to the maximum value
#            of the data type.
#            that's the reason why we cannot use WHERE <column> = @max_int_4 here.
#
eval SELECT '# check single-7 success: ' AS "",
COUNT(*) = 1 AS "" FROM t1 WHERE f_charbig = '#$max_int_4##';
# Revert this modification
--enable_query_log
eval DELETE FROM t1 WHERE f_charbig = '#$max_int_4##';
}
--enable_query_log


#-------------------------------------------------------------------------------
# 5    Experiments with NULL
# If the result of the partitioning function IS NULL partitioning treats
# this record as if the the result of the partitioning function is
#      MySQL 5.1 <  March 2006 : zero
#      MySQL 5.1 >= March 2006 : LONGLONG_MIN
# Let's INSERT a record where the result of the partitioning function is
# probably (depends on function currently used) zero and look if there are
# any strange effects during the execution of the next statements.
# Bug#17891: Partitions: NDB, crash on select .. where col is null or col = value
# Bug#18659: Partitions: wrong result on WHERE <col. used in part. function> IS NULL
DELETE FROM t1 WHERE f_int1 IS NULL OR f_int1 = 0;
# Attention: Zero should be tested
INSERT t1 SET f_int1 = 0 , f_int2 = 0,
                   f_char1 = CAST(0 AS CHAR), f_char2 = CAST(0 AS CHAR),
                   f_charbig = '#NULL#';
# 5.1  Insert one record with f_int1 IS NULL.
#      f1 "=" NULL is a delicate value which might stress the partitioning
#      mechanism if the result of the expression in the partitioning algorithm
#      becomes NULL.
#      This INSERT will fail, if f_int1 is PRIMARY KEY or UNIQUE INDEX
#      with ER_BAD_NULL_ERROR.
--disable_abort_on_error
INSERT INTO t1
   SET f_int1 = NULL , f_int2 = -@max_row,
       f_char1 = CAST(-@max_row AS CHAR), f_char2 = CAST(-@max_row AS CHAR),
       f_charbig = '#NULL#';
#      Some other NULL experiments if preceding INSERT was successfull
--enable_abort_on_error
if ($no_debug)
{
--disable_query_log
}
eval SET @my_errno = $mysql_errno;
let $run= `SELECT @my_errno = 0`;
if (`SELECT @my_errno NOT IN (0,$ER_BAD_NULL_ERROR)`)
{
   --echo #      The last command got an unexpected error response.
   --echo #      Expected/handled SQL codes are 0,$ER_BAD_NULL_ERROR
   SELECT '#      SQL code we got was: ' AS "", @my_errno AS "";
   --echo #      Sorry, have to abort.
   --echo #      Please check the error name to number mapping in inc/partition.pre.
   exit;
   --echo
}
--enable_query_log
# Give a success message like in the other following tests
--echo # check null success:    1
# The following checks do not make sense if f_int1 cannot be NULL
if ($run)
{
#      Check of preceding statement via Select
if ($no_debug)
{
--disable_query_log
}
# Bug#17432: Partitions: wrong result, SELECT ... where <column> is null
SELECT '# check null-1 success: ' AS "",COUNT(*) = 1 AS "" FROM t1
WHERE f_int1 IS NULL AND f_charbig = '#NULL#';
--enable_query_log
#
# 5.2  Update of f_int1 from NULL to negative value
# Bug#17432: Partitions: wrong result, SELECT ... where <column> is null
UPDATE t1 SET f_int1 = -@max_row
WHERE f_int1 IS NULL AND f_int2 = -@max_row AND f_char1 = CAST(-@max_row AS CHAR)
  AND f_char2 = CAST(-@max_row AS CHAR) AND f_charbig = '#NULL#';
if ($no_debug)
{
--disable_query_log
}
#      Check of preceding statement via Select
SELECT '# check null-2 success: ' AS "",COUNT(*) = 1 AS "" FROM t1
WHERE f_int1 = -@max_row AND f_charbig = '#NULL#';
--enable_query_log
# 5.3  Update of f_int1 from negative value to NULL
UPDATE t1 SET f_int1 = NULL
WHERE f_int1 = -@max_row AND f_int2 = -@max_row AND f_char1 = CAST(-@max_row AS CHAR)
  AND f_char2 = CAST(-@max_row AS CHAR) AND f_charbig = '#NULL#';
if ($no_debug)
{
--disable_query_log
}
#      Check of preceding statement via Select
SELECT '# check null-3 success: ' AS "",COUNT(*) = 1 AS "" FROM t1
WHERE f_int1 IS NULL AND f_charbig = '#NULL#';
--enable_query_log
# 5.4  DELETE of the record with f_int1 IS NULL
DELETE FROM t1
WHERE f_int1 IS NULL AND f_int2 = -@max_row AND f_char1 = CAST(-@max_row AS CHAR)
  AND f_char2 = CAST(-@max_row AS CHAR) AND f_charbig = '#NULL#';
#      Check of preceding statement via Select
if ($no_debug)
{
--disable_query_log
}
SELECT '# check null-4 success: ' AS "",COUNT(*) = 0 AS "" FROM t1
WHERE f_int1 IS NULL;
--enable_query_log
}
# Remove the "0" record
DELETE FROM t1
WHERE f_int1 = 0 AND f_int2 = 0
  AND f_char1 = CAST(0 AS CHAR) AND f_char2 = CAST(0 AS CHAR)
  AND f_charbig = '#NULL#';


#-------------------------------------------------------------------------------
## 6.   UPDATEs of columns used in the partitioning function and the PRIMARY KEY
#       the UNIQUE INDEX without using straight forward UPDATE.
#       INSERT .... ON DUPLICATE KEY UPDATE .... --> update existing record
#       REPLACE --> delete existing record + insert new record
#       Note:
#         - This test is skipped for tables without any PRIMARY KEY or
#           UNIQUE INDEX.
#         - MOD(<column>,n) with n = prime number, n <> 2 is used to cause
#           that many records and most probably more than one PARTITION/
#           SUBPARTITION are affected.
#         - Under certain circumstanditions a movement of one or more records
#           to other PARTITIONs/SUBPARTITIONs might appear.
#         - There are some storage engines, which are unable to revert changes
#           of a failing statement. This has to be taken into account when
#           checking if a DUPLICATE KEY might occur.
#
#       What to test ?
#         UNIQUE columns
#         f_int1         IU f_int1               IU f_int1,f_int2   R
#         f_int2                     IU f_int2   IU f_int1,f_int2   R
#         f_int1,f_int2                          IU f_int1,f_int2   R
#
#         IU column = INSERT .. ON DUPLICATE KEY UPDATE column
#         R         = REPLACE ..
#
# Current state of the data
#    1. f_int1 = f_int2, f_char1 = CAST(f_int1 AS CHAR), f_char2 = f_char1,
#       f_charbig = CONCAT('===',f_char1,'===);
#    2. f_int1 FROM 1 TO @max_row_div4
#                AND @max_row_div2 + @max_row_div4 TO @max_row
#
# Do not apply the following tests to tables without UNIQUE columns.
if ($any_unique)
{
   let $num= 1;
   if ($f_int1_is_unique)
   {
      ## 6.1 f_int1 is UNIQUE, UPDATE f_int1 when DUPLICATE KEY
      # Bug#15236 Partitions: crash, if Insert .. on duplicate key causes update of existing row
      INSERT INTO t1 (f_int1, f_int2, f_char1, f_char2, f_charbig)
      SELECT f_int1, f_int1, '', '', 'was inserted'
      FROM t0_template source_tab
      WHERE MOD(f_int1,3) = 0
        AND f_int1 BETWEEN @max_row_div2 AND @max_row
      ON DUPLICATE KEY
      UPDATE f_int1 = 2 * @max_row + source_tab.f_int1,
             f_charbig = 'was updated';
      --source suite/parts/inc/partition_20.inc
   }

   if ($f_int2_is_unique)
   {
      ## 6.2 f_int2 is UNIQUE, UPDATE f_int2 when DUPLICATE KEY
      # Bug#15236 Partitions: crash, if Insert .. on duplicate key causes update of existing row
      INSERT INTO t1 (f_int1, f_int2, f_char1, f_char2, f_charbig)
      SELECT f_int1, f_int1, '', '', 'was inserted'
      FROM t0_template source_tab
      WHERE MOD(f_int1,3) = 0
        AND f_int1 BETWEEN @max_row_div2 AND @max_row
      ON DUPLICATE KEY
      UPDATE f_int2 = 2 * @max_row + source_tab.f_int1,
             f_charbig = 'was updated';
      --source suite/parts/inc/partition_20.inc
   }

   ## 6.3 f_int1, f_int2 is UNIQUE, UPDATE f_int1, f_int2 when DUPLICATE KEY
   INSERT INTO t1 (f_int1, f_int2, f_char1, f_char2, f_charbig)
   SELECT f_int1, f_int1, '', '', 'was inserted'
   FROM t0_template source_tab
   WHERE MOD(f_int1,3) = 0
     AND f_int1 BETWEEN @max_row_div2 AND @max_row
   ON DUPLICATE KEY
   UPDATE f_int1 = 2 * @max_row + source_tab.f_int1,
          f_int2 = 2 * @max_row + source_tab.f_int1,
          f_charbig = 'was updated';
   --source suite/parts/inc/partition_20.inc

   ## 6.4 REPLACE
   # Bug#16782: Partitions: crash, REPLACE .. on table with PK, DUPLICATE KEY
   REPLACE INTO t1 (f_int1, f_int2, f_char1, f_char2, f_charbig)
   SELECT f_int1, - f_int1, '', '', 'was inserted or replaced'
   FROM t0_template source_tab
   WHERE MOD(f_int1,3) = 0 AND f_int1 BETWEEN @max_row_div2 AND @max_row;
   # DEBUG SELECT * FROM t1 ORDER BY f_int1, f_int2;
   # Check of preceding statement via Select
   if ($no_debug)
   {
      --disable_query_log
   }
   SELECT '# check replace success: ' AS "", COUNT(*) = @try_count AS ""
   FROM t1 WHERE f_charbig = 'was inserted or replaced';
   --enable_query_log
   # Revert the modification
   DELETE FROM t1
   WHERE f_int1 BETWEEN @max_row_div2 AND @max_row_div2 + @max_row_div4;
         # If there is only UNIQUE (f1,f2) we will have pairs f_int1,f_int2
         #    <n>, <n> and <n>, <-n>
         # where MOD(f_int1,3) = 0
         # and f_int1 between @max_row_div2 + @max_row_div4 and @max_row.
         # Delete the <n>, <n> records.
   DELETE FROM t1
   WHERE f_int1 = f_int2 AND MOD(f_int1,3) = 0 AND
         f_int1 BETWEEN @max_row_div2 + @max_row_div4 AND @max_row;
   UPDATE t1 SET f_int2 = f_int1,
             f_char1 = CAST(f_int1 AS CHAR),
             f_char2 = CAST(f_int1 AS CHAR),
             f_charbig = CONCAT('===',CAST(f_int1 AS CHAR),'===')
   WHERE f_charbig = 'was inserted or replaced' AND f_int1 = - f_int2;
   # DEBUG SELECT * FROM t1 ORDER BY f_int1, f_int2;
}


#-------------------------------------------------------------------------------
# 7    Transactions
SET AUTOCOMMIT= 0;
# DEBUG SELECT @max_row_div4 ,  @max_row_div2 + @max_row_div4;
if ($no_debug)
{
   --disable_query_log
}
SELECT COUNT(f_int1) INTO @start_count FROM t1
WHERE f_int1 BETWEEN @max_row_div4 AND @max_row_div2 + @max_row_div4;
--enable_query_log
let $run= `SELECT @start_count <> 0`;
if ($run)
{
   --echo #      Prerequisites for following tests not fullfilled.
   --echo #      The content of the table t1 is unexpected
   --echo #      There must be no rows BETWEEN @max_row_div4 AND @max_row_div2 + @max_row_div4;
   SELECT COUNT(f_int1) FROM t1
   WHERE f_int1 BETWEEN @max_row_div4 AND @max_row_div2 + @max_row_div4;
   --echo #      Sorry, have to abort.
   exit;
}
# Number of records to be inserted
if ($no_debug)
{
   --disable_query_log
}
SELECT @max_row_div2 + @max_row_div4 - @max_row_div4 + 1 INTO @exp_inserted_rows;
--enable_query_log
# 7.1  Successful INSERT + COMMIT
INSERT INTO t1 (f_int1, f_int2, f_char1, f_char2, f_charbig)
SELECT f_int1, f_int1, '', '', 'was inserted'
FROM t0_template source_tab
WHERE f_int1 BETWEEN @max_row_div4 AND @max_row_div2 + @max_row_div4;
# The inserted records must be visible (at least for our current session)
if ($no_debug)
{
   --disable_query_log
}
SELECT '# check transactions-1 success: ' AS "",
       COUNT(*) = @exp_inserted_rows AS ""
FROM t1 WHERE f_int1 BETWEEN @max_row_div4 AND @max_row_div2 + @max_row_div4;
--enable_query_log
# Make the changes persistent for all storage engines
COMMIT WORK;
# The inserted records must be visible (for all open and future sessions)
if ($no_debug)
{
   --disable_query_log
}
SELECT '# check transactions-2 success: ' AS "",
       COUNT(*) = @exp_inserted_rows AS ""
FROM t1 WHERE f_int1 BETWEEN @max_row_div4 AND @max_row_div2 + @max_row_div4;
--enable_query_log
# Let's assume we have a transactional engine + COMMIT is ill.
# A correct working ROLLBACK might revert the INSERT.
ROLLBACK WORK;
if ($no_debug)
{
   --disable_query_log
}
SELECT '# check transactions-3 success: ' AS "",
       COUNT(*) = @exp_inserted_rows AS ""
FROM t1 WHERE f_int1 BETWEEN @max_row_div4 AND @max_row_div2 + @max_row_div4;
--enable_query_log
# Revert the changes
DELETE FROM t1 WHERE f_charbig = 'was inserted';
COMMIT WORK;
ROLLBACK WORK;
if ($no_debug)
{
   --disable_query_log
}
SELECT '# check transactions-4 success: ' AS "",
       COUNT(*) = 0 AS ""
FROM t1 WHERE f_int1 BETWEEN @max_row_div4 AND @max_row_div2 + @max_row_div4;
--enable_query_log
#
# 7.2  Successful INSERT + ROLLBACK
INSERT INTO t1 (f_int1, f_int2, f_char1, f_char2, f_charbig)
SELECT f_int1, f_int1, '', '', 'was inserted'
FROM t0_template source_tab
WHERE f_int1 BETWEEN @max_row_div4 AND @max_row_div2 + @max_row_div4;
if ($no_debug)
{
   --disable_query_log
}
SELECT '# check transactions-5 success: ' AS "",
       COUNT(*) = @exp_inserted_rows AS ""
FROM t1 WHERE f_int1 BETWEEN @max_row_div4 AND @max_row_div2 + @max_row_div4;
--enable_query_log
ROLLBACK WORK;
if ($no_debug)
{
   --disable_query_log
}
SELECT COUNT(*) INTO @my_count
FROM t1 WHERE f_int1 BETWEEN @max_row_div4 AND @max_row_div2 + @max_row_div4;
SELECT '# check transactions-6 success: ' AS "",
       @my_count IN (0,@exp_inserted_rows) AS "";
let $run= `SELECT @my_count = 0`;
if ($run)
{
      --echo # INFO: Storage engine used for t1 seems to be transactional.
}
let $run= `SELECT @my_count = @exp_inserted_rows`;
if ($run)
{
      --echo # INFO: Storage engine used for t1 seems to be not transactional.
}
--enable_query_log
# Let's assume we have a transactional engine + ROLLBACK is ill.
# A correct working COMMIT might make the inserted records again visible.
COMMIT;
if ($no_debug)
{
   --disable_query_log
}
SELECT '# check transactions-7 success: ' AS "",
       COUNT(*) IN (0,@exp_inserted_rows) AS ""
FROM t1 WHERE f_int1 BETWEEN @max_row_div4 AND @max_row_div2 + @max_row_div4;
--enable_query_log
# Revert the changes
DELETE FROM t1 WHERE f_charbig = 'was inserted';
COMMIT WORK;
#
# 7.3  Failing INSERT (in mid of statement processing) + COMMIT
SET @@session.sql_mode = 'traditional';
# Number of records where a INSERT has to be tried
SELECT @max_row_div2 + @max_row_div4 - @max_row_div4 + 1 INTO @exp_inserted_rows;
#
--disable_abort_on_error
INSERT INTO t1 (f_int1, f_int2, f_char1, f_char2, f_charbig)
SELECT IF(f_int1 = @max_row_div2,f_int1 / 0,f_int1),f_int1,
       '', '', 'was inserted' FROM t0_template
WHERE f_int1 BETWEEN @max_row_div4 AND @max_row_div2 + @max_row_div4;
--enable_abort_on_error
COMMIT;
# How many new records should be now visible ?
# 1. storage engine unable to revert changes made by the failing statement
#         @max_row_div2 - 1 - @max_row_div4 + 1
# 2. storage engine able to revert changes made by the failing statement
#         0
if ($no_debug)
{
   --disable_query_log
}
SELECT COUNT(*) INTO @my_count
FROM t1 WHERE f_int1 BETWEEN @max_row_div4 AND @max_row_div2 + @max_row_div4;
SELECT '# check transactions-8 success: ' AS "",
       @my_count IN (@max_row_div2 - 1 - @max_row_div4 + 1,0) AS "";
let $run= `SELECT @my_count = @max_row_div2 - 1 - @max_row_div4 + 1`;
if ($run)
{
      --echo # INFO: Storage engine used for t1 seems to be unable to revert
      --echo #       changes made by the failing statement.
}
let $run= `SELECT @my_count = 0`;
if ($run)
{
      --echo # INFO: Storage engine used for t1 seems to be able to revert
      --echo #       changes made by the failing statement.
}
--enable_query_log
SET @@session.sql_mode = '';
SET AUTOCOMMIT= 1;
# Revert the changes
DELETE FROM t1 WHERE f_charbig = 'was inserted';
COMMIT WORK;

if ($debug)
{
   SELECT * FROM t1 ORDER BY f_int1;
}


#-------------------------------------------------------------------------------
# 8    Some special cases
# 8.1  Dramatic increase of the record/partition/subpartition/table sizes
UPDATE t1 SET f_charbig = REPEAT('b', 1000);
#      partial check of preceding statement via Select
if ($no_debug)
{
--disable_query_log
}
eval SELECT '# check special-1 success: ' AS "",1 AS "" FROM t1
WHERE f_int1 = 1 AND f_charbig = REPEAT('b', 1000);
--enable_query_log
#
# 8.2  Dramatic decrease of the record/partition/subpartition/table sizes
UPDATE t1 SET f_charbig = '';
#      partial check of preceding statement via Select
if ($no_debug)
{
--disable_query_log
}
eval SELECT '# check special-2 success: ' AS "",1 AS "" FROM t1
WHERE f_int1 = 1 AND f_charbig = '';
--enable_query_log
# Revert the changes
UPDATE t1 SET f_charbig = CONCAT('===',CAST(f_int1 AS CHAR),'===');

if ($debug)
{
   SELECT * FROM t1 ORDER BY f_int1;
}


#-------------------------------------------------------------------------------
# 9    TRIGGERs
let $num= 1;
# 9.1  BEFORE/AFTER INSERT/UPDATE/DELETE TRIGGER on table t0_aux causes that
#      column values used in partitioning function of t1 are changed.
let $tab_has_trigg= t0_aux;
let $tab_in_trigg=  t1;

#             Insert three records, which will be updated by the trigger
# Bug#18735: Partitions: NDB, UNIQUE INDEX, UPDATE, strange server response
eval INSERT INTO $tab_in_trigg(f_int1,f_int2,f_char1,f_char2,f_charbig)
SELECT f_int1,f_int2,f_char1,f_char2,'just inserted' FROM t0_template
WHERE f_int1 BETWEEN @max_row_div2 - 1 AND @max_row_div2 + 1;

let $statement= INSERT INTO t0_aux(f_int1,f_int2,f_char1,f_char2,f_charbig)
SELECT f_int1,f_int2,f_char1,f_char2,NULL FROM t0_template
WHERE f_int1 BETWEEN @max_row_div2 - 1 AND @max_row_div2 + 1;
let $event= BEFORE INSERT;
--source suite/parts/inc/partition_trigg1.inc
let $event= AFTER INSERT;
--source suite/parts/inc/partition_trigg1.inc

let $statement= UPDATE t0_aux SET f_int1 =  - f_int1, f_int2 = - f_int2
WHERE f_int1 IN (- (@max_row_div2 - 1),- @max_row_div2,- (@max_row_div2 + 1));
let $event= BEFORE UPDATE;
--source suite/parts/inc/partition_trigg1.inc
let $event= AFTER UPDATE;
--source suite/parts/inc/partition_trigg1.inc

let $statement= DELETE FROM t0_aux
WHERE f_int1 IN (- (@max_row_div2 - 1),- @max_row_div2,- (@max_row_div2 + 1));
let $event= BEFORE DELETE;
--source suite/parts/inc/partition_trigg1.inc
let $event= AFTER DELETE;
--source suite/parts/inc/partition_trigg1.inc

# Cleanup
eval DELETE FROM $tab_in_trigg
WHERE f_int1 BETWEEN @max_row_div2 - 1 AND @max_row_div2 + 1;

# Two currently (February 2006) impossible operations.
# 1442: 'Can't update table 't1' in stored function/trigger because it is
#        already used by statement which invoked this stored function/trigger.'
# 1362: 'Updating of OLD row is not allowed in trigger'

if ($debug)
{
   SELECT * FROM t1 ORDER BY f_int1;
}

if ($more_trigger_tests)
{
# 9.2  BEFORE/AFTER INSERT/UPDATE/DELETE TRIGGER on partitioned table t1 causes
#      that column values in not partitioned table t0_aux are changed.
let $tab_has_trigg= t1;
let $tab_in_trigg=  t0_aux;

#             Insert three records, which will be updated by the trigger
eval INSERT INTO $tab_in_trigg(f_int1,f_int2,f_char1,f_char2,f_charbig)
SELECT f_int1,f_int2,f_char1,f_char2,'just inserted' FROM t0_template
WHERE f_int1 BETWEEN @max_row_div2 - 1 AND @max_row_div2 + 1;

let $statement= INSERT INTO t1(f_int1,f_int2,f_char1,f_char2,f_charbig)
SELECT f_int1,f_int2,f_char1,f_char2,NULL FROM t0_template
WHERE f_int1 BETWEEN @max_row_div2 - 1 AND @max_row_div2 + 1;
let $event= BEFORE INSERT;
--source suite/parts/inc/partition_trigg1.inc
let $event= AFTER INSERT;
--source suite/parts/inc/partition_trigg1.inc

let $statement= UPDATE t1 SET f_int1 =  - f_int1, f_int2 = - f_int2
WHERE f_int1 IN (- (@max_row_div2 - 1),- @max_row_div2,- (@max_row_div2 + 1));
let $event= BEFORE UPDATE;
--source suite/parts/inc/partition_trigg1.inc
let $event= AFTER UPDATE;
--source suite/parts/inc/partition_trigg1.inc

let $statement= DELETE FROM t1
WHERE f_int1 IN (- (@max_row_div2 - 1),- @max_row_div2,- (@max_row_div2 + 1));
let $event= BEFORE DELETE;
--source suite/parts/inc/partition_trigg1.inc
let $event= AFTER DELETE;
--source suite/parts/inc/partition_trigg1.inc
eval DELETE FROM $tab_in_trigg
WHERE f_int1 BETWEEN @max_row_div2 - 1 AND @max_row_div2 + 1;
}

if ($debug)
{
   SELECT * FROM t1 ORDER BY f_int1;
}

# 9.3   BEFORE/AFTER UPDATE TRIGGER on partitioned table causes that the value
#       of columns in partitioning function is recalculated
if ($more_trigger_tests)
{
# 9.3.1 The UPDATE itself changes a column which is not used in the partitioning
#       function.
#       "old" values are used as source within the trigger.
let $statement= UPDATE t1
SET f_charbig = '####updated per update statement itself####';
let $source= old;
let $event= BEFORE UPDATE;
--source suite/parts/inc/partition_trigg2.inc
# FIXME when AFTER TRIGGER can be used
# Currently (2006-02-23) a AFTER TRIGGER is not allowed to modify a row, which
# was just modified: 1362: Updating of NEW row is not allowed in after trigger
}

# 9.3.2   The UPDATE itself changes a column which is used in the partitioning
#         function.
let $statement= UPDATE t1
SET f_int1 = f_int1 + @max_row, f_int2 = f_int2 - @max_row,
f_charbig = '####updated per update statement itself####';

# 9.3.2.1 "old" values are used as source within the trigger.
let $source= old;
let $event= BEFORE UPDATE;
--source suite/parts/inc/partition_trigg2.inc
# FIXME when AFTER TRIGGER can be used
# Currently (2006-02-23) a AFTER TRIGGER is not allowed to modify a row, which
# was just modified: 1362: Updating of NEW row is not allowed in after trigger
# 9.3.2.2 "new" values are used as source within the trigger.
let $source= new;
let $event= BEFORE UPDATE;
--source suite/parts/inc/partition_trigg2.inc
# FIXME when AFTER TRIGGER can be used

if ($debug)
{
   SELECT * FROM t1 ORDER BY f_int1;
}

# 9.4    BEFORE/AFTER INSERT TRIGGER on partitioned table causes that the value of
#        columns in partitioning function is recalculated.
# 9.4.1  INSERT assigns values to the recalculate columns
let $statement= INSERT INTO t1 (f_int1, f_int2, f_char1, f_char2, f_charbig)
SELECT f_int1, f_int1, CAST(f_int1 AS CHAR),
       CAST(f_int1 AS CHAR), 'just inserted' FROM t0_template
WHERE f_int1 BETWEEN @max_row_div2 - 1 AND @max_row_div2 + 1
ORDER BY f_int1;
let $event= BEFORE INSERT;
let $source= new;
--source suite/parts/inc/partition_trigg3.inc
# FIXME when AFTER TRIGGER can be used

# 9.4.2  INSERT assigns no values to the recalculate columns
let $statement= INSERT INTO t1 (f_char1, f_char2, f_charbig)
SELECT CAST(f_int1 AS CHAR),
       CAST(f_int1 AS CHAR), 'just inserted' FROM t0_template
WHERE f_int1 BETWEEN @max_row_div2 - 1 AND @max_row_div2 + 1
ORDER BY f_int1;
let $event= BEFORE INSERT;
let $source= new;
--source suite/parts/inc/partition_trigg3.inc
# FIXME when AFTER TRIGGER can be used

if ($debug)
{
   SELECT * FROM t1 ORDER BY f_int1;
}


#-------------------------------------------------------------------------------
# 10   ANALYZE/CHECK/CHECKSUM
ANALYZE  TABLE t1;
CHECK    TABLE t1 EXTENDED;
# Checksum depends on @max_row so we have to unify the value
--replace_column 2 <some_value>
CHECKSUM TABLE t1 EXTENDED;


#-------------------------------------------------------------------------------
# 11   Some special statements, which may lead to a rebuild of the trees
#      depending on the storage engine and some particular conditions
# 11.1 OPTIMIZE TABLE
#      Manual about OPTIMIZE <InnoDB table>:
#      ... , it is mapped to ALTER TABLE, which rebuilds the table.
#      Rebuilding updates index statistics and frees unused space in the
#      clustered index.
#      FIXME What will happen with NDB ?
OPTIMIZE TABLE t1;
--source suite/parts/inc/partition_layout_check2.inc
# 10.2 REPAIR TABLE
REPAIR   TABLE t1 EXTENDED;
--source suite/parts/inc/partition_layout_check2.inc
#
# 11.3 Truncate
# Manual about TRUNCATE on tables ( != InnoDB table with FOREIGN KEY ):
# Truncate operations drop and re-create the table ....
TRUNCATE t1;
#      Check of preceding statement via Select
if ($no_debug)
{
--disable_query_log
}
SELECT '# check TRUNCATE success: ' AS "",COUNT(*) = 0 AS "" FROM t1;
--enable_query_log
--source suite/parts/inc/partition_layout_check2.inc
--echo # End usability test (inc/partition_check.inc)

