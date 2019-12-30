######## include/ddl1.inc ######
#
# Purpose of include/ddl1.inc - include/ddl8.inc:
#
#    Stress storage engines with rapid CREATE/DROP TABLE/INDEX
#    and following SELECT/INSERT/SHOW etc.
#
# The variables
#     $loop_size -- number of rounds till we look at the clock again
#     $runtime   -- rough intended runtime per subtest variant
#                Real runtime without server restarts and comparison is:
#                   -   >= $runtime
#                   -   > runtime needed for $loop_size execution loops
#     $engine_type -- storage engine to be used in CREATE TABLE
# must be set within the routine sourcing this script.
#
# Other stuff which must already exist:
# - connection con2
# - stmt_start and stmt_break prepared by the default connection
#
# Attention:
#    The test does suppress the writing of most statements, server error
#    messages and result sets.
#    This is needed because their number is usual not deterministic.
#    The test is partially self checking. That means is prints some
#    helpful hints into the protocol and aborts if something is wrong.
#
# Creation of this test:
# 2007-07-04 mleich
#
############################################################################
#
# Some details:
#
# 1. Base question of the test:
#       There was just a create or drop of some object (TABLE/INDEX).
#
#       Could it happen that the next statement referring to this
#       object gets a somehow wrong server response (result set,
#       error message, warning) because the creation or removal of
#       the object is in an incomplete state?
#
#       Thinkable reasons for incomplete state of creation or removal:
#       The server performs the creation or removal
#       - all time incomplete.
#         Example:
#            Bug#28309 First insert violates unique constraint
#                      - was "memory" table empty ?
#       - asynchronous
#         In that case the next statement has probably to wait till
#         completion.
#
# 2. Why do we use in some scripts "--error 0,<expected error>" followed
#    a check of $mysql_errno?
#
#    System reactions when running with "--error 0,<expected error>":
#    - RC=0 --> no error message
#    - RC=<expected error> --> no error message
#    - RC not in (0,<expected error>) --> error message + abort of script
#                                         execution
#
#    Requirements and tricky solution for statements which are expected
#    to fail:
#    1. RC=<expected error>
#       - no abort of script execution
#         --> add "--error <expected error>"
#       - no error message into the protocol, because the number of
#         executions is NOT deterministic
#         --> use "--error 0,<expected error>"
#    2. RC=0 = failure
#       - abort of script execution
#       "--error 0,<expected error>" prevents the automatic abort of
#       execution. Therefore we do not need to code the abort.
#       --> Check $mysql_errno and do an explicit abort if $mysql_errno = 0.
#    3. RC not in (0,<expected error>)
#       - abort of script execution
#       "--error 0,<expected error>" causes an automatic abort.
#
# 3. We do not check the correctness of the SHOW CREATE TABLE output
#    in detail. This must be done within other tests.
#    We only check here that
#    - same CREATE TABLE/INDEX statements lead to the same
#    - different CREATE TABLE/INDEX statements lead to different
#    SHOW CREATE TABLE output
#    (Applies to ddl4.inc. and ddl8.inc.)
#
# 4. It could be assumed that running this test with
#    - PS-PROTOCOL
#      There are already subtests using prepared statements contained.
#    - SP/CURSOR/VIEW-PROTOCOL
#      These protocol variants transform SELECTs to hopefully much
#      stressing statement sequencies using SP/CURSOR/VIEW.
#      The SELECTs within include/ddl*.inc are very simple.
#    does not increase the coverage.
#    Therefore we skip runs with these protocols.
#
# 5. The test consumes significant runtime when running on a non RAM
#    based filesystem (run without "--mem").
#    Therefore we adjust $runtime and $loop_size depending on "--big-test"
#    option.
#    $runtime and $loop_size do not influence the expected results.
#    Rough runtime in seconds reported by mysql-test-run.pl:
#        (engine_type = MEMORY)
#    option set    -> $runtime $loop_size  real runtime in seconds
#                         1        20             68
#    --mem                1        20             32
#          --big-test     5       100            200
#    --mem --big-test     5       100            400
#    I assume that runs with slow filesystems are as much valuable
#    as runs with extreme fast filesystems.
#
# 6. Hints for analysis of test failures:
#    1. Look into the protocol and check in which ddl*.inc
#       script the difference to the expected result occurred.
#    2. Comment the sourcing of all other ddl*.inc scripts
#       out.
#    3. Edit the ddl*.inc script where the error occurred and
#       remove all
#       - "--disable_query_log", "--disable_result_log"
#       - successful passed subtests.
#    4. Alternative:
#       Have a look into VARDIR/master-data/mysql/general_log.CSV
#       and construct a new testcase from that.
#    5. If the problem is not deterministic, please try the following
#       - increase $runtime (important), $loop_size (most probably
#         less important) within the "t/ddl_<engine>.test" and
#         maybe the "--testcase-timeout" assigned to mysqltest-run.pl
#       - vary the I/O performance of the testing machine by using
#         a RAM or disk based filesystem for VARDIR
#


#----------------------------------------------------------------------
# Settings for Subtest 1 variants
# Scenario: CREATE with UNIQUE KEY/INSERT/DROP TABLE like in Bug#28309
let $create_table= CREATE TABLE t1 (f1 BIGINT,f2 BIGINT,UNIQUE(f1),UNIQUE(f2))
ENGINE = $engine_type;
let $insert_into=  INSERT INTO t1 VALUES (1,1), (2,2), (3,3);
let $drop_table=   DROP TABLE t1;
#----------------------------------------------------------------------

#
--echo # Subtest 1A (one connection, no PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_table
--echo #    default:    $insert_into
--echo #    default:    $drop_table
--disable_query_log
connection default;
let $run= 1;
# Determine the current time.
EXECUTE stmt_start;
# Run execution loops till the planned runtime is reached
while ($run)
{
   let $loop_run= $loop_size;
   while ($loop_run)
   {
      eval $create_table;
      eval $insert_into;
      eval $drop_table;
      dec $loop_run;
   }
   if (`EXECUTE stmt_break`)
   {
      let $run= 0;
   }
}
--enable_query_log
#
--echo # Subtest 1B (one connection, use PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_table
--echo #    default:    $insert_into
--echo #    default:    $drop_table
--disable_query_log
connection default;
eval PREPARE create_table FROM "$create_table";
EXECUTE create_table;
eval PREPARE insert_into FROM "$insert_into";
eval PREPARE drop_table FROM "$drop_table";
EXECUTE drop_table;
let $run= 1;
# Determine the current time.
EXECUTE stmt_start;
# Run execution loops till the planned runtime is reached
while ($run)
{
   let $loop_run= $loop_size;
   while ($loop_run)
   {
      EXECUTE create_table;
      EXECUTE insert_into;
      EXECUTE drop_table;
      dec $loop_run;
   }
   if (`EXECUTE stmt_break`)
   {
      let $run= 0;
   }
}
DEALLOCATE PREPARE create_table;
DEALLOCATE PREPARE insert_into;
DEALLOCATE PREPARE drop_table;
--enable_query_log
#
--echo # Subtest 1C (two connections, no PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_table
--echo #    con2:       $insert_into
--echo #    con2:       $drop_table
--disable_query_log
connection default;
let $run= 1;
# Determine the current time.
EXECUTE stmt_start;
# Run execution loops till the planned runtime is reached
while ($run)
{
   let $loop_run= $loop_size;
   while ($loop_run)
   {
      eval $create_table;
      connection con2;
      eval $insert_into;
      eval $drop_table;
      connection default;
      dec $loop_run;
   }
   if (`EXECUTE stmt_break`)
   {
      let $run= 0;
   }
}
--enable_query_log
#
--echo # Subtest 1D (two connections, use PREPARE/EXECUTE)
--echo #    connection  action
--echo #    default:    $create_table
--echo #    con2:       $insert_into
--echo #    con2:       $drop_table
--disable_query_log
connection default;
eval PREPARE create_table FROM "$create_table";
EXECUTE create_table;
connection con2;
eval PREPARE insert_into FROM "$insert_into";
eval PREPARE drop_table FROM "$drop_table";
EXECUTE drop_table;
connection default;
let $run= 1;
# Determine the current time.
EXECUTE stmt_start;
# Run execution loops till the planned runtime is reached
while ($run)
{
   let $loop_run= $loop_size;
   while ($loop_run)
   {
      EXECUTE create_table;
      connection con2;
      EXECUTE insert_into;
      EXECUTE drop_table;
      connection default;
      dec $loop_run;
   }
   if (`EXECUTE stmt_break`)
   {
      let $run= 0;
   }
}
DEALLOCATE PREPARE create_table;
connection con2;
DEALLOCATE PREPARE insert_into;
DEALLOCATE PREPARE drop_table;
connection default;
--enable_query_log
