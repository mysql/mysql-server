#
# Create XA transaction to conclude it with COMMIT or ROLLBACK.
# For either logging phase preceed it with supplied value of GTID_NEXT.
#
# param  $conclude_as            Commit or Rollback
# param  $gtid_phase1            GTID_NEXT value for the prepared round
# param  $gtid_phase2	         GTID_NEXT value for the conclusion round
# param  $conn_external [master] connection to return control too
#                                and randomly run conclusive query
# param  $xa_table      [t1]     Table to operate on in transaction
# param  $xa_error_simul         Provoke a failure to set GTID_NEXT correctly.
#                                When the first phase is not AUTOMATIC the 2nd
#                                must be preceeded with a proper GTID_NEXT.
#                                When the value is true, $disconnect_prepare is
#                                to be false.
#

--let $xid = 'xid_1'
--let $disconnect_prepare = 0
if (`SELECT concat('a','$conn_external') = 'a'`)
{
  --let $conn_external = master
}
if (`SELECT concat('a','$gtid_phase1') = 'a'`)
{
  --let $gtid_phase1 = automatic
}
if (`SELECT concat('a','$gtid_phase2') = 'a'`)
{
  --let $gtid_phase2 = automatic
}
if (`SELECT concat('a','$xa_table') = 'a'`)
{
  --let $xa_table = t1
}
if (`SELECT concat('a','$xa_error_simul') = 'a'`)
{
  --let $xa_error_simul = 0
}
if (!$xa_error_simul)
{
  --let $disconnect_prepare = `SELECT floor(rand()*10)%2`
}

--connection $conn_external

--connect (xa_conn, 127.0.0.1,root,,test,$MASTER_MYPORT,)
--let $conn_id = `SELECT connection_id()`
--replace_result $master_uuid SOURCE_UUID
--eval SET @@SESSION.GTID_NEXT = '$gtid_phase1'
--eval XA START   $xid
--disable_warnings
--eval INSERT INTO $xa_table VALUES(1)
--enable_warnings
--eval XA END     $xid
--eval XA PREPARE $xid

if ($disconnect_prepare)
{
  --disconnect xa_conn
  --connection $conn_external
  --let $wait_condition= SELECT count(*) = 0 FROM performance_schema.threads WHERE PROCESSLIST_ID = $conn_id and type = 'FOREGROUND'
  --source include/wait_condition.inc
}

if (`SELECT NOT $disconnect_prepare AND $xa_error_simul AND '$gtid_phase1' <> 'automatic'`)
{
  --replace_result $master_uuid SOURCE_UUID
  --error ER_GTID_NEXT_TYPE_UNDEFINED_GTID
  --eval XA $conclude_as $xid
}

--replace_result $master_uuid SOURCE_UUID
--eval SET @@SESSION.GTID_NEXT = '$gtid_phase2'
--eval XA $conclude_as $xid

if (!$disconnect_prepare)
{
  --disconnect xa_conn
}

--connection $conn_external
SET @@SESSION.GTID_NEXT = 'automatic';

