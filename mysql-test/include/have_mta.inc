# ==== Purpose ====
#
# Verify that this test executes with the Multi-Threaded Applier
#

if ( $rpl_applier_version != '' )
{
  --skip This test is designed to run exclusively with the Multi-Threaded Applier (MTA).
}

if ( $RPL_CHANGE_STREAM_APPLIER != '' )
{
  --skip This test is designed to run exclusively with the Multi-Threaded Applier (MTA).
}
