# Use a variable to decide which cluster is 'Secondary'
if (!$SWITCH_CLUSTER_ROLES)
{
  --connection slave
}
if ($SWITCH_CLUSTER_ROLES)
{
  --connection master
}