call mtr.add_suppression("Closed forcefully .* session.* left opened by plugin test_x_sessions_deinit");
##########################################
# Run plugin
##########################################
INSTALL PLUGIN test_x_sessions_deinit SONAME 'TEST_X_SESSIONS_DEINIT';
##########################################
# Stop plugin
##########################################
UNINSTALL PLUGIN test_x_sessions_deinit;
##########################################
# Plugin log
##########################################
========================================================================
Test in a server thread
nb_sessions = 1
srv_session_open 1 - Success
srv_session_close 1 - Success
srv_session_close 1 - Failed
Follows threaded run
========================================================================
init thread
nb_sessions = 1
srv_session_open 1 - Success
srv_session_close 1 - Success
srv_session_close 1 - Failed
deinit thread
Follows threaded run and leaves open session (Bug#21966621)
========================================================================
init thread
nb_sessions = 1
srv_session_open 1 - Success
deinit thread
========================================================================
init thread
nb_sessions = 1
srv_session_open 1 - Success
srv_session_close 1 - Success
srv_session_close 1 - Failed
deinit thread
Follows threaded run and leaves open session (Bug#21983102)
========================================================================
init thread
nb_sessions = 1
srv_session_open 1 - Success
deinit thread
