include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
include/install_semisync.inc
CREATE TABLE t1 (i INT);
DROP TABLE t1;
include/rpl_sync.inc
include/assert_grep.inc [Check that there is no 'Read semi-sync reply magic number error' in error log.]
include/uninstall_semisync.inc
include/rpl_end.inc
