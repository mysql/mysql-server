--let $include_filename= install_semisync.inc
--source include/begin_include_file.inc

--connection master
--source include/install_semisync_master.inc

--connection slave
--source include/install_semisync_slave.inc

--source include/end_include_file.inc
