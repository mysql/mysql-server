#
# Test CHANGE MASTER MASTER_BIND=xxx
#
# Parameters:
#  $source_bind - the address to use for MASTER_BIND
#  $source_bind_error_expected - expect an error when using the specified
#                                source_bind address
#
#

--let $source_bind=$master_bind
--let $source_bind_error_expected=$master_bind_error_expected

# Stop the slave
connection slave;
source include/stop_slave.inc;

# Create table and insert one record with the bind address on master
connection master;
create table t1(n int, b varchar(256));
--replace_result $source_bind <source_bind>
eval insert into t1 values(1, $source_bind);

# Configure slave to connect to master with the give bind address
# for source_bind and master_host unless it's an invalid address
connection slave;
let $_master_host=;
if (!$source_bind_error_expected)
{
  if ($source_bind != "''")
  {
    let $_master_host=SOURCE_HOST=$source_bind,;
  }
}

--replace_result $source_bind <source_bind>
eval CHANGE REPLICATION SOURCE to $_master_host SOURCE_BIND=$source_bind;
start slave;

# Check that SHOW SLAVE STATUS has Master_bind column set to $source_bind
let $source_bind_value= query_get_value(SHOW SLAVE STATUS, Master_Bind, 1);
if (`select '$source_bind_value' != $source_bind`)
{
  source include/show_rpl_debug_info.inc;
  echo 'source_bind_value: $source_bind_value' != 'source_bind: $source_bind';
  die Master_bind in SHOW SLAVE STAUS not showing configured value;
}

if ($source_bind_error_expected)
{
  # The given master bind address is not valid
  # and replication should fail
  let $slave_io_errno= $source_bind_error_expected;
  let $slave_io_error_is_nonfatal= 1;
  source include/wait_for_slave_io_error.inc;
  echo got expected error $source_bind_error_expected;
  source include/stop_slave.inc;

  # Reset the source_bind so that cleanup can run
  eval CHANGE REPLICATION SOURCE to SOURCE_BIND='';
  start slave;

}

source include/wait_for_slave_to_start.inc;

connection master;
sync_slave_with_master;

connection slave;
let $source_bind_repl= query_get_value(select b from t1, b, 1);
if (`select '$source_bind_repl' != $source_bind`)
{
  select * from t1;
  source include/show_rpl_debug_info.inc;
  echo 'source_bind_repl: $source_bind_repl' != 'source_bind: $source_bind';
  die The replicated value to show replication working was not correct;
}

# Clean up
connection master;
drop table t1;
sync_slave_with_master;
