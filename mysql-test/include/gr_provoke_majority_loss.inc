# ==== Purpose ====
#
# Provokes a group majority loss on a given member by killing every member
# of the group except that given member.
#
# NOTE: For this to work you must configure
# group_replication_unreachable_majority_timeout to a value greater than 0!
#
# ==== Usage ====
#
# --let $member_id = <the ID of the member you don't want to kill>
# --source include/gr_provoke_majority_loss.inc
#
# Parameters:
#   $member_id
#     The ID of the member we want to provoke the majority loss on.
#
--let $include_filename = gr_provoke_majority_loss.inc
--source include/begin_include_file.inc

if ($member_id == '')
{
  --die ERROR IN TEST: You must set $member_id before sourcing gr_expel_member_from_group.inc
}

# Save current connection.
--let $_old_connection= $CURRENT_CONNECTION

#
# First we find out which connection corresponds to the ID of the member
# we don't want to kill.
#
--let $itr = $rpl_server_count
while ($itr > 0)
{
  --let $rpl_connection_name=server$itr
  --source include/connection.inc

  #
  # Verify if this is the connection of the member to expel.
  # If so, we save the connection name.
  #
  --let $curr_member_id = `SELECT @@GLOBAL.server_uuid`
  if ($curr_member_id == $member_id)
  {
    --let $conn_name = server$itr
    --let $itr = 1
  }

  --dec $itr
}

#
# Then we kill every other member except the one we want to provoke the
# majority loss on.
#
# To kill each member we loop through and bring them down forcefully,
# wait for them to go down and wait for them to come back up.
# But we do this in two phases - first we kill and them and wait
# for them to go down and then we do another loop and wait for them
# to go back up.
#
# We separate the process in these two phases so that we don't have
# to waitfor each member to go back up before shutting everything
# down. If we didn't do that, due to the time it takes for a member
# to go down and back up again, the member we want to force the
# majority loss on would see the group go down 1 by 1, which we don't
# want.
#
--let $itr = $rpl_server_count
while ($itr > 0)
{
  # We skip the member that we don't want to kill
  --let curr_server = server$itr
  if ($curr_server != $conn_name)
  {
    --let $rpl_connection_name = $curr_server
    --source include/connection.inc

    --let $no_wait_for_reconnect = 1
    --source include/kill_and_restart_mysqld.inc
  }

  --dec $itr
}
--let $itr = $rpl_server_count
while ($itr > 0)
{
  # We skip the member that we don't want to kill
  --let curr_server = server$itr
  if ($curr_server != $conn_name)
  {
    --let $rpl_connection_name = $curr_server
    --source include/connection.inc

    --source include/wait_until_connected_again.inc

    --let $rpl_server_number= $itr
    --source include/rpl/reconnect.inc
  }

  --dec $itr
}
#
# Verify that we have indeed lost contact to a majority of the group.
#
--let $rpl_connection_name = $conn_name
--source include/connection.inc

# The member should enter the ERROR state
--let $group_replication_member_state = ERROR
--let $group_replication_member_id = $member_id
--source include/gr_wait_for_member_state.inc

# Verify that it enabled super_read_only
--let $assert_text= super_read_only should be enabled
--let $assert_cond= [SELECT @@GLOBAL.super_read_only] = 1;
--source include/assert.inc

# Lastly, the group should have only 1 member
--let $group_replication_number_of_members = 1
--source include/gr_wait_for_number_of_members.inc

# Revert old connection.
--connection $_old_connection

--let $include_filename = gr_provoke_majority_loss.inc
--source include/end_include_file.inc
