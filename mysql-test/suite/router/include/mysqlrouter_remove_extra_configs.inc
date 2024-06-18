# ==== Purpose ====
#
# Remove additional Router configuration files.
#
# NOTE: It will remove configuration files stored using
# mysqlrouter_remember_file_for_removal.inc
#
# ==== Usage ====
#
# --source include/mysqlrouter_remove_extra_configs.inc
#

if ($router_config_to_remove1)
{
  --remove_file $router_config_to_remove1
}

if ($router_config_to_remove2)
{
  --remove_file $router_config_to_remove2
}

if ($router_config_to_remove3)
{
  --remove_file $router_config_to_remove3
}