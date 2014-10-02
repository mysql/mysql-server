if (!$SWITCH_CLUSTER_ROLES)
{
  --connection slave
  --sync_slave_with_master master
  --connection master
}
if ($SWITCH_CLUSTER_ROLES)
{
  --connection master
  --sync_slave_with_master slave
  --connection slave
}