##########################################
# Run plugin
##########################################
INSTALL PLUGIN test_session_attach SONAME 'TEST_SESSION_ATTACH';
CREATE FUNCTION execute_test RETURNS INTEGER
SONAME 'TEST_SESSION_ATTACH';
##########################################
# Execute test case
##########################################
select execute_test();
execute_test()
0
##########################################
# Stop plugin
##########################################
DROP FUNCTION execute_test;
UNINSTALL PLUGIN test_session_attach;
##########################################
# Plugin log
##########################################
=========================================================================
Test in a server thread. Attach must fail on non srv_session thread.
Opening Session 1
Opening Session 2
Opening Session 3
Opening Session 4
Opening Session 5
Opening Session 6
Opening Session 7
Opening Session 8
Opening Session 9
Opening Session 10
=========================================================================

Query 01: SET SESSION test_session_attach_var_int = 10;

Query 02: SET SESSION test_session_attach_var_int = 20;

Query 03: SET SESSION test_session_attach_var_int = 30;

Query 04: SET SESSION test_session_attach_var_int = 40;

Query 05: SET SESSION test_session_attach_var_int = 50;

Query 06: SET SESSION test_session_attach_var_int = 60;

Query 07: SET SESSION test_session_attach_var_int = 70;

Query 08: SET SESSION test_session_attach_var_int = 80;

Query 09: SET SESSION test_session_attach_var_int = 90;

Query 10: SET SESSION test_session_attach_var_int = 100;
=========================================================================
Attach Session 1
Attach Session 1 failed.
Attach Session 2
Attach Session 2 failed.
Attach Session 3
Attach Session 3 failed.
Attach Session 4
Attach Session 4 failed.
Attach Session 5
Attach Session 5 failed.
Attach Session 6
Attach Session 6 failed.
Attach Session 7
Attach Session 7 failed.
Attach Session 8
Attach Session 8 failed.
Attach Session 9
Attach Session 9 failed.
Attach Session 10
Attach Session 10 failed.
=========================================================================
Close Session 1
Close Session 2
Close Session 3
Close Session 4
Close Session 5
Close Session 6
Close Session 7
Close Session 8
Close Session 9
Close Session 10
Closed all sessions
Follows threaded run. Successful scenario.
=========================================================================
init thread
Opening Session 1
Opening Session 2
Opening Session 3
Opening Session 4
Opening Session 5
Opening Session 6
Opening Session 7
Opening Session 8
Opening Session 9
Opening Session 10
=========================================================================

Query 01: SET SESSION test_session_attach_var_int = 10;

Query 02: SET SESSION test_session_attach_var_int = 20;

Query 03: SET SESSION test_session_attach_var_int = 30;

Query 04: SET SESSION test_session_attach_var_int = 40;

Query 05: SET SESSION test_session_attach_var_int = 50;

Query 06: SET SESSION test_session_attach_var_int = 60;

Query 07: SET SESSION test_session_attach_var_int = 70;

Query 08: SET SESSION test_session_attach_var_int = 80;

Query 09: SET SESSION test_session_attach_var_int = 90;

Query 10: SET SESSION test_session_attach_var_int = 100;
=========================================================================
Attach Session 1
Verify Session 1 variable
Detach Session 1
Attach Session 2
Verify Session 2 variable
Detach Session 2
Attach Session 3
Verify Session 3 variable
Detach Session 3
Attach Session 4
Verify Session 4 variable
Detach Session 4
Attach Session 5
Verify Session 5 variable
Detach Session 5
Attach Session 6
Verify Session 6 variable
Detach Session 6
Attach Session 7
Verify Session 7 variable
Detach Session 7
Attach Session 8
Verify Session 8 variable
Detach Session 8
Attach Session 9
Verify Session 9 variable
Detach Session 9
Attach Session 10
Verify Session 10 variable
Detach Session 10
=========================================================================
Close Session 1
Close Session 2
Close Session 3
Close Session 4
Close Session 5
Close Session 6
Close Session 7
Close Session 8
Close Session 9
Close Session 10
Closed all sessions
deinit thread
