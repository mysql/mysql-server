# ==== Purpose ====
#
# Extract the epoch values from SHOW ENGINE NDB STATUS.
#
# ==== Usage ====
#
# --source include/get_ndb_epochs.inc
# --echo $ndb_latest_epoch
# --echo $ndb_latest_trans_epoch
# --echo $ndb_latest_received_binlog_epoch
# --echo $ndb_latest_handled_binlog_epoch
# --echo $ndb_latest_applied_binlog_epoch
#
# ==== Parameters ====
#
# $ndb_status_binlog_row
#   The row number of the output of SHOW ENGINE NDB STATUS
#   last was seen.
#   No need to set explictly, but used between sourcing this
#   include file several times, since it is highly probable
#   (but not certain) that same row number also applies next
#   time.

if (!$ndb_status_binlog_row)
{
  let $ndb_status_binlog_row = 1;
}

# Try two times, in case the row number of binlog state changes
# between two invocations of SHOW ENGINE NDB STATUS.
let $attempts= 2;
while ($attempts)
{
  let $q= query_get_value(SHOW ENGINE NDB STATUS, Status, $ndb_status_binlog_row);
  if ( `SELECT '$q' = 'No such row'` )
  {
    dec $attempts;
    if (!$attempts)
    {
      echo ERROR IN TEST: No binlog row found in SHOW ENGINE NDB STATUS;
      die ERROR IN TEST: No binlog row found in SHOW ENGINE NDB STATUS;
    }
    # row will be increased to 1 further down
    let $ndb_status_binlog_row = 0;
  }
  if (`SELECT INSTR('$q', 'latest_epoch=')`)
  {
    let $attempts = 0 ;
  }
  if ($attempts)
  {
    inc $ndb_status_binlog_row;
  }
}

let $ndb_latest_epoch= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING('$q', INSTR('$q', 'latest_epoch=')), ',', 1), '=', -1)`;
let $ndb_latest_trans_epoch= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING('$q', INSTR('$q', 'latest_trans_epoch=')), ',', 1), '=', -1)`;
let $ndb_latest_received_binlog_epoch= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING('$q', INSTR('$q', 'latest_received_binlog_epoch=')), ',', 1), '=', -1)`;
let $ndb_latest_handled_binlog_epoch= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING('$q', INSTR('$q', 'latest_handled_binlog_epoch=')), ',', 1), '=', -1)`;
let $ndb_latest_applied_binlog_epoch= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING('$q', INSTR('$q', 'latest_applied_binlog_epoch=')), ',', 1), '=', -1)`;
