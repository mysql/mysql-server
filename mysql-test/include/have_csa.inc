# ==== Purpose ====
#
# Verify that this test executes with the Change Stream Applier
#

if ( $RPL_CHANGE_STREAM_APPLIER != 2 )
{
  --skip This test is designed to run exclusively with the Change Stream Applier (CSA).
}
