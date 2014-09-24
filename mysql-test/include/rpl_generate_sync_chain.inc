# ==== Purpose ====
#
# Setup $rpl_sync_chain, which is used by rpl_sync.inc. You normally
# don't need to source this file, it should only be sourced by
# rpl_sync.inc.
#
# $rpl_sync_chain is set to a string that specifies in what order
# servers should be synchronized in include/rpl_sync.inc. This has the
# form of a sequence of "chains" (with no separator between two
# chains). Each chain begins with $rpl_server_count_length space
# characters, followed by a sequence of numbers, each number
# whitespace-padded to $rpl_server_count_length characters. Each
# number in the sequence denotes a server, and the N'th server is a
# master of the (N+1)'th server. For example, if $rpl_topology is
# '1->2,2->3,3->1,2->4,5->6', then $rpl_sync_chain is ' 56 123124'.
#
#
# ==== Usage ====
#
# [--let $rpl_debug= 1]
# --source include/rpl_generate_sync_chain.inc
#
# Parameters:
#   $rpl_debug
#     See include/rpl_init.inc


--let $include_filename= rpl_generate_sync_chain.inc
--source include/begin_include_file.inc


# Algorithm:
# 0. Mark all servers as unseen and unsynced.
# 1. Let S be a server that is marked unseen.
# 2. Append S to the list of seen servers.
# 3. Check how S is marked:
#    3.1. If S has no master: append the list of seen servers (in
#         order from grand-master to grand-slave) to the end of
#         $rpl_sync_chain. Go to 3.
#    3.2. Elseif S is marked as synced: append the list of seen
#         servers (in order from grand-master to grand-slave) to the
#         end of $rpl_sync_chain. Go to 3.
#    3.3. Elseif S is marked as unsynced but seen: This means that the
#         graph of visited servers has a "6-shape": it is a loop with
#         a tail, such as 1->2->3->1->4->5. We should first sync the
#         loop, and then the tail. To ensure all servers in the loop
#         are synced, we must sync the loop two turns minus two
#         servers. For example, the loop 1->2->3->4->5->1 is fully
#         synced by this sequence of 1-step synchronizations:
#         1->2->3->4->5->1->2->3->4. Hence we do this: in the list of
#         traversed servers (in order from grand-master to
#         grand-slave), find the first occurrence of S. Take the
#         sub-list starting at the 3rd server and ending at the first
#         occurrence of S. Append this sub-list it to the end of
#         $rpl_sync_chain. Then append the entire list of traversed
#         servers (in order from grand-master to grand-slave) to
#         $rpl_sync_chain. Go to 3.
#    3.4. Else (i.e., S has a master and is not marked as seen or
#         synced): Mark S as seen. Set S=master(S) and go back to 2.
# 4. For each server that is marked as seen, mark it as synced.
# 5. If there are unseen servers, go back to 1.

# $_rpl_server_marks holds the marks of all servers. The i'th character
# corresponds to the mark of server i:
# '0' = unseen & unmarked, '1' = seen & unsynced, '2' = seen & synced.
--let $_rpl_server_marks= `SELECT REPEAT('0', $rpl_server_count)`
--let $_rpl_start_server= $rpl_server_count
--let $rpl_sync_chain=
while ($_rpl_start_server)
{
  --let $_rpl_server= `SELECT RPAD('$_rpl_start_server', $rpl_server_count_length, ' ')`
  --let $_rpl_seen_list=
  --let $_rpl_continue_loop= 1
  while ($_rpl_continue_loop)
  {
    # Set $_rpl_master to the master of $_rpl_server, or '' if it does
    # not have one.
    --let $_rpl_slave_position= LOCATE('s$_rpl_server ', '$rpl_connection_list')
    --let $_rpl_master= `SELECT IF($_rpl_slave_position > 0, SUBSTRING('$rpl_connection_list', $_rpl_slave_position - 1 - $rpl_server_count_length, $rpl_server_count_length), '$_rpl_no_server')`

    # We need to delimit elements of $_rpl_seen_list with commas, so
    # that LOCATE() below will not find spurious matches that begin in
    # the middle of one element and end in the middle of next element.
    --let $_rpl_seen_list= $_rpl_server,$_rpl_seen_list
    # If server is marked seen or synced, or has no master
    if (`SELECT SUBSTRING('$_rpl_server_marks', $_rpl_server, 1) != 0 OR '$_rpl_master' = ''`)
    {
      # If server is marked seen but not synced.
      if (`SELECT SUBSTRING('$_rpl_server_marks', $_rpl_server, 1) = 1`)
      {
        # Get sub-list of servers to prepend to server list.
        # E.g., if topology is 1->2->3->4->1->5, then at this point
        # $_rpl_seen_list='1,2,3,4,1,5,' and we have to prepend '4,3,'
        # to it. Hence, the sub-list starts at position
        # 1+2*($rpl_server_count_length+1) and ends at the first
        # occurrence of ',1,' in the list.
        --let $_rpl_extra_list= `SELECT SUBSTRING('$_rpl_seen_list', 1 + 2 * ($rpl_server_count_length + 1), LOCATE(',$_rpl_server,', '$_rpl_seen_list') - 2 * ($rpl_server_count_length + 1))`
        --let $_rpl_seen_list= $_rpl_extra_list$_rpl_seen_list
      }
      # Append the seen servers. Only need to append if the list
      # contains at least two elements.
      if (`SELECT LENGTH('$_rpl_seen_list') > $rpl_server_count_length + 1`)
      {
        --let $rpl_sync_chain= $rpl_sync_chain$_rpl_no_server$_rpl_seen_list
      }
      --let $_rpl_continue_loop= 0
    }
    --let $_rpl_server_marks= `SELECT INSERT('$_rpl_server_marks', $_rpl_server, 1, '1')`
    --let $_rpl_server= $_rpl_master
  }
  # Mark seen servers as synced
  --let $_rpl_server_marks= `SELECT REPLACE('$_rpl_server_marks', '1', '2')`
  # Get highest-numbered unmarked server.
  --let $_rpl_start_server= `SELECT IFNULL(NULLIF($rpl_server_count + 1 - LOCATE('0', REVERSE('$_rpl_server_marks')), $rpl_server_count + 1), 0)`
}
# Strip commas: they were only needed temporarily.
--let $rpl_sync_chain= `SELECT REPLACE('$rpl_sync_chain', ',', '')`

if ($rpl_debug)
{
  --echo Generated \$rpl_sync_chain = '$rpl_sync_chain'
}


--let $include_filename= rpl_generate_sync_chain.inc
--source include/end_include_file.inc
