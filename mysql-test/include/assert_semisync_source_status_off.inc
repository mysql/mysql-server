#
# An auxaliary file for asserting semisync master on
#
--let $semisync_status= query_get_value(SHOW STATUS LIKE 'rpl_semi_sync_source_status', Value, 1)
--let $assert_text= Semisync master is off
--let $assert_cond= "$semisync_status" = "OFF"
--source include/assert.inc
