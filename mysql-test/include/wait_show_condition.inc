# include/wait_show_condition.inc
#
# SUMMARY
#
#    Waits until the show statement ($show_statement) has one or all of the
#    rows of the result set for the field ($field) a value which fulfils
#    a condition ($condition), or the operation times out.
#    
#
# USAGE
#
#    All rows of the result must fulfil the condition if $wait_for_all is 1
#    else at least one of the result must fulfil the condition.
#    let $wait_for_all= 1; 
#    let $show_statement= SHOW PROCESSLIST;
#    let $field= State;
#    let $condition= = 'Updating';
#    --source include/wait_show_condition.inc
#
#   OR
#
#    let $wait_timeout= 60; # Override default of 30 seconds with 60.
#    let $show_statement= SHOW PROCESSLIST;
#    let $field= State;
#    let $condition= = 'Updating';
#    --source include/wait_show_condition.inc
#
# Please do not use this use routine if you can replace the SHOW statement
# with a select. In such a case include/wait_condition.inc is recommended.
#
# Created: 2009-02-18 mleich
#

if (!$condition)
{
  --die ERROR IN TEST: the "condition" variable must be set
}

if (!$field)
{
  --die ERROR IN TEST: the "field" variable must be set
}

if (!$show_statement)
{
  --die ERROR IN TEST: the "show_statement" variable must be set
}

let $max_run_time= 30;
if ($wait_timeout)
{
  let $max_run_time= $wait_timeout;
}
# Reset $wait_timeout so that its value won't be used on subsequent
# calls, and default will be used instead.
let $wait_timeout= 0;

# The smallest timespan till UNIX_TIMESTAMP() gets incremented is ~0 seconds.
# We add one second to avoid the case that somebody measures timespans on a
# real clock with fractions of seconds, detects that n seconds are sufficient,
# assigns n to this routine and suffers because he sometimes gets n - 1
# seconds in reality.
inc $max_run_time;

let $found= 0;
let $max_end_time= `SELECT UNIX_TIMESTAMP() + $max_run_time`;

if ($wait_for_all != 1)
{
  while (`SELECT UNIX_TIMESTAMP() <= $max_end_time AND $found = 0`)
  {
     # Sleep a bit to avoid too heavy load.
     real_sleep 0.2;
     let $rowno= 1;
     let $process_result= 1;
     let $do_loop= 1;
     while ($do_loop)
     {
        let $field_value= query_get_value($show_statement, $field, $rowno);
        if (`SELECT '$field_value' $condition`)
        {
           let $found= 1;
	   let $do_loop= 0;
        }
        if ($field_value == No such row)
        {
           # We are behind the last row of the result set.
           let $process_result= 0;
	   let $do_loop= 0;
           # For string fields and for "!=" comparision, it is possible that
           # above if condition (i.e., SELECT '$field_value' $condition) is
           # true and the current if condition is also true. So set $found=0
           # here explicitly
           let $found= 0;
        }
        inc $rowno;
     }
  }
}

if ($wait_for_all == 1)
{
  while (`SELECT UNIX_TIMESTAMP() <= $max_end_time AND $found = 0`)
  {
     # Sleep a bit to avoid too heavy load.
     real_sleep 0.2;
     let $rowno= 1;
     let $process_result= 1;
     let $do_loop= 1;
     while ($do_loop)
     {
        let $field_value= query_get_value($show_statement, $field, $rowno);
        if ($field_value == No such row)
        {
           let $found= 1;
	   let $do_loop= 0;
        }
        if (`SELECT $found = 0 AND NOT '$field_value' $condition`)
        {
           let process_result= 0;
	   let $do_loop= 0;
        }
        inc $rowno;
     }
  }
}

if (!$found)
{
  echo # Timeout in include/wait_show_condition.inc for $condition;
  echo #         show_statement : $show_statement;
  echo #         field          : $field;
  echo #         condition      : $condition;
  echo #         max_run_time   : $max_run_time;
}

