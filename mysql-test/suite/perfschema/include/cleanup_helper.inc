
# Tests for PERFORMANCE_SCHEMA

update performance_schema.setup_instruments set enabled='YES';

connection con1;
disconnect con1;
--source include/wait_until_disconnected.inc
connection con2;
disconnect con2;
--source include/wait_until_disconnected.inc
connection con3;
disconnect con3;
--source include/wait_until_disconnected.inc

connection default;

