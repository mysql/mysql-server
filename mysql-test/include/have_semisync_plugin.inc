#
# Check if server has support for loading plugins
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip Requires dynamic loading
}

#
# Check if the variable SEMISYNC_MASTER_SO is set
#
if (!$SEMISYNC_MASTER_SO)
{
  skip Need semisync plugins;
}

