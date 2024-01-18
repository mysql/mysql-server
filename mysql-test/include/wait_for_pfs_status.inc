# ==== Purpose ====
#
# Waits until column value from PFS replication tables has returned a specified 
# value, or until a timeout is reached.
#
# ==== Usage ====
#
# let $status_col= Threads_connected;
# let $status_col_value= 1;
# --source include/wait_for_pfs_status.inc
#
# Parameters:
#
# $status_col, $status_col_value
#   This macro will wait until the column value of PFS rpl table 
#   named $status_col gets the value $status_col_value.  See
#   the example above.
#
# $table
#   This will pass the PFS replication table from where the
#   stats will be collected.
# 
# $status_col_comparsion
#   By default, this file waits until $status_col becomes equal to
#   $status_col_value.  If you want to wait until $status_col
#   becomes *unequal* to $status_col_value, set this parameter to the
#   string '!=', like this:
#     let $status_col_comparsion= !=;
#
# $status_timeout
#   The default timeout is 1 minute. You can change the timeout by
#   setting $status_timeout. The unit is tenths of seconds.
#
# $status_fail_query
#   This can be set to an SQL statement which will be executed if the
#   script fails. Useful for debugging.

let $_status_timeout_counter= $status_timeout;
if (!$_status_timeout_counter)
{
  let $_status_timeout_counter= 600;
}

let $_status_col_comparsion= $status_col_comparsion;
if (!$_status_col_comparsion)
{
  let $_status_col_comparsion= =;
}

# Get type of status value
let $_is_number= 0;
if (`SELECT '$status_col_value' REGEXP '^[\+\-]*[0-9]+(\.[0-9]+)*\$'`)
{
  let $_is_number= 1;
}

let $_pfs_rpl_status_value= query_get_value("select $status_col from performance_schema.$table", $status_col, 1);

# Set way of comparing
let $_query= SELECT NOT('$_pfs_rpl_status_value' $_status_col_comparsion '$status_col_value');
if ($_is_number)
{
  let $_query= SELECT NOT($_pfs_rpl_status_value $_status_col_comparsion $status_col_value);
}

while (`$_query`)
{
  if (!$_status_timeout_counter)
  {
    if ($status_fail_query)
    { 
      --echo # debug output:
      eval $status_fail_query;
    }
    --echo **** ERROR: failed while waiting for $status_type $status_col $_status_col_comparsion $status_col_value ****
    --echo Note: the following output may have changed since the failure was detected
    --echo **** Showing PFS RPL TABLE STATUS, PROCESSLIST ****:
    eval select $status_col from performance_schema.$table;
    SHOW PROCESSLIST;
    die;
  }
  dec $_status_timeout_counter;
  sleep 0.1;
  let $_pfs_rpl_status_value= query_get_value("select $status_col from performance_schema.$table", $status_col, 1);
  let $_query= SELECT NOT('$_pfs_rpl_status_value' $_status_col_comparsion '$status_col_value');
  if ($_is_number)
  {
    let $_query= SELECT NOT($_pfs_rpl_status_value $_status_col_comparsion $status_col_value);
  }
}
