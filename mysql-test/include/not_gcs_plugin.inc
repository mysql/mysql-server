#  This inc file checks for the gcs plugin in the information_schema.plugins
#  and if the inc file is not available the particular test should be
#  skipped.
#
#  Used to suppress the test run in the GCS_REPLICATION mode.
#
--let $include_silent= 1
--source include/rpl_gcs_plugin_installed.inc
if ($gcs_installed)
{
  --skip Test doesn't support GCS replication plugin
}
--let $include_silent= 0
