#
# An auxaliary include file called by rpl_semi_sync_ack_thread.test
#
--let $rpl_connection_name= server_1
--source include/connection.inc
--let $yes_tx= query_get_value(SHOW STATUS LIKE 'Rpl_semi_sync_source_yes_tx', Value, 1)

INSERT INTO t1 VALUES(1);
INSERT INTO t1 VALUES(2);
INSERT INTO t1 VALUES(3);
TRUNCATE t1;

# Verify ack thread receives acknowledgements correctly
--let $assert_text= rpl_semi_sync_source_yes_tx increases 4
--disable_warnings
--let $assert_cond= VARIABLE_VALUE = $yes_tx+4 FROM performance_schema.global_status WHERE VARIABLE_NAME = "rpl_semi_sync_source_yes_tx"
--enable_warnings
--source include/assert.inc

--source include/rpl/sync.inc
