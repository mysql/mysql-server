# ==== Purpose ====
# 
# Several test primitives from mysql-test/extra/rpl_tests 
# are shared for test cases for MyISAM, InnoDB, NDB and
# other engines.
# For NDB engine all events will be added by NDB injector
# so tests only can continue after injector is ready, 
# this file waits for all committed transactions (at start
# of file) are injected into the binlog.
#
# ==== Usage ====
#
# let $wait_timeout= 150;
# --source include/wait_for_ndb_committed_to_binlog.inc
#
# ==== Parameters =====
#
# $wait_timeout
#   Time to wait in seconds for epoch to be injected in binlog.
#   On timeout test dies.
#

if (`SELECT 1 FROM information_schema.engines WHERE engine = 'ndbcluster' AND support IN ('YES', 'DEFAULT')`)
{
  let $max_run_time= 150;
  if ($wait_timeout)
  {
    let $max_run_time= $wait_timeout;
  }
  # Reset $wait_timeout so that its value won't be used on subsequent
  # calls, and default will be used instead.
  let $wait_timeout= 0;

  # The smallest timespan till UNIX_TIMESTAMP() gets incremented is ~0 seconds.
  # We add one second to avoid the case that somebody measures timespans on a
  # real clock with fractions of seconds, detects that n seconds are sufficient,
  # assigns n to this routine and suffers because he sometimes gets n - 1
  # seconds in reality.

  let $max_end_time= `SELECT UNIX_TIMESTAMP() + $max_run_time + 1`;

  source include/get_ndb_epochs.inc;

  while (`SELECT $ndb_latest_trans_epoch > $ndb_latest_handled_binlog_epoch AND UNIX_TIMESTAMP() <= $max_end_time`)
  {
    # Sleep a bit to avoid too heavy load.
    sleep 0.1;

    source include/get_ndb_epochs.inc;
  }

  if (`SELECT $ndb_latest_trans_epoch > $ndb_latest_handled_binlog_epoch`)
  {
    echo ERROR IN TEST: Timeout in waiting for epoch $ndb_latest_trans_epoch to be handled, last handled is $ndb_latest_handled_binlog_epoch;
    die ERROR IN TEST: Timeout in waiting for epoch to be handled;
  }
}
