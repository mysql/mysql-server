# ==== Purpose ====
#
# Waits until a variable from SHOW STATUS has returned a specified 
# value, or until a timeout is reached.
#
# ==== Usage ====
#
# let $status_var= Threads_connected;
# let $status_var_value= 1;
# --source include/wait_for_status_var.inc
#
# Parameters:
#
# $status_var, $status_var_value
#   This macro will wait until the variable of SHOW STATUS 
#   named $status_var gets the value $status_var_value.  See
#   the example above.
# 
# $status_type= GLOBAL|SESSION
#   To specify the type (attribute) of status variable and
#   run either SHOW GLOBAL STATUS or SHOW SESSION STATUS.
# 
# $status_var_comparsion
#   By default, this file waits until $status_var becomes equal to
#   $status_var_value.  If you want to wait until $status_var
#   becomes *unequal* to $status_var_value, set this parameter to the
#   string '!=', like this:
#     let $status_var_comparsion= !=;
#
# $status_timeout
#   The default timeout is 1 minute. You can change the timeout by
#   setting $status_timeout. The unit is tenths of seconds.
#

if (`SELECT STRCMP('$status_type', '') * STRCMP(UPPER('$status_type'), 'SESSION') * STRCMP(UPPER('$status_type'), 'GLOBAL')`)
{
  --echo **** ERROR: Unknown type of variable status_type: allowed values are: SESSION or GLOBAL ****
  die;
}

let $_status_timeout_counter= $status_timeout;
if (!$_status_timeout_counter)
{
  let $_status_timeout_counter= 600;
}

let $_status_var_comparsion= $status_var_comparsion;
if (!$_status_var_comparsion)
{
  let $_status_var_comparsion= =;
}

# Get type of variable
let $_is_number= 0;
if (`SELECT '$status_var_value' REGEXP '^[\+\-]*[0-9]+(\.[0-9]+)*\$'`)
{
  let $_is_number= 1;
}

let $_show_status_value= query_get_value("SHOW $status_type STATUS LIKE '$status_var'", Value, 1);

# Set way of comparing
let $_query= SELECT NOT('$_show_status_value' $_status_var_comparsion '$status_var_value');
if ($_is_number)
{
  let $_query= SELECT NOT($_show_status_value $_status_var_comparsion $status_var_value);
}

while (`$_query`)
{
  if (!$_status_timeout_counter)
  {
    --echo **** ERROR: failed while waiting for '$status_type' '$status_var' $_status_var_comparsion '$status_var_value' ****
    --echo Note: the following output may have changed since the failure was detected
    --echo **** Showing STATUS, PROCESSLIST ****
    eval SHOW $status_type STATUS LIKE '$status_var';
    SHOW PROCESSLIST;
    die;
  }
  dec $_status_timeout_counter;
  sleep 0.1;
  let $_show_status_value= query_get_value("SHOW $status_type STATUS LIKE '$status_var'", Value, 1);
  let $_query= SELECT NOT('$_show_status_value' $_status_var_comparsion '$status_var_value');
  if ($_is_number)
  {
    let $_query= SELECT NOT($_show_status_value $_status_var_comparsion $status_var_value);
  }
}
