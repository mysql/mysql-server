## This is common file for verification binding on multiple addresses
##

--source include/restart_mysqld.inc

--let $assert_cond= [SELECT variable_value = "$expect_address" AS result FROM performance_schema.global_status WHERE variable_name LIKE "Mysqlx_address"] = 1
--let $assert_text= There status variable must point to $expect_address
--source include/assert.inc

--let $assert_cond= [SELECT @@global.Mysqlx_bind_address = "$expect_bind_address"] = 1
--let $assert_text= There variable must point to $expect_bind_address
--source include/assert.inc

