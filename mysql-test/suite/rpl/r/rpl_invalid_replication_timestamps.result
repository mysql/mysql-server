include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1);
include/sync_slave_sql_with_master.inc
[connection slave]
call mtr.add_suppression("Invalid replication timestamps: original commit timestamp is more recent*");
call mtr.add_suppression("The replication timestamps have returned to normal values.");
[connection master]
# Adding debug point 'rpl_invalid_gtid_timestamp' to @@GLOBAL.debug
UPDATE t1 SET a=2;
DELETE FROM t1 WHERE a=2;
include/sync_slave_sql_with_master.inc
[connection master]
# Removing debug point 'rpl_invalid_gtid_timestamp' from @@GLOBAL.debug
INSERT INTO t1 VALUES (1);
DROP TABLE t1;
include/sync_slave_sql_with_master.inc
include/assert_grep.inc [Invalid replication timestamps warning is present in log]
include/assert_grep.inc [Normalized replication timestamps warning is present in log]
[connection master]
include/rpl_end.inc
