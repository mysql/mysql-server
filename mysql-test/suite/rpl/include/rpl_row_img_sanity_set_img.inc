-- let $old_con= $CURRENT_CONNECTION

if (`SELECT $iteration = 1`)
{
  -- connection master
  -- echo ON MASTER
  # global so that flush tables restarts the 
  # insert dealyed thread
  SET GLOBAL binlog_row_image='FULL';
  SET SESSION binlog_row_image='FULL';
  SHOW VARIABLES LIKE 'binlog_row_image';
  FLUSH TABLES;

  -- connection slave
  -- echo ON SLAVE
  # global so that SQL thread also gets the change
  SET GLOBAL binlog_row_image='FULL';
  SET SESSION binlog_row_image='FULL';
  SHOW VARIABLES LIKE 'binlog_row_image';
}

if (`SELECT $iteration = 2`)
{
  -- connection master
  -- echo ON MASTER
  # global so that flush tables restarts the 
  # insert dealyed thread
  SET GLOBAL binlog_row_image='NOBLOB';
  SET SESSION binlog_row_image='NOBLOB';
  SHOW VARIABLES LIKE 'binlog_row_image';
  FLUSH TABLES;

  -- connection slave
  -- echo ON SLAVE
  # global so that SQL thread also gets the change
  SET GLOBAL binlog_row_image='NOBLOB';
  SET SESSION binlog_row_image='NOBLOB';
  SHOW VARIABLES LIKE 'binlog_row_image';


}

if (`SELECT $iteration = 3`)
{

  -- connection master
  -- echo ON MASTER

  # global so that flush tables restarts the 
  # insert dealyed thread
  SET GLOBAL binlog_row_image='MINIMAL';
  SET SESSION binlog_row_image='MINIMAL';
  SHOW VARIABLES LIKE 'binlog_row_image';
  FLUSH TABLES;

  -- connection master
  -- echo ON SLAVE

  # global so that SQL thread also gets the change
  SET GLOBAL binlog_row_image='MINIMAL';
  SET SESSION binlog_row_image='MINIMAL';
  SHOW VARIABLES LIKE 'binlog_row_image';
}

-- connection slave
-- source include/stop_slave.inc
-- source include/start_slave.inc

-- connection $old_con
