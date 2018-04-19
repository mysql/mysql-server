# Use a variable to decide which cluster is 'Primary'
if (!$SWITCH_CLUSTER_ROLES)
{
  --connection master
}
if ($SWITCH_CLUSTER_ROLES)
{
  --connection slave
}
