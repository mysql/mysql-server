#############################################################
# Author: Serge Kozlov <Serge.Kozlov@Sun.COM>
# Date:   02/19/2009
# Purpose: Testing heartbeat for schema
# 1 master and 2 slaves
#############################################################
--let $rpl_topology= 1->2,1->3
--source include/rpl_init.inc

--let $rpl_connection_name= master
--let $rpl_server_number= 1
--source include/rpl_connect.inc

--let $rpl_connection_name= slave_1
--let $rpl_server_number= 2
--source include/rpl_connect.inc

--let $rpl_connection_name= slave_2
--let $rpl_server_number= 3
--source include/rpl_connect.inc

#
# Set different heartbeat periods for slaves
#
--connection slave_1
--source include/stop_slave.inc
CHANGE REPLICATION SOURCE TO SOURCE_HEARTBEAT_PERIOD = 0.1;
--source include/start_slave.inc
--connection slave_2
--source include/stop_slave.inc
--replace_column 2 ####
CHANGE REPLICATION SOURCE TO SOURCE_HEARTBEAT_PERIOD = 1;
--source include/start_slave.inc

#
# Testing heartbeat for one master and two slaves
#

# Check that heartbeat events sent to both slaves with correct periods
--connection slave_1

# As per WL7817, status variables are moved to PS replication tables.
# Replacing slave_received_heartbeats with count_received_heartbeats.

let $status_col= count_received_heartbeats;
let $table=replication_connection_status;
let $status_col_value= query_get_value(select $status_col from performance_schema.$table, $status_col, 1);
let $status_col_comparsion= >;
--source include/wait_for_rpl_pfs_status.inc
--echo Slave has received heartbeat event
--connection slave_2

# As per WL7817, status variables are moved to PS replication tables.
# Replacing slave_received_heartbeats with count_received_heartbeats.

let $status_col= count_received_heartbeats;
let $table=replication_connection_status;
let $status_col_value= query_get_value(select count_received_heartbeats from
performance_schema.replication_connection_status, count_received_heartbeats, 1);
let $status_col_comparsion= >;
--source include/wait_for_rpl_pfs_status.inc

let $table=replication_connection_status;

--let $assert_cond= [slave_1:select count_received_heartbeats from performance_schema.replication_connection_status, count_received_heartbeats, 1]> [slave_2:select count_received_heartbeats from performance_schema.replication_connection_status, count_received_heartbeats, 1]

--let $assert_text= slave_1 should have received more heartbeats than slave_2
--source include/assert.inc
--echo

# Create topology master->slave_2->slave_1 and check that slave_1
# receives heartbeat while slave_2 gets data.

# slave_2 was started w/o --log-replica-updates because slave_2 should
# not send data from master to slave_1

--source include/rpl_stop_slaves.inc
--let $rpl_topology= 1->3->2
--source include/rpl_change_topology.inc
--source include/rpl_start_slaves.inc
--connection slave_1
--source include/stop_slave.inc
--replace_column 2 ####
CHANGE REPLICATION SOURCE TO SOURCE_HEARTBEAT_PERIOD=0.1;
--source include/start_slave.inc

# Check heartbeat for new replication channel slave_2->slave

# As per WL7817, status variables are moved to PS replication tables.
# Replacing slave_received_heartbeats with count_received_heartbeats.

let $status_col= count_received_heartbeats;
let $status_col_value= query_get_value(select count_received_heartbeats from performance_schema.replication_connection_status, count_received_heartbeats, 1);

let $status_col_comparsion= >;
--source include/wait_for_rpl_pfs_status.inc
--echo slave_1 has received heartbeat event
--connection master
--echo [on master]
CREATE TABLE t1 (a INT PRIMARY KEY, b VARCHAR(10), c LONGTEXT);
INSERT INTO t1 VALUES (1, 'on master', '');
SHOW TABLES;
--echo [on slave_2]
--let $sync_slave_connection= slave_2
--source include/sync_slave_sql_with_master.inc
SHOW TABLES;
let $slave_2_pos_before= query_get_value(SHOW SLAVE STATUS, Read_Master_Log_Pos, 1);
--echo [on slave_1]
--let $sync_slave_connection= slave_1
--source include/sync_slave_sql_with_master.inc
SHOW TABLES;
--connection master
--echo [on master]
--echo creating updates on master and send to slave_2 during 5 second
# Generate events on master and send to slave_2 during 5 second
let $i= 1;
let $j= 1;
let $k= 1;
--disable_query_log
while ($i) {
  eval SET @c_text=REPEAT('1234567890', $j);
  eval UPDATE t1 SET a=$j, c=@c_text;
  --connection slave_2
  let $slave_2_pos= query_get_value(SHOW SLAVE STATUS, Read_Master_Log_Pos, 1);  
  if (`SELECT ($k*($slave_2_pos - $slave_2_pos_before)) > 0`) {
    --connection slave_1
    let $slave_1_rcvd_heartbeats_before= query_get_value(SELECT count_received_heartbeats from performance_schema.replication_connection_status,count_received_heartbeats, 1);

    let $k= 0;
    let $time_before = `SELECT NOW()`;
  }
  if (`SELECT ((1-$k)*TIMESTAMPDIFF(SECOND,'$time_before',NOW())) > 5`) {
    let $i= 0;
  }
  --connection master
  inc $j;
  sleep 0.1;
}
--enable_query_log
--connection slave_1
--echo [on slave_1]

--let $assert_cond= [select count_received_heartbeats from performance_schema.replication_connection_status, count_received_heartbeats, 1]> $slave_1_rcvd_heartbeats_before

--let $assert_text= slave_1 should have received heartbeats
--source include/assert.inc
--echo

#
# Clean up
#
--echo *** Clean up ***
--connection master
DROP TABLE t1;

--source include/rpl_end.inc
