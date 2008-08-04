#############################################################
#
# Author:  Serge Kozlov <skozlov@mysql.com>
# Date:    03/11/2008
# Purpose: Set up circular replication based on schema 
# A->B->C->D->A
#
# Notes:
# 1. --slave-num=3 must be added to *-master.opt file
# 2. Even the test uses new names for servers but file names 
#    of log files are still old:
#    master_a -> master.[log|err]
#    master_b -> slave.[log|err]
#    master_c -> slave1.[log|err]
#    master_d -> slave2.[log|err]
#
#############################################################
--source include/master-slave.inc

#
# Set up circular ring by schema A->B->C->D->A 
#

--connection slave
STOP SLAVE;
RESET SLAVE;

# master a
--connection master
--disconnect master
connect (master_a,127.0.0.1,root,,test,$MASTER_MYPORT,);
RESET MASTER;
--disable_warnings
STOP SLAVE;
--enable_warnings
RESET SLAVE;
SET auto_increment_increment = 4;
SET auto_increment_offset = 1;
let $_binlog_file= query_get_value(SHOW MASTER STATUS, File, 1);

# master b
--connection slave
--disconnect slave
connect (master_b,127.0.0.1,root,,test,$SLAVE_MYPORT,);
RESET MASTER;
RESET SLAVE;
--replace_result $MASTER_MYPORT MASTER_A_PORT $_binlog_file MASTER_A_LOG_FILE
--eval CHANGE MASTER TO master_host='127.0.0.1',master_port=$MASTER_MYPORT,master_user='root',MASTER_LOG_FILE='$_binlog_file'
SET auto_increment_increment = 4;
SET auto_increment_offset = 2;
let $_binlog_file= query_get_value(SHOW MASTER STATUS, File, 1);

# master c
--connection slave1
--disconnect slave1
connect (master_c,127.0.0.1,root,,test,$SLAVE_MYPORT1,);
RESET MASTER;
--disable_warnings
STOP SLAVE;
--enable_warnings
RESET SLAVE;
--replace_result $SLAVE_MYPORT MASTER_B_PORT $_binlog_file MASTER_B_LOG_FILE
--eval CHANGE MASTER TO master_host='127.0.0.1',master_port=$SLAVE_MYPORT,master_user='root',MASTER_LOG_FILE='$_binlog_file'
SET auto_increment_increment = 4;
SET auto_increment_offset = 3;
let $_binlog_file= query_get_value(SHOW MASTER STATUS, File, 1);

# master d 
connect (master_d,127.0.0.1,root,,test,$SLAVE_MYPORT2,);
RESET MASTER;
--disable_warnings
STOP SLAVE;
--enable_warnings
RESET SLAVE;
--replace_result $SLAVE_MYPORT1 MASTER_C_PORT $_binlog_file MASTER_C_LOG_FILE
--eval CHANGE MASTER TO master_host='127.0.0.1',master_port=$SLAVE_MYPORT1,master_user='root',MASTER_LOG_FILE='$_binlog_file'
SET auto_increment_increment = 4;
SET auto_increment_offset = 4;
let $_binlog_file= query_get_value(SHOW MASTER STATUS, File, 1);

# master a
--connection master_a
--replace_result $SLAVE_MYPORT2 MASTER_D_PORT $_binlog_file MASTER_D_LOG_FILE
--eval CHANGE MASTER TO master_host='127.0.0.1',master_port=$SLAVE_MYPORT2,master_user='root',MASTER_LOG_FILE='$_binlog_file'



# Check server_ids: they should be different
--connection master_a
let $_id_a= query_get_value(SHOW VARIABLES LIKE 'server_id', Value, 1);
SHOW VARIABLES LIKE 'auto_increment_%';
--connection master_b
let $_id_b= query_get_value(SHOW VARIABLES LIKE 'server_id', Value, 1);
SHOW VARIABLES LIKE 'auto_increment_%';
--connection master_c
let $_id_c= query_get_value(SHOW VARIABLES LIKE 'server_id', Value, 1);
SHOW VARIABLES LIKE 'auto_increment_%';
--connection master_d
let $_id_d= query_get_value(SHOW VARIABLES LIKE 'server_id', Value, 1);
SHOW VARIABLES LIKE 'auto_increment_%';
--connection master_a
let $_compared_ids= (($_id_a <> $_id_b) AND ($_id_a <> $_id_c) AND ($_id_a <> $_id_d) AND ($_id_b <> $_id_c) AND ($_id_b <> $_id_d) AND ($_id_c <> $_id_d)) AS a;
let $_compared_ids_result= query_get_value(SELECT $_compared_ids, a, 1);
--echo $_compared_ids_result

# Start ring
--connection master_a
connect(slave,127.0.0.1,root,,test,$MASTER_MYPORT);
START SLAVE;
--source include/wait_for_slave_to_start.inc
--disconnect slave

--connection master_b
connect(slave,127.0.0.1,root,,test,$SLAVE_MYPORT1);
START SLAVE;
--source include/wait_for_slave_to_start.inc
--disconnect slave

--connection master_c
connect(slave,127.0.0.1,root,,test,$SLAVE_MYPORT);
START SLAVE;
--source include/wait_for_slave_to_start.inc
--disconnect slave

--connection master_d
connect(slave,127.0.0.1,root,,test,$SLAVE_MYPORT2);
START SLAVE;
--source include/wait_for_slave_to_start.inc
--disconnect slave

