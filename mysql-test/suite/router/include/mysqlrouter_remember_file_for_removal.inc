# ==== Purpose ====
#
# Remember files that should be removed at end of the test-case.
#
# Note: The removal occurs in `mysqlrouter_remove_extra_configs.inc`
#
# ==== Usage ====
#
# --let $router_config_to_remove = FILE
#
# --source include/mysqlrouter_remember_file_for_removal.inc
#

if (!$router_config_to_remove)
{
    --die The variable must be set `router_config_to_remove`.
}

if ($router_config_to_remove2)
{
  if (!$router_config_to_remove3)
  {
    let $router_config_to_remove3=$router_config_to_remove;
  }
}

if ($router_config_to_remove1)
{
  if (!$router_config_to_remove2)
  {
    let $router_config_to_remove2=$router_config_to_remove;
  }
}

if (!$router_config_to_remove1)
{
  let $router_config_to_remove1=$router_config_to_remove;
}