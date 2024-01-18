#
# Test CHANGE REPLICATION SOURCE Source_Bind=xxx
#
# Parameters:
#  $source_bind - the address to use for Source_Bind
#  $source_bind_error_expected - expect an error when using the specified
#                                source_bind address
#
#

--let $source_bind=$source_bind
--let $source_bind_error_expected=$source_bind_error_expected

# Stop the slave
connection slave;
source include/rpl/stop_replica.inc;

# Create table and insert one record with the bind address on master
connection master;
create table t1(n int, b varchar(256));
--replace_result $source_bind <source_bind>
eval insert into t1 values(1, $source_bind);

# Configure slave to connect to master with the give bind address
# for source_bind and Source_Host unless it's an invalid address
connection slave;
let $_source_host=;
if (!$source_bind_error_expected)
{
  if ($source_bind != "''")
  {
    let $_source_host=SOURCE_HOST=$source_bind,;
  }
}

--replace_result $source_bind <source_bind>
eval CHANGE REPLICATION SOURCE to $_source_host SOURCE_BIND=$source_bind;
START REPLICA;

# Check that SHOW REPLICA STATUS has Source_Bind column set to $source_bind
let $source_bind_value= query_get_value(SHOW REPLICA STATUS, Source_Bind, 1);
if (`select '$source_bind_value' != $source_bind`)
{
  source include/rpl/debug/show_debug_info.inc;
  echo 'source_bind_value: $source_bind_value' != 'source_bind: $source_bind';
  die Master_bind in SHOW REPLICA STATUS not showing configured value;
}

if ($source_bind_error_expected)
{
  # The given master bind address is not valid
  # and replication should fail
  let $slave_io_errno= $source_bind_error_expected;
  let $slave_io_error_is_nonfatal= 1;
  source include/rpl/wait_for_receiver_error.inc;
  echo got expected error $source_bind_error_expected;
  source include/rpl/stop_replica.inc;

  # Reset the source_bind so that cleanup can run
  eval CHANGE REPLICATION SOURCE to SOURCE_BIND='';
  START REPLICA;

}

source include/rpl/wait_for_replica_to_start.inc;

connection master;
sync_slave_with_master;

connection slave;
let $source_bind_repl= query_get_value(select b from t1, b, 1);
if (`select '$source_bind_repl' != $source_bind`)
{
  select * from t1;
  source include/rpl/debug/show_debug_info.inc;
  echo 'source_bind_repl: $source_bind_repl' != 'source_bind: $source_bind';
  die The replicated value to show replication working was not correct;
}

# Clean up
connection master;
drop table t1;
sync_slave_with_master;
