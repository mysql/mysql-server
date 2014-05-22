################################################################################
# Build connections to master server
# Stress test that execute massive queries on every connection
#
# ################################################################################
--connection master
--disable_query_log
--disable_result_log
--let $n= $connections
--let $i= 1
while ($i < $n)
{
  --eval CREATE TABLE t$i(c1 INT) ENGINE= $engine;
  --inc $i
}
--enable_result_log
--enable_query_log

--source include/sync_slave_sql_with_master.inc

--echo # Build connections to master server
--disable_query_log
--disable_result_log
--let $i= 1
while ($i < $n)
{
  --connect(con$i,127.0.0.1,root,,test,$MASTER_MYPORT,)
  --inc $i
}
--let $i= 1
while ($i < $connections)
{
  --connection con$i
  set session debug="+d,simulate_binlog_flush_error";
  --inc $i
}
--enable_result_log
--enable_query_log

--echo # Stress test that execute massive queries on every connection
--let $m= $loops
--let $j= 1
--disable_query_log
--disable_result_log
while ($j < $m)
{
  # Execute INSERT statement on these connections in parallel
  --let $i= 1
  while ($i < $n)
  {
    --connection con$i
    --send
    --eval INSERT INTO t$i VALUES(2)
    --inc $i
  }
  # reap on every connection
  --let $i= 1
  while ($i < $n)
  {
    --connection con$i
    --reap
    --inc $i
  }
  --inc $j
}

--let $i= 1
while ($i < $connections)
{
  --connection con$i
  set session debug="-d,simulate_binlog_flush_error";
  --inc $i
}
--enable_result_log
--enable_query_log

--let $slave_timeout= 3600
if ($enable_diff_table)
{
--source include/sync_slave_sql_with_master.inc
  --echo # Test if the results are consistent on master and slave
  --let $i= 1
  while ($i < $n)
  {
    --let $diff_tables= master:t$i, slave:t$i
    --source include/diff_tables.inc
    --inc $i
  }
}

# clean up
--connection master
--disable_query_log
--disable_result_log
# disconnect connections
--let $i= 1
while ($i < $n)
{
  --disconnect con$i
  --inc $i
}
# drop tables
--let $i= 1
while ($i < $n)
{
  --eval DROP TABLE t$i
  --inc $i
}
--enable_result_log
--enable_query_log
--source include/sync_slave_sql_with_master.inc
