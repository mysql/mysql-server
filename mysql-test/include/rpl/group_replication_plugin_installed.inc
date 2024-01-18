--let $include_filename= rpl/group_replication_plugin_installed.inc
--source include/begin_include_file.inc

--let $group_replication_plugin_installed=`select count(*) from information_schema.plugins where PLUGIN_NAME like 'group_replication'`

--let $include_filename= rpl/group_replication_plugin_installed.inc
--source include/end_include_file.inc
