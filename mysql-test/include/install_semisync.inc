--let $include_filename= install_semisync.inc
--source include/begin_include_file.inc

--connection master
--source include/install_semisync_source.inc

--connection slave
--source include/install_semisync_replica.inc

--source include/end_include_file.inc
