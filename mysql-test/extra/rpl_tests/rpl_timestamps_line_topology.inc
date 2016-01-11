# The test file is invoked from rpl.rpl_timestamps_line_topology and
# group_replication.gr_replicaton_timestamps
#
# This file has two main sections:
#   1) Retrieve the OCT and ICT from each server
#   2) Verify that the timestamps are correct
#
# param $gtid - the gtid of the transaction to be checked


--echo # 1. Save both timestamps (original/immediate) on all servers.

# Switch to server_1, save both OCT and ICT.
--let $rpl_connection_name= $server_1
--source include/rpl_connection.inc

--source include/get_original_commit_timestamp.inc
--let $server_1_OCT= $original_commit_timestamp

--source include/get_immediate_commit_timestamp.inc
--let $server_1_ICT= $immediate_commit_timestamp

# Switch to server_2, save both OCT and ICT.
--let $rpl_connection_name= $server_2
--source include/rpl_connection.inc

--source include/get_original_commit_timestamp.inc
--let $server_2_OCT= $original_commit_timestamp

--source include/get_immediate_commit_timestamp.inc
--let $server_2_ICT= $immediate_commit_timestamp

# Switch to server_3, save both OCT and ICT.
--let $rpl_connection_name= $server_3
--source include/rpl_connection.inc

--source include/get_original_commit_timestamp.inc
--let $server_3_OCT= $original_commit_timestamp

--source include/get_immediate_commit_timestamp.inc
--let $server_3_ICT= $immediate_commit_timestamp

--echo # 2. Verify that the timestamps are correct.

--let $assert_text= Assert that the OCT is the same on $server_1 and $server_2.
--let $assert_cond= $server_1_OCT = $server_2_OCT
--source include/assert.inc

--let $assert_text= Assert that the OCT is the same on $server_2 and $server_3.
--let $assert_cond= $server_2_OCT = $server_3_OCT
--source include/assert.inc

--let $assert_text= Assert that OCT is the same as ICT on $server_1
--let $assert_cond= $server_1_OCT = $server_1_ICT
--source include/assert.inc

# Check if the platform is windows to select the appropriate assert condition
--let $is_windows= 0
if (`SELECT CONVERT(@@VERSION_COMPILE_OS USING latin1) IN ('Win32', 'Win64', 'Windows')`)
{
  --let $is_windows= 1
}

--let $assert_text= Assert that ICT > OCT on $server_2.
if ($is_windows)
{
  # Due to windows lower resolution timestamps, it may happen that $server_2_ICT
  # is equal to $server_2_OCT
  --let $assert_cond= $server_2_ICT >= $server_2_OCT
}
if (!$is_windows)
{
  --let $assert_cond= $server_2_ICT > $server_2_OCT
}
--source include/assert.inc

--let $assert_text= Assert that ICT > OCT on $server_3.
if ($is_windows)
{
  # Due to windows lower resolution timestamps, it may happen that $server_2_ICT
  # is equal to $server_3_OCT
  --let $assert_cond= $server_3_ICT >= $server_3_OCT
}
if (!$is_windows)
{
  --let $assert_cond= $server_3_ICT > $server_3_OCT
}
--source include/assert.inc

--let $assert_text= Assert that ICT on server_1 < ICT on $server_2.
if ($is_windows)
{
  # Due to windows lower resolution timestamps, it may happen that $server_1_ICT
  # is equal to $server_2_ICT
  --let $assert_cond= $server_1_ICT <= $server_2_ICT
}
if (!$is_windows)
{
  --let $assert_cond= $server_1_ICT < $server_2_ICT
}
--source include/assert.inc

--let $assert_text= Assert that ICT on server_2 < ICT on $server_3.
if ($is_windows)
{
  # Due to windows lower resolution timestamps, it may happen that $server_2_ICT
  # is equal to $server_3_ICT
  --let $assert_cond= $server_2_ICT <= $server_3_ICT
}
if (!$is_windows)
{
  --let $assert_cond= $server_2_ICT < $server_3_ICT
}
--source include/assert.inc
