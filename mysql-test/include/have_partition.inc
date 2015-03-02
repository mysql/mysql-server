if (`SELECT COUNT(*) = 0 FROM information_schema.plugins WHERE plugin_name = 'partition' AND plugin_status = 'ACTIVE'`)
{
  --skip The generic partitioning engine ha_partition is not active!
}
