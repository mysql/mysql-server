#
# Sets up chained replication for 3 hosts:
#  
#   mysqld_a -> mysqld_b -> mysqld_c

connect (mysqld_a,127.0.0.1,root,,test,$MASTER_MYPORT,);
connect (mysqld_b,127.0.0.1,root,,test,$SLAVE_MYPORT,);
connect (mysqld_c,127.0.0.1,root,,test,$SLAVE_MYPORT1,);

### CLEAN UP INSTANCES

-- let $i= 3

while($i)
{
  if($i == 3)
  {
    -- connection mysqld_a
  }

  if($i == 2)
  {
    -- connection mysqld_b
  }

  if($i == 1)
  {
    -- connection mysqld_c
  }

  -- source include/have_log_bin.inc

  -- disable_warnings
  -- disable_query_log
  -- source include/stop_slave.inc
  DROP TABLE IF EXISTS t1,t2,t3,t4,t5,t6,t7,t8,t9;
  RESET MASTER;
  RESET SLAVE;
  -- enable_warnings
  -- enable_query_log

  -- dec $i
}

### SETTING UP CHAIN

# mysql_a
-- connection mysqld_a
-- let $binlog_file_m= query_get_value(SHOW MASTER STATUS, File, 1)

# mysqld_b
-- connection mysqld_b
-- replace_result $MASTER_MYPORT MASTER_A_PORT $binlog_file_m MASTER_A_LOG_FILE
-- eval CHANGE MASTER TO master_host='127.0.0.1',master_port=$MASTER_MYPORT,master_user='root',MASTER_LOG_FILE='$binlog_file_m'
-- let $binlog_file= query_get_value(SHOW MASTER STATUS, File, 1)

# mysqld_c
-- connection mysqld_c
-- replace_result $SLAVE_MYPORT MASTER_B_PORT $binlog_file MASTER_B_LOG_FILE
-- eval CHANGE MASTER TO master_host='127.0.0.1',master_port=$SLAVE_MYPORT,master_user='root',MASTER_LOG_FILE='$binlog_file'
-- let $binlog_file= query_get_value(SHOW MASTER STATUS, File, 1)


### ASSERT THAT ALL SERVERS Ids ARE DIFFERENT

--connection mysqld_a
SHOW VARIABLES LIKE 'server_id';
--connection mysqld_b
SHOW VARIABLES LIKE 'server_id';
--connection mysqld_c
SHOW VARIABLES LIKE 'server_id';

### START SLAVES
-- connection mysqld_b
-- source include/start_slave.inc

-- connection mysqld_c
-- source include/start_slave.inc

-- connection mysqld_a
