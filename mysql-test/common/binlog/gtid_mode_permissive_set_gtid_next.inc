# ==== Purpose ====
#
# Test that GTID_NEXT can be set to any value, when
# GTID_MODE=OFF_PERMISSIVE or ON_PERMISSIVE.
#
# This is used in binlog_gtid_mode_set_gtid_next.test

--let $gtid_next= AUTOMATIC
--source common/binlog/gtid_mode_set_gtid_next.inc

--let $gtid_next= ANONYMOUS
--source common/binlog/gtid_mode_set_gtid_next.inc

--let $gtid_next= ANONYMOUS
--source common/binlog/gtid_mode_set_gtid_next.inc

--let $gtid_next= $uuida:1
--source common/binlog/gtid_mode_set_gtid_next.inc

--let $gtid_next= $uuida:1
--source common/binlog/gtid_mode_set_gtid_next.inc

--let $gtid_next= $uuida:2
--source common/binlog/gtid_mode_set_gtid_next.inc

--let $gtid_next= AUTOMATIC
--source common/binlog/gtid_mode_set_gtid_next.inc

--let $gtid_next= $uuida:1
--source common/binlog/gtid_mode_set_gtid_next.inc

--let $gtid_next= ANONYMOUS
--source common/binlog/gtid_mode_set_gtid_next.inc

--let $gtid_next= AUTOMATIC
--source common/binlog/gtid_mode_set_gtid_next.inc
