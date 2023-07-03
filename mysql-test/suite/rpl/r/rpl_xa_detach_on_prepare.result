#
# 0. Create asynchronous replication topology between server 1 and 2
#.
include/rpl_init.inc [topology=1->2->1]
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
#
# 1. On server1 enable option `xa_detach_on_prepare` for current session.
#.
[connection server_1]
SET @@session.xa_detach_on_prepare=1;
include/assert.inc [The value of xa_detach_on_prepare should be 1]
#
# 2. On server1 create a table.
#
#
[connection server_1]
CREATE TABLE t1 (a INT) ENGINE = InnoDB;
include/rpl_sync.inc
#
# 3. On server1 create and prepare an XA transaction which inserts a value on
#    the table.
[connection server_1]
FLUSH LOGS;
XA START 'trx1';
INSERT INTO t1 VALUES (1);
XA END 'trx1';
XA PREPARE 'trx1';
include/rpl_sync.inc
#
# 4. On server2 commit the XA transaction.
#
[connection server_2]
XA COMMIT 'trx1';
XA RECOVER;
formatID	gtrid_length	bqual_length	data
include/rpl_sync.inc
#
# 5. Verify that insert was replicated to server1.
#
[connection server_1]
#
# 6. Clean up
#
[connection server_1]
SET @@session.xa_detach_on_prepare=0;
[connection server_1]
DROP TABLE t1;
include/rpl_sync.inc
include/rpl_end.inc
