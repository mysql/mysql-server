--let $_agmo_gtid_mode= `SELECT @@GLOBAL.GTID_MODE`
if ($_agmo_gtid_mode != 'ON')
{
  --source include/rpl/debug/show_debug_info.inc
  --echo !!!ERROR IN TEST: GTID_MODE was $_agmo_gtid_mode, needs to be ON
  --die !!!ERROR IN TEST: GTID_MODE was not ON
}
