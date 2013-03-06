# Re-Install semi_sync_master_plugin

UNINSTALL PLUGIN rpl_semi_sync_master;
--replace_regex /\.dll/.so/
eval INSTALL PLUGIN rpl_semi_sync_master SONAME '$SEMISYNC_MASTER_PLUGIN';
SET GLOBAL rpl_semi_sync_master_enabled = 1;

--echo # Test AFTER_COMMIT, it should work well
SET GLOBAL rpl_semi_sync_master_wait_point= AFTER_COMMIT;
--let $yes_tx= query_get_value(SHOW STATUS LIKE 'Rpl_semi_sync_master_yes_tx', Value, 1)

INSERT INTO t1 VALUES(1);
INSERT INTO t1 VALUES(2);

--source include/rpl_sync.inc

--let $assert_text= rpl_semi_sync_master_yes_tx increased 2
--let $assert_cond= VARIABLE_VALUE = $yes_tx+2 FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME = "rpl_semi_sync_master_yes_tx"
--source include/assert.inc

--echo # Test AFTER_SYNC, it should does not work
SET GLOBAL rpl_semi_sync_master_wait_point= AFTER_SYNC;
--let $yes_tx= query_get_value(SHOW STATUS LIKE 'Rpl_semi_sync_master_yes_tx', Value, 1)
--let $no_tx= query_get_value(SHOW STATUS LIKE 'Rpl_semi_sync_master_no_tx', Value, 1)

INSERT INTO t1 VALUES(2);
INSERT INTO t1 VALUES(3);

--source include/rpl_sync.inc

--let $assert_text= rpl_semi_sync_master_yes_tx increased 0
--let $assert_cond= VARIABLE_VALUE = $yes_tx FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME = "rpl_semi_sync_master_yes_tx"
--source include/assert.inc

--let $assert_text= rpl_semi_sync_master_yes_no increased 0
--let $assert_cond= VARIABLE_VALUE = $no_tx FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME = "rpl_semi_sync_master_no_tx"
--source include/assert.inc
