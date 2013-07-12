# include/wait_condition_or-abort.inc
#
# SUMMARY
#    Waits until the passed statement returns true, or the operation
#    times out and test case breaks.
#    This is wrapper for wait_condition.inc.
#
# USAGE
#
#    let $wait_condition=
#      SELECT c = 3 FROM t;
#    --source include/wait_condition_or_abort.inc
#
#   OR
#
#    let $show_rpl_debug_info= 1; # to force post-failure printout
#    let $wait_timeout= 60; # Override default 30 seconds with 60.
#    let $wait_condition=
#      SELECT c = 3 FROM t;
#    --source include/wait_condition_or_abort.inc
#    --echo Executed the test condition $wait_condition_reps times
#

--source include/wait_condition.inc

if (!$success)
{
  if ($show_rpl_debug_info)
  {
    --source include/show_rpl_debug_info.inc
  }
  die;
}
