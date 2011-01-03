# ==== Purpose ====
#
# Waits until the SQL thread of the current connection has got an
# error, or until a timeout is reached. Also waits until the SQL
# thread has completely stopped.
#
# ==== Usage ====
#
# --let $slave_sql_errno= NUMBER
# [--let $show_slave_sql_error= 1]
# [--let $rpl_debug= 1]
# [--let $slave_timeout= NUMBER]
# --source include/wait_for_slave_sql_error.inc
#
# Parameters:
#   $slave_sql_errno
#     The expected SQL error number.  This is required.
#     (After BUG#41956 has been fixed, this will be required to be a
#     symbolic name instead of a number.)
#
#   $show_slave_sql_error
#     If set, will print the error to the query log.
# 
#   $slave_timeout
#     See include/wait_for_slave_param.inc
# 
#   $rpl_debug
#     See include/rpl_init.inc


--let $include_filename= wait_for_slave_sql_error.inc [errno=$slave_sql_errno]
--source include/begin_include_file.inc


# Note: here, we should not wait for Slave_SQL_Errno!=0.
# Slave_SQL_Errno and Slave_SQL_Running are not set atomically.
# Slave_SQL_Errno is set first, then Slave_SQL_Running.  So we wait
# until Slave_SQL_Running=No to be sure that both conditions hold.
let $slave_param= Slave_SQL_Running;
let $slave_param_value= No;
source include/wait_for_slave_param.inc;

let $_wfsse_errno= query_get_value(SHOW SLAVE STATUS, Last_SQL_Errno, 1);

if ($slave_sql_errno == '') {
  --echo !!!ERROR IN TEST: you must set \$slave_sql_errno before you source
  --echo !!!wait_for_slave_sql_error.inc. The error we got this time was '$_wfsse_errno',
  --echo !!!so you probably want to add the following line to your test case:
  --echo !!!  --let \$slave_sql_errno= $_wfsse_errno
  --die !!!ERROR IN TEST: you must set \$slave_sql_errno before sourcing wait_for_slave_sql_error.inc
}

if (`SELECT $_wfsse_errno NOT IN ($slave_sql_errno)`) {
  --echo **** Slave stopped with wrong error code: $_wfsse_errno (expected $slave_sql_errno) ****
  source include/show_rpl_debug_info.inc;
  --echo **** Slave stopped with wrong error code: $_wfsse_errno (expected $slave_sql_errno) ****
  --die Slave stopped with wrong error code
}

if ($show_slave_sql_error)
{
  --let $_wait_for_slave_sql_error_old_status_items= $status_items
  --let $status_items= Last_SQL_Error
  --source include/show_slave_status.inc
  --let $status_items= $_wait_for_slave_sql_error_old_status_items
}


--let $include_filename= wait_for_slave_sql_error.inc [errno=$slave_sql_errno]
--source include/end_include_file.inc
