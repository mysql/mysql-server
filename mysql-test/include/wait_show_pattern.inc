# include/wait_show_pattern.inc
#
# SUMMARY
#
#   Waits until output produced by SHOW statement which particular type is
#   specified as parameter matches certain pattern or maximum time reached.
#
# NOTES
#
#   Only the first row produced by the parameter statement is checked.
#
# USAGE
#
#   let $show_type= <Tail of SHOW statement>;
#   let $show_pattern= 'Pattern to be used for LIKE matching';
#   --source wait_show_pattern.inc
#
# EXAMPLES
# 
#   alter_table-big.test, wait_slave_status.inc
#
# SEE ALSO
#
#   wait_slave_status.inc, wait_condition.inc (>=5.1)
#
###############################################################################

--disable_query_log

# We accept to wait maximum 30 seconds (0.2 sec/loop).
let $wait_counter= 150;
while ($wait_counter)
{
  let $result= `SHOW $show_type`;
  let $success= `SELECT '$result' LIKE $show_pattern`;
  if ($success)
  {
    let $wait_counter= 0;
  }
  if (!$success)
  {
    real_sleep 0.2;
    dec $wait_counter;
  }
}
if (!$success)
{
  echo Timeout in wait_show_pattern.inc \$show_type= $show_type \$show_pattern= $show_pattern (\$result= '$result');
}

--enable_query_log
