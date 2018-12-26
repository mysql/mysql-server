# ==== Purpose ====
#
# Issues START GROUP_REPLICATION on the current connection.
# Validates that server was able to bind Group Replication
# local port, if it was not:
#   on Windows systems: the test will skip itself;
#   on other systems:   the test will fail.
#
#
# ==== Usage ====
#
# --source include/start_group_replication_command.inc
#

--let $_is_windows= `SELECT convert(@@version_compile_os using latin1) IN ("Win32","Win64","Windows") = 1`

if ($_is_windows)
{
  --error 0, ER_GROUP_REPLICATION_CONFIGURATION
}
START GROUP_REPLICATION;

if ($_is_windows)
{
  --let $_member_id= query_get_value(SELECT @@SERVER_UUID, @@SERVER_UUID, 1)
  --let $_is_member_offline= `SELECT COUNT(*)=1 FROM performance_schema.replication_group_members group_members WHERE group_members.channel_name="group_replication_applier" AND group_members.member_id="$_member_id" AND group_members.member_state="OFFLINE"`

  if ($_is_member_offline)
  {
    --source include/force_restart_if_skipped.inc
    --skip Group Replication local port was busy, test cannot continue
  }
}
