include/have_replication_observers_example_plugin.inc
include/install_replication_observers_example.inc

# 1. Establish 5 connections and create 2 tables
[connection con1]
CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
CREATE TABLE t2 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;

# 2. Lock a table so one query will block, this one is not counted
[connection con2]
LOCK TABLE t1 WRITE;

# 3. Leave a transaction open without commit (counted)
[connection con3]
INSERT INTO t1 VALUES (2);;

# 4. Leave a transaction stuck on commit
[connection con4]
BEGIN;
INSERT INTO t2 VALUES(1);

# 5. Execute a DML query. A hook will count the transactions
[connection con5]
SET @@GLOBAL.DEBUG='+d,group_replication_before_commit_hook_wait';
INSERT INTO t2 VALUES (3);;
[connection con1]
SET @@GLOBAL.DEBUG='-d,group_replication_before_commit_hook_wait';
#
# 6. Execute a query that on a server hook will count the number of
#    queries
SET @@GLOBAL.DEBUG= '+d,validate_replication_observers_plugin_counts_transactions';
INSERT INTO t2 VALUES (10);
#
# 7. Clean up
[connection con2]
UNLOCK TABLES;
[connection con3]
[connection con4]
COMMIT;
SET DEBUG_SYNC='now SIGNAL continue_commit';
[connection con5]
[connection con1]
DROP TABLE t1;
DROP TABLE t2;
SET @@GLOBAL.DEBUG= '-d,validate_replication_observers_plugin_counts_transactions';
include/uninstall_replication_observers_example.inc
