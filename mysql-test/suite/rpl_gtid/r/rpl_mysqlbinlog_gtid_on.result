include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
include/stop_slave.inc
CREATE TABLE t1 (a INT AUTO_INCREMENT PRIMARY KEY) engine=Innodb;
SET @@SESSION.AUTOCOMMIT= ON;
INSERT INTO t1 VALUES (NULL);
BEGIN;
INSERT INTO t1 VALUES (NULL);
INSERT INTO t1 VALUES (NULL);
COMMIT;
==== FULL FILE ====
include/diff_tables.inc [master:t1, slave:t1]
==== --include-gtids ====
INSERT INTO t1 VALUES (NULL) /* 4 */;
INSERT INTO t1 VALUES (NULL);
INSERT INTO t1 VALUES (NULL);
include/diff_tables.inc [master:t1, slave:t1]
==== --skip-gtids + --exclude-gtids  ====
INSERT INTO t1 VALUES (NULL) /* 7 */;
INSERT INTO t1 VALUES (NULL);
include/diff_tables.inc [master:t1, slave:t1]
==== --exclude-gtids ====
INSERT INTO t1 VALUES (NULL) /* 9 */;
include/diff_tables.inc [master:t1, slave:t1]
==== --skip-gtids ====
FLUSH LOGS;
INSERT INTO t1 VALUES (NULL) /* 10 */;
include/diff_tables.inc [master:t1, slave:t1]
==== --read-from-remote-source ====
FLUSH LOGS;
==== --read-from-remote-source with --exclude-gtids ====
==== --read-from-remote-source with --include-gtids ====
==== Error cases ====
ERROR: The option read-from-remote-server cannot be used when read-from-remote-source is defined and is not equal to BINLOG-DUMP-NON-GTIDS
ERROR: The --raw flag requires one of --read-from-remote-source or --read-from-remote-server
ERROR: You cannot use both of --exclude-gtids and --raw together with one of --read-from-remote-server or --read-from-remote-source=BINLOG-DUMP-NON-GTID.
ERROR: You cannot use both of --exclude-gtids and --raw together with one of --read-from-remote-server or --read-from-remote-source=BINLOG-DUMP-NON-GTID.
ERROR: You cannot use --include-gtids and --raw together.
FLUSH LOGS;
DROP TABLE t1;
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION=0;
