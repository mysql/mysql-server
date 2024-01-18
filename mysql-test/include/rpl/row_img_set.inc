#
# This is an auxiliar script that sets the binlog-row-image
# on a set of connections. These connections are passed as
# a parameter to this script. Its format is the following:
#
#   <conid,value,Y_or_N>:[<conid,value,Y_or_N>:...]
#
# In detail:
#
#   conid   -- connection id (eg, master)
#   value   -- binlog_row_image value to set (eg, FULL)
#   Y_or_N  -- Issue stop and start replica (eg, Y) 
# 
# Sample usage:
#
#  -- let $row_img_set=master:FULL:N,slave:MINIMAL:Y
#  -- source include/rpl/row_img_set.inc
#
# Notes:
#
#   1. This script saves and restores the original connection that was
#      in use at the time it was included.

-- let $old_conn= $CURRENT_CONNECTION

while (`SELECT HEX('$row_img_set') != HEX('')`)
{

  -- let $tuple= `SELECT SUBSTRING_INDEX('$row_img_set', ',', 1)`

  -- let $conn= `SELECT SUBSTRING_INDEX('$tuple', ':', 1)`
  -- let $rimg= `SELECT SUBSTRING_INDEX(LTRIM(SUBSTRING('$tuple', LENGTH('$conn') + 2)), ':', 1)`

  -- let $is_slave_restart= `SELECT LTRIM(SUBSTRING('$tuple', LENGTH('$conn') + LENGTH('$rimg')+ 3))`

  -- echo CON: '$conn', IMG: '$rimg', RESTART REPLICA: '$is_slave_restart'

  -- connection $conn
  -- eval SET SESSION binlog_row_image= '$rimg'
  -- eval SET GLOBAL binlog_row_image= '$rimg'

  if ($is_slave_restart == Y)
  {
    -- source include/rpl/stop_replica.inc
    -- source include/rpl/start_replica.inc
  }

  FLUSH TABLES;
  SHOW VARIABLES LIKE 'binlog_row_image';

  -- let $row_img_set= `SELECT LTRIM(SUBSTRING('$row_img_set', LENGTH('$tuple') + 2     ))`

}

-- connection $old_conn
