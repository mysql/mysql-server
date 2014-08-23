--let $include_filename= rpl_gcs_plugin_installed.inc
--source include/begin_include_file.inc

--let $gcs_installed=`select count(*) from information_schema.plugins where PLUGIN_NAME like 'gcs_replication_plugin'`

--let $include_filename= rpl_gcs_plugin_installed.inc
--source include/end_include_file.inc
