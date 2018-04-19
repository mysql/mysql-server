include/install_replication_observers_example.inc
include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
#
# Set up test
#
CREATE TABLE t1 (c1 INT PRIMARY KEY) Engine=InnoDB;
#
# Case 1. This will cause the before_commit hook to fail. As a
# consequence, the whole transaction shall fail.
#
SET @debug_saved= @@GLOBAL.DEBUG;
SET @@GLOBAL.DEBUG= '+d,force_error_on_before_commit_listener';
SET SESSION sql_log_bin= 0;
call mtr.add_suppression("Run function 'before_commit' in plugin");
SET SESSION sql_log_bin= 1;
BEGIN;
INSERT t1 VALUES(1);
COMMIT;
ERROR HY000: Error on observer while running replication hook 'before_commit'.
INSERT t1 VALUES(2);
ERROR HY000: Error on observer while running replication hook 'before_commit'.
include/assert.inc [GTID_EXECUTED must remain the same]
include/assert.inc [Table must remain empty]
include/sync_slave_sql_with_master.inc
include/assert.inc [GTID_EXECUTED must remain the same]
include/assert.inc [Table must remain empty]
SET @@GLOBAL.DEBUG='';
#
# Case 2. This will force the Transaction Context to report an invalid
# transaction certification outcome,
#
SET @@GLOBAL.DEBUG= '+d,force_invalid_certification_outcome';
SET SESSION sql_log_bin= 0;
call mtr.add_suppression("Plugin replication_observers_example reported: 'Unable to update transaction context service on server, thread_id:.*");
SET SESSION sql_log_bin= 1;
BEGIN;
INSERT t1 VALUES(1);
COMMIT;
ERROR HY000: Error on observer while running replication hook 'before_commit'.
INSERT t1 VALUES(2);
ERROR HY000: Error on observer while running replication hook 'before_commit'.
include/assert.inc [GTID_EXECUTED must remain the same]
include/assert.inc [Table must remain empty]
include/sync_slave_sql_with_master.inc
include/assert.inc [GTID_EXECUTED must remain the same]
include/assert.inc [Table must remain empty]
SET @@GLOBAL.DEBUG='';
#
# Case 3. This will force the Transaction Context to report an negative
# transaction certification outcome,
#
SET @@GLOBAL.DEBUG= '+d,force_negative_certification_outcome';
BEGIN;
INSERT t1 VALUES(1);
COMMIT;
ERROR 40000: Plugin instructed the server to rollback the current transaction.
INSERT t1 VALUES(2);
ERROR 40000: Plugin instructed the server to rollback the current transaction.
include/assert.inc [GTID_EXECUTED must remain the same]
include/assert.inc [Table must remain empty]
include/sync_slave_sql_with_master.inc
include/assert.inc [GTID_EXECUTED must remain the same]
include/assert.inc [Table must remain empty]
SET @@GLOBAL.DEBUG='';
#
# Case 4. This will force the Transaction Context to report a valid
# outcome without generating a GTID.
#
SET @@GLOBAL.DEBUG= '+d,force_positive_certification_outcome_without_gtid';
BEGIN;
INSERT t1 VALUES(1);
COMMIT;
INSERT t1 VALUES(2);
SELECT * FROM t1;;
c1	1
c1	2
include/assert.inc [At this point we should have 2 successful operations]
include/sync_slave_sql_with_master.inc
include/assert.inc [At this point we should have 2 successful operations]
SET @@GLOBAL.DEBUG='';
#
# Case 5. This will force the Transaction Context to report a valid
# outcome generating a GTID
#
SET @@GLOBAL.DEBUG= '+d,force_positive_certification_outcome_with_gtid';
BEGIN;
INSERT t1 VALUES(3);
COMMIT;
INSERT t1 VALUES(4);
SELECT * FROM t1;;
c1	1
c1	2
c1	3
c1	4
include/assert.inc [GTID_EXECUTED must contain all GTIDs with the server id and with the fake id]
include/sync_slave_sql_with_master.inc
include/assert.inc [GTID_EXECUTED must contain all GTIDs with the server id and with the fake id]
#
# Clean up
#
SET @@GLOBAL.DEBUG= @debug_saved;
DROP TABLE t1;
include/rpl_end.inc
include/uninstall_replication_observers_example.inc
End of test
