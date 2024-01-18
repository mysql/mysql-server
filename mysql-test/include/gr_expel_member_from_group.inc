# ==== Purpose ====
#
# Expels a given member from the group by sending a SIGSTOP to it to keep it
# from sending out his keep-alives.
#
# NOTE: you must source linux.inc in order to use this include file, due to the
# way we expel the member (we send a SIGSTOP/SIGCONT, that doesn't work in
# Windows).
#
# ==== Usage ====
#
# --let $member_id = <the ID of the member you want to expel>
# --source include/gr_expel_member_from_group.inc
#
# Parameters:
#   $member_id
#     The ID of the member we want to expel from the group
#
--let $include_filename = gr_expel_member_from_group.inc
--source include/begin_include_file.inc

if ($member_id == '')
{
  --die ERROR IN TEST: You must set $member_id before sourcing gr_expel_member_from_group.inc
}

# Save current connection.
--let $_old_connection= $CURRENT_CONNECTION

#
# First we find out which connection corresponds to the ID of the member
# we want to expel.
#
--let $itr = $rpl_server_count
while ($itr > 0)
{
  --let $rpl_connection_name=server$itr
  --source include/connection.inc

  #
  # Verify if this is the connection of the member to expel.
  # If so, we save the connection name.
  --let $curr_member_id = `SELECT @@GLOBAL.server_uuid`
  if ($curr_member_id == $member_id)
  {
    --let $conn_name = server$itr
    --let $itr = 1
  }

  --dec $itr
}

if ($conn_name == '')
{
  --die Could not find a connection with $member_id
}

#
# Now we need to obtain the PID of the server to expel, so we can send it the
# signal we want.
#
--let $rpl_connection_name = $conn_name
--source include/connection.inc
SET SESSION sql_log_bin = 0;
CREATE TABLE pid_table(pid_no INT PRIMARY KEY);
--let $pid_file=`SELECT @@pid_file`
--replace_result $pid_file pid_file
--eval LOAD DATA LOCAL INFILE '$pid_file' INTO TABLE pid_table
--let $server_pid=`SELECT pid_no FROM pid_table`
DROP TABLE pid_table;
SET SESSION sql_log_bin = 1;

#
# We send a SIGSTOP to the member to expel. This will cause the server process
# to suspend and thus won't send out his keep-alive packet. The xcom fault
# detector of some member of the group will think the member is dead and expel
# it from the group.
--exec kill -19 $server_pid

#
# We now go through each member of the group except the one we forced to be
# expelled, in order to verify that this member has indeed been expelled.
#
--let $itr = $rpl_server_count
--let $new_server_count = $rpl_server_count
--dec $new_server_count
while ($itr > 0)
{
  # Skip the expelled member
  --let curr_server = server$itr
  if ($curr_server != $conn_name)
  {
    --let $rpl_connection_name = server$itr
    --source include/connection.inc
    --let $group_replication_number_of_members = $new_server_count
    --source include/gr_wait_for_number_of_members.inc
  }

  --dec $itr
}

#
# Wake up the suspended member - he should have been expelled.
#
--let $rpl_connection_name = $conn_name
--source include/connection.inc
--exec kill -18 $server_pid

# Verify that the member entered an error state
--let $group_replication_member_state = ERROR
--let $group_replication_member_id = $member_id
--source include/gr_wait_for_member_state.inc

# Then verify that it enabled super_read_only
--let $rpl_connection_name = $conn_name
--source include/connection.inc
--let $assert_text = super_read_only should be enabled
--let $assert_cond = [SELECT @@GLOBAL.super_read_only] = 1;
--source include/assert.inc

# Revert old connection.
--connection $_old_connection

--let $include_filename = gr_expel_member_from_group.inc
--source include/end_include_file.inc
