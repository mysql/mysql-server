#
# An auxaliary include file called by rpl_semi_sync_ack_thread.test
#
--let $rpl_connection_name= server_1
--source include/rpl_connection.inc
--let $yes_tx= query_get_value(SHOW STATUS LIKE 'Rpl_semi_sync_master_yes_tx', Value, 1)

INSERT INTO t1 VALUES(1);
INSERT INTO t1 VALUES(2);
INSERT INTO t1 VALUES(3);
TRUNCATE t1;

# Running with show_compatibility_56=ON until WL#9001 is pushed.
select @@global.show_compatibility_56 into @show_compatibility_56_save;
SET @@global.show_compatibility_56=ON;

# Verify ack thread receives acknowledgements correctly
--let $assert_text= rpl_semi_sync_master_yes_tx increases 4
--disable_warnings
--let $assert_cond= VARIABLE_VALUE = $yes_tx+4 FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME = "rpl_semi_sync_master_yes_tx"
--enable_warnings
--source include/assert.inc
# Restoring the value of show_compatibility_56.
set @@global.show_compatibility_56=@show_compatibility_56_save;

--source include/rpl_sync.inc
