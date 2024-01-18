###############################################################################
# Use this .inc to create the channels on MSR slave.

# This .inc assumes that the MSR setup is already done
# as 1->$rpl_number_of_servers, 2->$rpl_number_of_servers,...,
# $rpl_number_of_servers -1 -> $rpl_number_of_servers.
# It also assumes that server_$rpl_number_of_servers
# is the server where we have to create the channels.
# The number of channels are equal to $rpl_number_of_servers -1.
# They are default channel, channel_2, channel_3,...,channel_n.
# First master (server1) will be replicated using
# the default channel(empty channel name). Starting from the
# second master, the rest of the masters will be replicated using
# channel_<SERVER NUMBER> channel.
#
# Usage:
# --let $rpl_number_of_servers=4
# [--let $no_sample_data= 0 | 1]
# --source common/rpl/create_msr_channels.inc
#
# $rpl_number_of_servers
#    Specify total number of servers in the MSR setup.
#
# $no_sample_data
#    This .inc by default creates some sample data and sync
#    it between master and slave after creating the channel between
#    them. Set this variable to 1 if it is not required.
#
###############################################################################

if (!$rpl_number_of_servers)
{
  --die "Please specify the total number of servers in the MSR setup."
}

--let $number_of_channels= `SELECT $rpl_number_of_servers -1`
--let $rpl_msr_server_number=$rpl_number_of_servers

--let $i=1
while ($i <= $number_of_channels)
{
  --let $rpl_connection_name= server_$i
  --source include/connection.inc
  RESET BINARY LOGS AND GTIDS;
  --let $source_port=`SELECT @@PORT`
  --let $rpl_connection_name=server_$rpl_msr_server_number
  --source include/connection.inc
  --let $rpl_channel_name='channel_$i'
  # First channel name should be default channel with empty name.
  if ($i == 1)
  {
    --let $rpl_channel_name=''
  }
  --replace_result $source_port port
  --eval CHANGE REPLICATION SOURCE TO SOURCE_USER='root', SOURCE_PORT= $source_port, SOURCE_HOST='localhost' FOR CHANNEL $rpl_channel_name
  --source include/rpl/start_replica.inc

  if (!$no_sample_data)
  {
    --let $rpl_connection_name= server_$i
    --source include/connection.inc
    --eval CREATE DATABASE db$i
    --eval CREATE TABLE db$i.t$i(i int)
    --eval DROP TABLE db$i.t$i
    --eval DROP DATABASE db$i

    --let $sync_slave_connection=server_$rpl_msr_server_number
    --source include/rpl/sync_to_replica.inc
  }
  --inc $i
}
