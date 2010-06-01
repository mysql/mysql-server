if (`SELECT HEX(@commands) = HEX('configure')`)
{
  --sync_slave_with_master
  SHOW CREATE TABLE mysql.rli_info;
  ALTER TABLE mysql.rli_info ENGINE= Innodb;
  SHOW CREATE TABLE mysql.rli_info;

  connection master;
  --source extra/rpl_tests/rpl_mixing_engines.inc

  --sync_slave_with_master
  SELECT * FROM mysql.rli_info;
}

if (`SELECT HEX(@commands) = HEX('clean')`)
{
  connection master;
  --source extra/rpl_tests/rpl_mixing_engines.inc
}

if (`SELECT HEX(@commands) != HEX('')`)
{
  if (`SELECT HEX(@failure) != HEX('')`)
  {
    connection slave;
    STOP SLAVE SQL_THREAD;
    source include/wait_for_slave_sql_to_stop.inc;
    --eval SET GLOBAL debug="$failure";
    --exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.2.expect
  }

  connection master;
  let $master_before= query_get_value(SHOW MASTER STATUS, Position, 1);
  --source extra/rpl_tests/rpl_mixing_engines.inc
  let $master_after= query_get_value(SHOW MASTER STATUS, Position, 1);

  if (`SELECT HEX(@failure) != HEX('')`)
  {
    connection slave;
    --error 0, 2013
    START SLAVE SQL_THREAD;
    --source include/wait_until_disconnected.inc
    --enable_reconnect
    --source include/wait_until_connected_again.inc
    connection slave;
    --let $slave_before= `SELECT rli_master_log_pos FROM mysql.rli_info`
    START SLAVE;
    source include/wait_for_slave_to_start.inc;
  }

  connection master;
  sync_slave_with_master;
  --let $slave_after= `SELECT rli_master_log_pos FROM mysql.rli_info`
  connection master;
  SHOW MASTER STATUS;
}

connection master;
