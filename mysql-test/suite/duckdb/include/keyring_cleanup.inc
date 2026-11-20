if ($duckdb_keyring_rpl)
{
  --connect (duckdb_keyring_cleanup,localhost,root,,test,$SLAVE_MYPORT,$SLAVE_MYSOCK)
  --let CURRENT_DATADIR= `SELECT @@datadir`
  --let KEYRING_FILE_PATH= $MYSQLTEST_VARDIR/keyring_duckdb_replica
  --source suite/component_keyring_file/inc/teardown_component.inc
  --disconnect duckdb_keyring_cleanup
  --connection default
  connect;
}
--let CURRENT_DATADIR= `SELECT @@datadir`
--let KEYRING_FILE_PATH= $MYSQLTEST_VARDIR/keyring_duckdb_source
--source suite/component_keyring_file/inc/teardown_component.inc
--enable_query_log
