#
# The test file is invoked from rpl.rpl_xa_survive_disconnect_mixed_engines
#
# The test file is orginized as three sections: setup, run and cleanup.
# The main logics is resided in the run section which generates
# three types of XA transaction: two kinds of mixed and one on non-transactional
# table.
#
# param $command          one of three of: 'setup', 'run' or 'cleanup'
# param $xa_terminate     how to conclude: 'XA COMMIT' or 'XA ROLLBACK'
# param $one_phase        'one_phase' can be opted with XA COMMIT above
# param $xa_prepare_opt   '1' or empty can be opted to test with and without XA PREPARE
# param $xid              arbitrary name for xa trx, defaults to 'xa_trx'
# Note '' is merely to underline, not a part of the value.
#

if ($command == setup)
{
  # Test randomizes the following variable's value:
  SET @@session.binlog_direct_non_transactional_updates := if(floor(rand()*10)%2,'ON','OFF');
  CREATE TABLE t (a INT) ENGINE=innodb;
  CREATE TABLE tm (a INT) ENGINE=myisam;
}
if (!$xid)
{
  --let $xid=xa_trx
}
if ($command == run)
{
  # Non transactional table goes first

  --eval XA START '$xid'
  --disable_warnings
  INSERT INTO tm VALUES (1);
  INSERT INTO t VALUES (1);
  --enable_warnings
  --eval XA END '$xid'
  if ($xa_prepare_opt)
  {
   --eval XA PREPARE '$xid'
  }
  --eval $xa_terminate '$xid' $one_phase

  # Transactional table goes first

  --eval XA START '$xid'
  --disable_warnings
  INSERT INTO t VALUES (2);
  INSERT INTO tm VALUES (2);
  --enable_warnings
  --eval XA END '$xid'
  if ($xa_prepare_opt)
  {
   --eval XA PREPARE '$xid'
  }
  --eval $xa_terminate '$xid' $one_phase

  # The pure non-transactional table

  --eval XA START '$xid'
  --disable_warnings
  INSERT INTO tm VALUES (3);
  --enable_warnings
  --eval XA END '$xid'
  if ($xa_prepare_opt)
  {
   --eval XA PREPARE '$xid'
  }
  --eval $xa_terminate '$xid' $one_phase
}

if ($command == cleanup)
{
  DROP TABLE t, tm;
}
