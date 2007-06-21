# include/wait_for_slave_param.inc
#
# SUMMARY
#
#    Waits until SHOW SLAVE STATUS has returned a spicified value.
#
# USAGE
#
#    let $slave_param= Slave_SQL_Running;
#    let $slave_param_value= No;
#    --source include/slave_wait_param.inc

let $slave_wait_param_counter= 300;
let $slave_value= query_get_value("SHOW SLAVE STATUS", $slave_param, 1);
while (`select "$slave_value" != "$slave_param_value"`)
{
  dec $slave_wait_param_counter;
  if (!$slave_wait_param_counter)
  {
    --echo ERROR: failed while waiting for slave parameter $slave_param: $slave_param_value
    query_vertical show slave status;
    exit;
  }
  sleep 0.1;
  let $slave_value= query_get_value("SHOW SLAVE STATUS", $slave_param, 1);
}
