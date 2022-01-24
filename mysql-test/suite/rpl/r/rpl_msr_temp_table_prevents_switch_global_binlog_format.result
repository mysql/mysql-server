#
# Start Multi-source replication, set up masters server_1 and server_3
# with server_2 being a slave.
#
include/rpl_init.inc [topology=1->2,3->2]
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection server_2]
include/stop_slave_sql.inc [FOR CHANNEL 'channel_1']
include/stop_slave_sql.inc [FOR CHANNEL 'channel_3']
#
# Verify that SET @@global.binlog_format is allowed if none
# of all replication channels have open temporary table(s).
#
SET @@global.binlog_format=ROW;
#
# Set binlog format to statement for replicating
# CREATE TEMPORARY TABLE temp1 on server_1
#
SET @@global.binlog_format=STATEMENT;
#
# Start slave applier on channel_1, so that
# SET @@global.binlog_format=STATEMENT takes
# effect on channel_1.
#
include/start_slave_sql.inc [FOR CHANNEL 'channel_1']
connection_server_1
#
# Replicate the 'CREATE TEMPORARY TABLE temp1' to  slave, so that
# the slave channel_1 has an open temporary table.
#
CREATE TEMPORARY TABLE temp1(a INT);
include/sync_slave_sql_with_master.inc [FOR CHANNEL 'channel_1']
[connection server_2]
#
# On a running slave, verify that SET @@global.binlog_format is
# disallowed if any replication channel has open temporary table(s).
#
SET @@global.binlog_format=ROW;
ERROR HY000: Changing @@global.binlog_format or @@persist.binlog_format is disallowed when any replication channel has open temporary table(s). You could wait until Replica_open_temp_tables = 0 and try again
#
# On a running slave, verify that SET @@persist.binlog_format is
# disallowed if any replication channel has open temporary table(s).
#
SET @@persist.binlog_format=ROW;
ERROR HY000: Changing @@global.binlog_format or @@persist.binlog_format is disallowed when any replication channel has open temporary table(s). You could wait until Replica_open_temp_tables = 0 and try again
include/stop_slave.inc [FOR CHANNEL 'channel_1']
Warnings:
Warning	3022	This operation may not be safe when the slave has temporary tables. The tables will be kept open until the server restarts or until the tables are deleted by any replicated DROP statement. Suggest to wait until replica_open_temp_tables = 0.
#
# On a stopped slave, verify that SET @@global.binlog_format is
# disallowed if any replication channel has open temporary table(s).
#
SET @@global.binlog_format=ROW;
ERROR HY000: Changing @@global.binlog_format or @@persist.binlog_format is disallowed when any replication channel has open temporary table(s). You could wait until Replica_open_temp_tables = 0 and try again
#
# On a stopped slave, verify that SET @@persist.binlog_format is
# disallowed if any replication channel has open temporary table(s).
#
SET @@persist.binlog_format=ROW;
ERROR HY000: Changing @@global.binlog_format or @@persist.binlog_format is disallowed when any replication channel has open temporary table(s). You could wait until Replica_open_temp_tables = 0 and try again
include/start_slave.inc [FOR CHANNEL 'channel_1']
connection_server_1
DROP TEMPORARY TABLE temp1;
include/sync_slave_sql_with_master.inc [FOR CHANNEL 'channel_1']
include/rpl_end.inc
RESET REPLICA ALL FOR CHANNEL 'channel_1';
RESET REPLICA ALL FOR CHANNEL 'channel_3';
