# ==== Purpose ====
#
# Install and enable the semisync plugins.
#
# ==== Usage ====
#
# [--let $use_old_semisync_plugin_names = 1]
# --source include/rpl/install_semisync.inc
#
# Parameters:
#   $use_old_semisync_plugin_names
#     By default, uses the new-named libraries, semisync_source and
#     semisync_replica. If this parameter is set, uses the old-name
#     libraries, semisync_master and semisync_slave.

--let $include_filename= rpl/install_semisync.inc
--source include/begin_include_file.inc

--connection master
--source include/rpl/install_semisync_source.inc

--connection slave
--source include/rpl/install_semisync_replica.inc

--source include/end_include_file.inc
