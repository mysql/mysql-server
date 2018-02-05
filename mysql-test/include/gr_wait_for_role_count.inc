# ==== Purpose ====
#
# Wait for the number of servers in the group playing
# the expected roles.
#
# ==== Usage ====
#
# --let $group_replication_primaries_count_in_var=N
# --let $group_replication_secondaries_count_in_var=M
# [--let $wait_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --source include/gr_wait_for_role_count.inc

--let $include_filename= gr_wait_for_role_count.inc
--source include/begin_include_file.inc

# lets wait until a new primary is elected because that's an
# asynchronous procedure
# count the primaries and secondaries in the group
--let $_timeout= $wait_timeout
--let $_ok= 0

# there are N - 1 secondaries, where N is the number of servers
# in the group
--let $_expected_secondaries= $group_replication_secondaries_count_in_var
--let $_expected_primaries= $group_replication_primaries_count_in_var

while ($_timeout > 0)
{
  # count the primaries in the group
  --source include/gr_count_primaries_secondaries.inc

  # there are N - 1 secondaries, where N is the number of servers
  # in the group
  --let $_counted_secondaries= $group_replication_count_secondaries_out_var

  # there is only one primary
  --let $_counted_primaries= $group_replication_count_primaries_out_var

  if (`SELECT $_counted_secondaries <> $_expected_secondaries OR $_counted_primaries <> $_expected_primaries`)
  {
    --sleep 1
    --dec $_timeout
  }

  if (`SELECT $_counted_secondaries = $_expected_secondaries AND $_counted_primaries = $_expected_primaries`)
  {
    --let $_ok= 1
    # bail out
    --let $_timeout= -1
  }
}

# assert to display which is not matching expectations
if (!$_ok)
{
  --echo PRIMARIES: Expected=$_expected_primaries, Counted=$_counted_primaries
  --echo SECONDARIES: Expected=$_expected_secondaries, Counted=$_counted_secondaries
  --die "PRIMARIES and SECONDARIES count does not match expectations.".
}

--let $include_filename= gr_wait_for_role_count.inc
--source include/end_include_file.inc
