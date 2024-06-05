--source ../include/have_debug.inc

SET GLOBAL DEBUG="+d, sleep_during_disconnect";

--connect (con2,localhost,root,,test)
--disconnect con2

--connection default
# Without the patch, I_S processlist should report one 'Killed' connection.
# With the patch, no connection should be reported as 'Killed' here.
--let $assert_text= No connection should be in state 'Killed'.
--let $assert_cond= "[select count(*) from information_schema.processlist where Command=\"Killed\"]" = 0
--source include/assert.inc

# If we comment out the line below, MTR's internal check testcase will fail
# without the patch because there is one connection still being 'Killed'.
SET GLOBAL DEBUG="-d, sleep_during_disconnect";
