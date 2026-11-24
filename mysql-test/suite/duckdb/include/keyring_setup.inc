--source include/have_component_keyring_file.inc
--let KEYRING_FILE_PATH_OVERRIDE= $MYSQLTEST_VARDIR/keyring_duckdb_source
--source suite/component_keyring_file/inc/setup_component.inc

if ($duckdb_keyring_rpl)
{
  --connect (duckdb_keyring_bootstrap,localhost,root,,test,$SLAVE_MYPORT,$SLAVE_MYSOCK)
  --let KEYRING_FILE_PATH_OVERRIDE= $MYSQLTEST_VARDIR/keyring_duckdb_replica
  --source suite/component_keyring_file/inc/setup_component.inc
  --disconnect duckdb_keyring_bootstrap
  --connection default
}
--enable_query_log
