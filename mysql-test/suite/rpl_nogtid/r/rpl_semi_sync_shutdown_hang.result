include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
include/install_semisync.inc
call mtr.add_suppression("Read semi-sync reply network error");
call mtr.add_suppression("SEMISYNC: Forced shutdown. Some updates might not be replicated.");
[connection master]
SET GLOBAL rpl_semi_sync_source_timeout = 10000000;
[connection slave]
include/stop_slave_io.inc
[connection master]
CREATE TABLE t(f INT) ENGINE=INNODB;;
[connection master1]
[connection master]
include/rpl_start_server.inc [server_number=1]
[connection slave]
include/start_slave_io.inc
[connection master]
DROP TABLE t;
include/sync_slave_sql_with_master.inc
include/uninstall_semisync.inc
include/rpl_end.inc
