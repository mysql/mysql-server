--let $include_filename= install_semisync.inc
--source include/begin_include_file.inc

--source include/not_embedded.inc
--source include/have_semisync_plugin.inc

--connection master

--disable_query_log
--let $value = query_get_value(show variables like 'rpl_semi_sync_master_enabled', Value, 1)
if ($value == No such row)
{
    SET sql_log_bin = 0;
    CALL mtr.add_suppression("Read semi-sync reply.*");
    eval INSTALL PLUGIN rpl_semi_sync_master SONAME '$SEMISYNC_MASTER_PLUGIN';
    SET GLOBAL rpl_semi_sync_master_enabled = 1;
    SET sql_log_bin = 1;
}
--enable_query_log

--connection slave
--source include/stop_slave_io.inc

--disable_query_log
--let $value= query_get_value(show variables like 'rpl_semi_sync_slave_enabled', Value, 1)
if ($value == No such row)
{
    SET sql_log_bin = 0;
    eval INSTALL PLUGIN rpl_semi_sync_slave SONAME '$SEMISYNC_SLAVE_PLUGIN';
    SET GLOBAL rpl_semi_sync_slave_enabled = 1;
    SET sql_log_bin = 1;
}
--source include/start_slave_io.inc
--enable_query_log

--source include/end_include_file.inc
