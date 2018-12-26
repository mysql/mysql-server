# include/socket_summary_check.inc
#
# Auxiliary routine to be sourced by socket_summary_by_instance_func.test
# or other routines sourced within this script.
#
# Purpose
#    Various checks for the content of the table socket_summary_by_instance.
#
# It is intentional that we do not try to cram as much checks as possible into
# one single SQL statement.
# Reasons:
# - We check performance_schema here and NOT something like optimizer.
# - This test should work even if some other feature has become buggy.
# - In case some check gives unexpected results than we print the
#   relevant content of the table and the values which we expect.
#   In case of all checks in one statement such a printout would be too huge.
#
# IMPORTANT:
#   The maximum number of rows which the table socket_summary_by_instance
#   can keep is limited via the system variables max_socket_classes and
#   max_socket_instances.  We are running with the default values here.
#   They are sufficient high so that these limits cannot harm the current test.
#   FIXME: Check at the beginning of the test that the limits are sufficient
#          for the current test.
#

--disable_query_log
# Insert the current state into mysqltest.my_socket_summary_by_instance.
eval $insert_after;
--enable_query_log
--enable_result_log


# 1. The content of socket_summary_by_instance must be consistent to the
#    content of socket_instances
#=======================================================================
let $part1=
FROM performance_schema.socket_summary_by_instance
WHERE (EVENT_NAME,OBJECT_INSTANCE_BEGIN)
      NOT IN (SELECT EVENT_NAME,OBJECT_INSTANCE_BEGIN
FROM performance_schema.socket_instances);
if(`SELECT COUNT(*) $part1`)
{
   --echo # There is an inconsistency between the content of the tables
   --echo # socket_instances and socket_summary_by_instance
   --echo #
   eval
   SELECT 'not in socket_instances' AS state, EVENT_NAME, OBJECT_INSTANCE_BEGIN
   $part1;
}

--vertical_results


# 2. The computation of statistics must be roughly correct.
#
#    If we run this check sufficient frequent than AVG_TIMER_* can be removed from other checks.
#===============================================================================================
let $my_lo= 0.98;
let $my_hi= 1.02;

let $my_rules=
COUNT_STAR  * AVG_TIMER_WAIT  BETWEEN SUM_TIMER_WAIT  * $my_lo AND SUM_TIMER_WAIT  * $my_hi AND
COUNT_READ  * AVG_TIMER_READ  BETWEEN SUM_TIMER_READ  * $my_lo AND SUM_TIMER_READ  * $my_hi AND
COUNT_WRITE * AVG_TIMER_WRITE BETWEEN SUM_TIMER_WRITE * $my_lo AND SUM_TIMER_WRITE * $my_hi AND
COUNT_MISC  * AVG_TIMER_MISC  BETWEEN SUM_TIMER_MISC  * $my_lo AND SUM_TIMER_MISC  * $my_hi;

let $part=
SUM_TIMER_WAIT  * $my_lo, COUNT_STAR  * AVG_TIMER_WAIT,  SUM_TIMER_WAIT  * $my_hi,
   COUNT_STAR,  SUM_TIMER_WAIT,  AVG_TIMER_WAIT,
SUM_TIMER_READ  * $my_lo, COUNT_READ  * AVG_TIMER_READ,  SUM_TIMER_READ  * $my_hi,
   COUNT_READ,  SUM_TIMER_READ,  AVG_TIMER_READ,
SUM_TIMER_WRITE * $my_lo, COUNT_WRITE * AVG_TIMER_WRITE, SUM_TIMER_WRITE * $my_hi,
   COUNT_WRITE, SUM_TIMER_WRITE, AVG_TIMER_WRITE,
SUM_TIMER_MISC  * $my_lo, COUNT_MISC  * AVG_TIMER_MISC,  SUM_TIMER_MISC  * $my_hi,
   COUNT_MISC,  SUM_TIMER_MISC,  AVG_TIMER_MISC;

if(`SELECT SUM($my_rules) <> COUNT(*) FROM mysqltest.my_socket_summary_by_instance
    WHERE pk = 'After'`)
{
   --echo # The statistics looks suspicious.
   --echo # We expect
   --echo #    $my_rules
   eval
   SELECT EVENT_NAME, OBJECT_INSTANCE_BEGIN,
      $part
   FROM mysqltest.my_socket_summary_by_instance
   WHERE pk = 'After' AND NOT ($my_rules)
   ORDER BY EVENT_NAME, OBJECT_INSTANCE_BEGIN;
   let $print_details= 1;
}


# 3. Check the relation between AVG_*, MIN_TIMER_* and MAX_TIMER_*
#
#    If we run this check sufficient frequent than only the following
#    additional checks are required:
#    a) If (SUM_TIMER_*(new) - SUM_TIMER_*(old) < MIN_TIMER_*(old))
#       than MIN_TIMER_*(new) = SUM_TIMER_*(new) - SUM_TIMER_*(old).
#    b) If (SUM_TIMER_*(new) - SUM_TIMER_*(old) > MAX_TIMER_*(old))
#       than MAX_TIMER_*(new) = SUM_TIMER_*(new) - SUM_TIMER_*(old).
#    in order to remove MIN_TIMER_* and MAX_TIMER_* from other checks
#    Between the states "new" and "old" must be exact one statement.
#-----------------------------------------------------------------------------------------------
let $my_rules=
AVG_TIMER_WAIT  >= MIN_TIMER_WAIT  AND MAX_TIMER_WAIT  >= AVG_TIMER_WAIT  AND
AVG_TIMER_READ  >= MIN_TIMER_READ  AND MAX_TIMER_READ  >= AVG_TIMER_READ  AND
AVG_TIMER_WRITE >= MIN_TIMER_WRITE AND MAX_TIMER_WRITE >= AVG_TIMER_WRITE AND
AVG_TIMER_MISC  >= MIN_TIMER_MISC  AND MAX_TIMER_MISC  >= AVG_TIMER_MISC;

let $part=
MIN_TIMER_WAIT,  AVG_TIMER_WAIT,  MAX_TIMER_WAIT,
MIN_TIMER_READ,  AVG_TIMER_READ,  MAX_TIMER_READ,
MIN_TIMER_WRITE, AVG_TIMER_WRITE, MAX_TIMER_WRITE,
MIN_TIMER_MISC,  AVG_TIMER_MISC,  MAX_TIMER_MISC;

if(`SELECT SUM($my_rules) <> COUNT(*) FROM mysqltest.my_socket_summary_by_instance
    WHERE pk = 'After'`)
{
   --echo # The statistics looks suspicious.
   --echo # We expect
   --echo #    $my_rules
   eval
   SELECT EVENT_NAME, OBJECT_INSTANCE_BEGIN,
      $part
   FROM mysqltest.my_socket_summary_by_instance
   WHERE pk = 'After' AND NOT ($my_rules)
   ORDER BY EVENT_NAME, OBJECT_INSTANCE_BEGIN;
   let $print_details= 1;
}


# 4. Check the aggregate columns COUNT_STAR and SUM_TIMER_WAIT
#
#    The specification says:
#    The columns COUNT_STAR, SUM/MIN/AVG/MAX TIMER_WAIT aggregates all operations.
#
#    If we run this check sufficient frequent than COUNT_STAR and SUM_TIMER_WAIT
#    can be removed from other checks.
#---------------------------------------------------------------------------------
let $my_rules=
COUNT_STAR     = COUNT_READ     + COUNT_WRITE     + COUNT_MISC      AND
SUM_TIMER_WAIT = SUM_TIMER_READ + SUM_TIMER_WRITE + SUM_TIMER_MISC;

let $part=
COUNT_STAR, COUNT_READ + COUNT_WRITE + COUNT_MISC, COUNT_READ, COUNT_WRITE, COUNT_MISC,
SUM_TIMER_WAIT, SUM_TIMER_READ + SUM_TIMER_WRITE + SUM_TIMER_MISC, SUM_TIMER_READ,
SUM_TIMER_WRITE, SUM_TIMER_MISC;

if(`SELECT SUM($my_rules) <> COUNT(*) FROM mysqltest.my_socket_summary_by_instance
    WHERE pk = 'After'`)
{
   --echo # The statistics looks suspicious.
   --echo # We expect
   --echo #    $my_rules
   --echo #
   eval
   SELECT EVENT_NAME, OBJECT_INSTANCE_BEGIN,
      $part
   FROM mysqltest.my_socket_summary_by_instance
   WHERE pk = 'After'
   ORDER BY EVENT_NAME, OBJECT_INSTANCE_BEGIN;
   let $print_details= 1;
}


# 5. Check the aggregate column MIN_TIMER_WAIT
#
#    If we run this check sufficient frequent than MIN_TIMER_WAIT
#    can be removed from other checks.
#---------------------------------------------------------------------------------
let $my_rules=
MIN_TIMER_WAIT >= mysqltest.min_of_triple(MIN_TIMER_READ,MIN_TIMER_WRITE,MIN_TIMER_MISC);

let $part=
MIN_TIMER_WAIT,
mysqltest.min_of_triple(MIN_TIMER_READ, MIN_TIMER_WRITE, MIN_TIMER_MISC) AS "Min_of_Triple",
MIN_TIMER_READ, MIN_TIMER_WRITE, MIN_TIMER_MISC;

if(`SELECT SUM($my_rules) <> COUNT(*) FROM mysqltest.my_socket_summary_by_instance
    WHERE pk = 'After'`)
{
   --echo # The statistics looks suspicious.
   --echo # We expect
   --echo #    $my_rules
   --echo #
   eval
   SELECT EVENT_NAME, OBJECT_INSTANCE_BEGIN,
      $part
   FROM mysqltest.my_socket_summary_by_instance
   WHERE pk = 'After'
   ORDER BY EVENT_NAME, OBJECT_INSTANCE_BEGIN;
   let $print_details= 1;
}


# 6. Check the aggregate column MAX_TIMER_WAIT
#
#    If we run this check sufficient frequent than MAX_TIMER_WAIT
#    can be removed from other checks.
#---------------------------------------------------------------------------------
let $my_rules=
MAX_TIMER_WAIT >= mysqltest.max_of_triple(MAX_TIMER_READ,MAX_TIMER_WRITE,MAX_TIMER_MISC);

let $part=
MAX_TIMER_WAIT,
mysqltest.max_of_triple(MAX_TIMER_READ, MAX_TIMER_WRITE, MAX_TIMER_MISC) AS "Max_of_Triple",
MAX_TIMER_READ, MAX_TIMER_WRITE, MAX_TIMER_MISC;

if(`SELECT SUM($my_rules) <> COUNT(*) FROM mysqltest.my_socket_summary_by_instance
    WHERE pk = 'After'`)
{
   --echo # The statistics looks suspicious.
   --echo # We expect
   --echo #    $my_rules
   --echo #
   eval
   SELECT EVENT_NAME, OBJECT_INSTANCE_BEGIN,
      $part
   FROM mysqltest.my_socket_summary_by_instance
   WHERE pk = 'After'
   ORDER BY EVENT_NAME, OBJECT_INSTANCE_BEGIN;
   let $print_details= 1;
}

--horizontal_results

