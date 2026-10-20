--let $assert_text = Expect OFFLINE_MODE = OFF
--let $assert_variable_name = global.offline_mode
--let $assert_variable_value = 0
--source include/assert_variable.inc

--let $assert_text = Expect SUPER_READ_ONLY = OFF
--let $assert_variable_name = global.super_read_only
--let $assert_variable_value = 0
--source include/assert_variable.inc

--let $assert_text = Expect status 'super_read_only_disk_full' = OFF
--let $assert_cond = "[show status like \"super_read_only_disk_full\", Value, 1]" = "OFF"
--source include/assert.inc

--let $assert_text = Expect status 'offline_mode_disk_full' = OFF
--let $assert_cond = "[show status like \"offline_mode_disk_full\", Value, 1]" = "OFF"
--source include/assert.inc
