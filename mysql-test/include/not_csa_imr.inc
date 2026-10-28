# ==== Purpose ====
#
# Skip the test when it runs with the Change Stream Applier (CSA) and the
# in-memory relay log (IMR). The test still runs with the Multi-Threaded
# Applier and with the CSA without IMR.
#
# ==== Usage ====
#
# # <one-line reason>
# --source include/not_csa_imr.inc
# --source include/rpl/init_source_replica.inc
#
# Source it before include/rpl/init_source_replica.inc or include/rpl/init.inc.
# It decides from mysqltest variables only and reads no replication state, so
# it works before the replication setup:
#
# - CSA: $RPL_CHANGE_STREAM_APPLIER is 2, or the test set $rpl_applier_version.
# - IMR: the IMR value is not OFF. Empty, DEFAULT and ON count as IMR, because
#   IMR is ON by default.
#   The IMR value is resolved exactly as in
#   include/rpl/impl/change_topology_helper.inc: $rpl_in_memory_relaylog when
#   it is not empty, else the RPL_IN_MEMORY_RELAYLOG environment variable. The
#   environment variable is read first: mysqltest variable names are
#   case-insensitive and mysqltest reads the environment only for a variable
#   that does not exist, so an explicitly empty
#   `--let $rpl_in_memory_relaylog=` hides the environment value.
#
# The IMR value is compared with a SELECT on the current connection, as in the
# topology helper. That SELECT runs only when the run is CSA.
#
# This file is listed in include/excludenoskip.list, so it also skips under
# --no-skip.

--let $_not_csa_imr_csa= 0
if ($RPL_CHANGE_STREAM_APPLIER == 2)
{
  --let $_not_csa_imr_csa= 1
}
if ($rpl_applier_version != '')
{
  --let $_not_csa_imr_csa= 1
}

if ($_not_csa_imr_csa)
{
  --let $_not_csa_imr_value= $RPL_IN_MEMORY_RELAYLOG
  if (`SELECT "$rpl_in_memory_relaylog" != ""`)
  {
    --let $_not_csa_imr_value= $rpl_in_memory_relaylog
  }
  if (`SELECT "$_not_csa_imr_value" != "OFF"`)
  {
    --skip This test does not support the in-memory relay log (IMR) under the Change Stream Applier.
  }
}
