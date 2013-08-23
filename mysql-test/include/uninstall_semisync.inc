#
# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
#
--let $include_filename= uninstall_semisync.inc
--source include/begin_include_file.inc

--disable_query_log
--connection slave
--source include/stop_slave_io.inc

# Uninstall rpl_semi_sync_slave first
--disable_warnings
UNINSTALL PLUGIN rpl_semi_sync_slave;

--connection master
UNINSTALL PLUGIN rpl_semi_sync_master;
--enable_warnings

--connection slave
--source include/start_slave_io.inc
--enable_query_log

--source include/end_include_file.inc
