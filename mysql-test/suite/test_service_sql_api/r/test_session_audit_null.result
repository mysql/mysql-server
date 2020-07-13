##########################################
# Run audit_null plugin
##########################################
INSTALL PLUGIN null_audit SONAME 'adt_null.<expected_extension>';
# Before installing test_session_in_thd plugin
SHOW STATUS LIKE 'audit_null_connection%';
Variable_name	Value
Audit_null_connection_change_user	0
Audit_null_connection_connect	0
Audit_null_connection_disconnect	0
Audit_null_connection_pre_authenticate	0
##########################################
# Start plugin with audit null
##########################################
INSTALL PLUGIN test_session_in_thd SONAME 'TEST_SESSION_IN_THD';
##########################################
# Stop plugin
##########################################
UNINSTALL PLUGIN test_session_in_thd;
##########################################
# Plugin log
##########################################
Follows threaded run
============================================================================================
init thread
sql open session 0.
sql open session 1.
sql open session 2.
sql open session 3.
sql open session 4.
close following nb of sessions: 5
sql session close session 4.
sql session close session 3.
sql session close session 2.
sql session close session 1.
sql session close session 0.
sql open session 0.
sql open session 1.
sql open session 2.
sql open session 3.
sql open session 4.
close following nb of sessions: 5
sql session close session 0.
sql session close session 1.
sql session close session 2.
sql session close session 3.
sql session close session 4.
deinit thread
# After installing test_session_in_thd plugin
SHOW STATUS LIKE 'audit_null_connection%';
Variable_name	Value
Audit_null_connection_change_user	0
Audit_null_connection_connect	0
Audit_null_connection_disconnect	0
Audit_null_connection_pre_authenticate	0
##########################################
# Stop audit_null plugin
##########################################
UNINSTALL PLUGIN null_audit;
Warnings:
Warning	1620	Plugin is busy and will be uninstalled on shutdown
