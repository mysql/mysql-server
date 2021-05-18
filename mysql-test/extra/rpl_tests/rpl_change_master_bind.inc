#
# Test CHANGE MASTER MASTER_BIND=xxx
#
# Parameters:
#  $master_bind - the address to use for MASTER_BIND
#  $master_bind_error_expected - expect an error when using the specified
#                                master_bind address
#
#

# Stop the slave
connection slave;
source include/stop_slave.inc;

# Create table and insert one record with the bind address on master
connection master;
create table t1(n int, b varchar(256));
--replace_result $master_bind <master_bind>
eval insert into t1 values(1, $master_bind);

# Configure slave to connect to master with the give bind address
# for master_bind and master_host unless it's an invalid address
connection slave;
let $_master_host=;
if (!$master_bind_error_expected)
{
  if ($master_bind != "''")
  {
    let $_master_host=master_host=$master_bind,;
  }
}

--replace_result $master_bind <master_bind>
eval change master to $_master_host master_bind=$master_bind;
start slave;

# Check that SHOW SLAVE STATUS has Master_bind column set to $master_bind
let $master_bind_value= query_get_value(SHOW SLAVE STATUS, Master_Bind, 1);
if (`select '$master_bind_value' != $master_bind`)
{
  source include/show_rpl_debug_info.inc;
  echo 'master_bind_value: $master_bind_value' != 'master_bind: $master_bind';
  die Master_bind in SHOW SLAVE STAUS not showing configured value;
}

if ($master_bind_error_expected)
{
  # The given master bind address is not valid
  # and replication should fail
  let $slave_io_errno= $master_bind_error_expected;
  let $slave_io_error_is_nonfatal= 1;
  source include/wait_for_slave_io_error.inc;
  echo got expected error $master_bind_error_expected;
  source include/stop_slave.inc;

  # Reset the master_bind so that cleanup can run
  eval change master to master_bind='';
  start slave;

}

source include/wait_for_slave_to_start.inc;

connection master;
sync_slave_with_master;

connection slave;
let $master_bind_repl= query_get_value(select b from t1, b, 1);
if (`select '$master_bind_repl' != $master_bind`)
{
  select * from t1;
  source include/show_rpl_debug_info.inc;
  echo 'master_bind_repl: $master_bind_repl' != 'master_bind: $master_bind';
  die The replicated value to show replication working was not correct;
}

# Clean up
connection master;
drop table t1;
sync_slave_with_master;
