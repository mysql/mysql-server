# ==== Purpose ====
#
# Asserts that a given notification set has happened.
#
# ==== Usage ====
#
# --let $expected_notifications= N,NOTIFICATION_TYPE2|M,NOTIFICATION_TYPE2
# [--let $truncate_notification_table= 0]
# --source ../include/assert_notifications.inc
#
# Parameters:
#
#   $expected_notifications
#     A string describing the set of notifications expected.
#     The string takes the form of N,DESCRIPTION1|M,DESCRIPTION2
#
#     - N and M are the occurrences of the given notification description
#     - DESCRIPTION1 and DESCRIPTION2 are strings used to describe the
#       the notification and will be compared using the operator LIKE,
#       e.g., LIKE "DESCRIPTION1"
#
#   $truncate_notification_table
#     Whether to truncate the notification table or not at the end
#     of the assertion


if (!$expected_notifications)
{
  --die "Missing argument '$expected_notifications'"
}

if ($truncate_notification_table)
{
  --let $_truncate_notification_table= $truncate_notification_table
}

if (!$_truncate_notification_table)
{
  --let $_truncate_notification_table= 1
}

--let $_expected_notifications=$expected_notifications
--let $_expected_notifications_count=0

# count notifications
while (`SELECT HEX('$_expected_notifications') != HEX('')`)
{
  # next notification
  --let $_tuple= `SELECT SUBSTRING_INDEX('$_expected_notifications', '|', 1)`
  --let $_notification_count= `SELECT SUBSTRING_INDEX('$_tuple', ',', 1)`

  # increment the number of notifications to check at the end
  --let $_expected_notifications_count= `SELECT $_expected_notifications_count + $_notification_count`

  # next
  --let $_expected_notifications= `SELECT LTRIM(SUBSTRING('$_expected_notifications', LENGTH('$_tuple') + 2 ))`
}

# wait for the notifications to be inserted in the table
--let $_saved_show_rpl_debug_info= $show_rpl_debug_info
--let show_rpl_debug_info=0
--let $_c = $_expected_notifications_count
--let $wait_condition= SELECT COUNT(*) = $_c FROM gms_listener_example
--source include/wait_condition.inc
--let $show_rpl_debug_info= $_saved_show_rpl_debug_info

if (!$success)
{
  --query_vertical SELECT * FROM test.gms_listener_example
  --die "Timed out while waiting for gms_listener_example to have $_c rows!"
}

# reset _expected_notifications
--let $_expected_notifications=$expected_notifications

# assert each of them
while (`SELECT HEX('$_expected_notifications') != HEX('')`)
{
  # next notification
  --let $_tuple= `SELECT SUBSTRING_INDEX('$_expected_notifications', '|', 1)`
  --let $_notification_count= `SELECT SUBSTRING_INDEX('$_tuple', ',', 1)`
  --let $_notification= `SELECT SUBSTRING_INDEX(LTRIM(SUBSTRING('$_tuple', LENGTH('$_notification_count') + 2)), '|', 1)`

  # assert
  --let $assert_cond= COUNT(*)=$_notification_count FROM test.gms_listener_example WHERE log_message LIKE "$_notification"
  --let $assert_text= Assert that there are $_notification_count notifications logged of type $_notification
  --source include/assert.inc

  # move to the next notification
  --let $_expected_notifications= `SELECT LTRIM(SUBSTRING('$_expected_notifications', LENGTH('$_tuple') + 2 ))`
}

# assert again that no other notification was inserted meanwhile
--let $assert_cond= COUNT(*)=$_expected_notifications_count FROM test.gms_listener_example
--let $assert_text= Assert that there are $_expected_notifications_count notifications logged
--source include/assert.inc

if (`SELECT $_truncate_notification_table <> 0`)
{
  --echo [Truncating gms_listener_example Table]
  --disable_result_log
  --disable_query_log

  # clean up the notification table
  SET SESSION SQL_LOG_BIN=0;
  --let $_saved_super_read_only=`SELECT @@global.super_read_only`
  SET GLOBAL SUPER_READ_ONLY=OFF;
  TRUNCATE gms_listener_example;
  --eval SET GLOBAL SUPER_READ_ONLY=$_saved_super_read_only
  SET SESSION SQL_LOG_BIN=1;

  --enable_query_log
  --enable_result_log
}
