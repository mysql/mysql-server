#
# Check if dynamic loading is supported
#
--require r/have_dynamic_loading.require
disable_query_log;
show variables like 'have_dynamic_loading';
enable_query_log;

#
# Check if the variable SEMISYNC_MASTER_PLUGIN is set
#
if (`select LENGTH('$SEMISYNC_MASTER_PLUGIN') = 0`)
{
  skip Need semisync plugins;
}
