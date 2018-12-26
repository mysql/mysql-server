# Example how to use this auxiliary script
#-----------------------------------------
#
### The table/tables used in $part must have the right content.
### $title_prefix is used for the generation of titles
#
# let $title_prefix= 4.3;
### $check_num is used for the generation of titles and gets incremented after
### every call of the current script.
# let $check_num= 1;
### $diff_column_list is used for the generation of error information and valid for
### every sub test.
# let $diff_column_list=
# t2.COUNT_READ - t1.COUNT_READ AS D_COUNT_READ,
# t2.COUNT_READ                 AS S2_COUNT_READ,
# t1.COUNT_READ                 AS S1_COUNT_READ,
# t2.SUM_NUMBER_OF_BYTES_READ - t1.SUM_NUMBER_OF_BYTES_READ AS D_SUM_NUMBER_OF_BYTES_READ,
# t2.SUM_NUMBER_OF_BYTES_READ                               AS S2_SUM_NUMBER_OF_BYTES_READ,
# t1.SUM_NUMBER_OF_BYTES_READ                               AS S1_SUM_NUMBER_OF_BYTES_READ,
# t2.COUNT_WRITE - t1.COUNT_WRITE AS D_COUNT_WRITE,
# t2.COUNT_WRITE                  AS S2_COUNT_WRITE,
# t1.COUNT_WRITE                  AS S1_COUNT_WRITE,
# t2.SUM_NUMBER_OF_BYTES_WRITE - t1.SUM_NUMBER_OF_BYTES_WRITE AS D_UM_NUMBER_OF_BYTES_WRITE,
# t2.SUM_NUMBER_OF_BYTES_WRITE                                AS S2_SUM_NUMBER_OF_BYTES_WRITE,
# t1.SUM_NUMBER_OF_BYTES_WRITE                                AS S1_SUM_NUMBER_OF_BYTES_WRITE,
# t2.COUNT_MISC - t1.COUNT_MISC AS D_COUNT_MISC,
# t2.COUNT_MISC                 AS S2_COUNT_MISC,
# t1.COUNT_MISC                 AS S1_COUNT_MISC;
### $part is used for the generation of "check" statements + error information
### and valid for every sub test.
# let $part=
# FROM mysqltest.socket_summary_by_instance_detail t1
# JOIN mysqltest.socket_summary_by_instance_detail t2
# USING (EVENT_NAME, OBJECT_INSTANCE_BEGIN, run)
# WHERE OBJECT_INSTANCE_BEGIN <> @default_object_instance_begin
#   AND EVENT_NAME LIKE ('%client_connection')
#   AND run = 1;
#
# --echo # $title_prefix Check the differences caused by SQL statement
#
# let stmt1= SELECT col2 FROM does_not_exist;
# let stmt2= SELECT col2 FROM does_not_exist WHERE col1 = 0;
### $msg is used to generate some explanation of what we compare.
# let $msg=
# # One statement is longer than the other.
# # Both statements fail with the same error message (table does not exist);
# let $my_rules=
# t2.COUNT_READ                - t1.COUNT_READ                = 0  AND
# t2.SUM_NUMBER_OF_BYTES_READ  - t1.SUM_NUMBER_OF_BYTES_READ  = LENGTH('$stmt2') - LENGTH('$stmt1') AND
# t2.COUNT_WRITE               - t1.COUNT_WRITE               = 0  AND
# t2.SUM_NUMBER_OF_BYTES_WRITE - t1.SUM_NUMBER_OF_BYTES_WRITE = 0  AND
# t2.COUNT_MISC                - t1.COUNT_MISC                = 0;
# --source ../include/socket_check1.inc
#
# let stmt1= ....
# let stmt2= ....
# let $my_rules= ...
# --source ../include/socket_check1.inc
#
# ...
#

--echo # $title_prefix.$check_num Compare impact of statements
--echo #       $stmt2
--echo #       $stmt1
--echo $msg

# Enable this when extending the checks for SQL statements.
if(0)
{
   if (`SELECT CONCAT("$stmt1","$stmt2","$my_rules") LIKE '%_not_set%'`)
   {
      --echo # INTERNAL ERROR:
      --echo # At least one of the variables has no value (is like '%_not_set')
      --echo # stmt1 : $stmt1
      --echo # stmt2 : $stmt2
      --echo # my_rules : $my_rules
      --echo # Sorry, have to abort
      exit;
   }
}

if(`SELECT NOT ( $my_rules )
    $part
    AND t2.statement = '$stmt2' AND t1.statement = '$stmt1'`)
{
   --enable_query_log
   --enable_result_log
   --echo # The compared statistics looks suspicious
   --echo # We expect
   --echo $my_rules
   --echo
   --horizontal_results
   eval
   SELECT $my_rules AS Expect_1
   $part
   AND t2.statement = '$stmt2' AND t1.statement = '$stmt1';
   --echo
   --vertical_results
   eval
   SELECT $diff_column_list
   $part
   AND t1.statement = '$stmt1' AND t2.statement = '$stmt2';
   --echo
   --horizontal_results

   eval
   SELECT
      LPAD(COUNT_READ, 8, ' ') AS CNT_READ,
      LPAD(SUM_NUMBER_OF_BYTES_READ,  10,' ') AS BYTES_READ,
      LPAD(COUNT_WRITE,9, ' ') AS CNT_WRITE,
      LPAD(SUM_NUMBER_OF_BYTES_WRITE, 11,' ') AS BYTES_WRITE,
      LPAD(COUNT_MISC, 8, ' ') AS CNT_MISC, statement
   FROM mysqltest.socket_summary_by_instance_detail
   WHERE OBJECT_INSTANCE_BEGIN <> @default_object_instance_begin
     AND EVENT_NAME LIKE ('%client_connection')
     AND run = 1
     AND statement IN('$stmt2','$stmt1');
   let $print_details= 1;
}
# Initialize all variables which depend on the statements to be checked.
# This prevents that we run with wrong data.
let $stmt1=    stmt1_not_set;
let $stmt2=    stmt2_not_set;
let $my_rules= my_rules_not_set;
let $msg=      msg_not_set;
inc $check_num;


